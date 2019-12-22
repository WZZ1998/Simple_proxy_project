//
//  main.c
//  proxy_project
//
//  Created by 王梓州 on 2018/11/27.
//  Copyright © 2018 王梓州. All rights reserved.
//
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include "csapp.h"
#include "io_mul.h"

#define CONN_TRY_TIME 3
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
/* necessary content for a connecting event */
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void parse_uri(char *uri,char *hostname,char *path,int *port);
void build_http_header(char *http_header,char *hostname,char* path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port);
void func1(pool *p, int i)
{
    if(IsinSet(p->client_fd[i],&p->ready_read_set))
    {
        rio_t rio;
        int cfd = p->client_fd[i];
        char buf[MAXLINE];
        char method[MAXLINE],uri[MAXLINE],version[MAXLINE];
        char hostname[MAXLINE],path[MAXLINE];
        char endserver_http_header[MAXLINE];
        int port;
        //printf("[]processing a new added client\n");
        rio = p->clientrio[i];
        
        RemovefromSet(p, cfd, &p->read_set);
        
        /*!!!!!!!! change to while? */
        if(Rio_readlineb(&rio, buf, MAXLINE) != 0) {
            
            sscanf(buf,"%s %s %s",method,uri,version);
            if(strcasecmp(method,"GET"))
            {
                printf("********METHOD UNKNOWN:The method is %s \n",method);
                exit(1);
            }
            
            parse_uri(uri, hostname, path, &port);
            build_http_header(endserver_http_header, hostname, path, port, &rio);
            
            char portStr[100];
            sprintf(portStr, "%d",port);
            
            //printf("reading and parsing finish.Trying to connect server.\n");
            
            if(Get_socket_list(hostname, portStr, p, i) != 0)
            {
                printf("********DNS FAILURE:client_fd = %d\n",cfd);
                printf("The hostname and port is %s:%s\n",hostname,portStr);
                
                Stop_work(p, i);
                exit(1);
            }
            
            int end_fd;
            switch (New_try_open_serverfd(i, p, &end_fd)) {
                case 0:
                    Place_endserver(end_fd, p, i);
                    long len = strlen(endserver_http_header);
                    if(rio_writen(end_fd, endserver_http_header, len) != len)
                    {
                        printf("********WRITE SERVER FAILURE:The clientfd = %d,serverfd = %d \n ",cfd,end_fd);
                        Terminate_work(p, i);
                        break;
                    }
                    AddtoSet(p, end_fd, &p->read_set);
                    p->state[i] = 3;
                    break;
                case 1:
                    Save_http_header(p, i, endserver_http_header);
                    p->server_fd[i] = end_fd;
                    AddtoSet(p,end_fd, &p->write_set);
                    p->state[i] = 4;
                    break;
                case -1:
                    Save_http_header(p, i, endserver_http_header);
                    p->state[i] = 2;
                    break;
                default:
                    printf("********OPEN ENDSERVERFD RETURN LEAK:client_fd = %d\n",p->client_fd[i]);
                    exit(1);
                    break;
            }
        }
        else
        {
            printf("********CLIENT UMPTY LINE:clientfd = %d\n",cfd);
            Stop_work(p, i);
        }
    }
    return;
}
void func2(pool *p, int i)
{
    if((++(p->fail_time[i])) > CONN_TRY_TIME){
        printf("********FAILURE TOO MANY TIME:client_fd = %d\n",p->client_fd[i]);
        Free_http_header(p, i);
        Stop_work(p, i);
    }
    else
    {
        p->state[i] = 1;
        int end_fd;
        switch (New_try_open_serverfd(i, p, &end_fd)) {
            case 0:
                Place_endserver(end_fd,p,i);
                long len = strlen(p->end_http_header_p[i]);
                if(rio_writen(end_fd, p->end_http_header_p[i], len) != len)
                {
                    printf("********WRITE SERVER FAILURE:The clientfd = %d,serverfd = %d \n ",p->client_fd[i],end_fd);
                    Free_http_header(p, i);
                    Terminate_work(p, i);
                    break;
                }
                AddtoSet(p, end_fd, &p->read_set);
                Free_http_header(p, i);
                p->state[i] = 3;
                break;
            case 1:
                p->server_fd[i] = end_fd;
                AddtoSet(p,end_fd, &p->write_set);
                p->state[i] = 4;
                break;
            case -1:
                p->state[i] = 2;
                break;
            default:
                printf("********OPEN ENDSERVERFD RETURN LEAK:client_fd = %d\n",p->client_fd[i]);
                exit(1);
                break;
        }
    }
    return;
}
void func3(pool *p, int i)
{
    if(IsinSet(p->server_fd[i],&p->ready_read_set)){
        int sfd = p->server_fd[i];
        
        p->state[i] = 5;
    }
    return;
}
void func5(pool *p, int i)
{
    int cfd = p->client_fd[i];
    int sfd = p->server_fd[i];
    char buf[MAXLINE];
    int n = rio_readlineb(&p->serverrio[i], buf,MAXLINE);
    switch (n) {
        case -1:
            printf("********READ ENDSERVER FAILURE:The clientfd = %d,the endserver = %d \n ",cfd,sfd);
			RemovefromSet(p, sfd, &p->read_set);
            Terminate_work(p, i);
            break;
        case 0:
			RemovefromSet(p, sfd, &p->read_set);
            Terminate_work(p, i);
            break;
            
        default:
            if(rio_writen(cfd, buf, n) != n)
            {
                printf("********WRITE CLIENT FAILURE:The clientfd = %d \n ",cfd);
				RemovefromSet(p, sfd, &p->read_set);
                Terminate_work(p, i);
            }
            break;
    }
    return;
}
void func4(pool *p, int i)
{
    if(IsinSet(p->server_fd[i],&p->ready_write_set)){
        
        int sfd = p->server_fd[i];
        RemovefromSet(p, sfd, &p->write_set);
        int error = Check_nb_soc_state(sfd);
        if(error != 0)
        {
            /* special close situation */
            Close_nb_serverfd(p, i);
            p->state[i] = 2;
        }
        else
        {
            Set_serfd_to_block(p, i);
            Place_endserver(p->server_fd[i],p,i);
            long len = strlen(p->end_http_header_p[i]);
            if(rio_writen(p->server_fd[i], p->end_http_header_p[i], len) != len)
            {
                printf("********WRITE SERVER FAILURE:The clientfd = %d,serverfd = %d \n ",p->client_fd[i],p->server_fd[i]);
                Free_http_header(p, i);
                Terminate_work(p, i);
                return;
            }
            AddtoSet(p, p->server_fd[i], &p->read_set);
            Free_http_header(p, i);
            p->state[i] = 3;
        }
    }
    return;
}
/*  state 1:Normal added client
 state 2:connection fail client
 state 3:connected endserver
 state 4:fd in connecting progress
 */
void check_work_pool(pool *p)
{
    for(int i = p->maxi; (i >= 0); i--){
        if(p->client_fd[i] <= 0)
            continue;
        switch (p->state[i]) {
            case 1:
                func1(p, i);
                break;
            case 2:
                func2(p, i);
                break;
            case 3:
                func3(p, i);
                break;
            case 4:
                func4(p, i);
                break;
            case 5:
                func5(p, i);
                break;
            default:
                printf("********STATE LEAK:client_fd = %d\n",p->client_fd[i]);
                exit(1);
                break;
        }
    }
    //Make_file_record(p);
}

int main(int argc,char **argv)
{
    /* prepare fds */
    int listenfd,connfd;
    static pool pool_s;
    socklen_t clientlen;
    
    /* generic sockaddr struct which is 28 Bytes.The same use as sockaddr */
    struct sockaddr_storage clientaddr;
    struct timeval t_out;
    t_out.tv_sec = 0;
    t_out.tv_usec = 0;
    
    if(argc != 2)
    {
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }
    /* set handler to do with the SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);
    /* start listening(with given port) */
    listenfd = Open_listenfd(argv[1]);
    Init_pool(listenfd, &pool_s);
    
    //make_file_record(&pool_s);
    
    /* use the loop to accept th client */
    while(1)
    {
        /* reset the moni set everytime */
        pool_s.ready_read_set = pool_s.read_set;
        pool_s.ready_write_set = pool_s.write_set;
        if(pool_s.maxfd + 1 >= FD_SETSIZE){
            printf("********SELECT FDSET OVERLOADED:maxfd = %d\n",pool_s.maxfd);
            exit(1);
        }
        pool_s.nready = Select(pool_s.maxfd + 1, &pool_s.ready_read_set,&pool_s.ready_write_set,NULL,NULL);
        if(FD_ISSET(listenfd,&pool_s.ready_read_set)){
            //printf("[]processing listenfd = %d \n",listenfd);
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Add_client(connfd,&pool_s);
            AddtoSet(&pool_s, connfd, &pool_s.read_set);
        }
        
        check_work_pool(&pool_s);
        //printf("maxi = %d ,maxfd = %d\n",pool_s.maxi,pool_s.maxfd);
    }
    
    return 0;
}

void build_http_header(char *http_header,char *hostname,char* path,int port,rio_t *client_rio)
{
    char buf[MAXLINE],request_hdr[MAXLINE],other_hdr[MAXLINE],host_hdr[MAXLINE];
    /* request line */
    sprintf(request_hdr, requestlint_hdr_format, path);
    /* get other request header for client rio and change it properly */
    ssize_t n;
    while( (n = Rio_readlineb(client_rio, buf, MAXLINE) )> 0)
    {
        /* EOF */
        if(strcmp(buf,endof_hdr) == 0)
            break;
        /*Host : */
        if(!strncasecmp(buf,host_key,strlen(host_key)))
        {
            strcpy(host_hdr, buf);
            continue;
        }
        /* other things */
        if(!strncasecmp(buf, connection_key, strlen(connection_key)) && !strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key)) && !strncasecmp(buf, user_agent_key, strlen(user_agent_key)))
        {
            strcat(other_hdr,buf);
        }
    }
    if(strlen(host_hdr) == 0)
        sprintf(host_hdr, host_hdr_format,hostname);
    /* form the final result */
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);
    
    return;
}
/* parse thr uri to get hostname,file path,port */
void parse_uri(char *uri,char *hostname,char *path, int *port)
{
    /* default port */
    *port = 80;
    /* cut and get the hostname,port,path */
    char *pos = strstr(uri, "//");
    pos = pos != NULL ? pos + 2 : uri;
    char *pos2 = strstr(pos,":");
    if(pos2 != NULL)/* port contained */
    {
        *pos2 = '\0';
        sscanf(pos, "%s",hostname);
        sscanf(pos2 + 1, "%d%s",port,path);
    }
    else/* port not contained */
    {
        pos2 = strstr(pos,"/");
        if(pos2 != NULL)/*  not home */
        {
            *pos2 = '\0';
            sscanf(pos,"%s",hostname);
            *pos2 = '/';
            sscanf(pos2, "%s",path);
        }
        else/* home */
        {
            sscanf(pos, "%s",hostname);
        }
    }
    return;
}


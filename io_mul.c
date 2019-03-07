////////////////////////////////////////////////////////////////

#include "csapp.h"
#include "io_mul.h"
/*  You are only supposed to use 3 functions below
 when manipulating fd_set, since they can change the
 state of the pool.
 */

/* Add fd to set */
void AddtoSet(pool *p,int fd,fd_set *set)
{
    FD_SET(fd,set);
    if(fd > p->maxfd)
        p->maxfd = fd;
    return;
}
/* Remove fd from set */
void RemovefromSet(pool *p,int fd,fd_set *set)
{
    FD_CLR(fd,set);
    if(fd == p->maxfd)
    {
        int tmpfd = fd - 1;
        while(tmpfd >= 0 && !FD_ISSET(tmpfd, &p->read_set) && !FD_ISSET(tmpfd, &p->write_set))
            tmpfd--;
        p->maxfd = tmpfd;
    }
    return;
}
/* return 1 when fd is in set */
int IsinSet(int fd, fd_set *set)
{
    return FD_ISSET(fd,set);
}


/* Clear one item in the work pool */
void Clear_pool_item(pool *po, int i)
{
    if(i == po->maxi) {
        int tmp = i - 1;
        while(tmp >= 0 && po->client_fd[tmp] == -1)
            tmp--;
        po->maxi = tmp;
    }
    if(po->listp[i] != NULL)
        Freeaddrinfo(po->listp[i]);
    po->fail_time[i] = 0;
    po->socket_oldoption[i] = 0;
    po->state[i] = -1;
    po->client_fd[i] = -1;
    po->server_fd[i] = -1;
    po->listp[i] = NULL;
    po->p[i] = NULL;
    po->end_http_header_p[i] = NULL;
}

/* Init the pool */
void Init_pool(int listenfd, pool *po)
{
    printf("Init the whole pool!\n");
    int i;
    po->maxi = -1;
    po->maxfd = -1;
    for(i = 0; i < MAX_WORKBUF_SIZE; i++)
        Clear_pool_item(po, i);
   
    FD_ZERO(&po->read_set);
    FD_ZERO(&po->write_set);
    
    AddtoSet(po, listenfd, &po->read_set);
    return;
}

void Add_client(int connfd, pool *p)
{
    int i;
    for(i = 0;i < MAX_WORKBUF_SIZE;i++) {
        
        if(p->client_fd[i] < 0){
            p->client_fd[i] = connfd;
            p->state[i] = 1;
            Rio_readinitb(&p->clientrio[i],connfd);
            if(i > p->maxi)
                p->maxi = i;
            break;
        }
        
    }
    //printf("add clientfd = %d, to pool location %d \n",connfd,i);
    if(i == MAX_WORKBUF_SIZE)
    {
        printf("********ADD_CLIENT ERROR: Too many clients!\n");
        exit(1);
    }
}

/* Make fd the serverfd of item i and set rio */
void Place_endserver(int connfd,pool *p,int i)
{
    p->server_fd[i] = connfd;
    Rio_readinitb(&p->serverrio[i], connfd);
    return;
}

/* delete an item whose serverfd is not set. */
void Stop_work(pool *p, int i)
{
    Close(p->client_fd[i]);
    Clear_pool_item(p, i);
    return;
}

/* delete an item whose serverfd is already set. */
void Terminate_work(pool *p, int i)
{
    Close(p->client_fd[i]);
    Close(p->server_fd[i]);
    Clear_pool_item(p, i);
    return;
}

/* Given a writable nonblock fd, return 0 when it is connected, or not-zero when connection failed */
int Check_nb_soc_state(int fd)
{
    int c_error = -1;
    socklen_t length = sizeof(c_error);
    if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &c_error, &length) < 0) {
        printf("********GET SOCKET OPTION failed!\n");
        Close(fd);
        exit(1);
    }
    return c_error;
}
void Set_serfd_to_block(pool *po, int i)
{
    fcntl(po->server_fd[i], F_SETFL,po->socket_oldoption[i]);
}
void Close_nb_serverfd(pool *po, int i)
{
    Set_serfd_to_block(po, i);
    Close(po->server_fd[i]);
    return;
}
/* Given Hostname and port, set socket list for an item, return 0 when success,or -1 when failed */
int Get_socket_list(char *hostname,char *port,pool *po, int i)
{
    struct addrinfo hints;
    
    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;  /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV;  /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG;  /* Recommended for connections */
    //printf("DNS: The host name is %s, the port/service is %s \n",hostname,port);
    int rc = getaddrinfo(hostname, port, &hints, &(po->listp[i]) );
    if(rc != 0){
        po->listp[i] = NULL;
        return -1;
    }
    po->p[i] = po->listp[i];
    return 0;
}
/*  THIS IS A NON_BLOCK FUNCTION
    Make one try to connect in non-block way,return 0 when connection succeed.
    Attention: The function will return -1 when connection is going or failed.
    Error will be EINPROGRESS when TCP connection is in progress.
    Otherwise, The connection is surely failed.
    When the connection is OK or in progress, the fd will be in *waiting_fd.
*/
int Nonblock_try_connect(int i, pool *po, int *waiting_fd) {
    int try_fd = -1;
    /* Walk the list for one that we can successfully connect to */
    for (; po->p[i]; po->p[i] = po->p[i]->ai_next) {
        /* Create a socket descriptor */
        if ((try_fd = socket(po->p[i]->ai_family, po->p[i]->ai_socktype, po->p[i]->ai_protocol)) < 0)
            continue; /* Socket failed, try he next */
        //printf("success in finding a socket fd,it is %d\n", clientfd);
        break;
    }
    if(!po->p[i]){
        printf("********ALL SOCKET FAILURE:the client_fd = %d\n",po->client_fd[i]);
        *waiting_fd = -1;
        return -1;
    }
    
    po->socket_oldoption[i] = fcntl(try_fd, F_GETFL);
    int new_option = po->socket_oldoption[i] | O_NONBLOCK;
    fcntl(try_fd, F_SETFL,new_option);
    int res = connect(try_fd, po->p[i]->ai_addr, po->p[i]->ai_addrlen);
    *waiting_fd = try_fd;
    return res;
}


/*  make one try to connect target server and get fd in non-block way.
    success : return 0, the fd will be set to block.
    inprogress ： return 1, keep the fd nonblock
    Surely failed : return -1, no fd available.
*/

int New_try_open_serverfd(int i, pool *po, int *endserverfd)
{
    if(Nonblock_try_connect(i, po, endserverfd) == 0)
    {
        fcntl(*endserverfd, F_SETFL,po->socket_oldoption[i]);
        return 0;
    }

    if (errno == EINPROGRESS)
        return 1;
    if(*endserverfd > 0)
    {
        fcntl(*endserverfd, F_SETFL,po->socket_oldoption[i]);
        Close(*endserverfd);
    }
    return -1;
}
/*  help you save *http_hd to *http_header_p. Malloc used.
    You MUST use Save_http_header() to free.
 */
void Save_http_header(pool *po, int i, char *http_hd)
{
    po->end_http_header_p[i] = (char*)malloc(sizeof(char) * strlen(http_hd));
    memcpy(po->end_http_header_p[i],http_hd,strlen(http_hd));
    return;
}
/* Free http_header_p. */
void Free_http_header(pool *po,int i)
{
    free(po->end_http_header_p[i]);
    return;
}

/*     Print some pool state to a file. Use vis_buf.py to
 check it graphically.
 */
int Make_file_record(pool *p)
{
    FILE *fp;
    fp = fopen("buf_record", "wb");
    if(fp == NULL)
    {
        printf("File Open Error!");
        return -1;
    }
    for(int i = 0;i < VIS_SIZE;i++)
        fprintf(fp, "%d\n",p->state[i]);
    fclose(fp);
    usleep(2000);
    return 0;
}


/////////////////////////////////////////////////////////////

#define MAX_WORKBUF_SIZE 8192
#define VIS_SIZE 500

/* Multi-I/O work pool */
typedef struct {
    int maxfd;
    int nready;
    fd_set read_set;
    fd_set write_set;
    fd_set ready_read_set;
    fd_set ready_write_set;
    int maxi;
    int socket_oldoption[MAX_WORKBUF_SIZE];
    int fail_time[MAX_WORKBUF_SIZE];
    int state[MAX_WORKBUF_SIZE];
    int client_fd[MAX_WORKBUF_SIZE];
    int server_fd[MAX_WORKBUF_SIZE];
    struct addrinfo *listp[MAX_WORKBUF_SIZE];
    struct addrinfo *p[MAX_WORKBUF_SIZE];
    char *end_http_header_p[MAX_WORKBUF_SIZE];
    rio_t clientrio[MAX_WORKBUF_SIZE];
    rio_t serverrio[MAX_WORKBUF_SIZE];
    
} pool;

/*  Wrappers for fd_set usage */
void AddtoSet(pool *p,int fd,fd_set *set);
void RemovefromSet(pool *p,int fd,fd_set *set);
int IsinSet(int fd, fd_set *set);

/* 	work function for the pool */ 
void Clear_pool_item(pool *po, int i);
void Init_pool(int listenfd, pool *po);
void Add_client(int connfd, pool *p);
void Place_endserver(int connfd,pool *p,int i);
void Stop_work(pool *p, int i);
void Terminate_work(pool *p, int i);

/* 	Non-block programming wrappers. Use carefully. 
	Attention:Most of them work for one pool item.
 */
int Check_nb_soc_state(int fd);
void Set_serfd_to_block(pool *po, int i);
void Close_nb_serverfd(pool *po, int i);
int Get_socket_list(char *hostname,char *port,pool *po, int i);
int Nonblock_try_connect(int i, pool *po, int *waiting_fd);
int New_try_open_serverfd(int i, pool *po, int *endserverfd);

/*	Help you to save the http header content to one pool item.
	Useful when the connection result is failed or unknown.
*/
void Save_http_header(pool *po, int i, char *http_hd);
void Free_http_header(pool *po,int i);

/* 	Print some pool state to a file. Use vis_buf.py to 
	check it graphically.
*/
int Make_file_record(pool *p);

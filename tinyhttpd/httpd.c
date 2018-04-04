#include "threadpool.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#define LISTENQ 1024
#define MAX_SPECIFIC_DATA 20
#define MAX_SPECIFIC_DATA_SIZE 1024
#define TOTALBUFSIZE 4096
#define MAXEVENTS 4096
#define SERV_STRING "Server: jlxhttpd\r\n"
#define MAXFILETYPES 10

pthread_once_t buf_once = PTHREAD_ONCE_INIT;
pthread_key_t buf_key[MAX_SPECIFIC_DATA];
pthread_key_t buf_id;
char buf[1024], PATH[512];
char file_types[MAXFILETYPES][2][64] = {
    { "html", "text/html" },
    { "jpg", "image/jpeg" }
};
thread_pool *pool = NULL;
int get_line(int fd, char *buf, int size);

void *malloc_specific_data()
{
    int *id = (int*)pthread_getspecific(buf_id);
    ++(*id);
    return pthread_getspecific(buf_key[(*id)-1]);
}

void buf_init_once(void)
{
    for(int i=0; i<MAX_SPECIFIC_DATA; ++i)
        pthread_key_create(&buf_key[i], free);
    pthread_key_create(&buf_id, free);
}

void err_quit(const char* str)
{
    perror(str);
    exit(1);
}

/***************************************/
/* Read and discard the header of HTTP
/* packet.
/***************************************/
void discard_headers(int fd, char* trash_can)
{
    trash_can[0]='A';trash_can[1]='\0';
    int numchars = 1;
    while((numchars>0)&&strcmp("\n", trash_can))
    {
        numchars = get_line(fd, trash_can, MAX_SPECIFIC_DATA_SIZE);
    }
}

/***************************************/
/* Description: send the contents of 
/* file specified with filename to fd
/***************************************/
void cat(int fd, const char *filename)
{
    FILE *resource = fopen(filename, "r");
    char *buf = malloc_specific_data();
    fgets(buf, MAX_SPECIFIC_DATA_SIZE, resource);
    while(!feof(resource))
    {
        send(fd, buf, strlen(buf), 0);
        fgets(buf, MAX_SPECIFIC_DATA_SIZE, resource);
    }
    fclose(resource);
}

/***************************************/
/* Description: send the contents of 
/* file specified with filename to fd
/* by bytes flow.
/***************************************/
void cat_bytes(int fd, const char *filename)
{
    FILE *resource = fopen(filename, "rb");
    char *buf = malloc_specific_data();
    size_t n;
    n = fread(buf, 1, 1024, resource);
    while(!feof(resource))
    {
        send(fd, buf, n, 0);
        n = fread(buf, 1, 1024, resource);
    }
    fclose(resource);
}

const char* get_filetype(char *buf, const char *filename)
{
    char *tmp = malloc_specific_data();
    int i = 0;
    while(filename[i]!='\0'&&filename[i]!='.') ++i;
    if(filename[i] == '\0') strcpy(buf, "text/html");
    else 
    {
        int j = 0;
        ++i;
        while(filename[i]!='\0') 
            tmp[j++]=filename[i++];
        tmp[j]='\0';
        for(i=0; i<MAXFILETYPES; ++i)
            if(strcasecmp(tmp, file_types[i][0]) == 0)
            {
                strcpy(buf, file_types[i][1]);
                break;
            }
    }
    return buf;
}

void unimplemented(int fd)
{
    char *buf = malloc_specific_data();
    discard_headers(fd, buf);
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, SERV_STRING);
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Connection: closed\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(fd, buf, strlen(buf), 0);
    char *filename = malloc_specific_data();
    sprintf(filename, "%s/501.html",PATH);
    cat(fd, filename);
}

void not_found(int fd)
{
    char *buf = malloc_specific_data();
    discard_headers(fd, buf);
    sprintf(buf, "HTTP/1.0 404 NOTFOUND\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, SERV_STRING);
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Connection: closed\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(fd, buf, strlen(buf), 0);
    char *filename = malloc_specific_data();
    sprintf(filename, "%s/404.html",PATH);
    cat(fd, filename);
}

void respond_ok(int fd, const char *filename)
{
    char *buf = malloc_specific_data();
    discard_headers(fd, buf);
    char *ft = malloc_specific_data();
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, SERV_STRING);
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: %s\r\n", get_filetype(ft, filename));
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Connection: closed\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(fd, buf, strlen(buf), 0);
    //cat(fd, filename);
    if(strcmp(ft, "image/jpeg") == 0) 
        cat_bytes(fd, filename);
    else cat(fd, filename);
}


void *accept_request(void *arg)
{
    printf("start accept request\n");
    pthread_once(&buf_once, buf_init_once);
    /* init thread-specific data  */
    for(int i=0; i<MAX_SPECIFIC_DATA; ++i)
    {
        char *tbuf;
        if((tbuf = pthread_getspecific(buf_key[i])) == NULL)
        {
            tbuf = (char*)malloc(MAX_SPECIFIC_DATA_SIZE);
            pthread_setspecific(buf_key[i], tbuf);
        }
    }
    {
        int *id = NULL;
        if((id = pthread_getspecific(buf_id)) == NULL)
        {
            id = (int*)malloc(sizeof(int));
            pthread_setspecific(buf_id, id);
        }

    }
    int clifd = *(int*)arg;
    printf("start to process request on socket %d\n", clifd);
    char *buf = malloc_specific_data();
    char *url = malloc_specific_data();
    char *method = malloc_specific_data();
    char *path = malloc_specific_data();
    int i,j;
    struct stat st;
    int numchars = get_line(clifd, buf, MAX_SPECIFIC_DATA_SIZE);
    i=0;j=0;
    while(!isspace(buf[i]) && (i<MAX_SPECIFIC_DATA_SIZE-1))
    {
        method[i] = buf[i];
        ++i;
    }
    j=i;
    method[i] = '\0';
    if(strcasecmp(method, "GET"))
    {
        unimplemented(clifd);
        close(clifd);
        return NULL;
    }
    i=0;
    /* ignoring spaces */
    while(isspace(buf[j]) && j<numchars) 
        ++j;
    while(!isspace(buf[j])&&i<MAX_SPECIFIC_DATA_SIZE-1&&j<numchars)
    {
        url[i++]=buf[j++];
    }
    url[i]='\0';
    sprintf(path, "%s%s", PATH, url);
    printf("method is %s, path is %s\n", method, path);
    if(path[strlen(path)-1] == '/')
        strcat(path, "index.html");
    if(stat(path, &st) == -1){
        printf("not found start\n");
        not_found(clifd);
    }
    else 
    {
        if((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        if(stat(path, &st) == -1){
            not_found(clifd);
        }
        respond_ok(clifd, path);
    }
    printf("connection terminated\n");
    close(clifd);
    return NULL;
}

/***************************************/
/* Description: read a line from socket.
/* It is guaranteed the returned line 
/* ended with '\n'.
/* Return value: the length of theline.*/
/***************************************/
int get_line(int fd, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while(i<size-1 && c != '\n')
    {
        n = recv(fd, &c, 1, 0);
        if(n > 0)
        {
            if(c == '\r')
            {
                n = recv(fd, &c, 1, MSG_PEEK);
                if(n>0 && c=='\n')
                    recv(fd, &c, 1, 0);
                else c = '\n';
            }
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}
int main(int argc, char **argv)
{
    if(argc != 3)
    {
        fprintf(stderr, "usage: %s <directory> <port>", argv[0]);
        exit(0);
    }
    strcpy(PATH, argv[1]);
    int pathlen = strlen(PATH);
    if(PATH[pathlen-1] == '/')
    {
        PATH[pathlen-1] = '\0';
    }
    int listenfd, epfd, on=1;
    struct sockaddr_in servaddr, cliaddr, sockname;
    socklen_t clilen;
    struct epoll_event events[MAXEVENTS], evt;
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        err_quit("socket");
    /* set SO_REUSEADDR to 1 */
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        err_quit("setsockopt");
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(argv[2]));
    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
        err_quit("bind");
    if(listen(listenfd, LISTENQ) < 0)
        err_quit("listen");
    if((epfd = epoll_create(LISTENQ)) < 0)
        err_quit("epoll_create");
    bzero(&evt, sizeof(evt));
    evt.events = EPOLLIN | EPOLLERR;
    evt.data.fd = listenfd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &evt) < 0)
        err_quit("epoll_ctl");
    pool = (thread_pool*)malloc(sizeof(thread_pool));
    pool_init(pool, MAXEVENTS);
    printf("server start listening\n");
    char ch;
    for(;;)
    {
        int n = epoll_wait(epfd, events, MAXEVENTS, -1);
        for(int i=0; i<n; ++i)
        {
            if(events[i].data.fd == listenfd)
            {
                if(events[i].events & EPOLLERR)
                {
                    fprintf(stderr, "listen socket failed\n");
                    exit(1);
                }
                else if(events[i].events & EPOLLIN)
                {
                	clilen = sizeof(cliaddr);
                    int clifd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
                    bzero(&evt, sizeof(evt));
                    evt.events = EPOLLIN | EPOLLERR;
                    evt.data.fd = clifd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, clifd, &evt);
                    printf("connection from %s:%d was built\n", inet_ntop(AF_INET, &(cliaddr.sin_addr.s_addr), buf, sizeof(buf)),
                            ntohs(cliaddr.sin_port));
                }
            }
            else 
            {
                if(events[i].events & EPOLLERR)
                {
                    fprintf(stderr, "connection socket failed\n");
                    close(events[i].data.fd);
                }
                else if(events[i].events & EPOLLIN)
                {
                    printf("EPOLLIN accepted\n");
                    int clifd = events[i].data.fd;
                    printf("before add task\n");
                    pool_add_task(pool, accept_request, (void*)&clifd);
                    printf("after add task");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clifd, NULL);
                }
            }
        }
    }
    close(listenfd);
    pool_destroy(pool);
}

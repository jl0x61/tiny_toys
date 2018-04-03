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
#define MAXEVENTS 4096
#define SERV_STRING "Server: jlxhttpd\r\n"
char buf[1024], PATH[512];
thread_pool *pool = NULL;
int get_line(int fd, char *buf, int size);
void err_quit(const char* str)
{
    perror(str);
    exit(1);
}

void discard_headers(int fd)
{
    char buf[1024];
    buf[0]='A';buf[1]='\0';
    int numchars = 1;
    while((numchars>0)&&strcmp("\n", buf))
    {
        numchars = get_line(fd, buf, sizeof(buf));
    }
}

void cat(int fd, const char *filename)
{
    FILE *resource = fopen(filename, "r");
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource))
    {
        send(fd, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
    fclose(resource);
}

void unimplemented(int fd)
{
    discard_headers(fd);
    char buf[1024];
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
    sprintf(buf, "%s/501.html",PATH);
    cat(fd, buf);
}

void not_found(int fd)
{
    discard_headers(fd);
    char buf[1024];
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
    sprintf(buf, "%s/404.html",PATH);
    cat(fd, buf);
}

void serve_file(int fd, const char *filename)
{
    discard_headers(fd);
    char buf[1024];
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, SERV_STRING);
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Connection: closed\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(fd, buf, strlen(buf), 0);
    cat(fd, filename);
}


void *accept_request(void *arg)
{
    int clifd = *(int*)arg;
    printf("start to process request on socket %d\n", clifd);
    char buf[1024], url[255], method[255], path[255]; // xingneng
    int numchars;
    int i,j;
    struct stat st;
    numchars = get_line(clifd, buf, sizeof(buf));
    i=0;j=0;
    while(!isspace(buf[i]) && (i<sizeof(method)-1))
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
    while(!isspace(buf[j])&&i<sizeof(url)-1&&j<numchars)
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
        serve_file(clifd, path);
    }
    printf("connection terminated\n");
    close(clifd);
    return NULL;
}
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
                    int clifd = events[i].data.fd;
                    pool_add_task(pool, accept_request, (void*)&clifd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clifd, NULL);
                }
            }
        }
    }
    close(listenfd);
    pool_destroy(pool);
}

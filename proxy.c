#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAXLINE 8192

struct Uri{
    char host[MAXLINE],port[MAXLINE],path[MAXLINE];
};

void parse(char *uri, struct Uri *data){
    char *host_start = strstr(uri, "//");
    if(host_start != NULL){
        host_start += 2;
    }else{
        host_start = uri;
    }

    char *path_start = strchr(host_start, '/');
    char *end;

    if(path_start == NULL){
        end = host_start + strlen(host_start);
        strcpy(data->path,"/");
    }else{
        end = path_start;
        strcpy(data->path,path_start);
    }

    char *port_start = strchr(host_start, ':');

    if(port_start != NULL && port_start < end){
        strncpy(data->host, host_start, port_start - host_start);
        data->host[port_start - host_start] = '\0';

        strncpy(data->port,
                port_start + 1,
                end - port_start - 1);
        data->port[end - port_start - 1] = '\0';
    }else{
        strncpy(data->host, host_start, end - host_start);
        data->host[end - host_start] = '\0';
        strcpy(data->port,"80");
    }
}

void build_header(char *server, struct Uri *data, rio_t *rio){
    char buf[MAXLINE];
    char host_header[MAXLINE];

    server[0] = '\0';
    host_header[0] = '\0';

    sprintf(server, "GET %s HTTP/1.0\r\n", data->path);

    while(Rio_readlineb(rio, buf, MAXLINE) > 0){
        if(!strcmp(buf, "\r\n")){
            break;
        }

        if(!strncasecmp(buf, "Host:", 5)){
            strcpy(host_header, buf);
        }else if(!strncasecmp(buf, "Connection:", 11)){
            continue;
        }else if(!strncasecmp(buf, "Proxy-Connection:", 18)){
            continue;
        }else if(!strncasecmp(buf, "User-Agent:", 11)){
            continue;
        }else{
            strcat(server, buf);
        }
    }

    if(host_header[0] != '\0'){
        strcat(server, host_header);
    }else{
        sprintf(buf, "Host: %s\r\n", data->host);
        strcat(server, buf);
    }

    // strcat(server, user_agent_hdr);
    strcat(server, "Connection: close\r\n");
    strcat(server, "Proxy-Connection: close\r\n");
    strcat(server, "\r\n");
}

/* You won't lose style points for including this long line in your code */
void doit(int fd){
    rio_t rio,server_rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE],server[MAXLINE];
    Rio_readinitb(&rio, fd);
    if(Rio_readlineb(&rio, buf, MAXLINE) <= 0){
        return;
    }
    if(sscanf(buf, "%s %s %s",method, uri, version) != 3){
        return;
    }
    if(strcasecmp(method, "GET") != 0){
        return;
    }
    struct Uri data;
    parse(uri, &data);
    build_header(server, &data, &rio);
    int serverfd = Open_clientfd(data.host, data.port);
    if(serverfd < 0){
        printf("connection failed\n");
        return;
    }
    Rio_writen(serverfd,server, strlen(server));
    Rio_readinitb(&server_rio, serverfd);
    ssize_t n;
    while((n=Rio_readnb(&server_rio, buf, MAXLINE)) > 0){
        Rio_writen(fd, buf, n);
    }
    Close(serverfd);
}

void *thread(void *vargp){
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

int main(int argc, char **argv)
{
    if(argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    struct sockaddr_storage addr;
    socklen_t addrlen;
    signal(SIGPIPE, SIG_IGN);

    int listenfd = Open_listenfd(argv[1]);

    while(1) {
        addrlen = sizeof(addr);
        int connfd = Accept(listenfd,(SA *)&addr,&addrlen);
        int *connfd_ = malloc(sizeof(int));
        *connfd_ = connfd;
        pthread_t tid;
        Pthread_create(&tid, NULL, thread, connfd_);
    }

    // printf("%s", user_agent_hdr);
    return 0;
}
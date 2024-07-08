#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "threadPool.h"
typedef struct Sockinfo{
    struct sockaddr_in addr;
    int cfd;
}Sockinfo;
Sockinfo infos[512];

typedef struct Poolinfo{
    ThreadPool *pool;
    int fd;
}Poolinfo;
void working(void *arg);
void acceptConnect(void *arg);

int main() {
    //1.监听
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    //2.绑定
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(7890);
    saddr.sin_addr.s_addr = INADDR_ANY;  //0 = 0.0.0.0
    int ret = bind(fd, (struct sockaddr *) &saddr, sizeof(saddr));
    if (ret == -1) {
        perror("bind");
        return -1;
    }
    //3.设置监听
    ret = listen(fd, 128);
    if (ret == -1) {
        perror("bind");
        return -1;
    }

    //4.阻塞并等待客户端的连接
    int max = sizeof(infos) / sizeof(infos[0]);
    for (int i = 0; i < max; ++i) {
        bzero(&infos, sizeof(infos[i]));
        infos[i].cfd = -1;
    }
    ThreadPool *pool = threadPoolCreate(3,10,100);
    Poolinfo* poolinfo = (Poolinfo*) malloc(sizeof(Poolinfo));
    poolinfo->pool = pool;
    poolinfo->fd = fd;
    threadPoolAdd(pool, acceptConnect, poolinfo);
    pthread_exit(NULL);
    return 0;
}
void acceptConnect(void *arg){
    int addrlen = sizeof(struct sockaddr_in);
    Poolinfo* pPoolinfo = (Poolinfo*)arg;
    while (1) {
        Sockinfo* pSockinfo;
        pSockinfo =(Sockinfo*) malloc(sizeof(Sockinfo));
        pSockinfo->cfd = accept(pPoolinfo->fd, (struct sockaddr *) &pSockinfo->addr, &addrlen);

        if (pSockinfo->cfd == -1) {
            perror("accept");
            break;
        }
        threadPoolAdd(pPoolinfo->pool, working, pSockinfo);
    }
    close(pPoolinfo->fd);

}

void working(void *arg) {
        char ip[32];
        Sockinfo* infoPtr = (Sockinfo*)arg;
        printf("客户端的IP:%s, 端口：%d\n",
               inet_ntop(AF_INET, &infoPtr->addr.sin_addr.s_addr, ip, sizeof(ip)),
               ntohs(infoPtr->addr.sin_port));
        while (1) {
            char buff[1024];
            memset(buff, 0, sizeof(buff));
            int len = recv(infoPtr->cfd, buff, sizeof(buff), 0);
            if (len > 0) {
                printf("client says:%s port:%d\n", buff,ntohs(infoPtr->addr.sin_port));
                send(infoPtr->cfd, buff, len, 0);
            } else if (len == 0) {
                perror("client disconnected");
                break;
            } else {
                perror("recv");
                break;
            }
        }

        close(infoPtr->cfd);
        infoPtr->cfd = -1;

    }

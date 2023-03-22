#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>

#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);

void addsig(int sig,void(handler)(int),bool restart = true){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler =  handler;
    if(restart){
        sa.sa_flags |= SA_RESTART; //设置信号处理行为，重新调用被该信号终止的系统调用
    }
    //设置信号掩码，在当前信号的处理期间，不会处理其他信号
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL) != -1);
}

void show_error(int sockfd,const char* info){//内部错误，将消息发送给客户端
    printf("%s\n",info);
    send(sockfd,info,strlen(info),0);
    close(sockfd);
}

int main(int argc,char* argv[]){
    if(argc <= 2){
        printf("usage:%s ip port\n",basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    addsig(SIGPIPE,SIG_IGN); //当handler为SIG_IGN,表示收到该信号时忽略它

    threadpool<http_conn>* pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch(...)
    {
        printf("error\n");
        return 1;
    }
    printf("creat a threadpool\n");

    http_conn* users = new http_conn[MAX_FD];
    assert(users);
    int user_count = 0;
    printf("create some users\n");

    sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET6,ip,&addr.sin_addr);
    addr.sin_port = htons(port);

    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);
    linger tmp = {1,0};
    setsockopt(listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));

    int ret = bind(listenfd,(sockaddr*)&addr,sizeof(addr));
    assert(ret != -1);

    ret = listen(listenfd,5);
    assert(ret != -1);
    printf("listening\n");

    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    epoll_event events[MAX_EVENT_NUMBER];
    addfd(epollfd,listenfd,false);
    http_conn::m_epollfd = epollfd;

    while(true){
        int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number < 0) && (errno!=EAGAIN)){
            printf("epoll failure\n");
            break;
        }

        for(int i=0;i<number;++i){
            int sockfd = events[i].data.fd;
            //当该sockfd为listenfd时，接受连接
            if(sockfd == listenfd){
                sockaddr_in clientAddr;
                socklen_t len = sizeof(clientAddr);
                int connfd = accept(listenfd,(sockaddr*)&clientAddr,&len);
                if(connfd < 0){
                    printf("accept errno\n");
                    continue;
                }
                if(http_conn::m_user_count >= MAX_FD){
                    show_error(connfd,"Internal server busy\n");
                    continue;
                }
                users[connfd].init(connfd,clientAddr); //用connfd来标识唯一的连接
            }
                //当错误时，关闭
            else if(events[i].events & (EPOLLRDBAND | EPOLLHUP | EPOLLERR)){
                users[sockfd].close_conn();
            }
                //当有读写事件发生时，插入请求队列
            else if(events[i].events & EPOLLIN){
                if(users[sockfd].read()){
                    pool->append(users + sockfd);
                }
                else{
                    users[sockfd].close_conn();
                }
            }
            else if(events[i].events & EPOLLOUT){
                if(!users[sockfd].write()){
                    users[sockfd].close_conn();
                }
                printf("write ok\n\n\n");
            }
            else{}
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}

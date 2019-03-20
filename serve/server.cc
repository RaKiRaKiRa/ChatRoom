#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define USER_LIMIT 5         //最大用户数
#define BUFFER_SIZE 64       //接收缓存区大小
#define FD_LIMIT 65535       //文件描述符限制

struct client_data{
    sockaddr_in address;
    char* write_buf;         //发送缓存
    char buf[ BUFFER_SIZE ]; //接收缓存
};

int set_nonblocking( int fd ){
    int old_opt = fcntl( fd, F_GETFL );
    int new_opt = old_opt & O_NONBLOCK;
    fcntl( fd, F_SETFL, new_opt );
    return old_opt;
}

int main( int argc, char* argv[] ){
    if( argc <= 2){
        printf("usage : %s address and port", argv[1]);
    }

    const char *ip = argv[1];
    int port = atoi( argv[2] );
    sockaddr_in server;
    bzero( &server, sizeof( server ) );
    server.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &server.sin_addr );
    server.sin_port = htons( port );

    //创建监听接口
    int listenfd = socket( AF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0);
    bind(listenfd, (sockaddr*)&server, (socklen_t)sizeof(server));
    int ret = listen( listenfd, USER_LIMIT );
    assert( ret != -1);

    client_data* user = new client_data[FD_LIMIT];
    pollfd fds[FD_LIMIT + 1];
    int user_counter = 0;
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;
    for(int i = 1; i < FD_LIMIT; ++i){
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    while( 1 ){
        ret = poll( fds, user_counter + 1, -1);
        if( ret < 0 ){
            printf( "poll failed\n " );
            break;
        }
        //轮询
        for(int i = 0; i <= user_counter ; ++i){
            //新连接
            if( fds[i].fd == listenfd && fds[i].revents & POLLIN){
                sockaddr_in client;
                socklen_t addr_size = sizeof( client );
                bzero( &client, addr_size );
                client.sin_family = AF_INET;
                int connfd = accept( listenfd, (sockaddr*)&client, &addr_size );
                //连接失败
                if( connfd < 0){
                    printf( "connection failed: %s\n", errno );
                    continue;
                }
                //连接过多
                if( user_counter >= USER_LIMIT ){
                    char* warn = "TOO MANY USER!\n";
                    send( connfd, warn, strlen(warn), 0);
                    close(connfd);
                    printf( "TOO MANY USER!\n" );
                    continue;
                }

                //正常连接
                ++user_counter;
                set_nonblocking( connfd );
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLERR | POLLRDHUP;
                fds[user_counter].revents = 0;
                user[connfd].address = client;
                printf("a new user join,now have $d user(s)\n ", user_counter );
            }
            //已连接出错
            else if( fds[i].revents & POLLERR ){
                printf( "get a error form %d\n", fds[i].fd );
                /*



                */
            }
            //客户端断开连接
            else if( fds[i].revents & POLLRDHUP){
                //user[fds[i].fd] = user[fds[user_counter].fd];
                close(fds[i].fd);
                printf( "a client left, close the connection with %d ", fds[i].fd );
                fds[i] = fds[user_counter];
                --user_counter;
                --i;
            }

            //客户端发送程序
            else if( fds[i].revents & POLLIN ){
                int connfd = fds[i].fd;
                memset( user[connfd].write_buf, '\0', BUFFER_SIZE );
                int ret = recv( connfd, user[connfd].write_buf, BUFFER_SIZE - 1, 0);
                //recv出错
                /*
                errno被设为以下的某个值 

                EAGAIN：套接字已标记为非阻塞，而接收操作被阻塞或者接收超时 
                EBADF：sock不是有效的描述词 
                ECONNREFUSE：远程主机阻绝网络连接 
                EFAULT：内存空间访问出错 
                EINTR：操作被信号中断 
                EINVAL：参数无效 
                ENOMEM：内存不足 
                ENOTCONN：与面向连接关联的套接字尚未被连接上 
                ENOTSOCK：sock索引的不是套接字 当返回值是0时，为正常关闭连接；
                */
                if(ret < 0){
                    if( errno != EAGAIN ){
                        close( connfd );
                        //user[fds[i].fd] = user[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        --user_counter;
                        --i;
                    }
                }
                //如果recv函数在等待协议接收数据时网络中断了，那么它返回0
                else if (ret == 0){

                }
                //接收到数据
                else{
                    //除了收到数据的用户，向其他用户发送数据
                    for( int j = 1; j <= user_counter; ++j){
                        if( connfd == fds[j].fd ){
                            continue;
                        }
                        user[fds[j].fd].write_buf = user[connfd].buf; 
                        fds[j].events &= ~POLLIN;
                        fds[j].events &= POLLOUT;
                    }
                }
            }
            //发送数据
            else if( fds[i].revents & POLLOUT ){
                int connfd = fds[i].fd;
                char* info = user[fds[i].fd].write_buf;
                send( connfd, info, strlen(info), 0);
                user[fds[i].fd].write_buf = NULL;
                fds[i].events &= ~POLLOUT;
                fds[i].events &= POLLIN;

            }

        }
    }
    delete[] user;
    close( listenfd );

    return 0;
}
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE 64
int main(int argc, char* argv[]){
    if(argc <= 2)
        printf( "usage : %s ip_address and porty_number\n", basename( argv[0]) );
    struct sockaddr_in server_addr;
    const char* ip = argv[1];
    int port = atoi( argv[2] );
    //初始化
    bzero( &server_addr, sizeof( server_addr ) );
    inet_pton( AF_INET, ip, &server_addr.sin_addr );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( port );

    int sockfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( socket >= 0 );
    if( connect( sockfd, (const sockaddr*)&server_addr, sizeof(server_addr)) < 0 ){
        printf( " connetion failed\n" );
        close( sockfd );
        return 1;
    }

    pollfd fds[2];
    //fds[0]监听标准输入
    fds[0].events = POLLIN;
    fds[0].fd = 0;
    fds[0].revents = 0;
    //fds[1]监听服务器连接
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].fd = sockfd;
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe( pipefd );
    assert( ret != -1 );

    while( 1 ){

        ret = poll( fds, 2, -1) ;

        if( ret < 0 ){//连接失败
            printf( "poll failed \n" );
            break;
        }
        else if( fds[1].revents & POLLRDHUP ){//服务器关闭连接
            printf( "server close the connection\n" );
            break;
        }
        else if( fds[1].revents & POLLIN ){//服务器发来数据
            memset( read_buf, '\0', BUFFER_SIZE );
            recv( sockfd, read_buf, BUFFER_SIZE - 1, 0);
            printf(" %s\n" , read_buf );
        }

        if(fds[0].revents & POLLIN ){//标准输入，发送至服务器
            ret = splice( 0, NULL, pipefd[0], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );
            ret = splice( pipefd[1], NULL, sockfd, NULL,32768, SPLICE_F_MORE | SPLICE_F_MOVE );
        }
        
    }

    close( sockfd );
    return 0;
}
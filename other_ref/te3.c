#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
    
#define SERV_PORT         8031
#define MAXDATASIZE       1024
#define FD_SET_SIZE        128
  
  
   
int main(void)
{
    int fd_ready_num;               // select返回的准备好的描述符个数     
    int listenfd, connectfd, maxfd, scokfd;   
    struct sockaddr_in serv_addr;
    fd_set read_set, allset;
  
    int client[FD_SETSIZE];
    int i;
    int maxi = -1;
      
    int sin_size;               //地址信息结构体大小
      
    char recvbuf[MAXDATASIZE];
    int len;
  
  
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  
    {                           
        perror("套接字描述符创建失败");  
        exit(1);  
    }
  
    printf("listenfd :%d\n", listenfd);
  
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
      
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_PORT);
       
    if(bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("绑定失败");
        exit(1);
    }
        
    if(listen(listenfd, FD_SET_SIZE) == -1)
    {
        perror("监听失败");
        exit(1);
    }
  
    maxfd = listenfd + 1;
  
    for (i = 0; i < FD_SET_SIZE; ++i)
    {
        client[i] = -1;
    }
  
    FD_ZERO(&allset);
    FD_SET(listenfd,&allset);
   
    while(1){
        struct sockaddr_in addr;  
        read_set = allset;
        fd_ready_num = select(maxfd, &read_set, NULL, NULL, NULL);
        // printf("有 %d 个文件描述符准备好了\n", fd_ready_num);
  
        if (FD_ISSET(listenfd,&read_set))
        {
            sin_size = sizeof(addr);
            if ((connectfd = accept(listenfd, (struct sockaddr *)&addr, &sin_size)) == -1)
            {
                perror("接收错误\n");
                continue;
            }
  
            for (i = 0; i < FD_SETSIZE; ++i)
            {
                if (client[i] < 0)
                {
                    client[i] = connectfd;
                    printf("接收client[%d]一个请求来自于: %s:%d\n", i, inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));  
                    break;
                }
            }
  
            if (i == FD_SETSIZE)
            {
                printf("连接数过多\n");
            }
  
            FD_SET(connectfd,&allset);
  
            maxfd = (connectfd > maxfd) ? (connectfd + 1) : maxfd;
            maxi  = (i > maxi) ? i : maxi;
  
            if (--fd_ready_num <= 0)
            {
                continue;
            }
        }
  
        for (i = 0; i < maxi; ++i)
        {
            if ((scokfd = client[i]) < 0)
            {
                continue;
            }
  
            if (FD_ISSET(scokfd,&read_set))
            {
                if((len = recv(scokfd,recvbuf,MAXDATASIZE,0)) == 0)
                {
                    close(scokfd);
                    printf("clinet[%d] 连接关闭\n", i);
                    FD_CLR(scokfd, &read_set);
                    client[i] = -1;
                }
                else
                {
                    char web_result[] = "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\nContent-Length: 11\r\nServer: mengkang\r\n\r\nhello world";
                    write(client[1],web_result,sizeof(web_result));
                }
  
                if (--fd_ready_num <= 0)
                {
                    break;
                }
            }
        }
    }
   
    close(listenfd);
       
    return 0;
}
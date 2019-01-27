/*
 * echoclient.c - An echo client
 */
/* $begin echoclientmain */
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];     // 主机名
    port = argv[2];       // 端口号

    clientfd = Open_clientfd(host, port);   
    Rio_readinitb(&rio, clientfd);    // 初始化 缓冲区

    // while (Fgets(buf, MAXLINE, stdin) != NULL) {
    //     Rio_writen(clientfd, buf, strlen(buf));     // 似乎是不断尝试，最后成功把所有字节成功输出
    //     Rio_readlineb(&rio, buf, MAXLINE);     // 利用封装好的rio来读数据
    //     Fputs(buf, stdout);
    // }

    // 长度为0的字符串会让 server 读取函数得到一个 EOF  其实也不是长度为0的字符串，感觉是一个条件
    // char * buff = "";
    // printf("%d\n", strlen(buff));
    // Rio_writen(clientfd, buff, strlen(buff));

    // buff = "123";
    // printf("%d\n", strlen(buff));
    // Rio_writen(clientfd, buff, strlen(buff));

    Rio_readlineb(&rio, buf, MAXLINE);   // 如果双方都只是等待的话，那么只会是大家都被会阻塞住。

    Close(clientfd); //line:netp:echoclient:close
    exit(0);
}
/* $end echoclientmain */

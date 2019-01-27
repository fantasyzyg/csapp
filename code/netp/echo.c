/*
 * echo - read and echo text lines until client closes connection
 */
/* $begin echo */
#include "csapp.h"

void echo(int connfd) 
{
    size_t n; 
    char buf[MAXLINE]; 
    rio_t rio;

    Rio_readinitb(&rio, connfd);

    

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) { //line:netp:echo:eof
        printf("From client: %s\n", buf);
        printf("server received %d bytes\n", (int)n);

        // printf("%d\n", buf[n-1]);     // 输出最后一个字符值 最后一个字符是换行符 --> 如果是命令行输入的话
        Rio_writen(connfd, buf, n);
    }
}
/* $end echo */


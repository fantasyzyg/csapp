/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

char * parse_num(char *p) {
    while (!(*p >= '0' && *p <= '9'))
        ++p;
    return p;
}

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1=0, n2=0;

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';
        strcpy(arg1, buf);
        strcpy(arg2, p+1);
        n1 = atoi(parse_num(arg1));
        n2 = atoi(parse_num(arg2));
    }

    /* Make the response body */
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet subtraction portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d - %d = %d\r\n<p>", 
	    content, n1, n2, n1 - n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);
  
    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);    // 输出到标准输出
    fflush(stdout);      // 已经被重定向了  --- > 指向与客户端传输数据的fd 

    exit(0);
}
/* $end adder */

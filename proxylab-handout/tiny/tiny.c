/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp, int method,char *cgiargs);
int parse_uri(char *uri, char *filename, char *cgiargs, int method);
void serve_static(int fd, char *filename, int filesize, int method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int method);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

// void handler_PipeError(int signal) {
//     printf("ignore this error!\n");
// }

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    // 目的就是为了在主进程中忽略 SIGPIPE 信号，使主进程不奔溃
    if (Signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        unix_error("mask signal pipe error!");
    }


    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                        port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);                                             //line:netp:tiny:doit
        Close(connfd);                                            //line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{

    sleep(5);

    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;


    int md = 0;       // 指定使用的方法

    /* Read request line and headers */
    // 刚开始只是会读到一个请求行 request line
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest

    if (!strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        md = 0;    // GET
    } else if (!strcasecmp(method, "HEAD")) {
        md = 1;      // HEAD
    } else if (!strcasecmp(method, "POST")) {
        md = 2;      // POST
    }
    else {
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }

    read_requesthdrs(&rio, md, cgiargs);                              //line:netp:doit:readrequesthdrs

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs, md);       //line:netp:doit:staticcheck
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
	    clienterror(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	    return;
    }                                                    //line:netp:doit:endnotfound

    if (is_static) { /* Serve static content */          
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
            clienterror(fd, filename, "403", "Forbidden",
                "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, md);        //line:netp:doit:servestatic
    }
    else { /* Serve dynamic content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
            clienterror(fd, filename, "403", "Forbidden",
                "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs, md);            //line:netp:doit:servedynamic
    }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 * 
 *  如果是POST请求的话，会在请求头之后还会有 request body
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp, int method, char *cgiargs) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }

    // POST 请求
    if (method == 2) {
        Rio_readnb(rp, buf, rp->rio_cnt);
        printf("%s\n", buf);
        strcpy(cgiargs, buf);
    }

    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs, int method) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
        strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
        strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
        strcat(filename, uri);                           //line:netp:parseuri:endconvert1
        if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
            strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
        return 1;
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic

        // 只有GET在uri上注明参数，POST的话，是把请求参数写到了body里面
        if (method == 0) {
            ptr = index(uri, '?');                           //line:netp:parseuri:beginextract

            // 是否带参数
            if (ptr) {
                strcpy(cgiargs, ptr+1);
                *ptr = '\0';
            }
            else 
                strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
        }
        strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
        // 找到可执行文件
        strcat(filename, uri);                           //line:netp:parseuri:endconvert2
        return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize, int method) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
 
    /* Send response headers to client */

    // 为什么一定要是这样的格式呢？
    // http 专门解析吗？ 

    get_filetype(filename, filetype);       //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));       //line:netp:servestatic:endserve
    printf("Response headers:\n");
    printf("%s", buf);

    if (method == 1)  // HEAD
        return;

    /* Send response body to client */
    // srcfd = Open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//line:netp:servestatic:mmap
    // Close(srcfd);                           //line:netp:servestatic:close
    // Rio_writen(fd, srcp, filesize);         //line:netp:servestatic:write
    // Munmap(srcp, filesize);                 //line:netp:servestatic:munmap

    // 一种方法
    srcfd = Open(filename, O_RDONLY, 0);
    rio_t rio;
    Rio_readinitb(&rio, srcfd);
    int n;
    while ((n = Rio_readlineb(&rio, buf, MAXBUF)) > 0) {
        printf("%s", buf);
        Rio_writen(fd, buf, n);
    }
    Close(srcfd);

    // 另一种方法
    // srcfd = Open(filename, O_RDONLY, 0);
    // srcp = Malloc(filesize);
    // Rio_readn(srcfd, (void *)srcp, filesize);
    // Rio_writen(fd, srcp, filesize);
    // Close(srcfd);
    // Free(srcp);

    // Close(srcfd);
    printf("ok!\n");
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	    strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	    strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	    strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	    strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mpg"))
        strcpy(filetype, "video/mpg");
    else if (strstr(filename, ".mp3"))
        strcpy(filetype, "audio/basic");
    else 
	    strcpy(filetype, "text/plain");
}  
/* $end serve_static */


void handler(int signal) {

    int olderrno = errno;   // errno 错误号
    int status;   // 子进程退出的返回状态

    // 当所有子进程都已经被回收了之后，再次调用 waitpid 会返回负数，并且设置 errno 为 ECHILD
    while (waitpid(-1, &status, 0) > 0) {
        printf("Child process exit, And the exit status is %d\n", status);
        continue;
    }

    if (errno != ECHILD) {
        Sio_error("waitpid error!\n");
    }

    errno = olderrno;    // 恢复原值
}

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs, int method) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    // 在 SIGCHLD信号位上 注册 handler 函数
    if (Signal(SIGCHLD, handler) == SIG_ERR) {
        unix_error("Signal error!\n");
    }

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (method == 1) // HEAD
        return;
  
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
        // 将fd复制到STDOUT_FILENO 文件描述符，即是都是指向了fd指向的值
        Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2

        // 在子进程恢复 SIGPIPE 的默认处理
        if (Signal(SIGPIPE, SIG_DFL) == SIG_ERR) {
            unix_error("mask signal pipe error!");
        }

        // 从磁盘加载程序替换现从父进程复制过来的镜像
        Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }

    // wait 函数是阻塞等待子进程运行完，并且回收子进程的资源再继续执行下去
    /*
        1、僵尸进程    子进程执行完，父进程没有回收资源
        2、孤儿进程    父进程已经退出，子进程被init进程接管
    */
    // Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait

}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

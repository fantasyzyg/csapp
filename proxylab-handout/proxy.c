#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */

// 一个缓存object最大100k，缓存区最大 差不多1M
/*
    基本思路就是原先就分配好10个槽，最多就缓存10个网页object
*/
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define CACHE_OBJ_SIZE 10
#define LRU_MAGIC_NUMBER 9999      // 可以理解为一个权重值，权重越大的就表示是越新的，每次淘汰都是淘汰最小的

// 为了构造写着优先
typedef struct {
    char data[MAX_OBJECT_SIZE];  // 缓冲区数据
    char url[MAXLINE];
    int LRU;
    int isEmpty;

    int readCnt;
    sem_t rcmutex;   // 为了保证 readCnt 的互斥

    int writeCnt;
    sem_t wcmutex;    // 为了保证 wcmutex 的互斥
    
    sem_t cacheMutex;     // 对 cache 操作的互斥
    sem_t mutex;       // 为的是当写着有请求时，可以随时可以隔绝读者的进入，让写着可以更快或者cache的拥有权

} Cache_blcok;

// 并没有将整个cache给锁起来，而是在每一个cache里面加入写着优先的模型
typedef struct {
    Cache_blcok blocks[CACHE_OBJ_SIZE];
    int cache_num;
} Cache;

Cache cache;     // 全局缓存区

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int connfd);
void parse_uri(char * uri, char *hostname, char *path, int *port);
void build_http_header(char *buf, char *hostname, char *path, rio_t * rp, int *port);
void *thread(void *arg);


// 写着优先的操作
void readerPre(int i);  
void readerAfter(int i); 
void writerPre(int i);
void writerAfter(int i);


// 关于cache的操作
void cache_init();
int cache_find(char *url);
int cache_eviction();       // 获取淘汰cache区
void cache_uri(char *url, char *cache);
void cache_LRU(int index);         // 改变优先级


// 是一个http代理 
int main(int argc, char **argv)
{
    // printf("%s", user_agent_hdr);
    // return 0;

    int listenfd, clientfd;
    char hostname[MAXLINE], port[MAXLINE]; 
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t ptid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // 使主进程忽略对已关闭的fd写数据发出的signal产生的错误，不让主进程崩溃
    if (Signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        unix_error("mark signal pipe error!\n");
    }

    cache_init();    // 初始化cache

    listenfd = Open_listenfd(argv[1]);   // 监听port 
    while (1) {
        clientlen = sizeof(clientaddr);
        clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 我的理解是这会是有一个队列的
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 获取client的信息
        printf("Connection from : (%s, %s)\n", hostname, port);

        // doit(clientfd);
        // Close(clientfd);
        Pthread_create(&ptid, NULL, thread, (void *)clientfd);
    }
}


void doit(int connfd) {
    printf("开始处理！\n");
    sleep(5);    // 睡眠5秒
    
    char buf[MAXLINE];   
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 请求方法  请求资源路径   HTTP版本号

    rio_t rio;
    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)) 
        return;

    printf("%s\n", buf);    // 将请求行输出
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("method: %s\n", method);
    printf("uri: %s\n", uri);
    printf("version: %s\n\n", version);

    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        printf("Proxy does not implement the method %s\n", method);
        return;
    } 

    char hostname[MAXLINE]; // 服务器hostname
    char path[MAXLINE];   // 资源路径
    int port;    // 请求服务器端口

    parse_uri(uri, hostname, path, &port);

    int index;
    if ((index = cache_find(uri)) != -1) {
        printf("cache hit!\n");
        readerPre(index);
        Rio_writen(connfd, cache.blocks[index].data, strlen(cache.blocks[index].data));
        readerAfter(index);

        return;
    }
    else 
        printf("cache not hit, and will be cached!\n");

    char http_header[MAXLINE];
    build_http_header(http_header, hostname, path, &rio, &port);

    printf("new http header!\n");
    printf("%s", http_header);
    printf("%s %d\n", hostname, port);

    // connect the server
    char portStr[100];
    sprintf(portStr,"%d",port);
    int clientfd = Open_clientfd(hostname, portStr);
    if (clientfd < 0) {
        printf("Proxy fails to connect server!\n");
        return;
    }

    // 发送 HTTP header
    rio_t client_rio;
    Rio_readinitb(&client_rio, clientfd);
    Rio_writen(clientfd, http_header, strlen(http_header));

    int n;

    char data[MAX_OBJECT_SIZE];
    int size = 0;
    
    // 读请求需要分多次读取
    while ((n = Rio_readlineb(&client_rio, buf, MAXLINE)) > 0) {
        Rio_writen(connfd, buf, n);
        if (size+= n < MAX_OBJECT_SIZE)
            strcat(data, buf);
    }

    cache_uri(uri, data);     // 找到一个cache并且缓存起来

    Close(clientfd);   // 关闭代理 fd
}


void parse_uri(char *uri, char *hostname, char *path, int *port) {
    *port = 80;   // http 

    char *pos = strstr(uri, "//");

    if (pos) {
        pos = pos + 2;    // 指向最开始的hostname
    } else {
        pos = uri;      // 并没有带 http:// 这些前缀
    }

    char *pos1 = strstr(pos, ":");   // 是否带有端口号
    if (pos1) {
        // 带有端口号
        *pos1 = '\0';
        sscanf(pos, "%s", hostname);   // 以pos字符串为输入
        sscanf(pos1+1, "%d%s", port, path);
    } else {
        char *pos2 = strstr(pos, "/");
        if (pos2) {
            *pos2 = '\0';
            sscanf(pos, "%s", hostname);
            *pos2 = '/';
            sscanf(pos2, "%s", path);
        } else {
            sscanf(pos, "%s", hostname);
            path = "/";
        }
    }
    return;
}

void build_http_header(char *http_header, char *hostname, char *path, rio_t *rp, int *port) {

    char buf[MAXLINE];      // 接受clientfd的输入
    char request_line[MAXLINE];    // 构建请求行
    char request_header[MAXLINE];     // 构建请求头
    char host_header[MAXLINE];   // host 字段
    char *proxy_connection_header = "Proxy-Connection: close\r\n";
    char *connection_header = "Connection: close\r\n";
    char *connection_key = "Connection";
    char *user_agent_key = "User-Agent";
    char *proxy_connection_key = "Proxy-Connection";
    char *host_key = "Host";

    sprintf(request_line, "GET %s HTTP/1.0\r\n", path);
    int n;
    while ((n = Rio_readlineb(rp, buf, MAXLINE)) > 0) {
        if (!strcmp("\r\n", buf))
            break;

        // 果然 HTTP 请求行是一行一行地读过来的
        // printf("get %d bytes, and the context is      %s\n", n, buf);
        if (!strncasecmp(buf, host_key, strlen(host_key))) {

            // printf("before: %s", buf);

            // C 处理字符串真的是很蛋疼。。
            char *p = strstr(buf, ":");
            if (p) {
                if (strstr(p+1, ":")) {
                    ++p; // 向前一步
                    while (*p == ' ')
                        ++p;
                    int i = 0;
                    while (*p != ':') {
                        hostname[i] = *p;
                        ++i;
                        ++p;
                    }
                    hostname[i] = '\0';

                    ++p;     // 跳过 ：     
                    i = 0;
                    char temp[10];
                    while (*p >= '0' && *p <= '9') {
                        temp[i] = *p;
                        ++p;
                        ++i;
                    }
                    temp[i] = '\0';
                    *port = atoi(temp);

                    sprintf(host_header, "Host: %s\r\n", hostname);
                } else {
                    strcpy(host_header, buf);
                }
            }

            // printf("%s %d\n", hostname, *port);
            continue;
        }

        if (strncasecmp(buf, connection_key, strlen(connection_key)) && 
            strncasecmp(buf, user_agent_key, strlen(user_agent_key)) &&
            strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))) {
                // printf("----- %s", buf);
                strcat(request_header, buf);
            }
    }

    // 若请求头有host字段，采用请求头的
    if (!strlen(host_header)) {
        printf("use the uri hostname!\n");
        sprintf(host_header, "Host: %s\r\n", hostname);
    }

    sprintf(http_header, 
            "%s%s%s%s%s%s%s",
            request_line,
            host_header,
            user_agent_hdr,
            connection_header,
            proxy_connection_header,
            request_header,
            "\r\n");
}

// 每个线程里面的具体逻辑
void *thread(void *arg) {
    int clientfd = (int)arg;

    // 使主进程忽略对已关闭的fd写数据发出的signal产生的错误，不让主进程崩溃
    if (Signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        unix_error("mark signal pipe error!\n");
    }

    Pthread_detach(pthread_self());    // 分离线程  让自己回收资源
    doit(clientfd);
    close(clientfd);

    return NULL;
}

/**************************************
 * Cache Function
 **************************************/

void cache_init() {
    cache.cache_num = 0;   // 使用数量
    for (int i = 0;i < CACHE_OBJ_SIZE;++i) {
        cache.blocks[i].LRU = 0;
        cache.blocks[i].isEmpty = 1;
        cache.blocks[i].readCnt = 0;
        cache.blocks[i].writeCnt = 0;


        /*
        在POSIX标准中，信号量分两种，一种是无名信号量，一种是命名信号量。无名信号量只用于线程间，命令信号量只用于进程间。
        */

        // // sem_unlink("a");
        // sem_t *a = sem_open("/a", O_CREAT|O_RDWR, 0666, 1);
        // // sem_unlink("b");
        // sem_t *b = sem_open("/b", O_CREAT|O_RDWR, 0666, 1);
        // // sem_unlink("c");
        // sem_t *c = sem_open("/c", O_CREAT|O_RDWR, 0666, 1);
        // // sem_unlink("d");
        // sem_t *d = sem_open("/d", O_CREAT|O_RDWR, 0666, 1);

        // cache.blocks[i].rcmutex = *a;
        // cache.blocks[i].wcmutex = *b;
        // cache.blocks[i].cacheMutex = *c;
        // cache.blocks[i].mutex = *d;

        Sem_init(&cache.blocks[i].rcmutex, 0, 1);
        Sem_init(&cache.blocks[i].wcmutex, 0, 1);
        Sem_init(&cache.blocks[i].cacheMutex, 0, 1);
        Sem_init(&cache.blocks[i].mutex, 0, 1);
    }
}


/**
 *  
 * 
*/


// 读第i个cache将采取读操作
void readerPre(int i) {
    P(&cache.blocks[i].mutex);   // 每一个读者进来都是需要首先越过这道坎,没有这道坎就一直是读者优先了
    P(&cache.blocks[i].rcmutex);   // 修改读者数量
    if (cache.blocks[i].readCnt == 0)  // 第一个进来
        P(&cache.blocks[i].cacheMutex);
    ++cache.blocks[i].readCnt;
    V(&cache.blocks[i].rcmutex);
    V(&cache.blocks[i].mutex);
}

// 读第i个cache之后采取的操作
void readerAfter(int i) {
    P(&cache.blocks[i].rcmutex);
    --cache.blocks[i].readCnt;
    if (cache.blocks[i].readCnt == 0)  // 最后一个读者需要释放锁
        V(&cache.blocks[i].cacheMutex);
    V(&cache.blocks[i].rcmutex);
}

void writerPre(int i) {
    P(&cache.blocks[i].wcmutex);
    if (cache.blocks[i].writeCnt == 0) 
        P(&cache.blocks[i].mutex);     // 当有写着想进来时，可以直接截断读者的入口
    ++cache.blocks[i].writeCnt;
    V(&cache.blocks[i].wcmutex);

    P(&cache.blocks[i].cacheMutex);   // 进入缓冲区
}

void writerAfter(int i) {
    V(&cache.blocks[i].cacheMutex);     // 退出缓冲区

    P(&cache.blocks[i].wcmutex);
    --cache.blocks[i].writeCnt;
    if (cache.blocks[i].writeCnt == 0)
        V(&cache.blocks[i].mutex);
    V(&cache.blocks[i].wcmutex);
}

// 根据url寻找cache 
int cache_find(char *url) {
    int i = 0;
    int flag = 0;
    for (;i < CACHE_OBJ_SIZE;++i) {
        readerPre(i);
        if (cache.blocks[i].isEmpty == 0 && strcmp(url, cache.blocks[i].url) == 0)
            flag = 1;
        readerAfter(i);
        if (flag)
            break;
    }

    if (i == CACHE_OBJ_SIZE)
        return -1;
    return i;
}

// 找出被淘汰的那一个
int cache_eviction() {
    int min = LRU_MAGIC_NUMBER;
    int minIndex = 0;

    for (int i = 0;i < CACHE_OBJ_SIZE;++i) {
        readerPre(i);
        if (cache.blocks[i].isEmpty == 1) {  // 没被使用
            minIndex = i;
            readerAfter(i);
            break;
        }

        if (cache.blocks[i].LRU < min) {
            min = cache.blocks[i].LRU;
            minIndex = i;
        }

        readerAfter(i);
    }

    return minIndex;
}

// 缓存并且更新权重
void cache_uri(char *url, char *buf) {
    int i = cache_eviction();

    writerPre(i);
    strcpy(cache.blocks[i].url, url);
    strcpy(cache.blocks[i].data, buf);
    cache.blocks[i].isEmpty = 0;
    cache.blocks[i].LRU = LRU_MAGIC_NUMBER;
    writerAfter(i);

    cache_LRU(i);
}

// 权重都减小一
void cache_LRU(int index) {
    int i = 0;
    for (;i < CACHE_OBJ_SIZE;++i) {
        if (i != index) {
            writerPre(i);
            if (cache.blocks[i].isEmpty == 0) 
                --cache.blocks[i].LRU;
            writerAfter(i);
        }
    }
}
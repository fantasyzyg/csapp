/* $begin hostinfo */
#include "csapp.h"

int main(int argc, char **argv) 
{
    struct addrinfo *p, *listp, hints;
    char buf[MAXLINE];
    int rc, flags;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(0);
    }

    /* Get a list of addrinfo records */
    memset(&hints, 0, sizeof(struct addrinfo));   
    // hints 是为了更好地控制返回的结果                      
    hints.ai_family = AF_INET;       /* IPv4 only */        //line:netp:hostinfo:family
    hints.ai_socktype = SOCK_STREAM; /* Connections only */ //line:netp:hostinfo:socktype
    // char * port = "80";
    if ((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(1);
    }

    /* Walk the list and display each IP address */
    flags = NI_NUMERICHOST; /* Display address string instead of domain name */
    for (p = listp; p; p = p->ai_next) {
        Getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, NULL, 0, flags);
        printf("%s\n", buf);
        // printf("%s\n", p->ai_addr->sa_data);
        // printf("%d\n", p->ai_addrlen);
        // printf("%d\n", p->ai_addr->sa_len);
        // printf("%d\n", p->ai_addr->sa_family);

        // int clientfd;
        // /* Create a socket descriptor */
        // if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
        //     continue; /* Socket failed, try the next */

        // /* Connect to the server */
        // if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) 
        //     break; /* Success */

        // printf("can't connect !");
        // if (close(clientfd) < 0) { /* Connect failed, try another */  //line:netp:openclientfd:closefd
        //     fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
        //     return -1;
        // }
    } 

    /* Clean up */
    Freeaddrinfo(listp);

    exit(0);
}
/* $end hostinfo */

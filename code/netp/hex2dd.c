/* $begin hex2dd */
#include "csapp.h"

int main(int argc, char **argv) 
{
    struct in_addr inaddr;  /* Address in network byte order */
    uint32_t addr;          /* Address in host byte order */
    char buf[MAXBUF];       /* Buffer for dotted-decimal string */  
    
    if (argc != 2) {
	fprintf(stderr, "usage: %s <hex number>\n", argv[0]);
	exit(0);
    }
    sscanf(argv[1], "%x", &addr);     // 转化为32位整数
    inaddr.s_addr = htonl(addr);     // 转为网络字节顺序

    // 事实证明，该计算机内部是使用小端，转化为网络字节为大端
    printf("%x\n", addr);
    int a = 300;
    printf("%x\n", a);
    printf("%x\n", inaddr.s_addr);
    
    if (!inet_ntop(AF_INET, &inaddr, buf, MAXBUF))   // 二进制网络字节顺序的IP地址转化为点分十进制地址
        unix_error("inet_ntop");
    printf("%s\n", buf);

    exit(0);
}
/* $end hex2dd */

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

void noly_set_tcp_nodelay(int sk) {
    int enable = 1;
    setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));
}

int noly_tcp_connect(char *ip, int port) {
    if(!ip || port < 0 || port > 65535) return -1;
    int sk;
    struct sockaddr_in dest;
    sk = socket(AF_INET, SOCK_STREAM, 0);
    if(sk < 0) return -1;
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = inet_addr(ip);
    noly_set_tcp_nodelay(sk);
    int ret = connect(sk, (struct sockaddr *)&dest, sizeof(dest));
    if(ret == 0) return sk;
    return -1;
}

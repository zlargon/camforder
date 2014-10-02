#ifndef __NOLY_H
#define __NOLY_H

void noly_set_tcp_nodelay(int sk);
int noly_tcp_connect(char *ip, int port);

#endif

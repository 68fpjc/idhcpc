#ifndef NWSUB_H
#define NWSUB_H

#include <socket.h>

int create_udp_socket(void);
int connect2(const int, const unsigned short, const int, struct sockaddr_in *);
int bind2(const int, const unsigned short, const int);
void init_sockaddr_in(const unsigned short, const int, struct sockaddr_in *);

#endif /* NWSUB_H */

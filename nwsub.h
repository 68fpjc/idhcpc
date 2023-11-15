#ifndef __NWSUB_H__
#define	__NWSUB_H__

#include	<etherdrv.h>
#include	<network.h>
#include	<socket.h>

enum { FALSE, TRUE };

#define	IPADDR(n1, n2, n3, n4)	((n1 << 24) | (n2 << 16) | (n3 << 8) | n4)

int get_mac_address(eaddr *, char *);
int create_udp_socket(void);
int connect2(struct sockaddr_in *, int, unsigned short, int);
int bind2(int, unsigned short, int);
void init_sockaddr_in(struct sockaddr_in *, unsigned short, int);

#endif	/* __NWSUB_H__ */


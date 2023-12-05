#include <etherdrv.h>
#include <socket.h>

enum { FALSE, TRUE };

#define IPADDR(n1, n2, n3, n4) ((n1 << 24) | (n2 << 16) | (n3 << 8) | n4)

int get_mac_address(const char *, eaddr *);
int create_udp_socket(void);
int connect2(const int, const unsigned short, const int, struct sockaddr_in *);
int bind2(const int, const unsigned short, const int);
void init_sockaddr_in(const unsigned short, const int, struct sockaddr_in *);

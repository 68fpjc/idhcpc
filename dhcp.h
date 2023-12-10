#ifndef DHCP_H
#define DHCP_H

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

typedef struct tagdhcp_msg0 {
  unsigned char op;
  unsigned char htype;
  unsigned char hlen;
  unsigned char hops;
  unsigned long xid;
  unsigned short secs;
  unsigned short flags;
  unsigned long ciaddr;
  unsigned long yiaddr;
  unsigned long siaddr;
  unsigned long giaddr;
  unsigned char chaddr[16];
  unsigned char sname[64];
  unsigned char file[128];
} dhcp_msg0;

typedef struct tagdhcp_msg {
  dhcp_msg0 h;
  unsigned char options[312]; /* 固定長… */
} dhcp_msg;

#define IPADDR(n1, n2, n3, n4) ((n1 << 24) | (n2 << 16) | (n3 << 8) | n4)

#define DHCP_LIMITEDBCAST \
  IPADDR(255, 255, 255, 255) /* ブロードキャストアドレス */
#define DHCP_MAGICCOOKIE IPADDR(99, 130, 83, 99) /* magic cookie */

#define DHCP_BCAST 0x8000

/* operation code */
#define DHCP_BOOTREQUEST 1 /* 要求 */
#define DHCP_BOOTREPLY 2   /* 応答 */

/* item type */
#define DHCP_SUBNETMASK 1
#define DHCP_GATEWAY 3
#define DHCP_TIMESVR 4
#define DHCP_IEN116SVR 5
#define DHCP_DOMAINSVR 6
#define DHCP_LOGSVR 7
#define DHCP_QUOTESVR 8
#define DHCP_LPRSVR 9
#define DHCP_IMRESSSVR 10
#define DHCP_RLPSVR 11
#define DHCP_HOSTNAME 12
#define DHCP_BOOTSIZE 13
#define DHCP_DOMAINNAME 15
#define DHCP_REQIPADDR 50
#define DHCP_LEASETIME 51
#define DHCP_MSGTYPE 53
#define DHCP_SERVERID 54
#define DHCP_PARAMLIST 55
#define DHCP_MAXMSG 57
#define DHCP_RENEWTIME 58
#define DHCP_REBINDTIME 59
#define DHCP_CLIENTID 61
#define DHCP_END 255

/* message type */
#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPDECLINE 4
#define DHCPACK 5
#define DHCPNAK 6
#define DHCPRELEASE 7
#define DHCPINFORM 8

#define MAX_DHCP_HW_ADDR_LEN 16
typedef struct tagdhcp_hw_addr {
  unsigned char arp_hw_type;
  unsigned char hw_addr_len;
  unsigned char hw_addr[MAX_DHCP_HW_ADDR_LEN];
} dhcp_hw_addr;

/* prototype */
void dhcp_make_dhcpdiscover(const dhcp_hw_addr *, const unsigned long,
                            const unsigned short, dhcp_msg *);
void dhcp_make_dhcprequest(const dhcp_hw_addr *, const unsigned long,
                           const unsigned long, const unsigned long,
                           const unsigned short, dhcp_msg *);
void dhcp_make_dhcprelease(const dhcp_hw_addr *, const unsigned long,
                           const unsigned long, const unsigned long,
                           dhcp_msg *);
unsigned char *dhcp_searchfromopt(const dhcp_msg *, const unsigned char);
unsigned char *dhcp_get_4octet(const dhcp_msg *, const unsigned char,
                               unsigned long *);
unsigned long dhcp_get_yiaddr(const dhcp_msg *);
unsigned char *dhcp_get_serverid(const dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_subnetmask(const dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_defroute(const dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_leasetime(const dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_renewtime(const dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_rebindtime(const dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_dns(const dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_domainname(const dhcp_msg *, char *);
int dhcp_isreply(const dhcp_msg *, const unsigned long, unsigned char *);

void dhcp_print(const dhcp_msg *);

#endif /* DHCP_H */

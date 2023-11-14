/*======================================================================
	dhcp.h
			Copyright 2002 Igarashi
======================================================================*/
/*======================================================================
　本ファイルの著作権は作者（いがらし）に属しますが、利用者の常識と責任の
範囲において、自由に使用 / 複製 / 配付 / 改造することができます。その際、
許諾を求める必要もなければ、ロイヤリティの類も一切必要ありません。

　本ファイルは無保証です。本ファイルを使用した結果について、作者は一切の
責任を負わないものとします。
======================================================================*/

#ifndef __DHCP_H__
#define	__DHCP_H__

#include	"nwsub.h"

#define	DHCP_SERVER_PORT	67
#define	DHCP_CLIENT_PORT	68

typedef struct tagdhcp_msg0 {
	unsigned char	op;
	unsigned char	htype;
	unsigned char	hlen;
	unsigned char	hops;
	unsigned long	xid;
	unsigned short	secs;
	unsigned short	flags;
	unsigned long	ciaddr;
	unsigned long	yiaddr;
	unsigned long	siaddr;
	unsigned long	giaddr;
	unsigned char	chaddr[16];
	unsigned char	sname[64];
	unsigned char	file[128];
} dhcp_msg0;

typedef struct tagdhcp_msg {
	dhcp_msg0		h;
	unsigned char	options[312];	/* 固定長… */
} dhcp_msg;

#define	DHCP_LIMITEDBCAST	IPADDR(255, 255, 255, 255)	/* ブロードキャストアドレス */
#define	DHCP_MAGICCOOKIE	IPADDR( 99, 130,  83,  99)	/* magic cookie */

#define	DHCP_BCAST			0x8000

/* operation code */
#define	DHCP_BOOTREQUEST	1	/* 要求 */
#define	DHCP_BOOTREPLY		2	/* 応答 */

/* item type */
#define	DHCP_SUBNETMASK		1
#define	DHCP_GATEWAY		3
#define	DHCP_TIMESVR		4
#define	DHCP_IEN116SVR		5
#define	DHCP_DOMAINSVR		6
#define	DHCP_LOGSVR			7
#define	DHCP_QUOTESVR		8
#define	DHCP_LPRSVR			9
#define	DHCP_IMRESSSVR		10
#define	DHCP_RLPSVR			11
#define	DHCP_HOSTNAME		12
#define	DHCP_BOOTSIZE		13
#define	DHCP_DOMAINNAME		15
#define	DHCP_REQIPADDR		50
#define	DHCP_LEASETIME		51
#define	DHCP_MSGTYPE		53
#define	DHCP_SERVERID		54
#define	DHCP_PARAMLIST		55
#define	DHCP_MAXMSG			57
#define	DHCP_RENEWTIME		58
#define	DHCP_REBINDTIME		59
#define	DHCP_CLIENTID		61
#define	DHCP_END			255

/* message type */
#define	DHCPDISCOVER		1
#define	DHCPOFFER			2
#define	DHCPREQUEST			3
#define	DHCPDECLINE			4
#define	DHCPACK				5
#define	DHCPNAK				6
#define	DHCPRELEASE			7
#define	DHCPINFORM			8


/* prototype */
void dhcp_make_dhcpdiscover(dhcp_msg *, eaddr *, unsigned long, unsigned short);
void dhcp_make_dhcprequest(dhcp_msg *, unsigned long, unsigned long, eaddr *, unsigned long, unsigned short);
void dhcp_make_dhcprelease(dhcp_msg *, unsigned long, unsigned long, eaddr *, unsigned long);
unsigned char *dhcp_searchfromopt(dhcp_msg *, unsigned char);
unsigned char *dhcp_get_4octet(dhcp_msg *, unsigned long *, unsigned char);
unsigned long dhcp_get_yiaddr(dhcp_msg *);
unsigned char *dhcp_get_serverid(dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_subnetmask(dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_defroute(dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_leasetime(dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_renewtime(dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_rebindtime(dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_dns(dhcp_msg *, unsigned long *);
unsigned char *dhcp_get_domainname(dhcp_msg *, char *);
int dhcp_isreply(dhcp_msg *, unsigned long, unsigned char *);

void dhcp_print(dhcp_msg *);

#endif	/* __DHCP_H__ */


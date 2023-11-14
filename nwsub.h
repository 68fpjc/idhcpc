/*======================================================================
	nwsub.h
			Copyright 2002 Igarashi
======================================================================*/
/*======================================================================
　本ファイルの著作権は作者（いがらし）に属しますが、利用者の常識と責任の
範囲において、自由に使用 / 複製 / 配付 / 改造することができます。その際、
許諾を求める必要もなければ、ロイヤリティの類も一切必要ありません。

　本ファイルは無保証です。本ファイルを使用した結果について、作者は一切の
責任を負わないものとします。
======================================================================*/

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


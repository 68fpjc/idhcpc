/*======================================================================
	nwsub.c
			Copyright 2002 Igarashi
======================================================================*/
/*======================================================================
　本ファイルの著作権は作者（いがらし）に属しますが、利用者の常識と責任の
範囲において、自由に使用 / 複製 / 配付 / 改造することができます。その際、
許諾を求める必要もなければ、ロイヤリティの類も一切必要ありません。

　本ファイルは無保証です。本ファイルを使用した結果について、作者は一切の
責任を負わないものとします。
======================================================================*/

#include	<stdio.h>
#include	<string.h>
#include	<etherdrv.h>
#include	<network.h>
#include	<socket.h>
#include	"nwsub.h"

/* ================================================================

	指定デバイスに対応するMACアドレスを取得する

	in:		dest	MACアドレス格納用バッファ
			devname	デバイス名（e.g. "dev/en0"）
	out:	TRUE	成功
			FALSE	失敗

   ================================================================ */
int get_mac_address(eaddr *dest, char *devname)
{
	int	ret = FALSE;
	int	no;

	if ( ETDGetDriverVersion(devname, &no) != -1 ) {
		ETDGetMacAddr(dest, no);
		ret = TRUE;
	}
	return ret;
}

/* ================================================================

	UDPソケット作成

	out:	ソケット識別子（socket()の戻り値）
			<0ならエラー

   ================================================================ */
int create_udp_socket(void)
{
	return socket(AF_INET, SOCK_DGRAM, 0);
}

/* ================================================================

	ソケット接続要求

	in:		p		sockaddr_in構造体へのポインタ
			sockno	ソケット識別子
			portno	ポート番号
			ipaddr	接続先IPアドレス
	out:	connect()の戻り値
			<0ならエラー

   ================================================================ */
int connect2(struct sockaddr_in *p, int sockno, unsigned short portno, int ipaddr)
{
	init_sockaddr_in(p, portno, ipaddr);
	return connect(sockno, (char *)p, sizeof(*p));
}

/* ================================================================

	ソケット結合

	in:		sockno	ソケット識別子
			portno	ポート番号
			ipaddr	接続先IPアドレス
	out:	bind()の戻り値
			<0ならエラー

   ================================================================ */
int bind2(int sockno, unsigned short portno, int ipaddr)
{
	struct sockaddr_in	inaddr;

	init_sockaddr_in(&inaddr, portno, ipaddr);
	return bind(sockno, (char *)&inaddr, sizeof(inaddr));
}

/* ================================================================

	sockaddr_in構造体の初期化

	in:		なし
	out:	なし

   ================================================================ */
void init_sockaddr_in(struct sockaddr_in *p, unsigned short portno, int ipaddr)
{
	memset(p, 0, sizeof(*p));	/* 0 で埋めておく */
	p->sin_family = AF_INET;
	p->sin_port = htons(portno);
	p->sin_addr.s_addr = htonl(ipaddr);
}


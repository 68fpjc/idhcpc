#include	<stdio.h>
#include	<string.h>
#include	<network.h>
#include	"dhcp.h"

static unsigned char *dhcp_msg_common(dhcp_msg *, eaddr *, unsigned long, unsigned short, unsigned char);

/* ================================================================

	DHCPDISCOVERメッセージ作成

	in:		pmsg		メッセージバッファ
			pmacaddr	MACアドレス
			xid			トランザクションID（32bit random）
			secs		起動からの経過時間

   ================================================================ */
void dhcp_make_dhcpdiscover(dhcp_msg *pmsg, eaddr *pmacaddr, unsigned long xid, unsigned short secs)
{
	unsigned char	*p;

	p = dhcp_msg_common(pmsg, pmacaddr, xid, secs, DHCPDISCOVER);	/* 共通部 */

	pmsg->h.flags = htons(DHCP_BCAST);	/* ブロードキャスト */

	/* DHCPメッセージ長 */
	*p++ = DHCP_MAXMSG;
	*p++ = 2;
	{
		unsigned short	tmp = htons(sizeof(*pmsg));	/* RFC2132、間違ってないか？ */
		memcpy(p, &tmp, 2);
		p += 2;
	}

	/* おしまい */
	*p = DHCP_END;
}

/* ================================================================

	DHCPREQUESTメッセージ作成

	in:		pmsg		メッセージバッファ
			yiaddr		要求IPアドレス
			sid			サーバID（=DHCPサーバIPアドレス）
			pmacaddr	MACアドレス
			xid			トランザクションID（32bit random）
			secs		起動からの経過時間

   ================================================================ */
void dhcp_make_dhcprequest(dhcp_msg *pmsg, unsigned long yiaddr, unsigned long sid, eaddr *pmacaddr, unsigned long xid, unsigned short secs)
{
	unsigned char	*p;

	p = dhcp_msg_common(pmsg, pmacaddr, xid, secs, DHCPREQUEST);	/* 共通部 */

	pmsg->h.flags = htons(DHCP_BCAST);	/* ブロードキャスト */

	/* 要求IPアドレス */
	*p++ = DHCP_REQIPADDR;
	*p++ = 4;
	{
		unsigned long	tmp = htonl(yiaddr);
		memcpy(p, &tmp, 4);
		p += 4;
	}

	/* サーバID */
	*p++ = DHCP_SERVERID;
	*p++ = 4;
	{
		unsigned long	tmp = htonl(sid);
		memcpy(p, &tmp, 4);
		p += 4;
	}

	*p++ = DHCP_PARAMLIST;
	*p++ = 9;
	*p++ = DHCP_SERVERID;
	*p++ = DHCP_SUBNETMASK;
	*p++ = DHCP_GATEWAY;
	*p++ = DHCP_DOMAINSVR;
	*p++ = DHCP_DOMAINNAME;
	*p++ = DHCP_SERVERID;
	*p++ = DHCP_LEASETIME;
	*p++ = DHCP_RENEWTIME;
	*p++ = DHCP_REBINDTIME;

	/* おしまい */
	*p = DHCP_END;
}

/* ================================================================

	DHCPRELEASEメッセージ作成

	in:		pmsg		メッセージバッファ
			myaddr		クライアントIPアドレス
			sid			サーバID（=DHCPサーバIPアドレス）
			pmacaddr	MACアドレス
			xid			トランザクションID（32bit random）

   ================================================================ */
void dhcp_make_dhcprelease(dhcp_msg *pmsg, unsigned long myaddr, unsigned long sid, eaddr *pmacaddr, unsigned long xid)
{
	unsigned char	*p;

	p = dhcp_msg_common(pmsg, pmacaddr, xid, 0, DHCPRELEASE);	/* 共通部 */

	/* クライアントIPアドレス */
	pmsg->h.ciaddr = htonl(myaddr);

	/* サーバID */
	*p++ = DHCP_SERVERID;
	*p++ = 4;
	{
		unsigned long	tmp = htonl(sid);
		memcpy(p, &tmp, 4);
		p += 4;
	}

	/* おしまい */
	*p = DHCP_END;
}

/* ================================================================

	DHCPメッセージ作成共通部

	in:		pmsg		メッセージバッファ
			pmacaddr	MACアドレス
			xid			トランザクションID（32bit random）
			secs		起動からの経過時間
			msgtype		メッセージタイプ

   ================================================================ */
static unsigned char *dhcp_msg_common(dhcp_msg *pmsg, eaddr *pmacaddr, unsigned long xid, unsigned short secs, unsigned char msgtype)
{
	unsigned char	*p = &pmsg->options[4];

	memset(pmsg, 0, sizeof(*pmsg));						/* 必ずゼロで埋めておくこと */

	pmsg->h.op = DHCP_BOOTREQUEST;						/* 要求 */
	pmsg->h.htype = 1;									/* Ethernet */
	pmsg->h.hlen = 6;									/* MACアドレス長 */
	memcpy( &pmsg->h.chaddr, &pmacaddr->_eaddr, 6 );	/* MACアドレス */
	pmsg->h.xid = xid;									/* トランザクションID */
	pmsg->h.secs = htons(secs);							/* マシン起動からの経過時間 */
	/* オプション部の先頭は必ずmagic cookie */
	*(unsigned long *)pmsg->options = htonl(DHCP_MAGICCOOKIE);

	/* 以下、オプション部を敷き詰めていく */

	/* magic cookieの直後は必ずメッセージタイプ */
	*p++ = DHCP_MSGTYPE;
	*p++ = 1;
	*p++ = msgtype;

	/* クライアントID */
	*p++ = DHCP_CLIENTID;
	*p++ = 7;
	*p++ = 1;							/* Ethernet */
	memcpy(p, &pmacaddr->_eaddr, 6);	/* MACアドレス */
	p += 6;

	return p;
}

/* ================================================================

	DHCPメッセージのオプション部から指定のアイテムを探す

	in:		pmsg		メッセージバッファ
			item		item type
	out:	アイテム先頭 + 1（オクテット数格納アドレス）
			NULLならエラー

   ================================================================ */
unsigned char *dhcp_searchfromopt(dhcp_msg *pmsg, unsigned char item)
{
	unsigned char	*p = &pmsg->options[4];
	unsigned char	itemtype;

	while (1) {
		itemtype = *p++;
		if (itemtype == item) {
			break;
		}
		if (itemtype == DHCP_END ) {
			p = NULL;
			break;
		}
		p += 1 + *p;
	}

	return p;
}

/* ================================================================

	DHCPメッセージからYOUR IP ADDRESSを取得する

	in:		pmsg		メッセージバッファ
	out:	YOUR IP ADDR

   ================================================================ */
unsigned long dhcp_get_yiaddr(dhcp_msg *pmsg)
{
	return ntohl(pmsg->h.yiaddr);
}

/* ================================================================

	DHCPメッセージのオプション領域からサブネットマスクを取得する

	in:		pmsg		メッセージバッファ
			pipaddr		サブネットマスク格納域
	out:	NULLなら存在しない

   ================================================================ */
unsigned char *dhcp_get_subnetmask(dhcp_msg *pmsg, unsigned long *pmask)
{
	return dhcp_get_4octet(pmsg, pmask, DHCP_SUBNETMASK);
}

/* ================================================================

	DHCPメッセージのオプション領域からサーバID（=DHCPサーバIPアドレス）を取得する

	in:		pmsg		メッセージバッファ
			pid			サーバID格納域
	out:	NULLなら見つからなかった

   ================================================================ */
unsigned char *dhcp_get_serverid(dhcp_msg *pmsg, unsigned long *pid)
{
	return dhcp_get_4octet(pmsg, pid, DHCP_SERVERID);
}

/* ================================================================

	DHCPメッセージのオプション領域からデフォルトゲートウェイアドレスを取得する

	in:		pmsg		メッセージバッファ
			proute		デフォルトゲートウェイアドレス格納域
	out:	NULLなら見つからなかった

   ================================================================ */
unsigned char *dhcp_get_defroute(dhcp_msg *pmsg, unsigned long *proute)
{
	return dhcp_get_4octet(pmsg, proute, DHCP_GATEWAY);
}

/* ================================================================

	DHCPメッセージのオプション領域からリース期間を取得する

	in:		pmsg		メッセージバッファ
			ptime		リース期間格納域
	out:	NULLなら見つからなかった

   ================================================================ */
unsigned char *dhcp_get_leasetime(dhcp_msg *pmsg, unsigned long *ptime)
{
	return dhcp_get_4octet(pmsg, ptime, DHCP_LEASETIME);
}

/* ================================================================

	DHCPメッセージのオプション領域から更新期間を取得する

	in:		pmsg		メッセージバッファ
			ptime		更新期間格納域
	out:	NULLなら見つからなかった

   ================================================================ */
unsigned char *dhcp_get_renewtime(dhcp_msg *pmsg, unsigned long *ptime)
{
	return dhcp_get_4octet(pmsg, ptime, DHCP_RENEWTIME);
}

/* ================================================================

	DHCPメッセージのオプション領域から再結合期間を取得する

	in:		pmsg		メッセージバッファ
			ptime		再結合期間格納域
	out:	NULLなら見つからなかった

   ================================================================ */
unsigned char *dhcp_get_rebindtime(dhcp_msg *pmsg, unsigned long *ptime)
{
	return dhcp_get_4octet(pmsg, ptime, DHCP_REBINDTIME);
}

/* ================================================================

	DHCPメッセージのオプション領域からドメインサーバアドレス配列を取得する

	in:		pmsg		メッセージバッファ
			pdomainsvr	ドメインサーバ配列格納域
	out:	NULLなら見つからなかった

   ================================================================ */
unsigned char *dhcp_get_dns(dhcp_msg *pmsg, unsigned long *pdomainsvr)
{
	unsigned char	*p;

	if((p = dhcp_searchfromopt(pmsg, DHCP_DOMAINSVR)) != NULL) {
		unsigned char	*p2 = p;
		int	i = *p2++ / 4;
		while (i--) {
			*pdomainsvr++ = ntohl(IPADDR(p2[0], p2[1], p2[2], p2[3]));
			p2 += 4;
		}
	}
	return p;
}

/* ================================================================

	DHCPメッセージのオプション領域からドメイン名を取得する

	in:		pmsg		メッセージバッファ
			pbuf		ドメイン名格納域
	out:	NULLなら見つからなかった

   ================================================================ */
unsigned char *dhcp_get_domainname(dhcp_msg *pmsg, char *pbuf)
{
	unsigned char	*p;

	strcpy(pbuf, "");				/* EOS */
	if ((p = dhcp_searchfromopt(pmsg, DHCP_DOMAINNAME)) != NULL) {
		int	len = *p;
		strncpy(pbuf, (char *)&p[1], len);
		strcpy(&pbuf[len], "");		/* EOS */
	}

	return p;
}

/* ================================================================

	DHCPメッセージのオプション部から指定のアイテムを探し、
	アイテム先頭 + 2バイト目からの4バイトデータを返す

	in:		pmsg		メッセージバッファ
	out:	NULLなら見つからなかった

   ================================================================ */
unsigned char *dhcp_get_4octet(dhcp_msg *pmsg, unsigned long *pipaddr, unsigned char item)
{
	unsigned char	*p;

	if((p = dhcp_searchfromopt(pmsg, item)) != NULL) {
		*pipaddr = ntohl(IPADDR(p[1], p[2], p[3], p[4]));
	}
	return p;
}

/* ================================================================

	

   ================================================================ */
int dhcp_isreply(dhcp_msg *pmsg, unsigned long xid, unsigned char *pmsgtype)
{
	if (	(pmsg->h.op != DHCP_BOOTREPLY) ||
			(pmsg->h.xid != xid) ||
			(ntohl(*((unsigned long *)pmsg->options)) != htonl(DHCP_MAGICCOOKIE)) ||
			(pmsg->options[4] != DHCP_MSGTYPE)) {
		return 0;
	};
	*pmsgtype = pmsg->options[6];

	return 1;
}

/* ================================================================

	デバッグ用

   ================================================================ */
void dhcp_print(dhcp_msg *pmsg)
{
	unsigned char	*p;
	unsigned char	itemtype;
	int				msglen;
	unsigned char	buf[255 + 1];

	printf("==== DHCP message start ====\n");
	printf("operation: %d\n", pmsg->h.op);
	printf("client IP addr: %-15s " ,n2a_ipaddr(pmsg->h.ciaddr, buf));
	printf("your IP addr:   %s\n" ,n2a_ipaddr(pmsg->h.yiaddr, buf));
	printf("server IP addr: %-15s " ,n2a_ipaddr(pmsg->h.siaddr, buf));
	printf("router IP addr: %s\n" ,n2a_ipaddr(pmsg->h.giaddr, buf));
	printf("flag: 0x%04x%s transaction ID: 0x%08x\n" ,
			ntohs(pmsg->h.flags),
			(ntohs(pmsg->h.flags) & 0x8000)? "(broadcast)": "", 
			(unsigned int)ntohl(pmsg->h.xid));

	printf("options:\n");
	p = &pmsg->options[4];	/* skip magic cookie */
	while (1) {
		if ((itemtype = *p++) == DHCP_END ) break;
		msglen = *p++;
		memset(buf, 0, 255 + 1);
		switch (itemtype) {
			case DHCP_SUBNETMASK:
printf("\t%02d (DHCP_SUBNETMASK): %s\n", DHCP_SUBNETMASK,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_GATEWAY:
printf("\t%02d (DHCP_GATEWAY): %s\n", DHCP_GATEWAY,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_TIMESVR:
printf("\t%02d (DHCP_TIMESVR): %s\n", DHCP_TIMESVR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_IEN116SVR:
printf("\t%02d (DHCP_IEN116SVR): %s\n", DHCP_IEN116SVR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_DOMAINSVR:
printf("\t%02d (DHCP_DOMAINSVR): %s\n", DHCP_DOMAINSVR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_LOGSVR:
printf("\t%02d (DHCP_LOGSVR): %s\n", DHCP_LOGSVR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_QUOTESVR:
printf("\t%02d (DHCP_QUOTESVR): %s\n", DHCP_QUOTESVR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_LPRSVR:
printf("\t%02d (DHCP_LPRSVR): %s\n", DHCP_LPRSVR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_IMRESSSVR:
printf("\t%02d (DHCP_IMRESSSVR): %s\n", DHCP_IMRESSSVR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_RLPSVR:
printf("\t%02d (DHCP_RLPSVR): %s\n", DHCP_RLPSVR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_HOSTNAME:
				strncpy(buf, p, msglen);
printf("\t%02d (DHCP_HOSTNAME): %s\n", DHCP_HOSTNAME, buf);
				break;
			case DHCP_BOOTSIZE:
printf("\t%02d (DHCP_BOOTSIZE): %s\n", DHCP_BOOTSIZE, "");
				break;
			case DHCP_DOMAINNAME:
				strncpy(buf, p, msglen);
printf("\t%02d (DHCP_DOMAINNAME): %s\n", DHCP_DOMAINNAME, buf);
				break;
			case DHCP_REQIPADDR:
printf("\t%02d (DHCP_REQIPADDR): %s\n", DHCP_REQIPADDR,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_LEASETIME:
printf("\t%02d (DHCP_LEASETIME): %u sec.\n", DHCP_LEASETIME,
	(unsigned int)ntohl(IPADDR(p[0], p[1], p[2], p[3])));
				break;
			case DHCP_MSGTYPE:
printf("\t%02d (DHCP_MSGTYPE): %d\n", DHCP_MSGTYPE, *p);
				break;
			case DHCP_SERVERID:
printf("\t%02d (DHCP_SERVERID): %s\n", DHCP_SERVERID,
	n2a_ipaddr((long)IPADDR(p[0], p[1], p[2], p[3]), buf));
				break;
			case DHCP_MAXMSG:
printf("\t%02d (DHCP_MAXMSG): %u octets\n", DHCP_MAXMSG,
	(unsigned int)ntohs((p[0] << 8) | p[1]));
				break;
			case DHCP_RENEWTIME:
printf("\t%02d (DHCP_RENEWTIME): %u sec.\n", DHCP_RENEWTIME,
	(unsigned int)ntohl(IPADDR(p[0], p[1], p[2], p[3])));
				break;
			case DHCP_REBINDTIME:
printf("\t%02d (DHCP_REBINDTIME): %u sec.\n", DHCP_REBINDTIME,
	(unsigned int)ntohl(IPADDR(p[0], p[1], p[2], p[3])));
				break;
			case DHCP_CLIENTID:
printf("\t%02d (DHCP_CLIENTID): %s\n", DHCP_CLIENTID, "");
				break;
			default:
printf("\t%02d (unknown)\n", itemtype);
				break;
		}
		p += msglen;
	}
	printf("==== DHCP message end ====\n");
}


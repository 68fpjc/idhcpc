#include "dhcp.h"

#include <string.h>

#include "mynetwork.h"
#include "nwsub.h"

static unsigned char *dhcp_msg_common(const dhcp_hw_addr *, const unsigned long,
                                      const unsigned short, const unsigned char,
                                      dhcp_msg *);

/**
 * @brief DHCPDISCOVERメッセージ作成
 * @param phwaddr ハードウェアアドレス情報
 * @param xid トランザクションID（32bit random）
 * @param secs 起動からの経過時間
 * @param[out] pmsg メッセージバッファ
 */
void dhcp_make_dhcpdiscover(const dhcp_hw_addr *phwaddr,
                            const unsigned long xid, const unsigned short secs,
                            dhcp_msg *pmsg) {
  unsigned char *p;

  p = dhcp_msg_common(phwaddr, xid, secs, DHCPDISCOVER, pmsg); /* 共通部 */

  pmsg->h.flags = htons(DHCP_BCAST); /* ブロードキャスト */

  /* DHCPメッセージ長 */
  *p++ = DHCP_MAXMSG;
  *p++ = 2;
  {
    unsigned short tmp = htons(sizeof(*pmsg)); /* RFC2132、間違ってないか？ */
    memcpy(p, &tmp, 2);
    p += 2;
  }

  /* おしまい */
  *p = DHCP_END;
}

/**
 * @brief DHCPREQUESTメッセージ作成
 * @param phwaddr ハードウェアアドレス情報
 * @param yiaddr 要求IPアドレス
 * @param sid サーバID（=DHCPサーバIPアドレス）
 * @param xid トランザクションID（32bit random）
 * @param secs 起動からの経過時間
 * @param[out] pmsg メッセージバッファ
 */
void dhcp_make_dhcprequest(const dhcp_hw_addr *phwaddr,
                           const unsigned long yiaddr, const unsigned long sid,
                           const unsigned long xid, const unsigned short secs,
                           dhcp_msg *pmsg) {
  unsigned char *p;

  p = dhcp_msg_common(phwaddr, xid, secs, DHCPREQUEST, pmsg); /* 共通部 */

  pmsg->h.flags = htons(DHCP_BCAST); /* ブロードキャスト */

  /* 要求IPアドレス */
  *p++ = DHCP_REQIPADDR;
  *p++ = 4;
  {
    unsigned long tmp = htonl(yiaddr);
    memcpy(p, &tmp, 4);
    p += 4;
  }

  /* サーバID */
  *p++ = DHCP_SERVERID;
  *p++ = 4;
  {
    unsigned long tmp = htonl(sid);
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

/**
 * @brief DHCPRELEASEメッセージ作成
 * @param phwaddr ハードウェアアドレス情報
 * @param myaddr クライアントIPアドレス
 * @param sid サーバID（=DHCPサーバIPアドレス）
 * @param xid トランザクションID（32bit random）
 * @param[out] pmsg メッセージバッファ
 */
void dhcp_make_dhcprelease(const dhcp_hw_addr *phwaddr,
                           const unsigned long myaddr, const unsigned long sid,
                           const unsigned long xid, dhcp_msg *pmsg) {
  unsigned char *p;

  p = dhcp_msg_common(phwaddr, xid, 0, DHCPRELEASE, pmsg); /* 共通部 */

  /* クライアントIPアドレス */
  pmsg->h.ciaddr = htonl(myaddr);

  /* サーバID */
  *p++ = DHCP_SERVERID;
  *p++ = 4;
  {
    unsigned long tmp = htonl(sid);
    memcpy(p, &tmp, 4);
    p += 4;
  }

  /* おしまい */
  *p = DHCP_END;
}

/**
 * @brief DHCPメッセージ作成共通部
 * @param phwaddr ハードウェアアドレス情報
 * @param xid トランザクションID（32bit random）
 * @param secs 起動からの経過時間
 * @param msgtype メッセージタイプ
 * @param[out] pmsg メッセージバッファ
 * @return
 */
static unsigned char *dhcp_msg_common(const dhcp_hw_addr *phwaddr,
                                      const unsigned long xid,
                                      const unsigned short secs,
                                      const unsigned char msgtype,
                                      dhcp_msg *pmsg) {
  unsigned char *p = &pmsg->options[4];

  memset(pmsg, 0, sizeof(*pmsg)); /* 必ずゼロで埋めておくこと */

  pmsg->h.op = DHCP_BOOTREQUEST; /* 要求 */
  pmsg->h.htype = phwaddr->arp_hw_type;
  pmsg->h.hlen = phwaddr->hw_addr_len;
  memcpy(&pmsg->h.chaddr, &phwaddr->hw_addr, phwaddr->hw_addr_len);
  pmsg->h.xid = xid;          /* トランザクションID */
  pmsg->h.secs = htons(secs); /* マシン起動からの経過時間 */
  /* オプション部の先頭は必ずmagic cookie */
  *(unsigned long *)pmsg->options = htonl(DHCP_MAGICCOOKIE);

  /* 以下、オプション部を敷き詰めていく */

  /* magic cookieの直後は必ずメッセージタイプ */
  *p++ = DHCP_MSGTYPE;
  *p++ = 1;
  *p++ = msgtype;

  /* クライアントID */
  *p++ = DHCP_CLIENTID;
  *p++ = phwaddr->hw_addr_len;
  memcpy(p, &phwaddr->hw_addr, phwaddr->hw_addr_len);
  p += phwaddr->hw_addr_len;

  return p;
}

/**
 * @brief DHCPメッセージのオプション部から指定のアイテムを探す
 * @param pmsg メッセージバッファ
 * @param item item type
 * @return アイテム先頭 + 1（オクテット数格納アドレス）。NULLならエラー
 */
unsigned char *dhcp_searchfromopt(const dhcp_msg *pmsg,
                                  const unsigned char item) {
  const unsigned char *p = &pmsg->options[4];
  unsigned char itemtype;

  while (1) {
    itemtype = *p++;
    if (itemtype == item) {
      break;
    }
    if (itemtype == DHCP_END) {
      p = NULL;
      break;
    }
    p += 1 + *p;
  }

  return (char *)p;
}

/**
 * @brief DHCPメッセージからYOUR IP ADDRESSを取得する
 * @param pmsg メッセージバッファ
 * @return YOUR IP ADDR
 */
unsigned long dhcp_get_yiaddr(const dhcp_msg *pmsg) {
  return ntohl(pmsg->h.yiaddr);
}

/**
 * @brief DHCPメッセージのオプション領域からサブネットマスクを取得する
 * @param pmsg メッセージバッファ
 * @param pmask サブネットマスク格納域
 * @return NULLなら存在しない
 */
unsigned char *dhcp_get_subnetmask(const dhcp_msg *pmsg, unsigned long *pmask) {
  return dhcp_get_4octet(pmsg, DHCP_SUBNETMASK, pmask);
}

/**
 * @brief
 * DHCPメッセージのオプション領域からサーバID（=DHCPサーバIPアドレス）を取得する
 * @param pmsg メッセージバッファ
 * @param[out] pid サーバID格納域
 * @return NULLなら見つからなかった
 */
unsigned char *dhcp_get_serverid(const dhcp_msg *pmsg, unsigned long *pid) {
  return dhcp_get_4octet(pmsg, DHCP_SERVERID, pid);
}

/**
 * @brief
 * DHCPメッセージのオプション領域からデフォルトゲートウェイアドレスを取得する
 * @param pmsg メッセージバッファ
 * @param proute デフォルトゲートウェイアドレス格納域
 * @return NULLなら見つからなかった
 */
unsigned char *dhcp_get_defroute(const dhcp_msg *pmsg, unsigned long *proute) {
  return dhcp_get_4octet(pmsg, DHCP_GATEWAY, proute);
}

/**
 * @brief DHCPメッセージのオプション領域からリース期間を取得する
 * @param pmsg メッセージバッファ
 * @param ptime リース期間格納域
 * @return NULLなら見つからなかった
 */
unsigned char *dhcp_get_leasetime(const dhcp_msg *pmsg, unsigned long *ptime) {
  return dhcp_get_4octet(pmsg, DHCP_LEASETIME, ptime);
}

/**
 * @brief DHCPメッセージのオプション領域から更新期間を取得する
 * @param pmsg メッセージバッファ
 * @param ptime 更新期間格納域
 * @return NULLなら見つからなかった
 */
unsigned char *dhcp_get_renewtime(const dhcp_msg *pmsg, unsigned long *ptime) {
  return dhcp_get_4octet(pmsg, DHCP_RENEWTIME, ptime);
}

/**
 * @brief DHCPメッセージのオプション領域から再結合期間を取得する
 * @param pmsg メッセージバッファ
 * @param ptime 再結合期間格納域
 * @return NULLなら見つからなかった
 */
unsigned char *dhcp_get_rebindtime(const dhcp_msg *pmsg, unsigned long *ptime) {
  return dhcp_get_4octet(pmsg, DHCP_REBINDTIME, ptime);
}

/**
 * @brief DHCPメッセージのオプション領域からドメインサーバアドレス配列を取得する
 * @param pmsg メッセージバッファ
 * @param pdomainsvr ドメインサーバ配列格納域
 * @return NULLなら見つからなかった
 */
unsigned char *dhcp_get_dns(const dhcp_msg *pmsg, unsigned long *pdomainsvr) {
  unsigned char *p;

  if ((p = dhcp_searchfromopt(pmsg, DHCP_DOMAINSVR)) != NULL) {
    unsigned char *p2 = p;
    int i = *p2++ / 4;
    while (i--) {
      *pdomainsvr++ = ntohl(IPADDR(p2[0], p2[1], p2[2], p2[3]));
      p2 += 4;
    }
  }
  return p;
}

/**
 * @brief DHCPメッセージのオプション領域からドメイン名を取得する
 * @param pmsg メッセージバッファ
 * @param pbuf ドメイン名格納域
 * @return NULLなら見つからなかった
 */
unsigned char *dhcp_get_domainname(const dhcp_msg *pmsg, char *pbuf) {
  unsigned char *p;

  strcpy(pbuf, ""); /* EOS */
  if ((p = dhcp_searchfromopt(pmsg, DHCP_DOMAINNAME)) != NULL) {
    int len = *p;
    strncpy(pbuf, (char *)&p[1], len);
    strcpy(&pbuf[len], ""); /* EOS */
  }

  return p;
}

/**
 * @brief DHCPメッセージのオプション部から指定のアイテムを探し、アイテム先頭 +
 * 2バイト目からの4バイトデータを返す
 * @param pmsg メッセージバッファ
 * @param item
 * @param[out] pout
 * @return NULLなら見つからなかった
 */
unsigned char *dhcp_get_4octet(const dhcp_msg *pmsg, const unsigned char item,
                               unsigned long *pout) {
  unsigned char *p;

  if ((p = dhcp_searchfromopt(pmsg, item)) != NULL) {
    *pout = ntohl(IPADDR(p[1], p[2], p[3], p[4]));
  }
  return p;
}

/**
 * @brief
 * @param pmsg
 * @param xid
 * @param[out] pmsgtype
 * @return
 */
int dhcp_isreply(const dhcp_msg *pmsg, const unsigned long xid,
                 unsigned char *pmsgtype) {
  if ((pmsg->h.op != DHCP_BOOTREPLY) || (pmsg->h.xid != xid) ||
      (ntohl(*((unsigned long *)pmsg->options)) != htonl(DHCP_MAGICCOOKIE)) ||
      (pmsg->options[4] != DHCP_MSGTYPE)) {
    return 0;
  };
  *pmsgtype = pmsg->options[6];

  return 1;
}

/**
 * @brief デバッグ用
 * @param pmsg
 */
void dhcp_print(const dhcp_msg *pmsg) {
  const unsigned char *p;
  unsigned char itemtype;
  int msglen;
  unsigned char buf[255 + 1];

  printf("==== DHCP message start ====\n");
  {
    char *s;
    switch (pmsg->h.op) {
      case DHCP_BOOTREQUEST:
        s = "DHCP_BOOTREQUEST";
        break;
      case DHCP_BOOTREPLY:
        s = "DHCP_BOOTREPLY";
        break;
      default:
        s = "";
        break;
    }
    printf("operation: %d (%s)\n", pmsg->h.op, s);
  }
  printf("client IP addr: %-15s ", n2a_ipaddr(pmsg->h.ciaddr, buf));
  printf("your IP addr:   %s\n", n2a_ipaddr(pmsg->h.yiaddr, buf));
  printf("server IP addr: %-15s ", n2a_ipaddr(pmsg->h.siaddr, buf));
  printf("router IP addr: %s\n", n2a_ipaddr(pmsg->h.giaddr, buf));
  printf("flag: 0x%04x%s transaction ID: 0x%08x\n", ntohs(pmsg->h.flags),
         (ntohs(pmsg->h.flags) & 0x8000) ? "(broadcast)" : "",
         (unsigned int)ntohl(pmsg->h.xid));

  printf("options:\n");
  p = &pmsg->options[4]; /* skip magic cookie */
  while (1) {
    if ((itemtype = *p++) == DHCP_END) break;
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
        /* */
        {
          char *s;
          switch (*p) {
            case DHCPDISCOVER:
              s = "DHCPDISCOVER";
              break;
            case DHCPOFFER:
              s = "DHCPOFFER";
              break;
            case DHCPREQUEST:
              s = "DHCPREQUEST";
              break;
            case DHCPDECLINE:
              s = "DHCPDECLINE";
              break;
            case DHCPACK:
              s = "DHCPACK";
              break;
            case DHCPNAK:
              s = "DHCPNAK";
              break;
            case DHCPRELEASE:
              s = "DHCPRELEASE";
              break;
            case DHCPINFORM:
              s = "DHCPINFORM";
              break;
            default:
              s = "";
              break;
          }
          printf("\t%02d (DHCP_MSGTYPE): %d (%s)\n", DHCP_MSGTYPE, *p, s);
        }
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
        printf("\t%02d (DHCP_CLIENTID):", DHCP_CLIENTID);
        {
          int i;
          for (i = 0; i < msglen; i++) {
            printf(" %02x", p[i] & 0xff);
          }
        }
        printf("\n");
        break;
      default:
        printf("\t%02d (unknown)\n", itemtype);
        break;
    }
    p += msglen;
  }
  printf("==== DHCP message end ====\n");
}

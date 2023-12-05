#include "idhcpc.h"

#include <stdlib.h>
#include <string.h>
#include <sys/dos.h>
#include <sys/iocs.h>

#include "dhcp.h"
#include "mynetwork.h"
#include "nwsub.h"

typedef struct {
  int s; /* 送信用UDPソケット識別子 */
  int r; /* 受信用UDPソケット識別子 */
} udpsockets;

static char g_devname[] = "/dev/en0";
static char g_ifname[] = "en0";

extern const char g_keepst; /* 常駐部先頭アドレス */
extern const char g_magic; /* 常駐判定チェック用文字列アドレス */
extern const char g_keeped; /* 常駐部終端アドレス */

extern int _keepchk(const struct _mep *, const size_t, struct _mep **);

static int offsetof_idhcpcinfo(void);
static udpsockets create_sockets(void);
static errno prepare_discover(udpsockets *, struct sockaddr_in *);
static errno discover_dhcp_server(const int, const eaddr *, const udpsockets *,
                                  unsigned long *, unsigned long *,
                                  struct sockaddr_in *);
static errno request_to_dhcp_server(const int, const eaddr *,
                                    const udpsockets *, const unsigned long,
                                    const unsigned long, idhcpcinfo *,
                                    struct sockaddr_in *);
static errno send_and_receive(const int, const eaddr *, const udpsockets *,
                              const unsigned long, const unsigned long,
                              const int, dhcp_msg *, struct sockaddr_in *);
static errno fill_idhcpcinfo(idhcpcinfo *, unsigned long *, char *, dhcp_msg *);
static errno release_config(const int, const eaddr *, udpsockets *);
static void close_sockets(udpsockets *);
static void iface_when_discover(const char *);
static void iface_when_request(const char *, const unsigned long, const char *);
static void iface_when_release(const char *);
static void delaysec(const int);
static void put_progress(void);

/**
 * @brief 常駐判定
 * @param[out] ppidhcpcinfo
 * idhcpcワークへのポインタ
 *   * 常駐している場合: 常駐プロセスのアドレス
 *   * 常駐していない場合: 自身のアドレス
 * @return 0: 常駐していない
 */
int keepchk(idhcpcinfo **ppidhcpcinfo) {
  struct _mep *pmep;
  int keepflg =
      _keepchk((struct _mep *)((char *)_dos_getpdb() - sizeof(struct _mep)),
               &g_magic - &g_keepst, &pmep);
  *ppidhcpcinfo = (idhcpcinfo *)((char *)pmep + sizeof(struct _mep) +
                                 sizeof(struct _psp) + offsetof_idhcpcinfo());
  return keepflg;
}

/**
 * @brief 常駐終了
 */
void keeppr_and_exit(void) { _dos_keeppr(&g_keeped - &g_keepst, 0); }

/**
 * @brief 常駐解除
 * @param pidhcpcinfo idhcpcワークへのポインタ
 * @return 0: 成功
 */
int freepr(const idhcpcinfo *pidhcpcinfo) {
  return _dos_mfree((void *)((char *)pidhcpcinfo - offsetof_idhcpcinfo() -
                             sizeof(struct _psp)));
}

/**
 * @brief IOCS _ONTIME をコールする
 * @return IOCS _ONTIME の戻り値
 */
int ontime(void) { return _iocs_ontime(); }

/**
 * @brief 常駐処理
 * @param verbose 非0でバーボーズモード
 * @param keepflag 常駐判定フラグ
 * @return エラーコード
 */
errno try_to_keep(const int verbose, const int keepflag) {
  errno err;
  eaddr macaddr;
  udpsockets sockets = create_sockets();
  struct sockaddr_in inaddr_s;
  unsigned long me;
  unsigned long server;

  while (1) {
    if (keepflag) {
      err = ERR_ALREADYKEPT; /* すでに常駐していた */
      break;
    }
    if ((err = prepare_discover(&sockets, &inaddr_s)) != NOERROR) break;
    {
      /* MACアドレス取得 */
      if (!get_mac_address(g_devname, &macaddr)) {
        return ERR_NODEVICE;
      }
      if ((err = discover_dhcp_server(verbose, &macaddr, &sockets, &me, &server,
                                      &inaddr_s)) != NOERROR)
        break;
      if ((err = request_to_dhcp_server(verbose, &macaddr, &sockets, me, server,
                                        &g_idhcpcinfo, &inaddr_s)) != NOERROR)
        break;
    }
    break;
  }

  close_sockets(&sockets);

  return err;
}

/**
 * @brief 常駐解除処理
 * @param verbose 非0でバーボーズモード
 * @param keepflag 常駐判定フラグ
 * @return エラーコード
 */
errno try_to_release(const int verbose, const int keepflag) {
  errno err;
  eaddr macaddr;
  udpsockets sockets = create_sockets();

  /* MACアドレス取得 */
  if (!get_mac_address(g_devname, &macaddr)) {
    return ERR_NODEVICE;
  }

  while (1) {
    if (!keepflag) {
      err = ERR_NOTKEPT; /* 常駐していなかった */
      break;
    }
    if ((err = release_config(verbose, &macaddr, &sockets)) != NOERROR) break;
    break;
  }

  close_sockets(&sockets);

  return err;
}

/**
 * @brief 残りリース期間表示処理メイン
 * @param leasetime （全体）リース期間（秒）
 * @param startat IPアドレス設定時のマシン起動時間（秒）
 */
void print_lease_time(const unsigned long leasetime,
                      const unsigned long startat) {
  int rest, rest_h, rest_m, rest_s;

  if (leasetime == 0xffffffff) {
    printf("リース期間は無期限です.\n");
  } else {
    rest = (int)leasetime - (ontime() / 100 - (int)startat);
    rest_s = rest % 60;
    rest /= 60;
    rest_m = rest % 60;
    rest_h = rest / 60;
    printf("残りリース期間は");
    if (rest_h) printf(" %d 時間", rest_h);
    if (rest_m) printf(" %d 分", rest_m);
    printf(" %d 秒 です.\n", rest_s);
  }
}

/**
 * @brief PSP末尾からidhcpcワーク先頭までのオフセットを返す
 * @return バイト数
 */
static int offsetof_idhcpcinfo(void) {
  return (char *)&g_idhcpcinfo - &g_keepst;
}

/**
 * @brief udpsockets構造体の初期値を返す
 * @return
 */
static udpsockets create_sockets(void) {
  udpsockets ret;
  ret.s = -1;
  ret.r = -1;
  return ret;
}

/**
 * @brief DHCPDISCOVER発行前の前処理
 * @param[out] psockets UDPソケット格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno prepare_discover(udpsockets *psockets,
                              struct sockaddr_in *pinaddr_s) {
  /* INIT時のネットワークインタフェース設定 */
  iface_when_discover(g_ifname);

  /* 送信用UDPソケット作成 */
  if ((psockets->s = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* 受信用UDPソケット作成 */
  if ((psockets->r = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* DHCPサーバポート（67）に接続 */
  if (connect2(psockets->s, DHCP_SERVER_PORT, DHCP_LIMITEDBCAST, pinaddr_s) <
      0) {
    return ERR_CONNECT;
  }
  /* DHCPクライアントポート（68）に接続 */
  if (bind2(psockets->r, DHCP_CLIENT_PORT, 0) < 0) {
    return ERR_BIND;
  }

  return NOERROR;
}

/**
 * @brief DHCPDISCOVERを発行してDHCPOFFERを受信する
 * @param verbose 非0でバーボーズモード
 * @param pmacaddr MACアドレス
 * @param psockets UDPソケット
 * @param[out] pme 要求IPアドレス格納域
 * @param[out] pserver DHCPサーバIPアドレス格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno discover_dhcp_server(const int verbose, const eaddr *pmacaddr,
                                  const udpsockets *psockets,
                                  unsigned long *pme, unsigned long *pserver,
                                  struct sockaddr_in *pinaddr_s) {
  errno err;
  dhcp_msg msg;

  if ((err = send_and_receive(verbose, pmacaddr, psockets, 0, 0, DHCPDISCOVER,
                              &msg, pinaddr_s)) != NOERROR) {
    return err;
  }

  if ((*pme = dhcp_get_yiaddr(&msg)) == 0) { /* 要求IPアドレス */
    return ERR_NOYIADDR;
  }
  if (dhcp_get_serverid(&msg, pserver) == NULL) { /* サーバID */
    return ERR_NOSID;
  }

  return NOERROR;
}

/**
 * @brief DHCPREQUESTを発行してDHCPACKを受信する
 * @param verbose 非0でバーボーズモード
 * @param pmacaddr MACアドレス
 * @param psockets UDPソケット
 * @param me 要求IPアドレス
 * @param server DHCPサーバIPアドレス
 * @param[out] pidhcpcinfo コンフィギュレーション情報格納域
 * @param[out] pdomain ドメイン名格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno request_to_dhcp_server(const int verbose, const eaddr *pmacaddr,
                                    const udpsockets *psockets,
                                    const unsigned long me,
                                    const unsigned long server,
                                    idhcpcinfo *pidhcpcinfo,
                                    struct sockaddr_in *pinaddr_s) {
  errno err;
  dhcp_msg msg;
  unsigned long subnetmask;
  char domainname[256];

  if ((err = send_and_receive(verbose, pmacaddr, psockets, me, server,
                              DHCPREQUEST, &msg, pinaddr_s)) != NOERROR) {
    return err;
  }

  /* 受信結果から要求IPアドレス / サーバIDその他を抜き出す */
  if ((err = fill_idhcpcinfo(pidhcpcinfo, &subnetmask, domainname, &msg)) !=
      NOERROR) {
    return err;
  }

  /* この時点でのマシン起動時間を覚えておく */
  g_idhcpcinfo.startat = (unsigned long)(ontime() / 100);

  /* REQUEST時のネットワークインタフェース設定 */
  iface_when_request(g_ifname, subnetmask, domainname);

  return NOERROR;
}

/**
 * @brief DHCPメッセージ送信 / 受信処理
 * @param verbose 非0でバーボーズモード
 * @param pmacaddr MACアドレス
 * @param psockets UDPソケット
 * @param me 要求IPアドレス
 * @param server DHCPサーバIPアドレス
 * @param msgtype_s DHCPメッセージタイプ（DHCPDISCOVER or DHCPREQUEST）
 * @param[out] prmsg DHCPメッセージバッファ格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno send_and_receive(const int verbose, const eaddr *pmacaddr,
                              const udpsockets *psockets,
                              const unsigned long me,
                              const unsigned long server, const int msgtype_s,
                              dhcp_msg *prmsg, struct sockaddr_in *pinaddr_s) {
  dhcp_msg smsg; /* DHCPメッセージバッファ（送信用） */
  struct sockaddr_in inaddr_r;
  unsigned char msgtype_r; /* 受信データのメッセージタイプ */
  int wait = 4;            /* タイムアウト秒数 */
  int timeout = 0;         /* タイムアウトフラグ */
  int i;                   /* ループカウンタ */

  /* 最大4回再送（計5回送信） */
  for (i = 0; i < 5; i++) {
    unsigned long xid;   /* トランザクションID */
    unsigned short secs; /* 起動からの経過時間（秒） */
    int endat;           /* タイムアウト判定用 */

    if (verbose) {
      if (i > 0) printf(" リトライします（%d回目）...\n", i);
      fflush(stdout);
    }

    /* メッセージ送信処理 */
    xid = random(); /* トランザクションID設定 */
    secs = (unsigned short)(ontime() / 100);
    switch (msgtype_s) {
      case DHCPDISCOVER:
        dhcp_make_dhcpdiscover(pmacaddr, xid, secs, &smsg);
        break;
      case DHCPREQUEST:
        dhcp_make_dhcprequest(me, server, pmacaddr, xid, secs, &smsg);
        break;
      default:
        break;
    }
    if (verbose) {
      dhcp_print(&smsg);
      printf("DHCPサーバポート（67）へ送信中 ...\n");
      fflush(stdout);
    }
    sendto(psockets->s, (char *)&smsg, sizeof(smsg), 0, (char *)pinaddr_s,
           sizeof(*pinaddr_s));
    /* while (socklen(g_sock_s, 1)) ;*/ /* 送信完了待ち */

    /* メッセージ受信処理 */
    if (verbose) {
      printf(
          "DHCPクライアントポート（68）から受信中. "
          "約%d秒後にタイムアウトします ...",
          wait);
      fflush(stdout);
    }

    /* タイムアウトリミット設定 */
    /* 4, 8, 16, 32, 64 ± 1秒くらい */
    endat = ontime() + (wait - 1) * 100 + random() % 200;
    timeout = 0;
    while (1) {
      int len = sizeof(inaddr_r);

      if (ontime() > endat) {
        timeout = 1;
        break;
      }
      if (!socklen(psockets->r, 0)) {
        if (verbose) put_progress();
        continue;
      }
      recvfrom(psockets->r, (char *)prmsg, sizeof(*prmsg), 0, (char *)&inaddr_r,
               &len);
      if (!dhcp_isreply(prmsg, xid, &msgtype_r)) continue;
      if (msgtype_s == DHCPDISCOVER) {
        if (msgtype_r == DHCPOFFER) break; /* 受信完了 */
      } else if (msgtype_s == DHCPREQUEST) {
        if ((msgtype_r == DHCPACK) || (msgtype_r == DHCPNAK))
          break; /* 受信完了 */
      }
    }
    if (!timeout) {
      if (verbose) {
        printf(" done.\n");
        dhcp_print(prmsg);
      }
      break; /* 受信完了でループ脱出 */
    }
    if (verbose) printf(" タイムアウトです.");
    wait <<= 1;
  }

  if (timeout) return ERR_TIMEOUT; /* タイムアウト */

  if (msgtype_s == DHCPREQUEST) {
    if (msgtype_r == DHCPNAK) return ERR_NAK;
  }

  return NOERROR;
}

/**
 * @brief コンフィギュレーション情報をセーブする
 * @param[out] pidhcpcinfo コンフィギュレーション情報格納域
 * @param[out] pmask サブネットマスク格納域
 * @param[out] pdomain ドメイン名格納域
 * @param[out] pmsg DHCPOFFERまたはDHCPACKで受信したDHCPメッセージ
 * @return エラーコード
 */
static errno fill_idhcpcinfo(idhcpcinfo *pidhcpcinfo, unsigned long *pmask,
                             char *pdomain, dhcp_msg *pmsg) {
  if ((pidhcpcinfo->me = dhcp_get_yiaddr(pmsg)) == 0) { /* 要求IPアドレス */
    return ERR_NOYIADDR;
  }
  if (dhcp_get_serverid(pmsg, &pidhcpcinfo->server) == NULL) { /* サーバID */
    return ERR_NOSID;
  }
  pidhcpcinfo->gateway = 0; /* デフォルトゲートウェイ */
  dhcp_get_defroute(pmsg, &pidhcpcinfo->gateway);
  dhcp_get_dns(pmsg, pidhcpcinfo->dns); /* ドメインサーバ（配列） */
  if (dhcp_get_leasetime(pmsg, &pidhcpcinfo->leasetime) ==
      NULL) { /* リース期間 */
    /* リース期間が渡されなかった！？ */
    return ERR_NOLEASETIME;
  }
  pidhcpcinfo->renewtime = 0; /* 更新時間 */
  dhcp_get_renewtime(pmsg, &pidhcpcinfo->renewtime);
  if (pidhcpcinfo->renewtime == 0) {
    pidhcpcinfo->renewtime = pidhcpcinfo->leasetime / 2;
  }
  pidhcpcinfo->rebindtime = 0; /* 再結合時間 */
  dhcp_get_rebindtime(pmsg, &pidhcpcinfo->rebindtime);
  if (pidhcpcinfo->rebindtime == 0) {
    pidhcpcinfo->rebindtime = pidhcpcinfo->leasetime * 857 / 1000;
  }
  *pmask = 0; /* サブネットマスク */
  dhcp_get_subnetmask(pmsg, pmask);
  dhcp_get_domainname(pmsg, pdomain); /* ドメイン名 */

  return NOERROR;
}

/**
 * @brief DHCPRELEASEを発行してネットワークインタフェースを初期化する
 * @param verbose 非0でバーボーズモード
 * @param pmacaddr MACアドレス
 * @param[out] psockets UDPソケット
 * @return エラーコード
 */
static errno release_config(const int verbose, const eaddr *pmacaddr,
                            udpsockets *psockets) {
  dhcp_msg msg; /* DHCPメッセージバッファ */
  struct sockaddr_in inaddr_s;

  /* 送信用UDPソケット作成 */
  if ((psockets->s = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* DHCPサーバポート（67）に接続 */
  if (connect2(psockets->s, DHCP_SERVER_PORT, g_idhcpcinfo.server, &inaddr_s) <
      0) {
    return ERR_CONNECT;
  }

  /* DHCPRELEASEメッセージ送信処理 */
  dhcp_make_dhcprelease(g_idhcpcinfo.me, g_idhcpcinfo.server, pmacaddr,
                        random(), &msg);
  if (verbose) {
    dhcp_print(&msg);
    printf("DHCPサーバポート（67）へ送信中 ...\n");
    fflush(stdout);
  }
  sendto(psockets->s, (char *)&msg, sizeof(msg), 0, (char *)&inaddr_s,
         sizeof(inaddr_s));
  /* while (socklen(g_sock_s, 1)) ;*/

  iface_when_release(g_ifname);

  return NOERROR;
}

/**
 * @brief オープン済みのソケットをすべてクローズする
 * @param[out] psockets UDPソケット
 */
static void close_sockets(udpsockets *psockets) {
  if (psockets->s != -1) {
    close_s(psockets->s);
    psockets->s = -1;
  }
  if (psockets->r != -1) {
    close_s(psockets->r);
    psockets->r = -1;
  }
}

/**
 * @brief DHCPDISCOVER時のネットワークインタフェース設定
 * @param ifname インタフェース名（"en0"）
 */
static void iface_when_discover(const char *ifname) {
  iface *p, *p0;

  if ((p0 = iface_lookupn((char *)ifname)) != NULL) {
    p = p0;
  } else {
    p = get_new_iface((char *)ifname);
  }
  p->my_ip_addr = 0;
  p->broad_cast = DHCP_LIMITEDBCAST;
  p->flag |= IFACE_UP | IFACE_BROAD;
  if (p0 == NULL) {
    link_new_iface(p);
  }
  /* delaysec(150); */
}

/**
 * @brief DHCPREQUEST時のネットワークインタフェース設定
 * @param ifname インタフェース名（"en0"）
 * @param subnetmask サブネットマスク
 * @param domainname ドメイン名
 */
static void iface_when_request(const char *ifname,
                               const unsigned long subnetmask,
                               const char *domainname) {
  iface *p = iface_lookupn((char *)ifname);

  p->my_ip_addr = g_idhcpcinfo.me;
  p->net_mask = subnetmask;
  p->broad_cast = (p->my_ip_addr & subnetmask) | ~subnetmask;
  p->flag |= IFACE_UP;

  {
    unsigned long *p = g_idhcpcinfo.dns, addr;

    while ((addr = *p++)) {
      dns_add((long)addr);
    }
  }
  if (strcmp(domainname, "")) {
    set_domain_name((char *)domainname);
  }
  if (g_idhcpcinfo.gateway) {
    route *def;
    rt_top(&def);
    def->gateway = (long)g_idhcpcinfo.gateway;
  }
/* delaysec(150);
*/}

/**
 * @brief DHCPRELEASE発行後のネットワークインタフェースの辻褄合わせ
 * @param ifname インタフェース名（"en0"）
 */
static void iface_when_release(const char *ifname) {
  iface *p = iface_lookupn((char *)ifname);

  if (p->flag & IFACE_UP) {
    p->stop(p); /* これを実行しないと割り込みが消えないらしい */
    delaysec(150); /* 多少ウェイトをかまさないとまずいみたい */
  }
  p->my_ip_addr = 0;
  p->net_mask = 0;
  p->broad_cast = 0;
  p->flag &= ~(IFACE_UP | IFACE_BROAD);

  {
    unsigned long *p = g_idhcpcinfo.dns, addr;

    while ((addr = *p++)) {
      dns_drop((long)addr);
    }
  }
  set_domain_name("");
  if (g_idhcpcinfo.gateway) {
    route *def;
    rt_top(&def);
    def->gateway = 0;
  }
}

/**
 * @brief 1/100秒単位でウェイトをかます
 * @param tm ウェイトカウント（）
 */
static void delaysec(const int tm) {
  int endat = ontime() + tm;

  while (endat > ontime())
    ;
}

/**
 * @brief インチキプログレス表示
 * @param
 */
static void put_progress(void) {
  static int ontime0 = 0;
  int ontime1 = ontime();
  if (ontime1 - ontime0 >= 50) {
    printf(".");
    fflush(stdout);
    ontime0 = ontime1;
  }
}

#include "idhcpc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dos.h>
#include <sys/iocs.h>
#include <time.h>
#include <unistd.h>

#include "dhcp.h"
#include "mynetwork.h"
#include "nwsub.h"

#define MAGIC_FORMAT "%s idhcpc https://github.com/68fpjc"

typedef struct {
  int s; /* 送信用UDPソケット識別子 */
  int r; /* 受信用UDPソケット識別子 */
} udpsockets;

extern const char g_keepst; /* 常駐部先頭アドレス */
extern const char g_keeped; /* 常駐部終端アドレス */

extern int _keepchk(const struct _mep *, const size_t, struct _mep **);

static int offsetof_idhcpcinfo(void);
static udpsockets create_sockets(void);
static errno prepare_iface(const char *, iface **, dhcp_hw_addr *);
static errno prepare_discover(iface *, udpsockets *, struct sockaddr_in *);
static errno discover_dhcp_server(const int, const dhcp_hw_addr *,
                                  const udpsockets *, unsigned long *,
                                  unsigned long *, struct sockaddr_in *);
static errno request_to_dhcp_server(const int, const dhcp_hw_addr *,
                                    const udpsockets *, const unsigned long,
                                    const unsigned long, idhcpcinfo *,
                                    struct sockaddr_in *, iface *);
static errno send_and_receive(const int, const dhcp_hw_addr *,
                              const udpsockets *, const unsigned long,
                              const unsigned long, const int, dhcp_msg *,
                              struct sockaddr_in *);
static errno fill_idhcpcinfo(idhcpcinfo *, unsigned long *, char *, dhcp_msg *);
static errno release_config(const int, iface *, dhcp_hw_addr *, udpsockets *);
static void close_sockets(udpsockets *);
static void iface_when_discover(iface *);
static void iface_when_request(const unsigned long, const char *, iface *);
static void iface_when_release(iface *);
static void fill_dhcp_hw_addr(const iface *, dhcp_hw_addr *);
static void delaysec(const int);
static void put_progress(void);

/**
 * @brief 常駐判定
 * @param ifname インタフェース名
 * @param[out] ppidhcpcinfo
 * idhcpcワークへのポインタ
 *   * 常駐している場合: 常駐プロセスのアドレス
 *   * 常駐していない場合: 自身のアドレス
 * @return 0: 常駐していない
 */
int keepchk(const char *ifname, idhcpcinfo **ppidhcpcinfo) {
  /* idhcpcワークのサイズチェック */
  assert(&g_idhcpcinfoed - (char *)&g_idhcpcinfo == sizeof(idhcpcinfo));

  /* インタフェース名をグローバルワークへコピーする */
  strncpy(g_idhcpcinfo.ifname, ifname, sizeof(g_idhcpcinfo.ifname));
  /* 常駐チェック用文字列を生成してグローバルワークへコピーする */
  snprintf(g_idhcpcinfo.magic, sizeof(g_idhcpcinfo.magic), MAGIC_FORMAT,
           g_idhcpcinfo.ifname);
  {
    struct _mep *pmep;
    int keepflg = _keepchk(
        (struct _mep *)((char *)_dos_getpdb() - sizeof(struct _mep)),
        g_idhcpcinfo.magic - (char *)&g_idhcpcinfo + offsetof_idhcpcinfo(),
        &pmep);
    *ppidhcpcinfo = (idhcpcinfo *)((char *)pmep + sizeof(struct _mep) +
                                   sizeof(struct _psp) + offsetof_idhcpcinfo());
    return keepflg;
  }
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
 * @param ifname インタフェース名
 * @return エラーコード
 */
errno try_to_keep(const int verbose, const int keepflag) {
  errno err;

  if (keepflag) {
    err = ERR_ALREADYKEPT;
  } else {
    iface *piface;
    dhcp_hw_addr hwaddr;

    if ((err = prepare_iface(g_idhcpcinfo.ifname, &piface, &hwaddr)) ==
        NOERROR) {
      udpsockets sockets = create_sockets();
      struct sockaddr_in inaddr_s;
      unsigned long me;
      unsigned long server;

      g_idhcpcinfo.startat = time(NULL); /* 起動日時 */

      (!0) &&
          ((err = prepare_discover(piface, &sockets, &inaddr_s)) == NOERROR) &&
          ((err = discover_dhcp_server(verbose, &hwaddr, &sockets, &me, &server,
                                       &inaddr_s)) == NOERROR) &&
          ((err = request_to_dhcp_server(verbose, &hwaddr, &sockets, me, server,
                                         &g_idhcpcinfo, &inaddr_s, piface)) ==
           NOERROR);
      close_sockets(&sockets);
    }
  }
  return err;
}

/**
 * @brief 常駐解除処理
 * @param verbose 非0でバーボーズモード
 * @param keepflag 常駐判定フラグ
 * @param ifname インタフェース名
 * @return エラーコード
 */
errno try_to_release(const int verbose, const int keepflag) {
  errno err;

  if (!keepflag) {
    err = ERR_NOTKEPT;
  } else {
    iface *piface;
    dhcp_hw_addr hwaddr;

    if (prepare_iface(g_idhcpcinfo.ifname, &piface, &hwaddr) != NOERROR) {
      err = NOERROR; /* DHCPRELEASEの発行は行わず、常駐解除のみ */
    } else {
      udpsockets sockets = create_sockets();

      if ((err = release_config(verbose, piface, &hwaddr, &sockets)) !=
          NOERROR) {
        return err;
      }
      close_sockets(&sockets);
    }
  }
  return err;
}

/**
 * @brief 残りリース期間を返す
 * @return 残りリース期間. 無期限の場合は -1
 */
int get_remaining(void) {
  return g_idhcpcinfo.leasetime == 0xffffffff
             ? -1
             : (int)g_idhcpcinfo.leasetime -
                   (int)difftime(time(NULL), g_idhcpcinfo.dhcpackat);
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
 * @brief インタフェースとハードウェアアドレス情報を取得する
 * @param ifname インタフェース名
 * @param ppiface ifaceポインタ格納域
 * @param phwaddr ハードウェアアドレス情報格納域
 * @return エラーコード
 */
static errno prepare_iface(const char *ifname, iface **ppiface,
                           dhcp_hw_addr *phwaddr) {
  if (_get_version() < 0) {
    return ERR_NODEVICE;
  }
  {
    iface *p;

    if ((p = get_new_iface((char *)ifname)) == NULL) {
      return ERR_NOIFACE;
    }
    *ppiface = p;
  }
  fill_dhcp_hw_addr(*ppiface, phwaddr);
  return NOERROR;
}

/**
 * @brief DHCPDISCOVER発行前の前処理
 * @param piface インタフェース
 * @param[out] psockets UDPソケット格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno prepare_discover(iface *piface, udpsockets *psockets,
                              struct sockaddr_in *pinaddr_s) {
  /* INIT時のネットワークインタフェース設定 */
  iface_when_discover(piface);

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
 * @param phwaddr ハードウェアアドレス情報
 * @param psockets UDPソケット
 * @param[out] pme 要求IPアドレス格納域
 * @param[out] pserver DHCPサーバIPアドレス格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno discover_dhcp_server(const int verbose,
                                  const dhcp_hw_addr *phwaddr,
                                  const udpsockets *psockets,
                                  unsigned long *pme, unsigned long *pserver,
                                  struct sockaddr_in *pinaddr_s) {
  errno err;
  dhcp_msg msg;

  if ((err = send_and_receive(verbose, phwaddr, psockets, 0, 0, DHCPDISCOVER,
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
 * @param phwaddr ハードウェアアドレス情報
 * @param psockets UDPソケット
 * @param me 要求IPアドレス
 * @param server DHCPサーバIPアドレス
 * @param[out] pidhcpcinfo コンフィギュレーション情報格納域
 * @param[out] pdomain ドメイン名格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @param[out] piface インタフェース
 * @return エラーコード
 */
static errno request_to_dhcp_server(
    const int verbose, const dhcp_hw_addr *phwaddr, const udpsockets *psockets,
    const unsigned long me, const unsigned long server, idhcpcinfo *pidhcpcinfo,
    struct sockaddr_in *pinaddr_s, iface *piface) {
  errno err;
  dhcp_msg msg;
  unsigned long subnetmask;
  char domainname[256];

  if ((err = send_and_receive(verbose, phwaddr, psockets, me, server,
                              DHCPREQUEST, &msg, pinaddr_s)) != NOERROR) {
    return err;
  }

  pidhcpcinfo->dhcpackat = time(NULL); /* DHCPACK 受信日時 */

  /* 受信結果から要求IPアドレス / サーバIDその他を抜き出す */
  if ((err = fill_idhcpcinfo(pidhcpcinfo, &subnetmask, domainname, &msg)) !=
      NOERROR) {
    return err;
  }

  /* REQUEST時のネットワークインタフェース設定 */
  iface_when_request(subnetmask, domainname, piface);

  return NOERROR;
}

/**
 * @brief DHCPメッセージ送信 / 受信処理
 * @param verbose 非0でバーボーズモード
 * @param phwaddr ハードウェアアドレス情報
 * @param psockets UDPソケット
 * @param me 要求IPアドレス
 * @param server DHCPサーバIPアドレス
 * @param msgtype_s DHCPメッセージタイプ（DHCPDISCOVER or DHCPREQUEST）
 * @param[out] prmsg DHCPメッセージバッファ格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno send_and_receive(const int verbose, const dhcp_hw_addr *phwaddr,
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
    secs = (unsigned short)difftime(
        time(NULL), g_idhcpcinfo.startat); /* 起動からの経過時間 */
    switch (msgtype_s) {
      case DHCPDISCOVER:
        dhcp_make_dhcpdiscover(phwaddr, xid, secs, &smsg);
        break;
      case DHCPREQUEST:
        dhcp_make_dhcprequest(phwaddr, me, server, xid, secs, &smsg);
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
 * @param piface インタフェース
 * @param phwaddr ハードウェアアドレス情報
 * @param[out] psockets UDPソケット
 * @return エラーコード
 */
static errno release_config(const int verbose, iface *piface,
                            dhcp_hw_addr *phwaddr, udpsockets *psockets) {
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
  dhcp_make_dhcprelease(phwaddr, g_idhcpcinfo.me, g_idhcpcinfo.server, random(),
                        &msg);
  if (verbose) {
    dhcp_print(&msg);
    printf("DHCPサーバポート（67）へ送信中 ...\n");
    fflush(stdout);
  }
  sendto(psockets->s, (char *)&msg, sizeof(msg), 0, (char *)&inaddr_s,
         sizeof(inaddr_s));
  delaysec(50); /* 少し待つ */
  iface_when_release(piface);

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
 * @param piface インタフェース
 */
static void iface_when_discover(iface *piface) {
  piface->my_ip_addr = 0;
  piface->broad_cast = DHCP_LIMITEDBCAST;
  piface->flag |= IFACE_UP | IFACE_BROAD;
  link_new_iface(piface);
}

/**
 * @brief DHCPREQUEST時のネットワークインタフェース設定
 * @param subnetmask サブネットマスク
 * @param domainname ドメイン名
 * @param[out] piface インタフェース
 */
static void iface_when_request(const unsigned long subnetmask,
                               const char *domainname, iface *piface) {
  piface->my_ip_addr = g_idhcpcinfo.me;
  piface->net_mask = subnetmask;
  piface->broad_cast = (piface->my_ip_addr & subnetmask) | ~subnetmask;
  piface->flag |= IFACE_UP;
  link_new_iface(piface);
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
  /* delaysec(150);*/
}

/**
 * @brief DHCPRELEASE発行後のネットワークインタフェースの辻褄合わせ
 * @param ifname インタフェース名
 */
static void iface_when_release(iface *piface) {
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
  {
    if (piface->flag & IFACE_UP) {
      piface->stop(piface);
    }
    piface->my_ip_addr = 0;
    piface->net_mask = 0;
    piface->broad_cast = 0;
    piface->flag &= ~(IFACE_UP | IFACE_BROAD);
    link_new_iface(piface);
  }
}

/**
 * @brief インタフェースからハードウェアアドレス情報を取得する
 * @param piface インタフェース
 * @param[out] phwaddr ハードウェアアドレス情報格納域
 */
static void fill_dhcp_hw_addr(const iface *piface, dhcp_hw_addr *phwaddr) {
  phwaddr->arp_hw_type = piface->arp_hw_type;
  phwaddr->hw_addr_len = piface->hw_addr_len;
  memcpy(&phwaddr->hw_addr, &piface->my_hw_addr, piface->hw_addr_len);
}

/**
 * @brief 1/100秒単位でウェイトをかます
 * @param tm ウェイトカウント（）
 */
static void delaysec(const int tm) { usleep(tm * 10000); }

/**
 * @brief インチキプログレス表示
 * @param
 */
static void put_progress(void) {
  static time_t oldtime = 0;
  if (oldtime == 0) {
    oldtime = time(NULL);
  }
  { /* 1 秒おきにドットを表示する */
    time_t newtime = time(NULL);
    if (difftime(newtime, oldtime) > 0) {
      printf(".");
      fflush(stdout);
      oldtime = newtime;
    }
  }
}

#include "idhcpc.h"

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
  int s; /* 送信用 UDP ソケット識別子 */
  int r; /* 受信用 UDP ソケット識別子 */
} udpsockets;

static int initialize(const char *);
static struct _psp *getpdb(void);
static struct _mep *getmep(void);
static void *getkeepst(void);
static int offsetof_idhcpcinfo(void);
static int getmagicoffset(void);
static int getkeepsize(void);
static udpsockets create_sockets(void);
static errno prepare_iface(const char *, iface **, dhcp_hw_addr *);
static errno prepare_discover(iface *, udpsockets *, struct sockaddr_in *);
static errno discover_dhcp_server(const int, const dhcp_hw_addr *,
                                  const udpsockets *, unsigned long *,
                                  unsigned long *, struct sockaddr_in *);
static errno request_to_dhcp_server(const int, const dhcp_hw_addr *,
                                    const udpsockets *, const unsigned long,
                                    const unsigned long, struct sockaddr_in *,
                                    iface *);
static errno send_and_receive(const int, const dhcp_hw_addr *,
                              const udpsockets *, const unsigned long,
                              const unsigned long, const int, dhcp_msg *,
                              struct sockaddr_in *);
static errno fill_idhcpcinfo(unsigned long *, char *, dhcp_msg *);
static errno release_config(const int, iface *, dhcp_hw_addr *, udpsockets *);
static void close_sockets(udpsockets *);
static void iface_when_discover(iface *);
static void iface_when_request(const unsigned long, const char *, iface *);
static void iface_when_release(iface *);
static void fill_dhcp_hw_addr(const iface *, dhcp_hw_addr *);
static void delaysec(const int);
static void put_progress(void);

idhcpcinfo *g_pidhcpcinfo; /* initialize() で初期化される */

/**
 * @brief 常駐サイズ計算用のダミー関数
 */
static void keeped(void) {}

/**
 * @brief 常駐終了
 * @param ifname インタフェース名
 */
void keeppr_and_exit(void) { _dos_keeppr(getkeepsize(), 0); }

/**
 * @brief 常駐解除
 * @param ifname インタフェース名
 * @return 0: 成功
 * @remark 常駐していない場合は何もしない
 */
int freepr(const char *ifname) {
  return initialize(ifname)
             ? _dos_mfree((void *)((int)g_pidhcpcinfo - sizeof(struct _psp)))
             : -1;
}

/**
 * @brief IOCS _ONTIME をコールする
 * @return IOCS _ONTIME の戻り値
 */
int ontime(void) { return _iocs_ontime(); }

/**
 * @brief 常駐処理
 * @param verbose 非 0 でバーボーズモード
 * @param ifname インタフェース名
 * @return エラーコード
 */
errno try_to_keep(const int verbose, const char *ifname) {
  errno err;

  if (initialize(ifname)) {
    err = ERR_ALREADYKEPT;
  } else {
    iface *piface;
    dhcp_hw_addr hwaddr;

    if ((err = prepare_iface(g_pidhcpcinfo->ifname, &piface, &hwaddr)) ==
        NOERROR) {
      udpsockets sockets = create_sockets();
      struct sockaddr_in inaddr_s;
      unsigned long me;
      unsigned long server;

      g_pidhcpcinfo->startat = time(NULL); /* 起動日時 */

      (!0) &&
          ((err = prepare_discover(piface, &sockets, &inaddr_s)) == NOERROR) &&
          ((err = discover_dhcp_server(verbose, &hwaddr, &sockets, &me, &server,
                                       &inaddr_s)) == NOERROR) &&
          ((err = request_to_dhcp_server(verbose, &hwaddr, &sockets, me, server,
                                         &inaddr_s, piface)) == NOERROR);
      close_sockets(&sockets);
    }
  }
  return err;
}

/**
 * @brief 常駐解除処理
 * @param verbose 非 0 でバーボーズモード
 * @param ifname インタフェース名
 * @return エラーコード
 */
errno try_to_release(const int verbose, const char *ifname) {
  errno err;

  if (!initialize(ifname)) {
    err = ERR_NOTKEPT;
  } else {
    iface *piface;
    dhcp_hw_addr hwaddr;

    if (prepare_iface(g_pidhcpcinfo->ifname, &piface, &hwaddr) != NOERROR) {
      err = NOERROR; /* DHCPRELEASE の発行は行わず、常駐解除のみ */
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
 * @param ifname インタフェース名
 * @param force 非 0 で強制的にリース期間を取得する (常駐時専用)
 * @param[out] pleasetime 残りリース期間格納域. 無期限の場合は -1 が格納される
 * @return エラーコード
 */
errno get_remaining(const char *ifname, const int force, int *pleasetime) {
  errno err;

  if (!initialize(ifname) && !force) {
    err = ERR_NOTKEPT;
  } else {
    unsigned long leasetime = g_pidhcpcinfo->leasetime;
    *pleasetime = leasetime == 0xffffffff
                      ? -1
                      : (int)leasetime -
                            (int)difftime(time(NULL), g_pidhcpcinfo->dhcpackat);
    err = NOERROR;
  }
  return err;
}

/**
 * @brief 初期化処理
 *   * 常駐判定
 *   * g_pidhcpcinfo の初期化
 * @param ifname インタフェース名
 * @return 0: 常駐していない
 */
static int initialize(const char *ifname) {
  static int initialized = 0;
  static int kept = 0;

  if (!initialized) {
    extern idhcpcinfo g_idhcpcinfo; /* idhcpc ワーク */

    /* インタフェース名を現プロセスのワークへコピーしておく */
    strncpy(g_idhcpcinfo.ifname, ifname, sizeof(g_idhcpcinfo.ifname));
    /* 常駐チェック用文字列を生成して現プロセスのワークへコピーしておく */
    snprintf(g_idhcpcinfo.magic, sizeof(g_idhcpcinfo.magic), MAGIC_FORMAT,
             g_idhcpcinfo.ifname);
    {
      extern int _keepchk(const struct _mep *, const size_t, struct _mep **);

      struct _mep *pmep;

      kept = _keepchk(getmep(), getmagicoffset(), &pmep);

      g_pidhcpcinfo = /* 常駐している場合は常駐プロセスへのポインタ、そうでない場合は現プロセスへのポインタをセットする
                       */
          (idhcpcinfo *)((int)pmep + sizeof(struct _mep) + sizeof(struct _psp) +
                         offsetof_idhcpcinfo());
    }
    initialized = 1;
  }
  return kept;
}

/**
 * @brief DOS _GETPDB の結果を返す
 * @return DOS _GETPDB の結果
 */
static struct _psp *getpdb(void) {
  static struct _psp *ppsp = NULL;
  if (ppsp == NULL) {
    ppsp = _dos_getpdb();
  }
  return ppsp;
}

/**
 * @brief 現プロセスのメモリ管理ポインタを返す
 * @return 現プロセスのメモリ管理ポインタ
 */
static struct _mep *getmep(void) {
  static struct _mep *pmep = NULL;
  if (pmep == NULL) {
    pmep = (struct _mep *)((int)getpdb() - sizeof(struct _mep));
  }
  return pmep;
}

/**
 * @brief 現在のプログラム先頭アドレスを返す
 * @return 現在のプログラム先頭アドレス
 */
static void *getkeepst(void) {
  return (void *)((int)getpdb() + sizeof(struct _psp));
}

/**
 * @brief プログラム先頭から idhcpc ワーク先頭までのオフセットを返す
 * @return オフセット
 */
static int offsetof_idhcpcinfo(void) {
  extern idhcpcinfo g_idhcpcinfo; /* idhcpc ワーク */

  return (int)&g_idhcpcinfo - (int)getkeepst();
}

/**
 * @brief プログラム先頭から識別用文字列までのオフセットを返す
 * @return プログラム先頭から識別用文字列までのオフセット
 */
static int getmagicoffset(void) {
  extern idhcpcinfo g_idhcpcinfo; /* idhcpc ワーク */

  return (int)&g_idhcpcinfo.magic - (int)&g_idhcpcinfo;
}

/**
 * @brief 常駐サイズを返す
 * @return 常駐サイズ
 */
static int getkeepsize(void) { return (int)&keeped - (int)getkeepst(); }

/**
 * @brief udpsockets 構造体の初期値を返す
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
 * @param ppiface iface ポインタ格納域
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
 * @brief DHCPDISCOVER 発行前の前処理
 * @param piface インタフェース
 * @param[out] psockets UDP ソケット格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno prepare_discover(iface *piface, udpsockets *psockets,
                              struct sockaddr_in *pinaddr_s) {
  /* INIT 時のネットワークインタフェース設定 */
  iface_when_discover(piface);

  /* 送信用 UDP ソケット作成 */
  if ((psockets->s = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* 受信用 UDP ソケット作成 */
  if ((psockets->r = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* DHCP サーバポート (67) に接続 */
  if (connect2(psockets->s, DHCP_SERVER_PORT, DHCP_LIMITEDBCAST, pinaddr_s) <
      0) {
    return ERR_CONNECT;
  }
  /* DHCPクライアントポート (68) に接続 */
  if (bind2(psockets->r, DHCP_CLIENT_PORT, 0) < 0) {
    return ERR_BIND;
  }

  return NOERROR;
}

/**
 * @brief DHCPDISCOVER を発行して DHCPOFFER を受信する
 * @param verbose 非 0 でバーボーズモード
 * @param phwaddr ハードウェアアドレス情報
 * @param psockets UDP ソケット
 * @param[out] pme 要求 IP アドレス格納域
 * @param[out] pserver DHCP サーバ IP アドレス格納域
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

  if ((*pme = dhcp_get_yiaddr(&msg)) == 0) { /* 要求 IP アドレス */
    return ERR_NOYIADDR;
  }
  if (dhcp_get_serverid(&msg, pserver) == NULL) { /* サーバ ID */
    return ERR_NOSID;
  }

  return NOERROR;
}

/**
 * @brief DHCPREQUEST を発行して DHCPACK を受信する
 * @param verbose 非 0 でバーボーズモード
 * @param phwaddr ハードウェアアドレス情報
 * @param psockets UDP ソケット
 * @param me 要求 IP アドレス
 * @param server DHCP サーバ IP アドレス
 * @param[out] pdomain ドメイン名格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @param[out] piface インタフェース
 * @return エラーコード
 */
static errno request_to_dhcp_server(
    const int verbose, const dhcp_hw_addr *phwaddr, const udpsockets *psockets,
    const unsigned long me, const unsigned long server,
    struct sockaddr_in *pinaddr_s, iface *piface) {
  errno err;
  dhcp_msg msg;
  unsigned long subnetmask;
  char domainname[256];

  if ((err = send_and_receive(verbose, phwaddr, psockets, me, server,
                              DHCPREQUEST, &msg, pinaddr_s)) != NOERROR) {
    return err;
  }

  g_pidhcpcinfo->dhcpackat = time(NULL); /* DHCPACK 受信日時 */

  /* 受信結果から要求 IP アドレス / サーバIDその他を抜き出す */
  if ((err = fill_idhcpcinfo(&subnetmask, domainname, &msg)) != NOERROR) {
    return err;
  }

  /* REQUEST 時のネットワークインタフェース設定 */
  iface_when_request(subnetmask, domainname, piface);

  return NOERROR;
}

/**
 * @brief DHCP メッセージ送信 / 受信処理
 * @param verbose 非 0 でバーボーズモード
 * @param phwaddr ハードウェアアドレス情報
 * @param psockets UDP ソケット
 * @param me 要求 IP アドレス
 * @param server DHCP サーバ IP アドレス
 * @param msgtype_s DHCP メッセージタイプ (DHCPDISCOVER or DHCPREQUEST)
 * @param[out] prmsg DHCP メッセージバッファ格納域
 * @param[out] pinaddr_s 送信用ソケット情報格納域
 * @return エラーコード
 */
static errno send_and_receive(const int verbose, const dhcp_hw_addr *phwaddr,
                              const udpsockets *psockets,
                              const unsigned long me,
                              const unsigned long server, const int msgtype_s,
                              dhcp_msg *prmsg, struct sockaddr_in *pinaddr_s) {
  dhcp_msg smsg; /* DHCP メッセージバッファ (送信用) */
  struct sockaddr_in inaddr_r;
  unsigned char msgtype_r; /* 受信データのメッセージタイプ */
  int wait = 4;            /* タイムアウト秒数 */
  int timeout = 0;         /* タイムアウトフラグ */
  int i;                   /* ループカウンタ */

  /* 最大4回再送 (計5回送信) */
  for (i = 0; i < 5; i++) {
    unsigned long xid;   /* トランザクション ID */
    unsigned short secs; /* 起動からの経過時間 (秒) */
    int endat;           /* タイムアウト判定用 */

    if (verbose) {
      if (i > 0) printf(" リトライします (%d 回目) ...\n", i);
      fflush(stdout);
    }

    /* メッセージ送信処理 */
    xid = random(); /* トランザクションID設定 */
    secs = (unsigned short)difftime(
        time(NULL), g_pidhcpcinfo->startat); /* 起動からの経過時間 */
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
      printf("DHCP サーバポート (67) へ送信中 ...\n");
      fflush(stdout);
    }
    sendto(psockets->s, (char *)&smsg, sizeof(smsg), 0, (char *)pinaddr_s,
           sizeof(*pinaddr_s));
    /* while (socklen(g_sock_s, 1)) ;*/ /* 送信完了待ち */

    /* メッセージ受信処理 */
    if (verbose) {
      printf(
          "DHCP クライアントポート (68) から受信中. "
          "約 %d 秒後にタイムアウトします ...",
          wait);
      fflush(stdout);
    }

    /* タイムアウトリミット設定 */
    /* 4, 8, 16, 32, 64 ± 1 秒くらい */
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
 * @param[out] pmask サブネットマスク格納域
 * @param[out] pdomain ドメイン名格納域
 * @param[out] pmsg DHCPOFFER または DHCPACK で受信した DHCP メッセージ
 * @return エラーコード
 */
static errno fill_idhcpcinfo(unsigned long *pmask, char *pdomain,
                             dhcp_msg *pmsg) {
  if ((g_pidhcpcinfo->me = dhcp_get_yiaddr(pmsg)) == 0) { /* 要求 IP アドレス */
    return ERR_NOYIADDR;
  }
  if (dhcp_get_serverid(pmsg, &g_pidhcpcinfo->server) == NULL) { /* サーバ ID */
    return ERR_NOSID;
  }
  g_pidhcpcinfo->gateway = 0; /* デフォルトゲートウェイ */
  dhcp_get_defroute(pmsg, &g_pidhcpcinfo->gateway);
  dhcp_get_dns(pmsg, g_pidhcpcinfo->dns); /* ドメインサーバ (配列) */
  if (dhcp_get_leasetime(pmsg, &g_pidhcpcinfo->leasetime) ==
      NULL) { /* リース期間 */
    /* リース期間が渡されなかった！？ */
    return ERR_NOLEASETIME;
  }
  g_pidhcpcinfo->renewtime = 0; /* 更新時間 */
  dhcp_get_renewtime(pmsg, &g_pidhcpcinfo->renewtime);
  if (g_pidhcpcinfo->renewtime == 0) {
    g_pidhcpcinfo->renewtime = g_pidhcpcinfo->leasetime / 2;
  }
  g_pidhcpcinfo->rebindtime = 0; /* 再結合時間 */
  dhcp_get_rebindtime(pmsg, &g_pidhcpcinfo->rebindtime);
  if (g_pidhcpcinfo->rebindtime == 0) {
    g_pidhcpcinfo->rebindtime = g_pidhcpcinfo->leasetime * 857 / 1000;
  }
  *pmask = 0; /* サブネットマスク */
  dhcp_get_subnetmask(pmsg, pmask);
  dhcp_get_domainname(pmsg, pdomain); /* ドメイン名 */

  return NOERROR;
}

/**
 * @brief DHCPRELEASE を発行してネットワークインタフェースを初期化する
 * @param verbose 非 0 でバーボーズモード
 * @param piface インタフェース
 * @param phwaddr ハードウェアアドレス情報
 * @param[out] psockets UDP ソケット
 * @return エラーコード
 */
static errno release_config(const int verbose, iface *piface,
                            dhcp_hw_addr *phwaddr, udpsockets *psockets) {
  dhcp_msg msg; /* DHCP メッセージバッファ */
  struct sockaddr_in inaddr_s;

  /* 送信用 UDP ソケット作成 */
  if ((psockets->s = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* DHCP サーバポート (67) に接続 */
  if (connect2(psockets->s, DHCP_SERVER_PORT, g_pidhcpcinfo->server,
               &inaddr_s) < 0) {
    return ERR_CONNECT;
  }

  /* DHCPRELEASE メッセージ送信処理 */
  dhcp_make_dhcprelease(phwaddr, g_pidhcpcinfo->me, g_pidhcpcinfo->server,
                        random(), &msg);
  if (verbose) {
    dhcp_print(&msg);
    printf("DHCP サーバポート (67) へ送信中 ...\n");
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
 * @param[out] psockets UDP ソケット
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
 * @brief DHCPDISCOVER 時のネットワークインタフェース設定
 * @param piface インタフェース
 */
static void iface_when_discover(iface *piface) {
  piface->my_ip_addr = 0;
  piface->broad_cast = DHCP_LIMITEDBCAST;
  piface->flag |= IFACE_UP | IFACE_BROAD;
  link_new_iface(piface);
}

/**
 * @brief DHCPREQUEST 時のネットワークインタフェース設定
 * @param subnetmask サブネットマスク
 * @param domainname ドメイン名
 * @param[out] piface インタフェース
 */
static void iface_when_request(const unsigned long subnetmask,
                               const char *domainname, iface *piface) {
  piface->my_ip_addr = g_pidhcpcinfo->me;
  piface->net_mask = subnetmask;
  piface->broad_cast = (piface->my_ip_addr & subnetmask) | ~subnetmask;
  piface->flag |= IFACE_UP;
  link_new_iface(piface);
  {
    unsigned long *p = g_pidhcpcinfo->dns, addr;

    while ((addr = *p++)) {
      dns_add((long)addr);
    }
  }
  if (strcmp(domainname, "")) {
    set_domain_name((char *)domainname);
  }
  if (g_pidhcpcinfo->gateway) {
    route *def;
    rt_top(&def);
    def->gateway = (long)g_pidhcpcinfo->gateway;
  }
  /* delaysec(150);*/
}

/**
 * @brief DHCPRELEASE 発行後のネットワークインタフェースの辻褄合わせ
 * @param ifname インタフェース名
 */
static void iface_when_release(iface *piface) {
  {
    unsigned long *p = g_pidhcpcinfo->dns, addr;

    while ((addr = *p++)) {
      dns_drop((long)addr);
    }
  }
  set_domain_name("");
  if (g_pidhcpcinfo->gateway) {
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
 * @brief 1/100 秒単位でウェイトをかます
 * @param tm ウェイトカウント ()
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

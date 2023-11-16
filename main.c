#include <stdlib.h>
#include <string.h>
#include <sys/dos.h>
#include <sys/iocs.h>

#include "dhcp.h"
#include "mynetwork.h"
#include "nwsub.h"

/* 情報保存用ワーク（メインメモリ上に常駐） */
/* tsrarea.sと内容を合わせること */
typedef struct {
  char magic[64];        /* 常駐チェック用文字列 */
  unsigned long startat; /* IPアドレス設定時のマシン起動時間（秒） */
  unsigned long leasetime;        /* リース期間（秒） */
  unsigned long renewtime;        /* 更新開始タイマ（秒） */
  unsigned long rebindtime;       /* 再結合開始タイマ（秒） */
  unsigned long me;               /* 自分のIPアドレス */
  unsigned long server;           /* DHCPサーバIPアドレス */
  unsigned long gateway;          /* デフォルトゲートウェイ */
  unsigned long dns[256 / 4 - 1]; /* DNSサーバアドレス */
} tsrarea;

extern tsrarea g_tsrarea;
extern void g_keepst, g_magic, g_keeped;
static unsigned long g_subnetmask;
static char g_domainname[256];
static char g_devname[] = "/dev/en0";
static char g_ifname[] = "en0";
static eaddr g_macaddr;
static int g_sock_s = -1, g_sock_r = -1;
static int g_verbose = 0;

enum e_errno {
  NOERROR,
  ERR_NODEVICE,
  ERR_SOCKET,
  ERR_CONNECT,
  ERR_BIND,
  ERR_TIMEOUT,
  ERR_NAK,
  ERR_NOYIADDR,
  ERR_NOLEASETIME,
  ERR_NOSID,
  ERR_NOTKEPT,
  ERR_ALREADYKEPT,
};

static char g_title[] =
    "idhcpc.x - インチキDHCPクライアント - version 0.11 "
    "Copyright 2002,03 Igarashi\n";

static char g_usgmes[] =
    "Usage: idhcpc [options]\n"
    "Options:\n"
    "\t-r, --release\t\tコンフィギュレーション情報を破棄する\n"
    "\t-l, --print-lease-time\t残りリース期間を表示する\n"
    "\t-v, --verbose\t\t詳細表示モード\n";

static char *g_errmes[] = {
    "",
    "ネットワークデバイスがインストールされていません.",
    "ソケットを作成できません.",
    "DHCPサーバポートへの接続に失敗しました.",
    "DHCPクライアントポートへの接続に失敗しました.",
    "タイムアウトです.",
    "DHCPサーバから要求を拒否されました.",
    "IPアドレスを取得できませんでした.",
    "リース期間を取得できませんでした.",
    "DHCPサーバのIPアドレスを取得できません.",
    "idpcpcが起動していません.",
    "idpcpcはすでに起動しています.",
};
static char g_keepmes[] = "コンフィギュレーションが完了しました.\n";

static char g_removemes[] = "コンフィギュレーション情報を破棄しました.\n";

static int try_to_keep(int);
static int try_to_release(int);
static int try_to_print(int);
static void print_lease_time(unsigned long, unsigned long);
static int prepare_discover(struct sockaddr_in *);
static int discover_dhcp_server(unsigned long *, unsigned long *,
                                struct sockaddr_in *);
static int request_to_dhcp_server(tsrarea *, unsigned long *, char *,
                                  unsigned long, unsigned long,
                                  struct sockaddr_in *);
static int send_and_receive(dhcp_msg *, struct sockaddr_in *, unsigned long,
                            unsigned long, int);
static int fill_tsrarea(tsrarea *, unsigned long *, char *, dhcp_msg *);
static void close_sockets(void);
static int release_config(void);
static void iface_when_discover(char *);
static void iface_when_request(char *);
static void iface_when_release(char *);
static void delaysec(int);
static void put_error(int);
static void put_progress(void);
extern int keepchk(struct _mep *, size_t, struct _mep **);

/**
 * @brief メイン処理
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
  int i;
  int errno;
  int rflag = 0, lflag = 0;
  int keepflag;
  struct _mep *pmep;

  printf(g_title);

  /* オプション解析 */
  {
    int argerr = 0;

    for (i = 1; i < argc; i++) {
      if ((!strcmp(argv[i], "-r")) || (!strcmp(argv[i], "--release"))) {
        rflag = 1;
      } else if ((!strcmp(argv[i], "-l")) ||
                 (!strcmp(argv[i], "--print-lease-time"))) {
        lflag = 1;
      } else if ((!stricmp(argv[i], "-v")) || (!strcmp(argv[i], "--verbose"))) {
        g_verbose = 1;
      } else {
        argerr = 1;
        break;
      }
    }
    if (argerr || (rflag && lflag)) { /* -rと-pは同時指定できない */
      printf(g_usgmes);
      return EXIT_FAILURE;
    }
  }

  /* ここで常駐チェックしておく */
  keepflag =
      keepchk((struct _mep *)((void *)_dos_getpdb() - sizeof(struct _mep)),
              &g_magic - &g_keepst, &pmep);
  if (keepflag) {
    /* 常駐部に保存してあった情報をグローバルワークへ転送しておく */
    memcpy(&g_tsrarea, (void *)pmep + sizeof(struct _mep) + sizeof(struct _psp),
           sizeof(g_tsrarea));
  }

  if (rflag) {
    /* 常駐解除処理 */
    if ((errno = try_to_release(keepflag)) != NOERROR) {
      put_error(errno);
      return EXIT_FAILURE;
    } else {
      _dos_mfree((void *)pmep + sizeof(struct _mep)); /* 常駐部解放 */
      printf(g_removemes);
    }
  } else if (lflag) {
    /* リース期間表示 */
    if ((errno = try_to_print(keepflag)) != NOERROR) {
      put_error(errno);
      return EXIT_FAILURE;
    }
  } else {
    /* 常駐処理 */
    if ((errno = try_to_keep(keepflag)) != NOERROR) {
      put_error(errno);
      return EXIT_FAILURE;
    } else {
      printf(g_keepmes);
      print_lease_time(g_tsrarea.leasetime, g_tsrarea.startat);
      _dos_keeppr(&g_keeped - &g_keepst, 0); /* 常駐終了 */
    }
  }

  return EXIT_SUCCESS;
}

/**
 * @brief 常駐処理
 * @param keepflag 常駐判定フラグ
 * @return エラーコード
 */
static int try_to_keep(int keepflag) {
  int errno;
  struct sockaddr_in inaddr_s;
  unsigned long me;
  unsigned long server;

  while (1) {
    if (keepflag) {
      errno = ERR_ALREADYKEPT; /* すでに常駐していた */
      break;
    }
    if ((errno = prepare_discover(&inaddr_s)) != NOERROR) break;
    if ((errno = discover_dhcp_server(&me, &server, &inaddr_s)) != NOERROR)
      break;
    if ((errno = request_to_dhcp_server(&g_tsrarea, &g_subnetmask, g_domainname,
                                        me, server, &inaddr_s)) != NOERROR)
      break;
    break;
  }

  close_sockets();

  return errno;
}

/**
 * @brief 常駐解除処理
 * @param keepflag 常駐判定フラグ
 * @return エラーコード
 */
static int try_to_release(int keepflag) {
  int errno;

  while (1) {
    if (!keepflag) {
      errno = ERR_NOTKEPT; /* 常駐していなかった */
      break;
    }
    if ((errno = release_config()) != NOERROR) break;
    break;
  }

  close_sockets();

  return errno;
}

/**
 * @brief 残りリース期間表示処理
 * @param keepflag 常駐判定フラグ
 * @return エラーコード
 */
static int try_to_print(int keepflag) {
  int errno;

  while (1) {
    if (!keepflag) {
      errno = ERR_NOTKEPT;
      break;
    }
    print_lease_time(g_tsrarea.leasetime, g_tsrarea.startat);
    errno = NOERROR;
    break;
  }

  return errno;
}

/**
 * @brief 残りリース期間表示処理メイン
 * @param leasetime （全体）リース期間（秒）
 * @param startat IPアドレス設定時のマシン起動時間（秒）
 */
static void print_lease_time(unsigned long leasetime, unsigned long startat) {
  int rest, rest_h, rest_m, rest_s;

  if (leasetime == 0xffffffff) {
    printf("リース期間は無期限です.\n");
  } else {
    rest = (int)leasetime - (_iocs_ontime() / 100 - (int)startat);
    rest_s = rest % 60;
    rest /= 60;
    rest_m = rest % 60;
    rest_h = rest / 60;
    printf("残りリース期間は");
    if (rest_h) printf(" %d 時間", rest_h);
    if (rest_h) printf(" %d 分", rest_m);
    printf(" %d 秒 です.\n", rest_s);
  }
}

/**
 * @brief DHCPDISCOVER発行前の前処理
 * @param pinaddr_s 送信用ソケット情報格納域へのポインタ
 * @return エラーコード
 */
static int prepare_discover(struct sockaddr_in *pinaddr_s) {
  /* MACアドレス取得 */
  if (!get_mac_address(&g_macaddr, g_devname)) {
    return ERR_NODEVICE;
  }
  /* INIT時のネットワークインタフェース設定 */
  iface_when_discover(g_ifname);

  /* 送信用UDPソケット作成 */
  if ((g_sock_s = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* 受信用UDPソケット作成 */
  if ((g_sock_r = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* DHCPサーバポート（67）に接続 */
  if (connect2(pinaddr_s, g_sock_s, DHCP_SERVER_PORT, DHCP_LIMITEDBCAST) < 0) {
    return ERR_CONNECT;
  }
  /* DHCPクライアントポート（68）に接続 */
  if (bind2(g_sock_r, DHCP_CLIENT_PORT, 0) < 0) {
    return ERR_BIND;
  }

  return NOERROR;
}

/**
 * @brief DHCPDISCOVERを発行してDHCPOFFERを受信する
 * @param pme 要求IPアドレス格納域へのポインタ
 * @param pserver DHCPサーバIPアドレス格納域へのポインタ
 * @param pinaddr_s 送信用ソケット情報格納域へのポインタ
 * @return エラーコード
 */
static int discover_dhcp_server(unsigned long *pme, unsigned long *pserver,
                                struct sockaddr_in *pinaddr_s) {
  int ret;
  dhcp_msg msg;

  if ((ret = send_and_receive(&msg, pinaddr_s, 0, 0, DHCPDISCOVER)) !=
      NOERROR) {
    return ret;
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
 * @param ptsrarea コンフィギュレーション情報格納域へのポインタ
 * @param pmask サブネットマスク格納域へのポインタ
 * @param pdomain ドメイン名格納域へのポインタ
 * @param me 要求IPアドレス
 * @param server DHCPサーバIPアドレス
 * @param pinaddr_s 送信用ソケット情報格納域へのポインタ
 * @return エラーコード
 */
static int request_to_dhcp_server(tsrarea *ptsrarea, unsigned long *pmask,
                                  char *pdomain, unsigned long me,
                                  unsigned long server,
                                  struct sockaddr_in *pinaddr_s) {
  int ret;
  dhcp_msg msg;

  if ((ret = send_and_receive(&msg, pinaddr_s, me, server, DHCPREQUEST)) !=
      NOERROR) {
    return ret;
  }

  /* 受信結果から要求IPアドレス / サーバIDその他を抜き出す */
  if ((ret = fill_tsrarea(ptsrarea, pmask, pdomain, &msg)) != NOERROR) {
    return ret;
  }

  /* この時点でのマシン起動時間を覚えておく */
  g_tsrarea.startat = (unsigned long)(_iocs_ontime() / 100);

  /* REQUEST時のネットワークインタフェース設定 */
  iface_when_request(g_ifname);

  return NOERROR;
}

/**
 * @brief DHCPメッセージ送信 / 受信処理
 * @param prmsg DHCPメッセージバッファ格納域へのポインタ
 * @param pinaddr_s 送信用ソケット情報格納域へのポインタ
 * @param me 要求IPアドレス
 * @param server DHCPサーバIPアドレス
 * @param msgtype_s DHCPメッセージタイプ（DHCPDISCOVER or DHCPREQUEST）
 * @return エラーコード
 */
static int send_and_receive(dhcp_msg *prmsg, struct sockaddr_in *pinaddr_s,
                            unsigned long me, unsigned long server,
                            int msgtype_s) {
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

    if (g_verbose) {
      if (i > 0) printf(" リトライします（%d回目）...\n", i);
      fflush(stdout);
    }

    /* メッセージ送信処理 */
    xid = random(); /* トランザクションID設定 */
    secs = (unsigned short)(_iocs_ontime() / 100);
    switch (msgtype_s) {
      case DHCPDISCOVER:
        dhcp_make_dhcpdiscover(&smsg, &g_macaddr, xid, secs);
        break;
      case DHCPREQUEST:
        dhcp_make_dhcprequest(&smsg, me, server, &g_macaddr, xid, secs);
        break;
      default:
        break;
    }
    if (g_verbose) {
      dhcp_print(&smsg);
      printf("DHCPサーバポート（67）へ送信中 ...\n");
      fflush(stdout);
    }
    sendto(g_sock_s, (char *)&smsg, sizeof(smsg), 0, (char *)pinaddr_s,
           sizeof(*pinaddr_s));
    /* while (socklen(g_sock_s, 1)) ;*/ /* 送信完了待ち */

    /* メッセージ受信処理 */
    if (g_verbose) {
      printf(
          "DHCPクライアントポート（68）から受信中. "
          "約%d秒後にタイムアウトします ...",
          wait);
      fflush(stdout);
    }

    /* タイムアウトリミット設定 */
    /* 4, 8, 16, 32, 64 ± 1秒くらい */
    endat = _iocs_ontime() + (wait - 1) * 100 + random() % 200;
    timeout = 0;
    while (1) {
      int len = sizeof(inaddr_r);

      if (_iocs_ontime() > endat) {
        timeout = 1;
        break;
      }
      if (!socklen(g_sock_r, 0)) {
        if (g_verbose) put_progress();
        continue;
      }
      recvfrom(g_sock_r, (char *)prmsg, sizeof(*prmsg), 0, (char *)&inaddr_r,
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
      if (g_verbose) {
        printf(" done.\n");
        dhcp_print(prmsg);
      }
      break; /* 受信完了でループ脱出 */
    }
    if (g_verbose) printf(" タイムアウトです.");
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
 * @param ptsrarea コンフィギュレーション情報格納域へのポインタ
 * @param pmask サブネットマスク格納域へのポインタ
 * @param pdomain ドメイン名格納域へのポインタ
 * @param pmsg DHCPOFFERまたはDHCPACKで受信したDHCPメッセージ
 * @return エラーコード
 */
static int fill_tsrarea(tsrarea *ptsrarea, unsigned long *pmask, char *pdomain,
                        dhcp_msg *pmsg) {
  if ((ptsrarea->me = dhcp_get_yiaddr(pmsg)) == 0) { /* 要求IPアドレス */
    return ERR_NOYIADDR;
  }
  if (dhcp_get_serverid(pmsg, &ptsrarea->server) == NULL) { /* サーバID */
    return ERR_NOSID;
  }
  ptsrarea->gateway = 0; /* デフォルトゲートウェイ */
  dhcp_get_defroute(pmsg, &ptsrarea->gateway);
  dhcp_get_dns(pmsg, ptsrarea->dns); /* ドメインサーバ（配列） */
  if (dhcp_get_leasetime(pmsg, &ptsrarea->leasetime) == NULL) { /* リース期間 */
    /* リース期間が渡されなかった！？ */
    return ERR_NOLEASETIME;
  }
  ptsrarea->renewtime = 0; /* 更新時間 */
  dhcp_get_renewtime(pmsg, &ptsrarea->renewtime);
  if (ptsrarea->renewtime == 0) {
    ptsrarea->renewtime = ptsrarea->leasetime / 2;
  }
  ptsrarea->rebindtime = 0; /* 再結合時間 */
  dhcp_get_rebindtime(pmsg, &ptsrarea->rebindtime);
  if (ptsrarea->rebindtime == 0) {
    ptsrarea->rebindtime = ptsrarea->leasetime * 857 / 1000;
  }
  *pmask = 0; /* サブネットマスク */
  dhcp_get_subnetmask(pmsg, pmask);
  dhcp_get_domainname(pmsg, pdomain); /* ドメイン名 */

  return NOERROR;
}

/**
 * @brief オープン済みのソケットをすべてクローズする
 * @param
 */
static void close_sockets(void) {
  if (g_sock_s != -1) {
    close_s(g_sock_s);
    g_sock_s = -1;
  }
  if (g_sock_r != -1) {
    close_s(g_sock_r);
    g_sock_r = -1;
  }
}

/**
 * @brief DHCPRELEASEを発行してネットワークインタフェースを初期化する
 * @param
 * @return エラーコード
 */
static int release_config(void) {
  dhcp_msg msg; /* DHCPメッセージバッファ */
  struct sockaddr_in inaddr_s;

  /* MACアドレス取得 */
  if (!get_mac_address(&g_macaddr, g_devname)) {
    return ERR_NODEVICE;
  }

  /* 送信用UDPソケット作成 */
  if ((g_sock_s = create_udp_socket()) < 0) {
    return ERR_SOCKET;
  }
  /* DHCPサーバポート（67）に接続 */
  if (connect2(&inaddr_s, g_sock_s, DHCP_SERVER_PORT, g_tsrarea.server) < 0) {
    return ERR_CONNECT;
  }

  /* DHCPRELEASEメッセージ送信処理 */
  dhcp_make_dhcprelease(&msg, g_tsrarea.me, g_tsrarea.server, &g_macaddr,
                        random());
  if (g_verbose) {
    dhcp_print(&msg);
    printf("DHCPサーバポート（67）へ送信中 ...\n");
    fflush(stdout);
  }
  sendto(g_sock_s, (char *)&msg, sizeof(msg), 0, (char *)&inaddr_s,
         sizeof(inaddr_s));
  /* while (socklen(g_sock_s, 1)) ;*/

  iface_when_release(g_ifname);

  return NOERROR;
}

/**
 * @brief DHCPDISCOVER時のネットワークインタフェース設定
 * @param ifname インタフェース名（"en0"）
 */
static void iface_when_discover(char *ifname) {
  iface *p, *p0;

  if ((p0 = iface_lookupn(ifname)) != NULL) {
    p = p0;
  } else {
    p = get_new_iface(ifname);
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
 */
static void iface_when_request(char *ifname) {
  iface *p = iface_lookupn(ifname);

  p->my_ip_addr = g_tsrarea.me;
  p->net_mask = g_subnetmask;
  p->broad_cast = (p->my_ip_addr & g_subnetmask) | ~g_subnetmask;
  p->flag |= IFACE_UP;

  {
    unsigned long *p = g_tsrarea.dns, addr;

    while ((addr = *p++)) {
      dns_add((long)addr);
    }
  }
  if (strcmp(g_domainname, "")) {
    set_domain_name(g_domainname);
  }
  if (g_tsrarea.gateway) {
    route *def;
    rt_top(&def);
    def->gateway = (long)g_tsrarea.gateway;
  }
/* delaysec(150);
*/}

/**
 * @brief DHCPRELEASE発行後のネットワークインタフェースの辻褄合わせ
 * @param ifname インタフェース名（"en0"）
 */
static void iface_when_release(char *ifname) {
  iface *p = iface_lookupn(ifname);

  if (p->flag & IFACE_UP) {
    p->stop(p); /* これを実行しないと割り込みが消えないらしい */
    delaysec(150); /* 多少ウェイトをかまさないとまずいみたい */
  }
  p->my_ip_addr = 0;
  p->net_mask = 0;
  p->broad_cast = 0;
  p->flag &= ~(IFACE_UP | IFACE_BROAD);

  {
    unsigned long *p = g_tsrarea.dns, addr;

    while ((addr = *p++)) {
      dns_drop((long)addr);
    }
  }
  set_domain_name("");
  if (g_tsrarea.gateway) {
    route *def;
    rt_top(&def);
    def->gateway = 0;
  }
}

/**
 * @brief 1/100秒単位でウェイトをかます
 * @param tm ウェイトカウント（）
 */
static void delaysec(int tm) {
  int endat = _iocs_ontime() + tm;

  while (endat > _iocs_ontime())
    ;
}

/**
 * @brief エラーメッセージ表示
 * @param errno エラーコード
 */
static void put_error(int errno) {
  printf("エラーが発生しました. %s\n", g_errmes[errno]);
  fflush(stdout);
}

/**
 * @brief インチキプログレス表示
 * @param
 */
static void put_progress(void) {
  static int ontime0 = 0;
  int ontime = _iocs_ontime();
  if (ontime - ontime0 >= 50) {
    printf(".");
    fflush(stdout);
    ontime0 = ontime;
  }
}

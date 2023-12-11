#ifndef IDHCPC_H
#define IDHCPC_HDHCP_H

/* idhcpcワーク */
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
} idhcpcinfo;

typedef enum {
  NOERROR,
  ERR_NODEVICE,    /* デバイスエラー */
  ERR_NOIFACE,     /* インタフェースエラー */
  ERR_SOCKET,      /* ソケット作成エラー */
  ERR_CONNECT,     /* DHCPサーバポート接続エラー */
  ERR_BIND,        /* DHCPクライアントポート接続エラー */
  ERR_TIMEOUT,     /* タイムアウト */
  ERR_NAK,         /* DHCPサーバ要求拒否 */
  ERR_NOYIADDR,    /* IPアドレス取得エラー */
  ERR_NOLEASETIME, /* リース期間取得エラー */
  ERR_NOSID,       /* DHCPサーバIPアドレス取得エラー */
  ERR_NOTKEPT,     /* 常駐していない */
  ERR_ALREADYKEPT, /* すでに常駐している */
} errno;

extern idhcpcinfo g_idhcpcinfo; /* idhcpcワーク */

int keepchk(idhcpcinfo **);
void keeppr_and_exit(void);
int freepr(const idhcpcinfo *);
int ontime(void);

errno try_to_keep(const int, const int, const char *);
errno try_to_release(const int, const int, const char *);
void print_lease_time(const unsigned long, const unsigned long);

#endif /* IDHCPC_H */

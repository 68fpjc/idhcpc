#ifndef IDHCPC_H
#define IDHCPC_H

#include <time.h>

typedef enum {
  NOERROR,
  ERR_NODEVICE,    /* デバイスエラー */
  ERR_NOIFACE,     /* インタフェースエラー */
  ERR_SOCKET,      /* ソケット作成エラー */
  ERR_CONNECT,     /* DHCP サーバポート接続エラー */
  ERR_BIND,        /* DHCP クライアントポート接続エラー */
  ERR_TIMEOUT,     /* タイムアウト */
  ERR_NAK,         /* DHCP サーバ要求拒否 */
  ERR_NOYIADDR,    /* IP アドレス取得エラー */
  ERR_NOLEASETIME, /* リース期間取得エラー */
  ERR_NOSID,       /* DHCP サーバ IP アドレス取得エラー */
  ERR_NOTKEPT,     /* 常駐していない */
  ERR_ALREADYKEPT, /* すでに常駐している */
} errno;

/* idhcpc ワーク */
/* tsrarea.s とサイズを合わせること */
typedef struct {
  time_t startat;                 /* DHCP クライアント起動日時 */
  time_t dhcpackat;               /* DHCPACK 受信日時 */
  unsigned long leasetime;        /* リース期間 (秒) */
  unsigned long renewtime;        /* 更新開始タイマ (秒) */
  unsigned long rebindtime;       /* 再結合開始タイマ (秒) */
  unsigned long me;               /* 自分の IP アドレス */
  unsigned long server;           /* DHCP サーバ IP アドレス */
  unsigned long gateway;          /* デフォルトゲートウェイ */
  unsigned long dns[256 / 4 - 1]; /* DNS サーバアドレス */
  char ifname[8];                 /* インタフェース名 */
  char magic[64];                 /* 常駐チェック用文字列 */
} idhcpcinfo;

void keeppr_and_exit(void);
int freepr(const char *);
int ontime(void);

errno try_to_keep(const int, const char *);
errno try_to_release(const int, const char *);
errno get_remaining(const char *, const int, int *);

#endif /* IDHCPC_H */

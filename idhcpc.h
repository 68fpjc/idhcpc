#ifndef IDHCPC_H
#define IDHCPC_HDHCP_H

#include <time.h>

/* idhcpcワーク */
/* tsrarea.sとサイズを合わせること */
typedef struct {
  time_t startat;                 /* DHCP クライアント起動日時 */
  time_t dhcpackat;               /* DHCPACK 受信日時 */
  unsigned long leasetime;        /* リース期間（秒） */
  unsigned long renewtime;        /* 更新開始タイマ（秒） */
  unsigned long rebindtime;       /* 再結合開始タイマ（秒） */
  unsigned long me;               /* 自分のIPアドレス */
  unsigned long server;           /* DHCPサーバIPアドレス */
  unsigned long gateway;          /* デフォルトゲートウェイ */
  unsigned long dns[256 / 4 - 1]; /* DNSサーバアドレス */
  char ifname[8];                 /* インタフェース名 */
  char magic[64];                 /* 常駐チェック用文字列 */
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

extern idhcpcinfo g_idhcpcinfo;   /* idhcpcワーク */
extern const char g_idhcpcinfoed; /* idhcpcワーク終端 */

int keepchk(const char *, idhcpcinfo **);
void keeppr_and_exit(void);
int freepr(const idhcpcinfo *);
int ontime(void);

errno try_to_keep(const int, const int);
errno try_to_release(const int, const int);
void print_lease_time(const char *, const unsigned long, const time_t);

#endif /* IDHCPC_H */

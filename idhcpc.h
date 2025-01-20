#ifndef IDHCPC_H
#define IDHCPC_H

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

void keeppr_and_exit(void);
int freepr(void);
int ontime(void);

errno try_to_keep(const int, const char *);
errno try_to_release(const int, const char *);
errno get_remaining(const char *, const int, int *);

#endif /* IDHCPC_H */

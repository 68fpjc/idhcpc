#ifndef __IDHCPC_H__
#define __IDHCPC_H__

/* idhcpc ワーク */
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

int keepchk(idhcpcinfo **);
void keeppr_and_exit(void);
int freepr(const idhcpcinfo *);

int ontime(void);

#endif /* __IDHCPC_H__ */
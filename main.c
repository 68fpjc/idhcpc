#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idhcpc.h"

static char g_title[] =
    "idhcpc - インチキDHCPクライアント - version 0.11.1 "
    "https://github.com/68fpjc\n";

/**
 * @brief errnoに対応したエラーメッセージ
 */
static char *g_errmes[] = {
    /* NOERROR         */ "",
    /* ERR_NODEVICE    */ "ネットワークデバイスがインストールされていません.",
    /* ERR_SOCKET      */ "ソケットを作成できません.",
    /* ERR_CONNECT     */ "DHCPサーバポートへの接続に失敗しました.",
    /* ERR_BIND        */ "DHCPクライアントポートへの接続に失敗しました.",
    /* ERR_TIMEOUT     */ "タイムアウトです.",
    /* ERR_NAK         */ "DHCPサーバから要求を拒否されました.",
    /* ERR_NOYIADDR    */ "IPアドレスを取得できませんでした.",
    /* ERR_NOLEASETIME */ "リース期間を取得できませんでした.",
    /* ERR_NOSID       */ "DHCPサーバのIPアドレスを取得できません.",
    /* ERR_NOTKEPT     */ "idpcpcが起動していません.",
    /* ERR_ALREADYKEPT */ "idpcpcはすでに起動しています.",
};

static char g_usgmes[] =
    "Usage: idhcpc [options]\n"
    "Options:\n"
    "\t-r, --release\t\tコンフィギュレーション情報を破棄する\n"
    "\t-l, --print-lease-time\t残りリース期間を表示する\n"
    "\t-v, --verbose\t\t詳細表示モード\n";

static char g_keepmes[] = "コンフィギュレーションが完了しました.\n";

static char g_removemes[] = "コンフィギュレーション情報を破棄しました.\n";

static errno try_to_print(const int);
static void put_error(const int);

/**
 * @brief メイン処理
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
  errno err;
  int rflag = 0, lflag = 0;
  int vflag = 0;
  int keepflag;
  idhcpcinfo *pidhcpcinfo;
  int i;

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
        vflag = 1;
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
  keepflag = keepchk(&pidhcpcinfo);
  if (keepflag) {
    /* 常駐部に保存してあった情報をグローバルワークへ転送しておく */
    memcpy(&g_idhcpcinfo, pidhcpcinfo, sizeof(idhcpcinfo));
  }

  if (rflag) {
    /* 常駐解除処理 */
    if ((err = try_to_release(vflag, keepflag)) != NOERROR) {
      put_error(err);
      return EXIT_FAILURE;
    } else {
      freepr(pidhcpcinfo);
      printf(g_removemes);
    }
  } else if (lflag) {
    /* リース期間表示 */
    if ((err = try_to_print(keepflag)) != NOERROR) {
      put_error(err);
      return EXIT_FAILURE;
    }
  } else {
    /* 常駐処理 */
    if ((err = try_to_keep(vflag, keepflag)) != NOERROR) {
      put_error(err);
      return EXIT_FAILURE;
    } else {
      printf(g_keepmes);
      print_lease_time(g_idhcpcinfo.leasetime, g_idhcpcinfo.startat);
      keeppr_and_exit(); /* 常駐終了 */
    }
  }

  return EXIT_SUCCESS;
}

/**
 * @brief 残りリース期間表示処理
 * @param keepflag 常駐判定フラグ
 * @return エラーコード
 */
static errno try_to_print(const int keepflag) {
  int errno;

  while (1) {
    if (!keepflag) {
      errno = ERR_NOTKEPT;
      break;
    }
    print_lease_time(g_idhcpcinfo.leasetime, g_idhcpcinfo.startat);
    errno = NOERROR;
    break;
  }

  return errno;
}

/**
 * @brief エラーメッセージ表示
 * @param errno エラーコード
 */
static void put_error(const int errno) {
  printf("エラーが発生しました. %s\n", g_errmes[errno]);
  fflush(stdout);
}

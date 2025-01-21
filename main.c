#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idhcpc.h"

static char g_title[] =
    "idhcpc - インチキ DHCP クライアント - version 0.12.0 "
    "https://github.com/68fpjc\n";

/**
 * @brief errno に対応したエラーメッセージ
 */
static char *g_errmes[] = {
    /* NOERROR         */ "",
    /* ERR_NODEVICE    */ "ネットワークデバイスがインストールされていません.",
    /* ERR_NOIFACE     */ "インタフェースが見つかりません.",
    /* ERR_SOCKET      */ "ソケットを作成できません.",
    /* ERR_CONNECT     */ "DHCP サーバポートへ接続できません.",
    /* ERR_BIND        */ "DHCP クライアントポートへ接続できません.",
    /* ERR_TIMEOUT     */ "タイムアウトです.",
    /* ERR_NAK         */ "DHCP サーバから要求を拒否されました.",
    /* ERR_NOYIADDR    */ "IP アドレスを取得できません.",
    /* ERR_NOLEASETIME */ "リース期間を取得できません.",
    /* ERR_NOSID       */ "DHCP サーバの IP アドレスを取得できません.",
    /* ERR_NOTKEPT     */ "idpcpc が常駐していません.",
    /* ERR_ALREADYKEPT */ "idpcpc はすでに常駐しています.",
};

static char g_usgmes[] =
    "Usage: idhcpc [options] [インタフェース名]\n"
    "Options:\n"
    "\t-r, --release\t\tコンフィギュレーション情報を破棄する\n"
    "\t-l, --print-lease-time\t残りリース期間を表示する\n"
    "\t-v, --verbose\t\t詳細表示モード\n";

static char g_keepmes[] = "コンフィギュレーションが完了しました.\n";

static char g_removemes[] = "コンフィギュレーション情報を破棄しました.\n";

static errno print_lease_time(const char *, const int);
static void put_error(const char *, const int);
static void printf_with_iface(const char *, const char *);

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
  const char *ifname_default = "en0";
  const char *ifname = ifname_default;
  int i;

  printf(g_title);

  /* オプション解析 */
  {
    int argerr = 0;

    for (i = 1; i < argc; i++) {
      if (*argv[i] == '-') {
        if ((!strcmp(argv[i], "-r")) || (!strcmp(argv[i], "--release"))) {
          rflag = 1;
        } else if ((!strcmp(argv[i], "-l")) ||
                   (!strcmp(argv[i], "--print-lease-time"))) {
          lflag = 1;
        } else if ((!stricmp(argv[i], "-v")) ||
                   (!strcmp(argv[i], "--verbose"))) {
          vflag = 1;
        } else {
          argerr = 1;
          break;
        }
      } else {
        if (ifname == ifname_default) {
          ifname = argv[i];
        } else {
          argerr = 1;
          break;
        }
      }
    }
    if (argerr || (rflag && lflag)) { /* -r と -p は同時指定できない */
      printf(g_usgmes);
      return EXIT_FAILURE;
    }
  }

  if (rflag) {
    /* 常駐解除処理 */
    if ((err = try_to_release(vflag, ifname)) != NOERROR) {
      put_error(ifname, err);
      return EXIT_FAILURE;
    } else {
      freepr(ifname);
      printf_with_iface(ifname, g_removemes);
    }
  } else if (lflag) {
    /* 残りリース期間表示 */
    if ((err = print_lease_time(ifname, 0)) != NOERROR) {
      put_error(ifname, err);
      return EXIT_FAILURE;
    }
  } else {
    /* 常駐処理 */
    if ((err = try_to_keep(vflag, ifname)) != NOERROR) {
      put_error(ifname, err);
      return EXIT_FAILURE;
    } else {
      printf_with_iface(ifname, g_keepmes);
      print_lease_time(ifname, 1);
      keeppr_and_exit(); /* 常駐終了 */
    }
  }

  return EXIT_SUCCESS;
}

/**
 * @brief 残りリース期間を表示する
 * @param ifname インタフェース名
 * @param force 非 0 で強制的にリース期間を取得する (常駐時専用)
 * @return エラーコード
 */
static errno print_lease_time(const char *ifname, const int force) {
  errno err;
  int rest, rest_h, rest_m, rest_s;

  if ((err = get_remaining(ifname, force, &rest)) == NOERROR) {
    if (rest == 0xffffffff) {
      printf("%s: リース期間は無期限です.\n", ifname);
    } else {
      rest_s = rest % 60;
      rest /= 60;
      rest_m = rest % 60;
      rest_h = rest / 60;
      printf("%s: 残りリース期間は %d 時間 %02d 分 %02d 秒です.\n", ifname,
             rest_h, rest_m, rest_s);
    }
  }
  return err;
}

/**
 * @brief エラーメッセージ表示
 * @param ifname インタフェース名
 * @param errno エラーコード
 */
static void put_error(const char *ifname, const int errno) {
  printf("%s: %s\n", ifname, g_errmes[errno]);
  fflush(stdout);
}

/**
 * @brief インタフェース名付きprintf
 * @param ifname インタフェース名
 * @param s フォーマット
 */
static void printf_with_iface(const char *ifname, const char *s) {
  printf("%s: %s", ifname, s);
}

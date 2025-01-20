#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idhcpc.h"

static char g_title[] =
    "idhcpc - インチキDHCPクライアント - version 0.12.0 "
    "https://github.com/68fpjc\n";

/**
 * @brief errnoに対応したエラーメッセージ
 */
static char *g_errmes[] = {
    /* NOERROR         */ "",
    /* ERR_NODEVICE    */ "ネットワークデバイスがインストールされていません.",
    /* ERR_NOIFACE     */ "インタフェースが見つかりません.",
    /* ERR_SOCKET      */ "ソケットを作成できません.",
    /* ERR_CONNECT     */ "DHCPサーバポートへ接続できません.",
    /* ERR_BIND        */ "DHCPクライアントポートへ接続できません.",
    /* ERR_TIMEOUT     */ "タイムアウトです.",
    /* ERR_NAK         */ "DHCPサーバから要求を拒否されました.",
    /* ERR_NOYIADDR    */ "IPアドレスを取得できません.",
    /* ERR_NOLEASETIME */ "リース期間を取得できません.",
    /* ERR_NOSID       */ "DHCPサーバのIPアドレスを取得できません.",
    /* ERR_NOTKEPT     */ "idpcpcが常駐していません.",
    /* ERR_ALREADYKEPT */ "idpcpcはすでに常駐しています.",
};

static char g_usgmes[] =
    "Usage: idhcpc [options] [インタフェース名]\n"
    "Options:\n"
    "\t-r, --release\t\tコンフィギュレーション情報を破棄する\n"
    "\t-l, --print-lease-time\t残りリース期間を表示する\n"
    "\t-v, --verbose\t\t詳細表示モード\n";

static char g_keepmes[] = "コンフィギュレーションが完了しました.\n";

static char g_removemes[] = "コンフィギュレーション情報を破棄しました.\n";

static void print_lease_time(const char *);
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
  int keepflag;
  idhcpcinfo *pidhcpcinfo;
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
    if (argerr || (rflag && lflag)) { /* -rと-pは同時指定できない */
      printf(g_usgmes);
      return EXIT_FAILURE;
    }
  }

  /* ここで常駐チェックしておく */
  keepflag = keepchk(ifname, &pidhcpcinfo);
  if (keepflag) {
    /* 常駐部に保存してあった情報をグローバルワークへ転送しておく */
    memcpy(&g_idhcpcinfo, pidhcpcinfo, sizeof(idhcpcinfo));
  }

  if (rflag) {
    /* 常駐解除処理 */
    if ((err = try_to_release(vflag, keepflag)) != NOERROR) {
      put_error(ifname, err);
      return EXIT_FAILURE;
    } else {
      freepr(pidhcpcinfo);
      printf_with_iface(ifname, g_removemes);
    }
  } else if (lflag) {
    /* 残りリース期間表示 */
    if (!keepflag) {
      put_error(ifname, ERR_NOTKEPT);
      return EXIT_FAILURE;
    }
    print_lease_time(ifname);
  } else {
    /* 常駐処理 */
    if ((err = try_to_keep(vflag, keepflag)) != NOERROR) {
      put_error(ifname, err);
      return EXIT_FAILURE;
    } else {
      printf_with_iface(ifname, g_keepmes);
      print_lease_time(ifname);
      keeppr_and_exit(); /* 常駐終了 */
    }
  }

  return EXIT_SUCCESS;
}

/**
 * @brief 残りリース期間表示処理メイン
 * @param ifname インタフェース名
 */
static void print_lease_time(const char *ifname) {
  int rest, rest_h, rest_m, rest_s;

  if ((rest = get_remaining()) < 0) {
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
  printf("%s: %s", g_idhcpcinfo.ifname, s);
}

#include "idhcpc.h"

#include <stdlib.h>
#include <sys/dos.h>
#include <sys/iocs.h>

extern const char g_keepst;     /* 常駐部先頭アドレス */
extern const char g_idhcpcinfo; /* idhcpc ワーク先頭アドレス */
extern const char g_magic; /* 常駐判定チェック用文字列アドレス */
extern const char g_keeped; /* 常駐部終端アドレス */

extern int _keepchk(struct _mep *, size_t, struct _mep **);

/**
 * @brief 常駐判定
 * @param[out] ppidhcpcinfo
 * idhcpc ワークへのポインタ
 *   * 常駐している場合: 常駐プロセスのアドレス
 *   * 常駐していない場合: 自身のアドレス
 * @return 0: 常駐していない
 */
int keepchk(idhcpcinfo **ppidhcpcinfo) {
  struct _mep *pmep;
  int keepflg =
      _keepchk((struct _mep *)((char *)_dos_getpdb() - sizeof(struct _mep)),
               &g_magic - &g_keepst, &pmep);
  *ppidhcpcinfo = (idhcpcinfo *)((char *)pmep + sizeof(struct _mep) +
                                 sizeof(struct _psp) + g_idhcpcinfo - g_keepst);
  return keepflg;
}

/**
 * @brief 常駐終了
 */
void keeppr_and_exit(void) { _dos_keeppr(&g_keeped - &g_keepst, 0); }

/**
 * @brief 常駐解除
 * @param pidhcpcinfo idhcpc ワークへのポインタ
 * @return 0: 成功
 */
int freepr(const idhcpcinfo *pidhcpcinfo) {
  return _dos_mfree(
      (void *)(pidhcpcinfo - (g_idhcpcinfo - g_keepst) - sizeof(struct _psp)));
}

/**
 * @brief IOCS _ONTIME をコールする
 * @return IOCS _ONTIME の戻り値
 */
int ontime(void) { return _iocs_ontime(); }

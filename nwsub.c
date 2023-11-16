#include "nwsub.h"

#include <string.h>

#include "mynetwork.h"

/**
 * @brief 指定デバイスに対応するMACアドレスを取得する
 * @param dest MACアドレス格納用バッファ
 * @param devname デバイス名（e.g. "dev/en0"）
 * @return TRUE 成功 / FALSE 失敗
 */
int get_mac_address(eaddr *dest, char *devname) {
  int ret = FALSE;
  int no;

  if (ETDGetDriverVersion(devname, &no) != -1) {
    ETDGetMacAddr(dest, no);
    ret = TRUE;
  }
  return ret;
}

/**
 * @brief UDPソケット作成
 * @param
 * @return ソケット識別子（socket()の戻り値）。<0ならエラー
 */
int create_udp_socket(void) { return socket(AF_INET, SOCK_DGRAM, 0); }

/**
 * @brief ソケット接続要求
 * @param p sockaddr_in構造体へのポインタ
 * @param sockno ソケット識別子
 * @param portno ポート番号
 * @param ipaddr 接続先IPアドレス
 * @return connect()の戻り値。<0ならエラー
 */
int connect2(struct sockaddr_in *p, int sockno, unsigned short portno,
             int ipaddr) {
  init_sockaddr_in(p, portno, ipaddr);
  return connect(sockno, (char *)p, sizeof(*p));
}

/**
 * @brief ソケット結合
 * @param sockno ソケット識別子
 * @param portno ポート番号
 * @param ipaddr 接続先IPアドレス
 * @return bind()の戻り値。<0ならエラー
 */
int bind2(int sockno, unsigned short portno, int ipaddr) {
  struct sockaddr_in inaddr;

  init_sockaddr_in(&inaddr, portno, ipaddr);
  return bind(sockno, (char *)&inaddr, sizeof(inaddr));
}

/**
 * @brief sockaddr_in構造体の初期化
 * @param p
 * @param portno
 * @param ipaddr
 */
void init_sockaddr_in(struct sockaddr_in *p, unsigned short portno,
                      int ipaddr) {
  memset(p, 0, sizeof(*p)); /* 0 で埋めておく */
  p->sin_family = AF_INET;
  p->sin_port = htons(portno);
  p->sin_addr.s_addr = htonl(ipaddr);
}

#include "nwsub.h"

#include <string.h>

#include "mynetwork.h"

/**
 * @brief UDPソケット作成
 * @param
 * @return ソケット識別子（socket()の戻り値）。<0ならエラー
 */
int create_udp_socket(void) { return socket(AF_INET, SOCK_DGRAM, 0); }

/**
 * @brief ソケット接続要求
 * @param sockno ソケット識別子
 * @param portno ポート番号
 * @param ipaddr 接続先IPアドレス
 * @param[out] p sockaddr_in格納域
 * @return connect()の戻り値。<0ならエラー
 */
int connect2(const int sockno, const unsigned short portno, const int ipaddr,
             struct sockaddr_in *p) {
  init_sockaddr_in(portno, ipaddr, p);
  return connect(sockno, (char *)p, sizeof(*p));
}

/**
 * @brief ソケット結合
 * @param sockno ソケット識別子
 * @param portno ポート番号
 * @param ipaddr 接続先IPアドレス
 * @return bind()の戻り値。<0ならエラー
 */
int bind2(const int sockno, const unsigned short portno, const int ipaddr) {
  struct sockaddr_in inaddr;

  init_sockaddr_in(portno, ipaddr, &inaddr);
  return bind(sockno, (char *)&inaddr, sizeof(inaddr));
}

/**
 * @brief sockaddr_in構造体の初期化
 * @param portno
 * @param ipaddr
 * @param[out] p
 */
void init_sockaddr_in(const unsigned short portno, const int ipaddr,
                      struct sockaddr_in *p) {
  memset(p, 0, sizeof(*p)); /* 0 で埋めておく */
  p->sin_family = AF_INET;
  p->sin_port = portno;
  p->sin_addr.s_addr = ipaddr;
}

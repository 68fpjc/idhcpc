
	.text
	.even

_g_keepst::

_g_idhcpcinfo::
	.ds.l	1			; DHCP クライアント起動日時
	.ds.l	1			; DHCPACK 受信日時
	.ds.l	1			; リース期間（秒）
	.ds.l	1			; 更新開始タイマ（秒）
	.ds.l	1			; 再結合開始タイマ（秒）
	.ds.l	1			; 自分のIPアドレス
	.ds.l	1			; DHCPサーバIPアドレス
	.ds.l	1			; デフォルトゲートウェイ
	.ds.l	256/4-1			; DNSサーバアドレス
	.ds.b	8			; インタフェース名
	.ds.b	64			; 常駐チェック用文字列
	.even
_g_idhcpcinfoed::

_g_keeped::

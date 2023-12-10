
	.text
	.even

MAGIC	.reg	'idhcpc version 0.11.2 https://github.com/68fpjc'

_g_keepst::
_g_idhcpcinfo::
_g_magic::
	.dc.b	MAGIC,0			; 常駐チェック用文字列
	.ds.b	64-(.sizeof.(MAGIC)+1)	; パディング
	.ds.l	1			; IPアドレス設定時のマシン起動時間（秒）
	.ds.l	1			; リース期間（秒）
	.ds.l	1			; 更新開始タイマ（秒）
	.ds.l	1			; 再結合開始タイマ（秒）
	.ds.l	1			; 自分のIPアドレス
	.ds.l	1			; DHCPサーバIPアドレス
	.ds.l	1			; デフォルトゲートウェイ
	.ds.l	256/4-1			; DNSサーバアドレス

_g_keeped::

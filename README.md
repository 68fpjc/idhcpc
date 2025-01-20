# インチキ DHCP クライアント idhcpc

## これはなに？

Human68k で動作する DHCP クライアントです。

低機能・不安定・奇ッ怪な仕様と、あまりいいところがありませんが、ないよりはマシ、ということで。

## インストール

idhcpc.x を、環境変数 `path` の指すディレクトリへコピーしてください。

## 使い方

```
@>idhcpc --help
idhcpc - インチキ DHCP クライアント - version 0.xx.x https://github.com/68fpjc
Usage: idhcpc [options] [インタフェース名]
Options:
        -r, --release           コンフィギュレーション情報を破棄する
        -l, --print-lease-time  残りリース期間を表示する
        -v, --verbose           詳細表示モード
```

Neptune-X / Nereid 等の LAN ボードが正しく動作している必要があります。

また、あらかじめ TCP/IP ドライバ ([inetd.x](http://retropc.net/x68000/software/internet/kg/tcppacka/) / [hinetd.x](http://retropc.net/x68000/software/internet/tcpip/hinetd/) / [xip.x](http://retropc.net/x68000/software/internet/tcpip/xip/) 等) を常駐させておいてください。

```
idhcpc
```

ネットワーク上から DHCP サーバを探し、見つかった場合はサーバからコンフィギュレーション情報を取得し、IP アドレス・サブネットマスク等の設定を行い、メモリに常駐します。

DHCP サーバからの応答がない場合はリトライを 4 回行い、それでも応答がない場合はエラー終了します。

コマンドライン引数でインタフェースを指定できます。省略時は `en0` が指定されたものと見做します。

```
idhcpc -l
```

残りリース期間を表示します。

```
idhcpc -r
```

コンフィギュレーション情報を破棄し、常駐解除します。

```
idhcpc -v
```

```
idhcpc -v -r
```

常駐時 / 常駐解除時に DHCP サーバとの通信内容を表示します (気休め用) 。

### 例

```
@>xip                   ★ TCP/IP ドライバの常駐
X680x0 IP driver xip.x β5 Modified by K.Shirakata.

@>ifconfig lp0 up       ★ これは idhcpc とは関係ない

@>idhcpc                ★ idhcpc の常駐
idhcpc - インチキ DHCP クライアント - version 0.xx.x  https://github.com/68fpjc/idhcpc
en0: コンフィギュレーションが完了しました.
en0: 残りリース期間は 72 時間 0 分 0 秒 です.

@>ifconfig en0          ★ 情報表示には ifconfig.x や inetdconf.x を使ってください
en0: flags=1b<UP,RUNNING,NOTRAILERS,BROADCAST>
        inet 192.168.0.2 netmask 0xffffff00 broadcast 192.168.0.255

@>inetdconf             ★ 同上
default router:         192.168.0.1
rip           :         On
domain name servers:    192.168.0.1
domain name:            igarashi.net

        :
        :

@>idhcpc -l             ★ 残りリース期間の表示
idhcpc - インチキ DHCP クライアント - version 0.xx.x  https://github.com/68fpjc/idhcpc
en0: 残りリース期間は 70 時間 31 分 40 秒 です.

        :
        :

@>idhcpc -r             ★ idhcpc の常駐解除
idhcpc - インチキ DHCP クライアント - version 0.xx.x  https://github.com/68fpjc/idhcpc
en0: コンフィギュレーション情報を破棄しました.
```

## 注意

-   本来、DHCP クライアントは、IP アドレスのリース期間終了を監視し、終了が近づいた場合にはリース期間の延長をサーバに要求したりといった処理をしなければなりません。が、 idhcpc はこのあたりの処理をまったく行いません。なので、ユーザが残りリース期間を常に意識し、リース期間の終了前に idhcpc を常駐解除し、もう一度常駐させる、という操作を「手動で」行う必要があります。

-   動作テストが不十分です。

## ソースコードからのビルド

### [xdev68k](https://github.com/yosshin4004/xdev68k) の場合

```
make
```

事前にヘッダファイルとライブラリファイルを下記のように配置してください (xdev68k 標準の XC 環境ではなく、LIBC を使用します) 。

-   [LIBC 1.1.32A ぱっち ＤＯＮ版 その４](http://retropc.net/x68000/software/develop/lib/libcdon/) → 下記 `libc` ディレクトリに配置
-   [TCPPACKB](http://retropc.net/x68000/software/internet/kg/tcppackb/) → 下記 `misc` ディレクトリに配置

```
xdev68k/
│
├ include/
│	│	ヘッダファイル
│	├ libc/
│	│
│	└ misc/
│
└ lib/
	│	ライブラリファイル
	├ libc/
	│
	└ misc/
```

### X68000 実機 or エミュレータ環境の場合

```
make -f makefile.x68
```

xdev68k の場合と同様、LIBC と TCPPACKB が必要です。事前にヘッダファイルとライブラリファイルをそれぞれ、環境変数 `include` および `lib` の指すディレクトリに配置してください。

エミュレータ環境に下記をインストールし、ビルドが (とりあえず) 通ることを確認しています。

-   [GNU Make](https://github.com/kg68k/gnu-make-human68k)
-   [真里子版 GCC](http://retropc.net/x68000/software/develop/c/gcc_mariko/)
-   [LIBC 1.1.32A ぱっち ＤＯＮ版 その４](http://retropc.net/x68000/software/develop/lib/libcdon/)
-   [libgnu](https://www.vector.co.jp/soft/x68/prog/se023312.html)
-   [GCC2](http://retropc.net/x68000/software/develop/c/gcc2/) に収録されている libgcc.a
-   [HAS060.X](http://retropc.net/x68000/software/develop/as/has060/)
-   [HLK evolution](https://github.com/kg68k/hlk-ev)

## バイナリ配布

idhcpc のビルド済みバイナリは xdev68k でビルドしたものです。

### 参考文献

-   Network Working Group Request For Comments: 2131 (rfc2131)
-   Network Working Group Request For Comments: 2132 (rfc2132)

## 連絡先

https://github.com/68fpjc/idhcpc

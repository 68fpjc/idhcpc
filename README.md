# インチキ DHCP クライアント idhcpc.x

> [!NOTE]
> 本テキストは、オリジナルのアーカイブに含まれていた idhcpc.txt を Markdown 化したものです。

## これはなに？

Human68k で動作する DHCP クライアントです。

低機能・不安定・奇ッ怪な仕様と、あまりいいところがありませんが、ないよりはマシ、ということで。

## インストール

idhcpc.x を、環境変数 path の指すディレクトリへコピーしてください。

## 使い方

Neptune-X / Nereid 等の LAN ボードが正しく動作している必要があります。

また、あらかじめ TCP/IP ドライバ（xip.x 等）を常駐させておいてください。

```
idhcpc
```

LAN 上から DHCP サーバを探し、見つかった場合はサーバからコンフィギュレーション情報を取得し、IP アドレス・サブネットマスク等の設定を行い、メモリに常駐します。

DHCP サーバからの応答がない場合はリトライを 4 回行い、それでも応答がない場合はエラー終了します。

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

常駐時 / 常駐解除時に DHCP サーバとの通信内容を表示します（気休め用）。

### 例

```
@>xip                   ★ TCP/IPドライバの常駐
X680x0 IP driver xip.x β5 Modified by K.Shirakata.

@>ifconfig lp0 up       ★ これはidhcpc.xとは関係ない

@>idhcpc                ★ idhcpc.xの常駐
idhcpc.x - インチキDHCPクライアント - version 0.11 Copyright 2002,03 Igarashi
コンフィギュレーションが完了しました.
残りリース期間は 72 時間 0 分 0 秒 です.

@>ifconfig en0          ★ 情報表示にはifconfig.xやinetdconf.xを使ってください
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
idhcpc.x - インチキDHCPクライアント - version 0.11 Copyright 2002,03 Igarashi
残りリース期間は 70 時間 31 分 40 秒 です.

        :
        :

@>idhcpc -r             ★ idhcpc.xの常駐解除
idhcpc.x - インチキDHCPクライアント - version 0.11 Copyright 2002,03 Igarashi
コンフィギュレーション情報を破棄しました.
```

## 注意点

- 本来、DHCP クライアントは、IP アドレスのリース期間終了を監視し、終了が近づいた場合にはリース期間の延長をサーバに要求したりといった処理をしなければなりません。が、実は idhcpc.x はこのあたりの処理をまったく行いません。なので、ユーザが残りリース期間を常に意識し、リース期間の終了前に idhcpc.x を常駐解除し、もう一度常駐させる、という操作を「手動で」行う必要があります。

- 動作テストが不十分です。idhcpc.x を使って、うまく動かなかったり、ユーザがネットワーク管理者に怒られたりしても、私は知りません。

## 今後の予定

ありません。が、気が向いたらバージョンアップするかもしれません。

っつーか、誰かもっとまともなモノを作ってください。

## 配布規定

ご自由にどうぞ。

## 謝辞

idhcpc.x の開発にあたり、以下のツール / ライブラリ / 参考文献を使用しました。作者の方々に感謝いたします。

### ツール

- ether_ne.sys ver0.02 +M01
- TCP/IP ドライバ無償配布パッケージ(A PACK)
- xip.x β5
- MicroEMACS 3.10 j1.43 (rel.5c6)
- GNU Make version 3.77 [Human68k Release 2]
- gcc 2.95.2 うぉ～てぃ～ (β４ 人柱版)
- HAS060 version 3.09+85+12[g2as]
- HLK evolution version 3.01+14[g2lk]
- oar - Object ARchiver ver1.0.4
- X68k Source Code Debugger v3.01+12

### ライブラリ

- TCP/IP ドライバ無償配布パッケージ(B PACK)
- LIBC 1.1.32A ぱっち ＤＯＮ版 その４
- keepchk.s（大昔の Oh!X に載っていた常駐判定ルーチン）
- process.h（&lt;MUSH&gt;同梱のヘッダファイル）

### 参考文献

- Network Working Group Request For Comments: 2131（rfc2131）
- Network Working Group Request For Comments: 2132（rfc2132）

## 連絡先

https://github.com/68fpjc/idhcpc

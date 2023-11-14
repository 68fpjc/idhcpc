*****************************************************************
*	メモリ管理情報/プロセス管理情報構造定義			*
*				Written by Toshiyuki Murata	*
*****************************************************************
*			NOT COPYRIGHTED				*
*****************************************************************

*
*	psp
*
		.offset	0	*=a0
pspPREVMEM:	.ds.l	1	*直前のメモリブロック
pspKEEPFLAG:			*常駐しているかどうかのフラグ
				*（FFh...常駐）
pspMOTHER:	.ds.l	1	*このメモリブロックを確保したプロセス
pspMEMEND:	.ds.l	1	*メモリブロック末尾+1
pspNEXTMEM:	.ds.l	1	*つぎのメモリブロック
SIZEofMEMPTR:			*メモリ管理ポインタのバイト数
*
MEMCONTENTS:			*メモリブロック正味
pspENVIRON:	.ds.l	1	*環境（=a3）
pspEXITVEC:	.ds.l	1	*終了時の戻りアドレス
pspABORTVEC:	.ds.l	1	*CTRL+Cによる中断時の戻りアドレス
pspERRORVEC:	.ds.l	1	*エラーによる中断時の戻りアドレス
pspCOMMANDLINE:	.ds.l	1	*コマンドライン（=a2）
pspFILEHDLFLAG:	.ds.l	3	*ファイルハンドルの使用状況
pspBSSADR:	.ds.l	1	*bssの先頭アドレス
pspHEAPADR:	.ds.l	1	*ヒープ先頭アドレス（＝BSSADR）
pspSTACKADR:	.ds.l	1	*初期スタックアドレス（=a1）
pspMAMUSP:	.ds.l	1	*親のUSP
pspMAMSSP:	.ds.l	1	*親のSSP
pspMAMSR:	.ds.w	1	*親のSR
pspABORTSR:	.ds.w	1	*アボート時のSR
pspABORTSSP:	.ds.l	1	*アボート時のSSP
pspTRAP10VEC:	.ds.l	1	*trap #10
pspTRAP11VEC:	.ds.l	1	*trap #11
pspTRAP12VEC:	.ds.l	1	*trap #12
pspTRAP13VEC:	.ds.l	1	*trap #13
pspTRAP14VEC:	.ds.l	1	*trap #14
pspOSFLAG:	.ds.l	1	*フラグ（0...親, -1...SHELL=〜で起動）
pspMODULENO:	.ds.b	1	*EXEC モジュール番号	*** 未公開 ***
		.ds.b	3	*未使用
pspNEXTPSP:	.ds.l	1	*つぎのプロセス		*** 未公開 ***
		.ds.l	5	*未使用
pspEXECPATH:	.ds.b	68	*起動時のパス名
pspEXECNAME:	.ds.b	24	*起動時のファイル名
		.ds.l	9	*未使用
SIZEofPSP:
		.text

*
*	バックグラウンドプロセス用定数/構造体定義
*
thNMAX		equ	32	*最大スレッド数
*
thUSER		equ	$0000	*ユーザーモードで走る
thSUPER		equ	$2000	*スーパーバイザモードで走る

*
*	スレッド管理情報
*
		.offset	0
*
thNEXT:		.ds.l	1	*つぎのスレッド
thWAITFLAG:	.ds.b	1	*スリープ中かどうかのフラグ
				*  00h ... 起きている
				*  FEh ... 強制スリープ中
				*  FFh ... スリープ中
thCOUNT:	.ds.b	1	*実行優先順位づけ用カウンタ
thLEVEL:	.ds.b	1	*実行優先レベル-1
thDOSCOMMAND:	.ds.b	1	*実行中だったDOSコール番号
thPSP:		.ds.l	1	*psp
thUSP:		.ds.l	1	*usp
thDREG:		.ds.l	8	*d0〜d7
thAREG:		.ds.l	7	*a0〜a6
thSR:		.ds.w	1	*sr
thPC:		.ds.l	1	*pc
thSSP:		.ds.l	1	*ssp
thINDOSF:	.ds.w	1	*（DOSコールネスティングレベル）
thINDOSP:	.ds.l	1	*（DOSコール処理中のssp）
thBUFF:		.ds.l	1	*スレッド間通信バッファ
thNAME:		.ds.b	16	*スレッド名
thWAITTIME:	.ds.l	1	*スリープ時間残り
SIZEofTHREADINFO:
		.text
*
thNAMED		equ	-1	*スレッド名で指定する
thMYSELF	equ	-2	*自分自身

*
*	スレッド間通信バッファ
*
		.offset	0	*スレッド間通信バッファ
*
thCOMMDATALEN:	.ds.l	1	*バッファバイト数
				*（通信時はデータバイト数）
thCOMMDATA:	.ds.l	1	*データ格納領域先頭アドレス
thCOMMCOMMAND:	.ds.w	1	*コマンド
thCOMMID:	.ds.w	1	*送り手ID
SIZEofCOMMBUFF:
		.text
*
thEMPTY		equ	-1	*スレッド間通信バッファにデータがない

*
*	スレッド間通信コマンド
*
thKILL		equ	$fff9	*破棄要求
thWAKEUP	equ	$fffb	*アクティブにする
thSLEEP		equ	$fffc	*スリープ要求
thISBUSY	equ	$ffff	*スレッド間通信可能かどうか調べる

*
*	エラーコード
*
thCONFLICT	equ	-27	*同名のスレッドが存在する
thBUSY		equ	-28	*スレッド間通信が受けつけられない
thNOMORE	equ	-29	*これ以上スレッドを登録できない

.ifndef	_SUSPEND_PR
_SUSPEND_PR	equ	$fffb
.endif

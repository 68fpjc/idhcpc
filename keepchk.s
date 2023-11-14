*========================================================================
*	keepchk.s
*			原作: 村田敏幸氏
*				X68000マシン語プログラミング
*					Chapter_1FH, Oh!X Nov 1992
*========================================================================
		.cpu	68000
*========================================================================
		.include	doscall.mac
		.include	process.h
*========================================================================
		.xdef	keepchk
*========================================================================
		.text
		.even
*========================================================================
keepchk:
*自身が常駐しているかどうか調べる
*入力
*	4(sp).l	自身のメモリ管理ポインタ
*	8(sp).l	プログラム先頭から識別用文字列までの
*		バイト数
*出力
*	d0.b	常駐しているかどうか
*		=  0 ... 常駐していない
*		= -1 ... 常駐している
*	d1.b	Human68kから直接起動されたかどうか
*		=  0 ... command.xなどから起動
*		= -1 ... Human68kから直接起動
*	a0	メモリ管理ポインタ
*		d0.b =  0 ... 自身のメモリ管理ポインタ
*		d0.b = -1 ... 見つけた常駐プロセスのメモリ管理ポインタ
MYMP		=	8
IDOFST		=	MYMP+4
		link	a6,#0
		movem.l	d2-d3/a1-a4,-(sp)

		movem.l	MYMP(a6),a0/a4	*a0 = 自身のメモリ管理ポインタ
					*a4 = プログラム先頭からの
					*	ID文字列へのオフセット
		lea.l	SIZEofPSP(a4),a4	*a4 = メモリ管理ポインタ先頭からの
						*	ID文字列へのオフセット

		lea.l	0(a0,a4.l),a1	*a1 = ID文字列

		movea.l	a1,a2		*d3にID文字列の長さを得る
		moveq.l	#-1,d3		*（末尾の0は含まない）
lenlp:		addq.w	#1,d3		*
		tst.b	(a2)+		*
		bne	lenlp		*

		clr.l	-(sp)		*スーパーバイザモードへ移行する
		DOS	__SUPER
		move.l	d0,(sp)		*sspを退避

		movea.l	pspMOTHER(a0),a0	*a0 = 親プロセス
		move.l	pspMOTHER(a0),d0	*d0 = 親の親
		seq.b	d1		*d1.b =  0 ... command.xなどから起動
					*d1.b = -1 ... Human68kから直接起動
		beq	chk0
					*先頭のプロセスを探す
chklp0:		movea.l	d0,a0		*a0 = 注目中のプロセス
		move.l	pspMOTHER(a0),d0	*d0 = 親プロセス
		bne	chklp0		*親がいるあいだ繰り返す

chk0:		moveq.l	#-1,d2		*d2 = 常駐フラグ

chklp1:		cmp.b	pspKEEPFLAG(a0),d2	*常駐プロセスか？
		bne	chknx1		*　違う

		lea.l	0(a0,a4.l),a2	*a2 = ID文字列があるはずの位置
		adda.w	d3,a2		*a2 = ID文字列末尾が
					*	あるはずの位置
		cmpa.l	pspMEMEND(a0),a2	*それがメモリブロック外なら
		bcc	chknx1		*　比較するまでもなく不一致

		suba.w	d3,a2		*a2 = ID文字列があるはずの位置
		movea.l	a1,a3		*a3 = ID文字列
		move.w	d3,d0		*d0 = 文字列の長さ+1-1
cmplp:		cmpm.b	(a3)+,(a2)+	*ID文字列を比較する
		dbne	d0,cmplp	*
		beq	found		*一致した

					*一致しなかった
chknx1:		move.l	pspNEXTMEM(a0),d0	*d0 = つぎのメモリブロック
		beq	nfound		*0ならば最後のメモリブロック
					*（∴常駐していなかった）
		movea.l	d0,a0		*最後のメモリブロックに達するまで
		bra	chklp1		*　繰り返す

found:		moveq.l	#0,d2		*常駐していた

done:		tst.b	(sp)		*サブルーチンが呼び出された時点で
		bmi	skip		*　ユーザーモードだったのなら
		DOS	__SUPER		*　ユーザーモードに復帰する
skip:		addq.l	#4,sp

		tst.b	d2
		seq.b	d0		*d0.b =  0 ... 非常駐
					*（a0 = 自身のメモリ管理ポインタ）
					*d0.b = -1 ... 常駐
					*（a0 = そのメモリ管理ポインタ）
		movem.l	(sp)+,d2-d3/a1-a4
		unlk	a6

		rts
*
nfound:		movea.l	MYMP(a6),a0	*a0 = 自身のメモリ管理ポインタを復帰
		bra	done

*========================================================================
		.end


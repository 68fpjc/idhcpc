
		.xref	keepchk

		.text
		.even

; ================================================================
;
;	int keepchk(struct _mep *pme, size_t offset, struct _mep **ppmep)
;
; ================================================================
_keepchk::
		movea.l	(sp)+,a1
		jbsr	keepchk
		pea.l	(a1)
		move.l	12(sp),a1
		move.l	a0,(a1)
		ext.w	d0
		ext.l	d0
		rts

		.end


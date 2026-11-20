| memcheck_stubs.s: exec AllocMem and FreeMem patches of memcheck.c.
|
| mc_allocmem calls mc_pre_alloc(size, flags, sp), then the original
| AllocMem unless that returned -2 (the call is dropped: d0 = 0), then
| mc_post(slot, result). mc_freemem does the same with mc_pre_free(block,
| size, sp). sp points at the caller's return address. mc_busy counts the
| tasks inside a stub. mc_rawput is a RawDoFmt PutChProc for RawPutChar.
	.text
	.globl	_mc_allocmem
	.globl	_mc_freemem
	.globl	_mc_rawput
	.globl	_mc_orig_alloc
	.globl	_mc_orig_free
	.globl	_mc_busy

_mc_allocmem:
	addq.l	#1,_mc_busy
	movem.l	d0-d1/a0-a1,-(sp)
	pea	16(sp)
	move.l	d1,-(sp)
	move.l	d0,-(sp)
	jsr	_mc_pre_alloc
	lea	12(sp),sp
	move.l	d0,-(sp)		| slot
	moveq	#0,d0
	cmp.l	#-2,(sp)
	beq.s	1f
	movem.l	4(sp),d0-d1/a0-a1
	move.l	_mc_orig_alloc,a1
	jsr	(a1)
1:	move.l	(sp)+,d1
	move.l	d0,(sp)			| the saved d0 becomes the result
	move.l	d0,-(sp)
	move.l	d1,-(sp)
	jsr	_mc_post
	addq.l	#8,sp
	movem.l	(sp)+,d0-d1/a0-a1
	subq.l	#1,_mc_busy
	tst.l	d0
	rts

_mc_freemem:
	addq.l	#1,_mc_busy
	movem.l	d0-d1/a0-a1,-(sp)
	pea	16(sp)
	move.l	d0,-(sp)
	move.l	a1,-(sp)
	jsr	_mc_pre_free
	lea	12(sp),sp
	move.l	d0,-(sp)		| slot
	cmp.l	#-2,d0
	beq.s	1f
	movem.l	4(sp),d0-d1/a0-a1
	move.l	_mc_orig_free,a0
	jsr	(a0)
1:	move.l	(sp)+,d1
	clr.l	-(sp)
	move.l	d1,-(sp)
	jsr	_mc_post
	addq.l	#8,sp
	movem.l	(sp)+,d0-d1/a0-a1
	subq.l	#1,_mc_busy
	rts

_mc_rawput:
	move.l	a6,-(sp)
	move.l	_SysBase,a6
	jsr	-516(a6)		| RawPutChar
	move.l	(sp)+,a6
	rts

	.data
_mc_orig_alloc:
	.long	0
_mc_orig_free:
	.long	0
_mc_busy:
	.long	0

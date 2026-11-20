| mccspy_stubs.s: the patches of mccspy.c.
|
| ms_d<n> wraps the dispatcher of class slot n: it calls ms_pre(n, cl, obj,
| msg, sp). A negative result jumps to ms_old[n] with the registers as they
| came; otherwise the original dispatcher runs and ms_post(id, result)
| fills in the record id. ms_ccc runs the original MUI_CreateCustomClass,
| then ms_ccc_done(result, saved d0/a0-a3, sp). ms_lib<n> calls
| ms_lib(n, a0, d0, sp) and jumps to ms_orig_lib[n]. sp points at the
| caller's return address. ms_busy counts the tasks inside a stub.
	.text
	.globl	_ms_stubs
	.globl	_ms_old
	.globl	_ms_ccc
	.globl	_ms_orig_ccc
	.globl	_ms_libstubs
	.globl	_ms_orig_lib
	.globl	_ms_busy

.macro	DSTUB n
ms_d\n:	addq.l	#1,_ms_busy
	move.l	#\n,-(sp)
	bra.w	ms_dcommon
.endm
	.irp	n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
	DSTUB	\n
	.endr

ms_dcommon:
	movem.l	d0-d1/a0-a1,-(sp)	| d0 d1 a0 a1 n ret
	pea	20(sp)
	move.l	a1,-(sp)
	move.l	a2,-(sp)
	move.l	a0,-(sp)
	move.l	32(sp),-(sp)
	jsr	_ms_pre
	lea	20(sp),sp
	move.l	16(sp),d1
	lsl.l	#2,d1
	lea	_ms_old,a0
	move.l	0(a0,d1.l),a0
	tst.l	d0
	bmi.s	2f
	move.l	d0,16(sp)		| n becomes the record id
	move.l	a0,-(sp)		| orig d0 d1 a0 a1 id ret
	movem.l	4(sp),d0-d1/a0-a1
	pea	1f(pc)
	move.l	4(sp),-(sp)
	rts				| into the dispatcher, back at 1:
1:	lea	20(sp),sp		| id ret
	move.l	d0,-(sp)
	move.l	4(sp),-(sp)
	jsr	_ms_post		| returns the result
	lea	12(sp),sp
	subq.l	#1,_ms_busy
	rts
2:	move.l	a0,16(sp)		| n becomes the dispatcher
	movem.l	(sp)+,d0-d1/a0-a1
	subq.l	#1,_ms_busy
	rts

_ms_ccc:
	addq.l	#1,_ms_busy
	movem.l	d0/a0-a3,-(sp)		| d0 a0 a1 a2 a3 ret
	pea	1f(pc)
	move.l	_ms_orig_ccc,-(sp)
	rts
1:	move.l	sp,a0
	pea	20(a0)
	move.l	a0,-(sp)
	move.l	d0,-(sp)
	jsr	_ms_ccc_done		| returns the result
	lea	32(sp),sp
	subq.l	#1,_ms_busy
	rts

.macro	LSTUB n
ms_l\n:	addq.l	#1,_ms_busy
	move.l	#\n,-(sp)
	bra.w	ms_lcommon
.endm
	LSTUB	0
	LSTUB	1
	LSTUB	2

ms_lcommon:
	movem.l	d0-d1/a0-a1,-(sp)	| d0 d1 a0 a1 n ret
	pea	20(sp)
	move.l	d0,-(sp)
	move.l	a0,-(sp)
	move.l	28(sp),-(sp)
	jsr	_ms_lib
	lea	16(sp),sp
	move.l	16(sp),d0
	lsl.l	#2,d0
	lea	_ms_orig_lib,a0
	move.l	0(a0,d0.l),16(sp)
	movem.l	(sp)+,d0-d1/a0-a1
	subq.l	#1,_ms_busy
	rts

	.data
_ms_stubs:
	.irp	n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
	.long	ms_d\n
	.endr
_ms_libstubs:
	.long	ms_l0, ms_l1, ms_l2
_ms_old:
	.space	256
_ms_orig_ccc:
	.long	0
_ms_orig_lib:
	.space	12
_ms_busy:
	.long	0

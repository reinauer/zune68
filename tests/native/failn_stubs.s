| failn_stubs.s: the patch stubs of failn.c, one per hooked function.
|
| Each stub pushes its index and branches to fs_common, which saves the
| argument registers and asks fn_decide(index, saved) about the call:
| 0 runs the original function with the registers as they came, anything
| else returns fn_fail[index] in d0 at once. saved[0..4] are d0 d1 a0 a1 a6
| as the caller passed them, saved[5] the index, saved[6] the caller's
| return address and saved[7..] the caller's stack.
	.text
	.globl	_fn_stubs
	.globl	_fn_orig
	.globl	_fn_fail

.macro	STUB n
fs_\n:	move.l	#\n,-(sp)
	bra.w	fs_common
.endm

	STUB 0
	STUB 1
	STUB 2
	STUB 3
	STUB 4
	STUB 5
	STUB 6
	STUB 7
	STUB 8
	STUB 9
	STUB 10
	STUB 11
	STUB 12
	STUB 13
	STUB 14
	STUB 15

fs_common:
	movem.l	d0-d1/a0-a1/a6,-(sp)
	pea	(sp)
	move.l	24(sp),-(sp)
	jsr	_fn_decide
	addq.l	#8,sp
	tst.l	d0
	bne.s	1f
	movem.l	(sp)+,d0-d1/a0-a1/a6
	move.l	d0,-(sp)
	move.l	a0,-(sp)
	move.l	8(sp),d0
	lsl.l	#2,d0
	lea	_fn_orig,a0
	move.l	0(a0,d0.l),8(sp)
	move.l	(sp)+,a0
	move.l	(sp)+,d0
	rts
1:	movem.l	(sp)+,d0-d1/a0-a1/a6
	move.l	a0,-(sp)
	move.l	4(sp),d0
	lsl.l	#2,d0
	lea	_fn_fail,a0
	move.l	0(a0,d0.l),d0
	move.l	(sp)+,a0
	addq.l	#4,sp
	rts

	.data
_fn_stubs:
	.long	fs_0, fs_1, fs_2, fs_3, fs_4, fs_5, fs_6, fs_7
	.long	fs_8, fs_9, fs_10, fs_11, fs_12, fs_13, fs_14, fs_15
_fn_orig:
	.space	64
_fn_fail:
	.space	64

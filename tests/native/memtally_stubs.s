| memtally_stubs.s: exec AllocMem and FreeMem patches of memtally.c.
|
| mt_allocmem calls the original AllocMem, then mt_alloc(result, size, sp)
| with sp pointing at the caller's return address; mt_freemem calls
| mt_free(address, size) first and then the original FreeMem.
	.text
	.globl	_mt_allocmem
	.globl	_mt_freemem
	.globl	_mt_orig_alloc
	.globl	_mt_orig_free

_mt_allocmem:
	movem.l	d0-d1/a0-a1,-(sp)
	move.l	_mt_orig_alloc,a1
	jsr	(a1)
	move.l	d0,-(sp)
	pea	20(sp)
	move.l	8(sp),-(sp)
	move.l	8(sp),-(sp)
	jsr	_mt_alloc
	lea	12(sp),sp
	move.l	(sp)+,d0
	addq.l	#4,sp
	movem.l	(sp)+,d1/a0-a1
	rts

_mt_freemem:
	movem.l	d0-d1/a0-a1,-(sp)
	move.l	d0,-(sp)
	move.l	a1,-(sp)
	jsr	_mt_free
	addq.l	#8,sp
	movem.l	(sp)+,d0-d1/a0-a1
	move.l	_mt_orig_free,-(sp)
	rts

	.data
_mt_orig_alloc:
	.long	0
_mt_orig_free:
	.long	0

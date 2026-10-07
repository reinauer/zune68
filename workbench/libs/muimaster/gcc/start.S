/* First code in the load file: reject accidental execution from the Shell. */
        .text
        .globl _start
_start:
        moveq #-1,d0
        rts

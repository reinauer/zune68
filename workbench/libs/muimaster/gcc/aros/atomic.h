#ifndef ZUNE68_ATOMIC_H
#define ZUNE68_ATOMIC_H
/* One memory instruction; classic Exec cannot preempt a read/modify/write. */
#define AROS_ATOMIC_OR(value, mask) \
    __asm volatile("or.l %1,%0" : "+m"(value) : "d"((ULONG)(mask)) : "cc", "memory")
#define AROS_ATOMIC_AND(value, mask) \
    __asm volatile("and.l %1,%0" : "+m"(value) : "d"((ULONG)(mask)) : "cc", "memory")
#endif

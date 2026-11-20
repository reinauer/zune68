/*
 * memcheck: catch heap damage at the first exec memory call after it.
 *
 *   memcheck            patch exec AllocMem and FreeMem for every task and
 *                       run until Ctrl-C (or "memcheck stop")
 *   memcheck stop       stop a running memcheck and wait until it is gone
 *   memcheck read [ADDR]  print the record at ADDR (default: the running
 *                       memcheck's)
 *
 * Before and after each AllocMem/FreeMem, memcheck walks the free-chunk
 * list of every MemHeader: chunks aligned, inside the header, sizes
 * multiples of 8, each chunk above the end of the one before, the sizes
 * adding up to mh_Free. Before a FreeMem it also checks that the block is
 * inside a MemHeader and overlaps no free chunk (a double free).
 *
 * At start it prints "rec=<address>": a block holding struct mc_rec below,
 * readable with a debugger after the machine died. On the first damage it
 * records which check failed, the call that found it (operation, block,
 * size, task, three return addresses: the direct caller and the first two
 * RAM addresses on the stack after a call instruction), the bytes around
 * the damaged header and the last NCALL calls; it prints the report over
 * RawPutChar (serial) and to its own output. The report lists the frees
 * in the ring whose block covers the damaged spot (who used memory after
 * freeing it) and the allocations that cover it or end up to 64 bytes
 * below it (who overran a block). The call that found damage before it
 * ran is not passed to exec (AllocMem returns NULL, FreeMem frees
 * nothing) so exec does not follow a broken list; after the first damage
 * memcheck checks and records nothing more.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s tests/native/memcheck.c \
 *        tests/native/memcheck_stubs.s -o memcheck
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ULONG mc_orig_alloc, mc_orig_free;
extern volatile ULONG mc_busy;
void mc_allocmem(void);
void mc_freemem(void);
void mc_rawput(void);

#define MAGIC 0x4D43484BUL          /* 'MCHK' */
#define NCALL 1024
#define OP_ALLOC 1
#define OP_FREE 2

struct mc_call {
    ULONG seq, op;
    ULONG addr, size, attr;         /* AllocMem: result (0 until it
                                       returned), size, requirements */
    ULONG task;
    ULONG ret[3];
    char name[16];
};

enum { D_LIST = 1, D_ALIGN, D_OUTSIDE, D_SIZE, D_PASTEND, D_ORDER,
       D_FREECOUNT, D_TWICE, D_NOTRAM };
static const char *const dnames[] = { "?", "MemHeader list broken",
    "chunk misaligned", "chunk outside its MemHeader",
    "chunk size 0 or not a multiple of 8", "chunk runs past mh_Upper",
    "next chunk not above the end of this one",
    "free sizes do not add up to mh_Free", "FreeMem of free memory",
    "FreeMem outside every MemHeader" };

struct mc_rec {
    ULONG magic, size;
    ULONG damaged;                  /* 0, or a D_ code */
    ULONG seq;                      /* calls seen; ring[seq % NCALL] next */
    ULONG phase;                    /* 1 found before the call, 2 after */
    ULONG mh, chunk;                /* MemHeader, header holding bad data */
    ULONG next, bytes;              /* that header's mc_Next and mc_Bytes */
    ULONG lo, hi;                   /* FreeMem: the range freed */
    ULONG dumpat, dump[16];         /* 64 bytes from dumpat */
    struct mc_call call;            /* the call that found it */
    struct mc_call ring[NCALL];
};

static struct mc_rec *R;
static struct Task *mctask;
static ULONG dsig;
static ULONG rlo[16], rhi[16];
static int nr;

static struct mcport {
    struct MsgPort mp;
    struct mc_rec *rec;
} port;

static int in_ram(ULONG a)
{
    int i;

    if (a & 1)
        return 0;
    for (i = 0; i < nr; i++)
        if (a >= rlo[i] + 8 && a < rhi[i])
            return 1;
    return 0;
}

static int after_call(ULONG a)
{
    UWORD w2 = *(UWORD *)(a - 2), w4 = *(UWORD *)(a - 4),
          w6 = *(UWORD *)(a - 6);

    if ((w2 & 0xFFF8) == 0x4E90)
        return 1;
    if ((w2 & 0xFF00) == 0x6100 && (w2 & 0xFF) && (w2 & 0xFF) != 0xFF)
        return 1;
    if ((w4 & 0xFFF0) == 0x4EA0 && (w4 & 0xF) >= 8)
        return 1;
    if (w4 == 0x4EB8 || w4 == 0x4EBA || w4 == 0x4EBB || w4 == 0x6100)
        return 1;
    if (w6 == 0x4EB9 || w6 == 0x61FF)
        return 1;
    return 0;
}

static void kp(const char *fmt, ...)
{
    RawDoFmt((STRPTR)fmt, (APTR)(&fmt + 1), (void (*)())mc_rawput, NULL);
}

static void task_name(struct Task *t, char *d)
{
    const UBYTE *s = (const UBYTE *)t->tc_Node.ln_Name;
    int n = s ? strlen((const char *)s) : 0, i;

    if (t->tc_Node.ln_Type == NT_PROCESS) {
        struct CommandLineInterface *cli =
            BADDR(((struct Process *)t)->pr_CLI);
        if (cli && cli->cli_Module && cli->cli_CommandName) {
            s = (const UBYTE *)BADDR(cli->cli_CommandName);
            n = *s++;
        }
    }
    for (i = n; i > 0 && s[i - 1] != '/' && s[i - 1] != ':'; i--)
        ;
    s += i;
    n -= i;
    if (n > 15)
        n = 15;
    memcpy(d, s, n);
    d[n] = 0;
}

static void fill(struct mc_call *c, ULONG op, ULONG addr, ULONG size,
                 ULONG attr, ULONG *sp)
{
    struct Task *t = SysBase->ThisTask;
    ULONG *top = sp + 96;
    int k = 1, i;

    c->seq = R->seq;
    c->op = op;
    c->addr = addr;
    c->size = size;
    c->attr = attr;
    c->task = (ULONG)t;
    c->ret[0] = sp[0];
    c->ret[1] = c->ret[2] = 0;
    if ((APTR)sp >= t->tc_SPLower && (APTR)sp < t->tc_SPUpper
        && (ULONG *)t->tc_SPUpper < top)
        top = (ULONG *)t->tc_SPUpper;
    for (i = 1; sp + i < top && k < 3; i++) {
        ULONG a = sp[i];
        if (in_ram(a) && a != c->ret[k - 1] && after_call(a))
            c->ret[k++] = a;
    }
    task_name(t, c->name);
}

/* Walk every free list; lo < hi is a range about to be freed. Returns 0
 * or a D_ code with the details in R. */
static ULONG check(ULONG lo, ULONG hi)
{
    struct MemHeader *mh;
    int nh = 0, inside = lo >= hi;

    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head;;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ) {
        ULONG L, U, total = 0, end = 0, holder;
        struct MemChunk *c, *prev = NULL;

        if (!mh || ((ULONG)mh & 1) || ++nh > 32) {
            R->mh = R->chunk = (ULONG)mh;
            return D_LIST;
        }
        if (!mh->mh_Node.ln_Succ)
            break;
        L = (ULONG)mh->mh_Lower;
        U = (ULONG)mh->mh_Upper;
        if (lo < hi && lo >= L && hi <= U)
            inside = 1;
        R->mh = (ULONG)mh;
        for (c = mh->mh_First; c; prev = c, c = c->mc_Next) {
            ULONG a = (ULONG)c, b, kind = 0;

            holder = prev ? (ULONG)prev : (ULONG)mh;
            R->next = a;
            R->bytes = prev ? prev->mc_Bytes : 0;
            if (a & 7)
                kind = D_ALIGN;
            else if (a < L || a >= U)
                kind = D_OUTSIDE;
            else if (prev && a <= end)
                kind = D_ORDER;
            if (kind) {
                R->chunk = holder;
                return kind;
            }
            b = c->mc_Bytes;
            R->chunk = a;
            R->next = (ULONG)c->mc_Next;
            R->bytes = b;
            if (!b || (b & 7))
                return D_SIZE;
            if (b > U - a)
                return D_PASTEND;
            if (lo < hi && a < hi && a + b > lo)
                return D_TWICE;
            total += b;
            end = a + b;
        }
        if (total != mh->mh_Free) {
            R->chunk = (ULONG)mh;
            R->next = (ULONG)mh->mh_First;
            R->bytes = total;
            return D_FREECOUNT;
        }
    }
    if (!inside) {
        R->mh = R->chunk = 0;
        return D_NOTRAM;
    }
    return 0;
}

static const char *opname(ULONG op)
{
    return op == OP_ALLOC ? "AllocMem" : op == OP_FREE ? "FreeMem" : "?";
}

static void found(ULONG kind, ULONG phase, struct mc_call *c)
{
    ULONG at = R->chunk & ~3UL, i;

    R->damaged = kind;
    R->phase = phase;
    R->call = *c;
    if (kind == D_TWICE || kind == D_NOTRAM)
        at = R->lo;
    at = at >= 16 ? at - 16 : 0;
    R->dumpat = at;
    if (in_ram(at) && in_ram(at + 60))
        for (i = 0; i < 16; i++)
            R->dump[i] = ((ULONG *)at)[i];
    kp("memcheck: %s at %08lx (MemHeader %08lx, next %08lx bytes %08lx)\n",
       dnames[kind], R->chunk, R->mh, R->next, R->bytes);
    kp("memcheck: found %s call #%lu %s(%08lx, %lu) task %s %08lx"
       " ret %08lx %08lx %08lx rec=%08lx\n",
       phase == 1 ? "before" : "after", c->seq, opname(c->op),
       c->addr, c->size, c->name, c->task, c->ret[0], c->ret[1],
       c->ret[2], R);
    Signal(mctask, 1UL << dsig);
}

LONG mc_pre_alloc(ULONG size, ULONG attr, ULONG *sp)
{
    struct mc_call *c;
    ULONG kind, slot;

    if (R->damaged)
        return -1;
    Forbid();
    slot = R->seq % NCALL;
    c = &R->ring[slot];
    fill(c, OP_ALLOC, 0, size, attr, sp);
    R->seq++;
    R->lo = R->hi = 0;
    if ((kind = check(0, 0)) != 0) {
        found(kind, 1, c);
        Permit();
        return -2;
    }
    Permit();
    return slot;
}

LONG mc_pre_free(ULONG addr, ULONG size, ULONG *sp)
{
    struct mc_call *c;
    ULONG kind, slot, lo = 0, hi = 0;

    if (R->damaged)
        return -1;
    if (size) {
        lo = addr & ~7UL;
        hi = (addr + size + 7) & ~7UL;
    }
    Forbid();
    slot = R->seq % NCALL;
    c = &R->ring[slot];
    fill(c, OP_FREE, addr, size, 0, sp);
    R->seq++;
    R->lo = lo;
    R->hi = hi;
    if ((kind = check(lo, hi)) != 0) {
        found(kind, 1, c);
        Permit();
        return -2;
    }
    Permit();
    return slot;
}

void mc_post(LONG slot, ULONG result)
{
    struct mc_call *c;
    ULONG kind;

    if (slot < 0)
        return;
    Forbid();
    c = &R->ring[slot];
    if (c->op == OP_ALLOC)
        c->addr = result;
    if (!R->damaged) {
        R->lo = R->hi = 0;
        if ((kind = check(0, 0)) != 0)
            found(kind, 2, c);
    }
    Permit();
}

static void print_call(const struct mc_call *c)
{
    printf("  #%-6lu %-8s %08lx %7lu", (unsigned long)c->seq,
        opname(c->op), (unsigned long)c->addr, (unsigned long)c->size);
    if (c->op == OP_ALLOC)
        printf(" %05lx", (unsigned long)c->attr);
    else
        printf("      ");
    printf(" %-15s %08lx ret %08lx %08lx %08lx\n", c->name,
        (unsigned long)c->task, (unsigned long)c->ret[0],
        (unsigned long)c->ret[1], (unsigned long)c->ret[2]);
}

static void print_rec(const struct mc_rec *r)
{
    ULONG n = r->seq < NCALL ? r->seq : NCALL, i, target;
    int shown = 0;

    if (!r->damaged) {
        printf("memcheck: no damage in %lu calls\n", (unsigned long)r->seq);
        return;
    }
    printf("memcheck: %s at %08lx\n", r->damaged < sizeof(dnames) /
        sizeof(dnames[0]) ? dnames[r->damaged] : "?",
        (unsigned long)r->chunk);
    printf("  MemHeader %08lx, next %08lx, bytes %08lx",
        (unsigned long)r->mh, (unsigned long)r->next,
        (unsigned long)r->bytes);
    if (r->lo < r->hi)
        printf(", freeing %08lx-%08lx", (unsigned long)r->lo,
            (unsigned long)r->hi);
    printf("\nfound %s this call (not passed to exec when before):\n",
        r->phase == 1 ? "before" : "after");
    print_call(&r->call);
    printf("bytes at %08lx:", (unsigned long)r->dumpat);
    for (i = 0; i < 16; i++)
        printf("%s%08lx", i == 8 ? "\n                  " : " ",
            (unsigned long)r->dump[i]);
    printf("\nlast calls, newest first (the one before the finding call"
        " passed every check):\n");
    for (i = 0; i < n && i < 24; i++)
        print_call(&r->ring[(r->seq - 1 - i) % NCALL]);

    target = r->damaged == D_TWICE || r->damaged == D_NOTRAM ? r->lo
        : r->chunk;
    printf("frees covering %08lx:\n", (unsigned long)target);
    for (i = 1; i < n; i++) {
        const struct mc_call *c = &r->ring[(r->seq - 1 - i) % NCALL];
        if (c->op == OP_FREE && c->addr <= target
            && target < c->addr + c->size) {
            print_call(c);
            shown++;
        }
    }
    printf("allocations covering it or ending up to 64 bytes below:\n");
    for (i = 1; i < n; i++) {
        const struct mc_call *c = &r->ring[(r->seq - 1 - i) % NCALL];
        ULONG end = c->addr + c->size;
        if (c->op == OP_ALLOC && c->addr && c->addr <= target
            && target < end + 64) {
            print_call(c);
            shown++;
        }
    }
    if (!shown)
        printf("  (none in the last %lu calls)\n", (unsigned long)n);
}

static int read_rec(const char *arg)
{
    struct mc_rec *r = NULL;
    struct mcport *p;

    if (arg)
        r = (struct mc_rec *)strtoul(arg, NULL, 16);
    else {
        Forbid();
        if ((p = (struct mcport *)FindPort((STRPTR)"memcheck")) != NULL)
            r = p->rec;
        Permit();
    }
    if (!r || ((ULONG)r & 1) || TypeOfMem(r) == 0 || r->magic != MAGIC
        || r->size != sizeof(*r)) {
        printf("memcheck: no record%s%s\n", arg ? " at " : "",
            arg ? arg : " (memcheck is not running)");
        return 10;
    }
    print_rec(r);
    return 0;
}

static int stop(void)
{
    struct MsgPort *p;
    int i;

    Forbid();
    if ((p = FindPort((STRPTR)"memcheck")) != NULL)
        Signal(p->mp_SigTask, SIGBREAKF_CTRL_C);
    Permit();
    if (!p) {
        printf("memcheck: not running\n");
        return 5;
    }
    for (i = 0; i < 500; i++) {
        Forbid();
        p = FindPort((STRPTR)"memcheck");
        Permit();
        if (!p)
            return 0;
        Delay(2);
    }
    printf("memcheck: still running\n");
    return 5;
}

static int unpatch(void)
{
    UBYTE *base = (UBYTE *)SysBase;

    Forbid();
    if (*(ULONG *)(base - 198 + 2) != (ULONG)mc_allocmem
        || *(ULONG *)(base - 210 + 2) != (ULONG)mc_freemem) {
        Permit();
        return 0;
    }
    SetFunction((struct Library *)SysBase, -198, (APTR)mc_orig_alloc);
    SetFunction((struct Library *)SysBase, -210, (APTR)mc_orig_free);
    Permit();
    return 1;
}

int main(int argc, char **argv)
{
    struct MemHeader *mh;
    LONG sig;
    int printed = 0;

    if (argc >= 2 && !strcmp(argv[1], "read"))
        return read_rec(argc >= 3 ? argv[2] : NULL);
    if (argc >= 2 && !strcmp(argv[1], "stop"))
        return stop();
    if (argc >= 2) {
        printf("usage: memcheck [stop | read [ADDR]]\n");
        return 5;
    }
    Forbid();
    if (FindPort((STRPTR)"memcheck")) {
        Permit();
        printf("memcheck: already running\n");
        return 5;
    }
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head;
         mh->mh_Node.ln_Succ && nr < 16;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ) {
        rlo[nr] = (ULONG)mh->mh_Lower;
        rhi[nr++] = (ULONG)mh->mh_Upper;
    }
    Permit();
    if ((sig = AllocSignal(-1)) < 0 || (dsig = AllocSignal(-1)) == (ULONG)-1
        || !(R = AllocMem(sizeof(*R), MEMF_PUBLIC | MEMF_CLEAR))) {
        printf("memcheck: no signal or memory\n");
        return 20;
    }
    R->magic = MAGIC;
    R->size = sizeof(*R);
    mctask = FindTask(NULL);
    port.mp.mp_Node.ln_Type = NT_MSGPORT;
    port.mp.mp_Node.ln_Name = (char *)"memcheck";
    port.mp.mp_Flags = PA_SIGNAL;
    port.mp.mp_SigBit = sig;
    port.mp.mp_SigTask = mctask;
    port.mp.mp_MsgList.lh_Head = (struct Node *)&port.mp.mp_MsgList.lh_Tail;
    port.mp.mp_MsgList.lh_TailPred = (struct Node *)&port.mp.mp_MsgList;
    port.rec = R;
    AddPort(&port.mp);
    printf("rec=%08lx\n", (unsigned long)R);
    fflush(stdout);

    Forbid();
    mc_orig_alloc = (ULONG)SetFunction((struct Library *)SysBase, -198,
        (APTR)mc_allocmem);
    mc_orig_free = (ULONG)SetFunction((struct Library *)SysBase, -210,
        (APTR)mc_freemem);
    Permit();

    for (;;) {
        ULONG got = Wait(SIGBREAKF_CTRL_C | (1UL << dsig));
        if ((got & (1UL << dsig)) && !printed) {
            print_rec(R);
            fflush(stdout);
            printed = 1;
        }
        if (got & SIGBREAKF_CTRL_C) {
            if (unpatch())
                break;
            printf("memcheck: AllocMem/FreeMem were patched again after"
                " memcheck; it stays\n");
            fflush(stdout);
        }
    }
    RemPort(&port.mp);
    while (mc_busy)
        Delay(2);
    Delay(10);                      /* a task may be past the counter */
    if (!printed)
        print_rec(R);
    FreeMem(R, sizeof(*R));
    FreeSignal(dsig);
    FreeSignal(sig);
    return 0;
}

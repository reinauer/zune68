/*
 * failn: fault injection. The Nth resource call that library code makes
 * for a program fails, so that every failure path a program's start-up
 * and shut-down can take is run once.
 *
 *   failn PROG N [ARGS]           run PROG (in this process) with the Nth
 *                                 counted call failing; N = 0 fails
 *                                 nothing and prints the count
 *   failn sweep N0 N1 PROG [ARGS] run PROG once for each N from N0 to N1
 *                                 (N1 = 0 or above the count: up to the
 *                                 count of a first run that fails
 *                                 nothing), its output to NIL:, and print
 *                                 one line per run
 *
 * The hooked functions are listed in hooks[] below; a failed call returns
 * NULL (AllocSignal -1). A call counts when it is made on this task and
 * the first return address on the stack that lies in RAM is neither
 * PROG's code nor failn's: the call comes from a disk-loaded library
 * (muimaster.library, an MCC, an OS library from LIBS:) or from ROM code
 * such a library called. OpenLibrary from ROM code (dos.library opening a
 * handler's library) is not counted.
 *
 * Each sweep line: "N=<n> rc=<rc> lost=<bytes>" and, when the call was
 * reached, the function, the return address of the failed call and up to
 * six library return addresses found on the stack, each with the
 * library whose jump table points nearest below it ("lib-LVO+offset").
 * lost is the free memory before the run minus after it, both measured
 * after asking exec to expunge every library no one has open: a positive
 * figure is memory the failure path kept. "mui126=" is the address
 * muimaster.library's LVO -126 (MUI_Layout) jumps to: with the build's
 * link map, tests/native/failsym.py turns the addresses into functions.
 * A line "N=<n> start" is written before each run, so a run that crashes
 * the machine is known by the last such line.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s tests/native/failn.c \
 *        tests/native/failn_stubs.s -o failn
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Not NewObjectA: it fails only when rootclass cannot allocate the
 * object (AllocMem, hooked), after the classes' OM_NEW ran and could
 * dispose the objects in the tag list. Failing it at the call skips
 * them, a state no program meets. */
struct hook { const char *lib; WORD lvo; LONG fail; const char *name; };
static const struct hook hooks[] = {
    { "exec.library",      -198,  0, "AllocMem" },
    { "exec.library",      -684,  0, "AllocVec" },
    { "exec.library",      -708,  0, "AllocPooled" },
    { "exec.library",      -696,  0, "CreatePool" },
    { "exec.library",      -666,  0, "CreateMsgPort" },
    { "exec.library",      -330, -1, "AllocSignal" },
    { "exec.library",      -552,  0, "OpenLibrary" },
    { "graphics.library",  -918,  0, "AllocBitMap" },
    { "graphics.library",  -492,  0, "AllocRaster" },
    { "graphics.library",   -72,  0, "OpenFont" },
    { "graphics.library",  -516,  0, "NewRegion" },
    { "intuition.library", -606,  0, "OpenWindowTagList" },
    { "intuition.library", -204,  0, "OpenWindow" },
    { "intuition.library", -678,  0, "MakeClass" },
    { "dos.library",       -150,  0, "LoadSeg" },
};
#define NHOOK (sizeof(hooks) / sizeof(hooks[0]))
#define LVO_OPENLIBRARY 6

extern ULONG fn_stubs[16], fn_orig[16], fn_fail[16];

#define MAXR 16
struct ranges { ULONG lo[MAXR], hi[MAXR]; int n; };
static struct ranges prog, self;
static struct Task *target;
static volatile int armed;
static ULONG counts[NHOOK], total, failat;
static LONG failed_idx;
/* The record of the failed call, for a run that crashes the machine:
 * failn prints its address first ("rec=<address>"), for a debugger to
 * read: magic, N, function index, return address, callers. */
static volatile ULONG *REC;
#define RECMAGIC 0xFA11ED00
static int rec_ok;
static ULONG failed_ret, failed_at[6];

static void seg_ranges(BPTR seg, struct ranges *r)
{
    r->n = 0;
    while (seg && r->n < MAXR) {
        ULONG *s = (ULONG *)BADDR(seg);
        r->lo[r->n] = (ULONG)(s + 1);
        r->hi[r->n] = (ULONG)s + s[-1] - 4;
        r->n++;
        seg = (BPTR)s[0];
    }
}

static int in_ranges(ULONG a, const struct ranges *r)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (a >= r->lo[i] && a < r->hi[i])
            return 1;
    return 0;
}

static int in_rom(ULONG a)
{
    return (a >= 0xF80000 && a < 0x1000000) || (a >= 0xE00000 && a < 0xE80000);
}

static int in_ram(ULONG a)
{
    struct MemHeader *mh;

    if (a & 1)
        return 0;
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head;
         mh->mh_Node.ln_Succ; mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
        if (a >= (ULONG)mh->mh_Lower + 8 && a < (ULONG)mh->mh_Upper)
            return 1;
    return 0;
}

/* Is the word sequence before A a subroutine call, so that A can be a
 * return address? */
static int after_call(ULONG a)
{
    UWORD w2 = *(UWORD *)(a - 2), w4 = *(UWORD *)(a - 4),
          w6 = *(UWORD *)(a - 6);

    if ((w2 & 0xFFF8) == 0x4E90)                        /* jsr (An) */
        return 1;
    if ((w2 & 0xFF00) == 0x6100 && (w2 & 0xFF) && (w2 & 0xFF) != 0xFF)
        return 1;                                       /* bsr.b */
    if ((w4 & 0xFFF0) == 0x4EA0 && (w4 & 0xF) >= 8)     /* jsr d16(An), d8(An,Xn) */
        return 1;
    if (w4 == 0x4EB8 || w4 == 0x4EBA || w4 == 0x4EBB || w4 == 0x6100)
        return 1;                                       /* jsr abs.w, (pc); bsr.w */
    if (w6 == 0x4EB9 || w6 == 0x61FF)                   /* jsr abs.l; bsr.l */
        return 1;
    return 0;
}

static ULONG *stack_top(ULONG *sp)
{
    struct Task *t = SysBase->ThisTask;

    if ((APTR)sp >= t->tc_SPLower && (APTR)sp < t->tc_SPUpper)
        return (ULONG *)t->tc_SPUpper;
    return sp + 256;
}

/* A library return address: in RAM, after a call, not PROG's or ours. */
static int lib_return(ULONG a)
{
    return in_ram(a) && after_call(a) && !in_ranges(a, &prog)
        && !in_ranges(a, &self);
}

/* Called by the stubs; nonzero fails the call. */
LONG fn_decide(ULONG idx, ULONG *saved)
{
    ULONG ret = saved[6], *sp = saved + 7, *top, i, first = 0;
    int k;

    if (!armed || SysBase->ThisTask != target || idx >= NHOOK)
        return 0;
    if (in_ranges(ret, &prog) || in_ranges(ret, &self))
        return 0;
    if (in_ram(ret))
        first = ret;
    else {
        if (idx == LVO_OPENLIBRARY && in_rom(ret))
            return 0;
        top = stack_top(sp);
        for (i = 0; sp + i < top && i < 512; i++)
            if (in_ram(sp[i]) && after_call(sp[i])) {
                first = sp[i];
                break;
            }
        if (!first || in_ranges(first, &prog) || in_ranges(first, &self))
            return 0;
    }
    counts[idx]++;
    if (++total != failat)
        return 0;
    failed_idx = idx;
    failed_ret = ret;
    k = 0;
    if (lib_return(ret))
        failed_at[k++] = ret;
    top = stack_top(sp);
    for (i = 0; sp + i < top && i < 1024 && k < 6; i++)
        if (lib_return(sp[i]) && (k == 0 || sp[i] != failed_at[k - 1]))
            failed_at[k++] = sp[i];
    if (rec_ok) {
        REC[1] = total;
        REC[2] = idx;
        REC[3] = ret;
        for (k = 0; k < 6; k++)
            REC[4 + k] = failed_at[k];
        REC[0] = RECMAGIC;
    }
    return 1;
}

/* The library whose jump table points nearest below A. */
static void owner(ULONG a, char *buf)
{
    struct Node *n;
    struct Library *best_lib = NULL;
    ULONG best = 0x40000, best_lvo = 0, k;

    Forbid();
    for (n = SysBase->LibList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct Library *l = (struct Library *)n;
        for (k = 1; k <= l->lib_NegSize / 6U && k < 500; k++) {
            UBYTE *v = (UBYTE *)l - 6 * k;
            ULONG t;
            if (*(UWORD *)v != 0x4EF9)
                continue;
            t = *(ULONG *)(v + 2);
            if (t <= a && a - t < best) {
                best = a - t;
                best_lvo = k;
                best_lib = l;
            }
        }
    }
    if (best_lib && best_lib->lib_Node.ln_Name)
        sprintf(buf, " %08lx=%s-%ld+%lx", (unsigned long)a,
            best_lib->lib_Node.ln_Name, (long)(6 * best_lvo),
            (unsigned long)best);
    else
        sprintf(buf, " %08lx=?", (unsigned long)a);
    Permit();
}

static void out(const char *s)
{
    Write(Output(), (APTR)s, strlen(s));
}

/* Expunge until nothing more goes: a library expunged in one pass can
 * close one that exec's list holds before it, which goes in the next. */
static void flush_libs(void)
{
    ULONG last, now = AvailMem(MEMF_ANY);
    APTR m;
    int i = 0;

    do {
        last = now;
        if ((m = AllocMem(0x7FFFFFF0, MEMF_PUBLIC)))
            FreeMem(m, 0x7FFFFFF0);
        now = AvailMem(MEMF_ANY);
    } while (now != last && ++i < 8);
}

static ULONG mui126(void)
{
    struct Library *l;
    ULONG t = 0;

    Forbid();
    l = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
    if (l && *(UWORD *)((UBYTE *)l - 126) == 0x4EF9)
        t = *(ULONG *)((UBYTE *)l - 124);
    Permit();
    return t;
}

static char args[512];
static const char *progname;

/* Run PROG once with call N failing; returns its return code, -1 when it
 * cannot be loaded. */
static LONG run(ULONG n, BPTR redirect)
{
    struct Process *pr = (struct Process *)FindTask(NULL);
    BPTR seg, oldout = 0, home = 0, oldhome;
    char dir[256];
    LONG rc;
    int i;

    if (!(seg = LoadSeg((STRPTR)progname)))
        return -1;
    seg_ranges(seg, &prog);
    strncpy(dir, progname, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = 0;
    *(PathPart((STRPTR)dir)) = 0;
    home = Lock((STRPTR)(dir[0] ? dir : ""), SHARED_LOCK);
    oldhome = pr->pr_HomeDir;
    pr->pr_HomeDir = home;
    SetProgramName((STRPTR)progname);
    if (redirect)
        oldout = SelectOutput(redirect);
    for (i = 0; i < (int)NHOOK; i++)
        counts[i] = 0;
    total = 0;
    failat = n;
    failed_idx = -1;
    memset(failed_at, 0, sizeof(failed_at));
    armed = 1;
    rc = RunCommand(seg, 65536, (STRPTR)args, strlen(args));
    armed = 0;
    if (redirect)
        SelectOutput(oldout);
    pr->pr_HomeDir = oldhome;
    if (home)
        UnLock(home);
    UnLoadSeg(seg);
    return rc;
}

/* What the run failed, with the callers' owners: made right after the
 * run, while the libraries it used are still in memory. */
static char desc[640];

static void describe(ULONG n)
{
    char part[96];
    int k;

    desc[0] = 0;
    if (failed_idx >= 0) {
        sprintf(part, " failed=%s ret=%08lx", hooks[failed_idx].name,
            (unsigned long)failed_ret);
        strcat(desc, part);
        for (k = 0; k < 6 && failed_at[k]; k++) {
            owner(failed_at[k], part);
            strcat(desc, part);
        }
    } else if (n)
        strcat(desc, " not-reached");
    else {
        sprintf(part, " count=%lu", (unsigned long)total);
        strcat(desc, part);
        for (k = 0; k < (int)NHOOK; k++)
            if (counts[k]) {
                sprintf(part, " %s=%lu", hooks[k].name,
                    (unsigned long)counts[k]);
                strcat(desc, part);
            }
    }
    sprintf(part, " mui126=%08lx\n", (unsigned long)mui126());
    strcat(desc, part);
}

static void report(ULONG n, LONG rc, LONG lost)
{
    char line[64];

    sprintf(line, "N=%lu rc=%ld lost=%ld", (unsigned long)n, (long)rc,
        (long)lost);
    out(line);
    out(desc);
}

int main(int argc, char **argv)
{
    struct Process *pr = (struct Process *)FindTask(NULL);
    struct Library *bases[NHOOK];
    ULONG n0, n1, n, count;
    BPTR nil = 0;
    int sweep, first, i;
    char line[64];

    sweep = argc >= 5 && !strcmp(argv[1], "sweep");
    if (!sweep && argc < 3) {
        out("failn PROG N [ARGS] | failn sweep N0 N1 PROG [ARGS]\n");
        return 5;
    }
    if (sweep) {
        n0 = strtoul(argv[2], NULL, 0);
        n1 = strtoul(argv[3], NULL, 0);
        progname = argv[4];
        first = 5;
    } else {
        n0 = n1 = strtoul(argv[2], NULL, 0);
        progname = argv[1];
        first = 3;
    }
    args[0] = 0;
    for (i = first; i < argc; i++) {
        if (strlen(args) + strlen(argv[i]) + 3 >= sizeof(args))
            break;
        strcat(args, argv[i]);
        if (i + 1 < argc)
            strcat(args, " ");
    }
    strcat(args, "\n");
    if (pr->pr_CLI)
        seg_ranges(((struct CommandLineInterface *)BADDR(pr->pr_CLI))->cli_Module,
            &self);
    target = FindTask(NULL);
    REC = AllocMem(64, MEMF_PUBLIC | MEMF_CLEAR);
    rec_ok = REC != NULL;
    sprintf(line, "rec=%08lx\n", (unsigned long)REC);
    out(line);

    for (i = 0; i < (int)NHOOK; i++) {
        bases[i] = OpenLibrary((STRPTR)hooks[i].lib, 0);
        fn_fail[i] = hooks[i].fail;
    }
    Forbid();
    for (i = 0; i < (int)NHOOK; i++)
        if (bases[i])
            fn_orig[i] = (ULONG)SetFunction(bases[i], hooks[i].lvo,
                (APTR)fn_stubs[i]);
    Permit();

    if (sweep) {
        nil = Open((STRPTR)"NIL:", MODE_NEWFILE);
        /* a first run that fails nothing: the count, and the fonts and
         * other caches a first start loads stay out of the figures */
        flush_libs();
        run(0, nil);
        count = total;
        sprintf(line, "count=%lu\n", (unsigned long)count);
        out(line);
        if (n1 == 0 || n1 > count)
            n1 = count;
        for (n = n0; n <= n1; n++) {
            ULONG before, after;
            LONG rc;
            sprintf(line, "N=%lu start\n", (unsigned long)n);
            out(line);
            flush_libs();
            before = AvailMem(MEMF_ANY);
            rc = run(n, nil);
            describe(n);
            flush_libs();
            after = AvailMem(MEMF_ANY);
            report(n, rc, (LONG)(before - after));
        }
        Close(nil);
    } else {
        ULONG before, after;
        LONG rc;
        flush_libs();
        before = AvailMem(MEMF_ANY);
        rc = run(n0, 0);
        describe(n0);
        flush_libs();
        after = AvailMem(MEMF_ANY);
        report(n0, rc, (LONG)(before - after));
    }

    Forbid();
    for (i = 0; i < (int)NHOOK; i++)
        if (bases[i])
            SetFunction(bases[i], hooks[i].lvo, (APTR)fn_orig[i]);
    Permit();
    for (i = 0; i < (int)NHOOK; i++)
        if (bases[i])
            CloseLibrary(bases[i]);
    if (rec_ok) {
        REC[0] = 0;
        FreeMem((APTR)REC, 64);
    }
    return 0;
}

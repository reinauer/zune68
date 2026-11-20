/*
 * memtally: who holds a running program's memory.
 *
 *   memtally SECONDS PROG [ARGS]
 *
 * Starts PROG as a process of its own, records every block exec's
 * AllocMem hands to that process (AllocVec, pools and the OS functions it
 * calls allocate through AllocMem too) with the two first library return
 * addresses on the stack, forgets a block when anyone frees it, and after
 * SECONDS prints the blocks the program still holds, one per line:
 *
 *   <address> <size> <caller> <caller>
 *
 * A caller is the first return address on the stack that lies in RAM and
 * is not PROG's (0: the program's own call, or none found). The last line
 * is "mui126=<address>", where muimaster.library's LVO -126 jumps:
 * tests/native/memreport.py groups the blocks by muimaster function with
 * the build's link map. PROG keeps running; memtally then removes its
 * patches.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s tests/native/memtally.c \
 *        tests/native/memtally_stubs.s -o memtally
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ULONG mt_orig_alloc, mt_orig_free;
void mt_allocmem(void);
void mt_freemem(void);

#define SLOTS 16384                 /* a power of two */
struct block { ULONG addr, size, c1, c2; };
static struct block *table;
static ULONG live, dropped;
static struct Task *volatile target;
static ULONG plo[16], phi[16];
static int np;

static int in_prog(ULONG a)
{
    int i;
    for (i = 0; i < np; i++)
        if (a >= plo[i] && a < phi[i])
            return 1;
    return 0;
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

static ULONG slot(ULONG addr)
{
    return ((addr >> 3) * 2654435761UL) >> 18 & (SLOTS - 1);
}

void mt_alloc(ULONG mem, ULONG size, ULONG *sp)
{
    struct Task *t = SysBase->ThisTask;
    ULONG c[2] = { 0, 0 }, i, s, *top;
    int k = 0;

    if (!mem || t != target)
        return;
    top = (APTR)sp >= t->tc_SPLower && (APTR)sp < t->tc_SPUpper
        ? (ULONG *)t->tc_SPUpper : sp + 256;
    for (i = 0; sp + i < top && i < 768 && k < 2; i++) {
        ULONG a = sp[i];
        if (!in_ram(a) || !after_call(a))
            continue;
        if (in_prog(a)) {
            if (k == 0)
                break;              /* the program's own call */
            continue;
        }
        if (k == 0 || a != c[0])
            c[k++] = a;
    }
    Disable();
    for (s = slot(mem), i = 0; i < SLOTS; i++, s = (s + 1) & (SLOTS - 1))
        if (table[s].addr == 0 || table[s].addr == ~0UL) {
            table[s].addr = mem;
            table[s].size = size;
            table[s].c1 = c[0];
            table[s].c2 = c[1];
            live++;
            break;
        }
    if (i == SLOTS)
        dropped++;
    Enable();
}

void mt_free(ULONG mem, ULONG size)
{
    ULONG s, i;

    (void)size;
    if (!mem || !live)
        return;
    Disable();
    for (s = slot(mem), i = 0; i < SLOTS && table[s].addr;
         i++, s = (s + 1) & (SLOTS - 1))
        if (table[s].addr == mem) {
            table[s].addr = ~0UL;   /* a tombstone keeps later slots found */
            live--;
            break;
        }
    Enable();
}

int main(int argc, char **argv)
{
    static char args[512];
    struct Library *mb;
    struct Process *pr;
    BPTR seg, s;
    ULONG mui126 = 0, total = 0, n = 0, i;
    int a;

    if (argc < 3) {
        printf("memtally SECONDS PROG [ARGS]\n");
        return 5;
    }
    if (!(table = AllocVec(SLOTS * sizeof(*table), MEMF_CLEAR))) {
        printf("memtally: no memory for the table\n");
        return 20;
    }
    if (!(seg = LoadSeg((STRPTR)argv[2]))) {
        printf("memtally: cannot load %s\n", argv[2]);
        FreeVec(table);
        return 20;
    }
    for (s = seg; s && np < 16; s = *(BPTR *)BADDR(s)) {
        ULONG *p = (ULONG *)BADDR(s);
        plo[np] = (ULONG)(p + 1);
        phi[np++] = (ULONG)p + p[-1] - 4;
    }
    args[0] = 0;
    for (a = 3; a < argc && strlen(args) + strlen(argv[a]) + 3 < sizeof(args); a++) {
        strcat(args, argv[a]);
        strcat(args, " ");
    }
    strcat(args, "\n");

    Forbid();
    mt_orig_alloc = (ULONG)SetFunction((struct Library *)SysBase, -198,
        (APTR)mt_allocmem);
    mt_orig_free = (ULONG)SetFunction((struct Library *)SysBase, -210,
        (APTR)mt_freemem);
    pr = CreateNewProcTags(NP_Seglist, (ULONG)seg, NP_FreeSeglist, TRUE,
        NP_Name, (ULONG)FilePart((STRPTR)argv[2]), NP_StackSize, 65536,
        NP_Cli, TRUE, NP_CommandName, (ULONG)argv[2],
        NP_Arguments, (ULONG)args, NP_Input, (ULONG)Open((STRPTR)"NIL:", MODE_OLDFILE),
        NP_Output, (ULONG)Open((STRPTR)"NIL:", MODE_NEWFILE), TAG_DONE);
    target = (struct Task *)pr;
    Permit();
    if (!pr) {
        printf("memtally: cannot start %s\n", argv[2]);
        UnLoadSeg(seg);
    } else
        Delay(50 * atol(argv[1]));

    target = NULL;
    Forbid();
    SetFunction((struct Library *)SysBase, -198, (APTR)mt_orig_alloc);
    SetFunction((struct Library *)SysBase, -210, (APTR)mt_orig_free);
    mb = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
    if (mb && *(UWORD *)((UBYTE *)mb - 126) == 0x4EF9)
        mui126 = *(ULONG *)((UBYTE *)mb - 124);
    Permit();
    for (i = 0; i < SLOTS; i++)
        if (table[i].addr && table[i].addr != ~0UL) {
            printf("%08lx %lu %08lx %08lx\n", (unsigned long)table[i].addr,
                (unsigned long)table[i].size, (unsigned long)table[i].c1,
                (unsigned long)table[i].c2);
            total += table[i].size;
            n++;
        }
    printf("blocks %lu bytes %lu dropped %lu\n", (unsigned long)n,
        (unsigned long)total, (unsigned long)dropped);
    printf("mui126=%08lx\n", (unsigned long)mui126);
    Delay(50);                      /* let a task leave the stubs */
    FreeVec(table);
    return 0;
}

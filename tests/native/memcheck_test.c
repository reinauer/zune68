/*
 * memcheck_test: damage the heap on purpose, for memcheck to report.
 *
 *   memcheck_test twice   free a block twice
 *   memcheck_test over    write 8 bytes past a block into the header of
 *                         the free chunk behind it, then call AllocMem
 *                         (memcheck drops that call), then repair the
 *                         header
 *
 * Prints the blocks and the address ranges of the functions making each
 * call, to compare with memcheck's return addresses. Run it only while
 * memcheck runs: otherwise exec gets the double free and the broken
 * free chunk.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s tests/native/memcheck_test.c \
 *        -o memcheck_test
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#define NOINLINE __attribute__((noinline))

NOINLINE APTR alloc_block(ULONG size) { return AllocMem(size, MEMF_FAST); }
NOINLINE void free_once(APTR p, ULONG size) { FreeMem(p, size); }
NOINLINE void free_again(APTR p, ULONG size) { FreeMem(p, size); }
NOINLINE APTR next_call(void) { return AllocMem(8, MEMF_ANY); }
NOINLINE void end_of_calls(void) { }

static void show(const char *name, void (*f)(void), void (*g)(void))
{
    printf("%-11s %08lx-%08lx\n", name, (unsigned long)f, (unsigned long)g);
}

/* Is there a free chunk right at a? Caller holds Forbid. */
static int free_at(ULONG a)
{
    struct MemHeader *mh;
    struct MemChunk *c;

    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head;
         mh->mh_Node.ln_Succ; mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
        for (c = mh->mh_First; c; c = c->mc_Next)
            if ((ULONG)c == a)
                return 1;
    return 0;
}

int main(int argc, char **argv)
{
    show("alloc_block", (void (*)(void))alloc_block, (void (*)(void))free_once);
    show("free_once", (void (*)(void))free_once, (void (*)(void))free_again);
    show("free_again", (void (*)(void))free_again, (void (*)(void))next_call);
    show("next_call", (void (*)(void))next_call, end_of_calls);

    if (argc >= 2 && !strcmp(argv[1], "twice")) {
        APTR p;

        Forbid();
        p = alloc_block(64);
        free_once(p, 64);
        free_again(p, 64);
        Permit();
        printf("block %08lx freed twice\n", (unsigned long)p);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "over")) {
        APTR held[8], p = NULL, q;
        ULONG save[2], size = 64;
        int n = 0, i;

        Forbid();
        while (n < 8) {
            if (!(p = alloc_block(size)))
                break;
            if (free_at((ULONG)p + size))
                break;
            held[n++] = p;          /* no free chunk behind: try again */
            p = NULL;
        }
        if (!p) {
            Permit();
            printf("no block with a free chunk behind it\n");
            return 10;
        }
        memcpy(save, (UBYTE *)p + size, 8);
        memcpy((UBYTE *)p + size, "OVERRUN!", 8);
        q = next_call();
        memcpy((UBYTE *)p + size, save, 8);
        Permit();
        printf("block %08lx size %lu overrun into %08lx; next AllocMem"
            " gave %08lx\n", (unsigned long)p, (unsigned long)size,
            (unsigned long)p + size, (unsigned long)q);
        if (q)
            FreeMem(q, 8);
        FreeMem(p, size);
        for (i = 0; i < n; i++)
            FreeMem(held[i], size);
        return 0;
    }
    printf("usage: memcheck_test twice|over\n");
    return 5;
}

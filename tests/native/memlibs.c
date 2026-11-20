/*
 * memlibs: the libraries, devices and resources in memory, one per line
 * (name, version, the size of base plus jump table), and the free memory
 * after an expunge, to see what a program pulled in.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s tests/native/memlibs.c -o memlibs
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <stdio.h>

static void list(const char *what, struct List *l)
{
    struct Node *n;
    char buf[2048];
    int len = 0;

    Forbid();
    for (n = l->lh_Head; n->ln_Succ && len < 1900; n = n->ln_Succ) {
        struct Library *lib = (struct Library *)n;
        len += sprintf(buf + len, "%s %s %d.%d %d\n", what,
            n->ln_Name ? n->ln_Name : "?", lib->lib_Version, lib->lib_Revision,
            lib->lib_NegSize + lib->lib_PosSize);
    }
    Permit();
    fputs(buf, stdout);
}

int main(void)
{
    APTR m;

    if ((m = AllocMem(0x7FFFFFF0, MEMF_PUBLIC)))
        FreeMem(m, 0x7FFFFFF0);
    list("lib", &SysBase->LibList);
    list("dev", &SysBase->DeviceList);
    printf("free %lu\n", (unsigned long)AvailMem(MEMF_ANY));
    return 0;
}

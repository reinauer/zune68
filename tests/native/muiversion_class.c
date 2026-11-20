/*
 * muiversion_class: does a class library loaded for this program get the
 * muimaster.library base this program got?
 *
 *   muiversion_class CLASS
 *
 * Opens muimaster.library (version 0), makes an object of CLASS (an MCC
 * not yet in memory, e.g. NList.mcc) and prints the base's version and
 * how many longwords in RAM hold the base's address before and after the
 * class was loaded: a class that stored this base adds one.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s -Ibuild/muimaster/include \
 *        tests/native/muiversion_class.c -o muiversion_class -lamiga
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <stdio.h>

struct Library *MUIMasterBase;

static ULONG count(ULONG value)
{
    struct MemHeader *mh;
    ULONG n = 0, *p;

    Forbid();
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head;
         mh->mh_Node.ln_Succ; mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
        for (p = (ULONG *)(((ULONG)mh->mh_Lower + 3) & ~3);
             p < (ULONG *)mh->mh_Upper; p++)
            if (*p == value && p != (ULONG *)&MUIMasterBase)
                n++;
    Permit();
    return n;
}

int main(int argc, char **argv)
{
    ULONG before, after;
    Object *o;

    if (argc < 2 || !(MUIMasterBase = OpenLibrary((STRPTR)"muimaster.library", 0)))
        return 20;
    before = count((ULONG)MUIMasterBase);
    o = MUI_NewObject((STRPTR)argv[1], TAG_DONE);
    after = count((ULONG)MUIMasterBase);
    printf("base %d.%d: %s %s, base address held %lu times before, %lu after\n",
        MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision, argv[1],
        o ? "made" : "not made", (unsigned long)before, (unsigned long)after);
    if (o)
        MUI_DisposeObject(o);
    CloseLibrary(MUIMasterBase);
    return 0;
}

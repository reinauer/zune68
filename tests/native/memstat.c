/*
 * memstat: free memory after asking exec to expunge every library, device
 * and font no one has open (a failing AllocMem of 2 GB does that).
 *
 *   memstat LABEL         print "LABEL total <bytes> chip <bytes> fast <bytes>"
 *   memstat hold SECONDS  open muimaster.library, keep it open for SECONDS
 *                         and close it (for the library's own figure: run it
 *                         in the background and call memstat meanwhile)
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s tests/native/memstat.c -o memstat
 */
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void flush(void)
{
    APTR m;
    int i;

    for (i = 0; i < 3; i++)
        if ((m = AllocMem(0x7FFFFFF0, MEMF_PUBLIC)))
            FreeMem(m, 0x7FFFFFF0);
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "hold")) {
        struct Library *mb = OpenLibrary((STRPTR)"muimaster.library", 0);
        if (!mb)
            return 20;
        Delay(50 * atol(argv[2]));
        CloseLibrary(mb);
        return 0;
    }
    flush();
    printf("%s total %lu chip %lu fast %lu\n", argc > 1 ? argv[1] : "free",
        (unsigned long)AvailMem(MEMF_ANY), (unsigned long)AvailMem(MEMF_CHIP),
        (unsigned long)AvailMem(MEMF_FAST));
    return 0;
}

/*
 * muiversion: the MUI version a program sees. Prints the library base's
 * version, revision and $VER string, then MUIA_Version and MUIA_Revision
 * of an object of each of a few built-in classes, and again for a second
 * library open (same program).
 *
 *   muiversion [VERSION]   opens muimaster.library with VERSION (19)
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s -Ibuild/muimaster/include \
 *        tests/native/muiversion.c -o muiversion -lamiga
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *MUIMasterBase;

static void classver(const char *cl)
{
    Object *o = MUI_NewObject((CONST_STRPTR)cl, TAG_DONE);
    ULONG v = 0, r = 0;

    if (!o) { printf("  %s: no object\n", cl); return; }
    get(o, MUIA_Version, &v);
    get(o, MUIA_Revision, &r);
    printf("  %s %lu.%lu\n", cl, (unsigned long)v, (unsigned long)r);
    MUI_DisposeObject(o);
}

int main(int argc, char **argv)
{
    struct Library *second;
    char id[80];
    int n;

    if (!(MUIMasterBase = OpenLibrary((STRPTR)"muimaster.library",
            argc > 1 ? atol(argv[1]) : 19))) {
        printf("open failed\n");
        return 20;
    }
    strncpy(id, MUIMasterBase->lib_IdString ? (char *)MUIMasterBase->lib_IdString : "", 79);
    id[79] = 0;
    n = strlen(id);
    while (n && (id[n - 1] == '\n' || id[n - 1] == '\r'))
        id[--n] = 0;
    printf("base %d.%d \"%s\"\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision, id);
    classver(MUIC_Notify);
    classver(MUIC_Area);
    classver(MUIC_Group);
    classver(MUIC_Text);
    classver(MUIC_Window);
    classver(MUIC_Application);
    if ((second = OpenLibrary((STRPTR)"muimaster.library", 0))) {
        printf("second open %d.%d, %s base\n", second->lib_Version,
            second->lib_Revision, second == MUIMasterBase ? "same" : "another");
        CloseLibrary(second);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}

/* MUIM_GetConfigItem results for the string colour items Textinput.mcc
 * reads (0x84-0x87) and a few neighbours, printed for comparing native
 * and MUI 3.9 answers. "cfgitems <base> <firstid> <count>" asks for
 * other items as application <base> (TearOffPanel reads 0xfa34ffc0..).
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit cfgitems.c -o cfgitems -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#ifndef MUIM_GetConfigItem
#define MUIM_GetConfigItem 0x80423edb
#endif
struct Library *MUIMasterBase;
ULONG __stack = 65536;
int main(int argc, char **argv)
{
    Object *app, *win, *rect;
    ULONG open = 0, id, first = 0x80, last = 0x8f;
    STRPTR base = "cfgitems";
    if (argc > 3)
    {
        base = argv[1];
        first = strtoul(argv[2], NULL, 16);
        last = first + strtoul(argv[3], NULL, 10) - 1;
    }
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "cfgitems",
        MUIA_Application_Base, base,
        SubWindow, win = WindowObject, MUIA_Window_Title, "cfgitems",
            WindowContents, rect = RectangleObject, End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    for (id = first; id <= last; id++)
    {
        ULONG storage = 0, ok = DoMethod(rect, MUIM_GetConfigItem, id, (ULONG)&storage);
        printf("%08lx %lu", id, ok);
        if (ok && storage)
            printf(" %08lx \"%.40s\" first long %08lx", storage, (char *)storage, *(ULONG *)storage);
        printf("\n");
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

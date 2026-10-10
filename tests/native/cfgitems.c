/* MUIM_GetConfigItem results for the string colour items Textinput.mcc
 * reads (0x84-0x87) and a few neighbours, printed for comparing native
 * and MUI 3.9 answers.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit cfgitems.c -o cfgitems -lamiga
 */
#include <stdio.h>
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
int main(void)
{
    Object *app, *win, *rect;
    ULONG open = 0, id;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "cfgitems",
        SubWindow, win = WindowObject, MUIA_Window_Title, "cfgitems",
            WindowContents, rect = RectangleObject, End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    for (id = 0x80; id <= 0x8f; id++)
    {
        ULONG storage = 0, ok = DoMethod(rect, MUIM_GetConfigItem, id, (ULONG)&storage);
        printf("%02lx %lu", id, ok);
        if (ok && storage)
            printf(" %08lx \"%.40s\" first long %08lx", storage, (char *)storage, *(ULONG *)storage);
        printf("\n");
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

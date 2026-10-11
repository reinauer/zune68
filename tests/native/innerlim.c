/* MUIA_InnerTop/InnerLeft beyond the usual range (to 300 and -1): the
 * value MUI keeps and the minimum size it gives a Rectangle. Writes
 * innerlim.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit innerlim.c -o innerlim -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
int main(void)
{
    static const LONG v[] = { 31, 32, 33, 50, 100, 127, 128, 200, 255, 256, 300, -1 };
    int i;
    freopen("innerlim.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    for (i = 0; i < (int)(sizeof(v) / sizeof(v[0])); i++)
    {
        Object *app, *win, *r;
        ULONG open = FALSE, it = 0;
        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, r = RectangleObject, MUIA_InnerTop, v[i], MUIA_InnerLeft, v[i] / 4, End,
            End, End;
        if (!app) return 20;
        get(r, MUIA_InnerTop, &it);
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        printf("inner %ld get %ld open %lu min %ld,%ld\n", (long)v[i], (long)it, open,
            (long)_minwidth(r), (long)_minheight(r));
        set(win, MUIA_Window_Open, FALSE);
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}

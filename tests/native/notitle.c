/* Windows without a title, with a title and with an empty one: their
 * size, borders, flags (RMBTRAP, VISITOR) and object boxes; title gadgets
 * are read from the guest while the windows stay open for 20 seconds.
 * Writes notitle.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit notitle.c -o notitle -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
int main(void)
{
    Object *app, *win[3], *r[3];
    struct Window *w; ULONG open; int i;
    freopen("notitle.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject,
        SubWindow, win[0] = WindowObject, MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 250,
            WindowContents, r[0] = RectangleObject, MUIA_FixWidth, 20, MUIA_FixHeight, 20, End, End,
        SubWindow, win[1] = WindowObject, MUIA_Window_Title, "T", MUIA_Window_LeftEdge, 120, MUIA_Window_TopEdge, 250,
            WindowContents, r[1] = RectangleObject, MUIA_FixWidth, 20, MUIA_FixHeight, 20, End, End,
        SubWindow, win[2] = WindowObject, MUIA_Window_Title, "", MUIA_Window_LeftEdge, 220, MUIA_Window_TopEdge, 250,
            WindowContents, r[2] = RectangleObject, MUIA_FixWidth, 20, MUIA_FixHeight, 20, End, End,
        End;
    if (!app) return 20;
    for (i = 0; i < 3; i++)
    {
        set(win[i], MUIA_Window_Open, TRUE);
        open = FALSE; get(win[i], MUIA_Window_Open, &open);
        if (!open) { printf("win%d not open\n", i); continue; }
        get(win[i], MUIA_Window_Window, &w);
        printf("win%d %dx%d borders %d,%d,%d,%d rect %d,%d %dx%d flags %08lx\n", i, w->Width, w->Height,
            w->BorderLeft, w->BorderTop, w->BorderRight, w->BorderBottom,
            _left(r[i]), _top(r[i]), _width(r[i]), _height(r[i]), (unsigned long)w->Flags);
    }
    fflush(stdout);
    Delay(50 * 20);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

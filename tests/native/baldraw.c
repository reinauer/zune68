/* Pixels of a Balance in a horizontal and a vertical group. Writes
 * baldraw.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit baldraw.c -o baldraw -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static void dump(const char *n, Object *o, int w, int h)
{
    LONG x, y;
    printf("%s box %d,%d %dx%d\n", n, _left(o), _top(o), _width(o), _height(o));
    for (y = _top(o) - 1; y < _top(o) + h; y++)
    {
        printf("  ");
        for (x = _left(o) - 1; x < _left(o) + w; x++) printf("%ld", (long)ReadPixel(_rp(o), x, y) & 15);
        printf("\n");
    }
}
int main(void)
{
    Object *app, *win, *bh, *bv;
    ULONG open = FALSE;
    freopen("baldraw.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, VGroup,
            Child, HGroup,
                Child, RectangleObject, MUIA_FixHeight, 20, End,
                Child, bh = BalanceObject, End,
                Child, RectangleObject, End,
                End,
            Child, bv = BalanceObject, End,
            Child, RectangleObject, MUIA_FixHeight, 20, End,
        End, End, End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    if (open) { dump("horiz", bh, 5, 6); dump("vert", bv, 8, 4); }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

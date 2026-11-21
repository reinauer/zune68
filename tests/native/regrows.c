/* A Register with three rows of tabs changes to a page on another row.
 * Compares the register's pixels after that update redraw with those after
 * a full redraw (window closed and opened again); MUI 3.8 leaves none
 * different.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit regrows.c -o regrows -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static UBYTE a[400 * 200], b[400 * 200];
static const char *titles[] = {"One", "Two", "Three", "Four", "Five", "Six", NULL};

static LONG grab(Object *o, UBYTE *buf)
{
    LONG x, y, n = 0, w = _width(o), h = _height(o);
    for (y = 0; y < h && y < 200; y++)
        for (x = 0; x < w && x < 400; x++)
            buf[n++] = ReadPixel(_rp(o), _left(o) + x, _top(o) + y);
    return n;
}

static void cycle(Object *app)
{
    ULONG sigs = 0; int i;
    for (i = 0; i < 5; i++) { DoMethod(app, MUIM_Application_NewInput, &sigs); Delay(5); }
}

int main(void)
{
    Object *app, *win, *reg;
    LONG n, m, diff = 0, i, from, to;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject,
        SubWindow, win = WindowObject, MUIA_Window_Title, "regrows",
            WindowContents, reg = RegisterGroup(titles), MUIA_Register_Columns, 2,
                Child, TextObject, MUIA_Text_Contents, "Page one, quite a long line", End,
                Child, VGroup, Child, TextObject, MUIA_Text_Contents, "Page two", End,
                    Child, TextObject, MUIA_Text_Contents, "second line", End, End,
                Child, TextObject, MUIA_Text_Contents, "Page three", End,
                Child, TextObject, MUIA_Text_Contents, "Page four", End,
                Child, TextObject, MUIA_Text_Contents, "Page five", End,
                Child, TextObject, MUIA_Text_Contents, "Page six", End,
            End, End, End;
    if (!app) { CloseLibrary(MUIMasterBase); return 20; }
    for (from = 0; from < 6; from++)
        for (to = 0; to < 6; to++) {
            if (from / 2 == to / 2) continue;
            set(reg, MUIA_Group_ActivePage, from);
            set(win, MUIA_Window_Open, TRUE); cycle(app);
            set(reg, MUIA_Group_ActivePage, to); cycle(app);
            n = grab(reg, a);
            set(win, MUIA_Window_Open, FALSE); set(win, MUIA_Window_Open, TRUE); cycle(app);
            m = grab(reg, b);
            for (diff = 0, i = 0; i < n && i < m; i++) diff += a[i] != b[i];
            printf("from %ld to %ld: %ld pixels differ\n", from, to, diff);
            set(win, MUIA_Window_Open, FALSE);
        }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

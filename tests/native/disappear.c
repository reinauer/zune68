/* MUIA_HorizDisappear: group sizes and child boxes of an HGroup whose
 * texts may disappear (levels 0, 1, 2 and 1, 1), in windows 100..260
 * pixels wide. Writes disappear.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit disappear.c -o disappear -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#ifndef MUIA_HorizDisappear
#define MUIA_HorizDisappear 0x80429615
#endif

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static Object *text(const char *s, LONG level, LONG weight)
{
    return TextObject, TextFrame, MUIA_Text_Contents, s, MUIA_Text_SetMax, FALSE,
        MUIA_HorizDisappear, level, MUIA_HorizWeight, weight, End;
}

static void box(const char *n, Object *o)
{
    printf("  %-3s box %3d,%d %3dx%d min %d def %d max %d\n", n, _left(o), _top(o),
        _width(o), _height(o), _minwidth(o), _defwidth(o),
        _maxwidth(o) > 9999 ? 9999 : _maxwidth(o));
}

int main(void)
{
    static const int widths[] = { 260, 200, 160, 140, 120, 100 };
    int i;

    freopen("disappear.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (i = 0; i < (int)(sizeof(widths) / sizeof(widths[0])); i++)
    {
        Object *app, *win, *g1, *g2, *a, *b, *c, *d, *e;
        struct Window *w;
        ULONG open = FALSE;

        app = ApplicationObject, SubWindow, win = WindowObject,
            MUIA_Window_Width, widths[i],
            WindowContents, VGroup,
                Child, g1 = HGroup,
                    Child, a = text("Keep this", 0, 100),
                    Child, b = text("Level one", 1, 100),
                    Child, c = text("Level two", 2, 100),
                    End,
                Child, g2 = HGroup,
                    Child, d = text("First", 1, 50),
                    Child, e = text("Second text", 1, 100),
                    End,
                End,
            End, End;
        if (!app)
            return 20;
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        if (open)
        {
            get(win, MUIA_Window_Window, &w);
            printf("window %d (%d)\n", w->Width, widths[i]);
            box("g1", g1); box("a", a); box("b", b); box("c", c);
            box("g2", g2); box("d", d); box("e", e);
        }
        else
            printf("window %d not open\n", widths[i]);
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

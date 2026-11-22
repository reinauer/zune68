/* Fonts and spacing MUI picks when a window does not fit the screen:
 * windows of a growing column of Big-font texts, then of a growing row of
 * Normal-font texts (proportional fonts are narrower than topaz, so the
 * row shows every stage), printing each window's size, spacing and fonts.
 * Run without MUI settings and with Fonts:helvetica installed. Appends to
 * toobig.txt. Compare native and MUI 3.8.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit fontfallback.c -o fontfallback -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static void f(const char *n, Object *o)
{
    printf("  %-6s %s/%d h=%d\n", n, _font(o)->tf_Message.mn_Node.ln_Name,
        _font(o)->tf_YSize, _height(o));
}
int main(int argc, char **argv)
{
    Object *app, *win, *grp, *big, *norm, *tiny, *fixed, *list, *title, *button;
    ULONG open = FALSE; int i, n;
    struct Window *w;
    freopen("toobig.txt", "a", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    for (n = 2; n <= 70; n += n < 30 ? 1 : 4)
    {
        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, grp = VGroup,
                Child, norm = TextObject, MUIA_Text_Contents, "normal", End,
                Child, tiny = TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_Contents, "tiny", End,
                Child, fixed = TextObject, MUIA_Font, MUIV_Font_Fixed, MUIA_Text_Contents, "fixed", End,
                Child, big = TextObject, MUIA_Font, MUIV_Font_Big, MUIA_Text_Contents, "big", End,
                Child, list = TextObject, MUIA_Font, MUIV_Font_List, MUIA_Text_Contents, "list", End,
                Child, title = TextObject, MUIA_Font, MUIV_Font_Title, MUIA_Text_Contents, "title", End,
                Child, button = TextObject, MUIA_Font, MUIV_Font_Button, MUIA_Text_Contents, "button", End,
            End, End, End;
        if (!app) return 20;
        for (i = 1; i < n; i++)
            DoMethod(grp, OM_ADDMEMBER, TextObject, MUIA_Font, MUIV_Font_Big, MUIA_Text_Contents, "big", End);
        set(win, MUIA_Window_Open, TRUE);
        open = FALSE;
        get(win, MUIA_Window_Open, &open);
        if (open)
        {
            get(win, MUIA_Window_Window, &w);
            printf("n=%d window %dx%d group h=%d min=%d at %d,%d gap %d\n", n, w->Width, w->Height,
                _height(grp), _minheight(grp), _left(grp), _top(grp),
                _top(tiny) - _bottom(norm) - 1);
            printf("  insets top %d left %d right %d bottom %d screen %dx%d\n",
                _top(norm) - _top(grp), _left(norm) - _left(grp),
                _right(grp) - _right(norm), _bottom(grp) - _bottom(button),
                _screen(grp)->Width, _screen(grp)->Height);
            f("normal", norm); f("tiny", tiny); f("fixed", fixed); f("big", big);
            f("list", list); f("title", title); f("button", button);
        }
        else
            printf("n=%d not open\n", n);
        fflush(stdout);
        MUI_DisposeObject(app);
    }
    for (n = 4; n <= 22; n++)
    {
        Object *row, *col, *first, *second;
        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, row = HGroup,
                Child, col = VGroup,
                    Child, tiny = TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_Contents, "t", End,
                    Child, fixed = TextObject, MUIA_Font, MUIV_Font_Fixed, MUIA_Text_Contents, "f", End,
                    Child, big = TextObject, MUIA_Font, MUIV_Font_Big, MUIA_Text_Contents, "b", End,
                    Child, title = TextObject, MUIA_Font, MUIV_Font_Title, MUIA_Text_Contents, "T", End,
                End,
                Child, first = TextObject, MUIA_Text_Contents, "abcdefghij", End,
                Child, second = TextObject, MUIA_Text_Contents, "abcdefghij", End,
            End, End, End;
        if (!app) return 20;
        for (i = 2; i < n; i++)
            DoMethod(row, OM_ADDMEMBER, TextObject, MUIA_Text_Contents, "abcdefghij", End);
        set(win, MUIA_Window_Open, TRUE);
        open = FALSE;
        get(win, MUIA_Window_Open, &open);
        if (open)
        {
            get(win, MUIA_Window_Window, &w);
            printf("row n=%d window %dx%d text w=%d gap %d insets left %d top %d\n", n,
                w->Width, w->Height, _width(first), _left(second) - _right(first) - 1,
                _left(col) - _left(row), _top(col) - _top(row));
            f("normal", first); f("tiny", tiny); f("fixed", fixed); f("big", big);
            f("title", title);
        }
        else
            printf("row n=%d not open\n", n);
        fflush(stdout);
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

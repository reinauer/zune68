/* Fonts MUI picks when a window does not fit the screen: windows of 20
 * to 44 Big-font texts, printing each window's size and the fonts of a
 * Normal, Tiny, Fixed and Big text. Appends to toobig.txt. Compare native
 * and MUI 3.9.
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
    for (n = 24; n <= 70; n += 2)
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
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

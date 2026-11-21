/* Windows too tall for the screen with a Register and a framed group:
 * sizes of the register and its page, the group and its contents, and the
 * text font, for growing numbers of lines. Shows which stage of fitting a
 * window into the screen halves the group spacing and drops the frames'
 * inner spacing. Compare Zune68 and MUI 3.8 with the same settings; the
 * sizes depend on them.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit fitframes.c -o fitframes -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static const char *titles[] = {"One", "Two", NULL};
int main(void)
{
    Object *app, *win, *reg, *page, *grp, *in, *t;
    ULONG open; int n, i;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    for (n = 6; n <= 30; n += 2) {
        page = VGroup, End;
        in = VGroup, End;
        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, VGroup,
                Child, reg = RegisterGroup(titles), Child, page, Child, TextObject, MUIA_Text_Contents, "p2", End, End,
                Child, grp = VGroup, GroupFrameT("Frame"), Child, in, End,
            End, End, End;
        if (!app) break;
        for (i = 0; i < n; i++) {
            t = TextObject, MUIA_Text_Contents, "line", End;
            DoMethod(page, OM_ADDMEMBER, t);
            if (i < 3) DoMethod(in, OM_ADDMEMBER, TextObject, MUIA_Text_Contents, "x", End);
        }
        set(win, MUIA_Window_Open, TRUE);
        open = 0; get(win, MUIA_Window_Open, &open);
        if (open)
            printf("n=%d win=%ldx%ld reg=%dx%d page=%dx%d@%d,%d grp=%dx%d in=%dx%d@%d,%d font=%d\n", n,
                (long)XGET(win, MUIA_Window_Width), (long)XGET(win, MUIA_Window_Height),
                _width(reg), _height(reg), _width(page), _height(page), _left(page) - _left(reg), _top(page) - _top(reg),
                _width(grp), _height(grp), _width(in), _height(in), _left(in) - _left(grp), _top(in) - _top(grp),
                _font(t)->tf_YSize);
        else
            printf("n=%d no window\n", n);
        set(win, MUIA_Window_Open, FALSE);
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}

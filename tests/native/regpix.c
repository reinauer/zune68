/* A Register group's pixels after page switches: the window's inner area
 * at opening, then after going to each later page and back to the first.
 * MUI 3.8 gives the opening's pixels back every time.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit \
 *        regpix.c -o regpix -lamiga
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

static ULONG sum(Object *o)
{
    ULONG s = 0;
    LONG x, y;

    for (y = _top(o); y <= _bottom(o); y++)
        for (x = _left(o); x <= _right(o); x++)
            s = s * 31 + ReadPixel(_rp(o), x, y) + 1;
    return s;
}

int main(void)
{
    static const char *titles[] = { "First", "Second", "Third", NULL };
    Object *app, *win, *reg;
    ULONG open = FALSE, first;
    int page;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Title, "regpix",
        WindowContents, VGroup,
            Child, reg = RegisterGroup(titles),
                Child, TextObject, MUIA_Text_Contents, "page one", End,
                Child, TextObject, MUIA_Text_Contents, "page two", End,
                Child, TextObject, MUIA_Text_Contents, "page three", End,
                End,
            End, End, End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %s\n", open ? "yes" : "no");
    if (!open)
        return 0;
    DoMethod(app, MUIM_Application_InputBuffered);
    first = sum(reg);
    for (page = 1; page < 3; page++)
    {
        set(reg, MUIA_Group_ActivePage, page);
        set(reg, MUIA_Group_ActivePage, 0);
        printf("first page after page %d: %s\n", page + 1,
            sum(reg) == first ? "as at opening" : "differs");
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

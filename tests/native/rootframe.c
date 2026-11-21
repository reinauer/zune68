/* A window whose root group has a frame (and one with a frame title),
 * against one whose root has none: the window size, the root's box and
 * its inner offsets.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit \
 *        rootframe.c -o rootframe -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void run(const char *name, Object *app, Object *win, Object *root)
{
    struct Window *w = NULL;
    ULONG open = FALSE;

    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &w);
    if (open && w)
        printf("%-8s window %dx%d root at %d,%d size %dx%d inner %d,%d sub %d,%d\n",
            name, w->Width, w->Height, _left(root), _top(root), _width(root),
            _height(root), _addleft(root), _addtop(root), _subwidth(root),
            _subheight(root));
    else
        printf("%-8s not open\n", name);
    set(win, MUIA_Window_Open, FALSE);
}

int main(void)
{
    Object *app, *w1, *w2, *w3, *r1, *r2, *r3;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        SubWindow, w1 = WindowObject, MUIA_Window_Title, "plain",
            WindowContents, r1 = VGroup,
                Child, TextObject, MUIA_Text_Contents, "contents", End,
                End, End,
        SubWindow, w2 = WindowObject, MUIA_Window_Title, "framed",
            WindowContents, r2 = VGroup, GroupFrame,
                Child, TextObject, MUIA_Text_Contents, "contents", End,
                End, End,
        SubWindow, w3 = WindowObject, MUIA_Window_Title, "titled",
            WindowContents, r3 = VGroup, GroupFrameT("Title"),
                Child, TextObject, MUIA_Text_Contents, "contents", End,
                End, End,
        End;
    if (!app)
        return 20;
    run("plain", app, w1, r1);
    run("framed", app, w2, r2);
    run("titled", app, w3, r3);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

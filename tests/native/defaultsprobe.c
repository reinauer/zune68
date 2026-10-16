/* Defaults without preferences: the window's refresh type and the
 * spacings a toolkit uses for an ordinary window. Prints the refresh
 * flag of the opened window, the root group's inset from the window
 * border, and the gaps between two buttons in a vertical and in a
 * horizontal group. Run on each toolkit and compare.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit defaultsprobe.c -o defaultsprobe -lamiga
 */
#include <stdio.h>
#include <stdarg.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static Object *new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;

    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject new_object

int main(void)
{
    Object *app, *win, *root, *v1, *v2, *h1, *h2;
    struct Window *w = NULL;
    ULONG open = 0;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Base, "DEFPROBE",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "defaults",
            WindowContents, root = VGroup,
                Child, VGroup,
                    Child, v1 = SimpleButton("One"),
                    Child, v2 = SimpleButton("Two"),
                    End,
                Child, HGroup,
                    Child, h1 = SimpleButton("Left"),
                    Child, h2 = SimpleButton("Right"),
                    End,
                End,
            End,
        End;
    if (!app)
        goto close;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &w);
    if (!open || !w)
        goto dispose;
    printf("library %d.%d\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);
    printf("refresh %s (flags %08lx)\n",
        (w->Flags & WFLG_REFRESHBITS) == WFLG_SIMPLE_REFRESH ? "simple" :
        (w->Flags & WFLG_REFRESHBITS) == WFLG_SMART_REFRESH ? "smart" :
        "other", (unsigned long)w->Flags);
    printf("window inner inset left=%d top=%d right=%d bottom=%d\n",
        _left(root) - w->BorderLeft, _top(root) - w->BorderTop,
        w->Width - w->BorderRight - 1 - _right(root),
        w->Height - w->BorderBottom - 1 - _bottom(root));
    printf("vgroup gap=%d  hgroup gap=%d  groups gap=%d\n",
        _top(v2) - _bottom(v1) - 1, _left(h2) - _right(h1) - 1,
        _top(h1) - _bottom(v2) - 1);
    printf("button %dx%d window %dx%d\n", _width(v1), _height(v1),
        w->Width, w->Height);
    rc = 0;
dispose:
    MUI_DisposeObject(app);
close:
    CloseLibrary(MUIMasterBase);
    printf(rc ? "FAIL rc=%d\n" : "PASS rc=%d\n", rc);
    return rc;
}

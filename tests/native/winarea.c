/* What the SDK's area macros read on a Window object, closed and open.
 * _window(), _screen(), _rp() are for area objects: on a Window object they
 * read the long after the notify data as a MUI_RenderInfo pointer. MUI 3.8
 * keeps a pointer to zeros there, so all of them are NULL; a program that
 * passes _window(win) as ASLFR_Window gets asl.library's default screen.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit \
 *        winarea.c -o winarea -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static const char *what(APTR p)
{
    return p ? "set" : "NULL";
}

static void show(const char *when, Object *win)
{
    struct MUI_RenderInfo *mri = muiRenderInfo(win);

    if (!mri)
    {
        printf("%s: render info NULL\n", when);
        return;
    }
    printf("%s: screen %s window %s rastport %s\n", when,
        what(mri->mri_Screen), what(mri->mri_Window), what(mri->mri_RastPort));
}

int main(void)
{
    Object *app, *win;
    ULONG open = FALSE;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Title, "winarea",
        WindowContents, VGroup, Child, TextObject,
            MUIA_Text_Contents, "x", End, End, End, End;
    if (!app)
        return 20;
    show("closed", win);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %s\n", open ? "yes" : "no");
    if (open)
        show("open", win);
    set(win, MUIA_Window_Open, FALSE);
    show("closed again", win);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

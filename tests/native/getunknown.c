/* What OM_GET leaves in the storage for an attribute no class knows, on a
 * Window (open and closed), a Process-less Application, a Group and a
 * Text: the storage is preset to 0x12345678 before each get.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit getunknown.c -o getunknown -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void probe(const char *name, Object *o)
{
    IPTR v = 0x12345678;
    ULONG rc = GetAttr(0x80ABCDEF, o, &v);

    printf("%-12s rc=%lu value=%08lx\n", name, (unsigned long)rc,
        (unsigned long)v);
}

int main(void)
{
    Object *app, *win, *grp, *txt;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, grp = VGroup,
            Child, txt = TextObject, MUIA_Text_Contents, "text", End,
        End, End, End;
    if (!app)
        return 20;
    probe("window", win);
    probe("application", app);
    probe("group", grp);
    probe("text", txt);
    set(win, MUIA_Window_Open, TRUE);
    probe("window open", win);
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

/* Balance object MinMax comparison in horizontal and vertical groups.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit balancebounds.c -o balancebounds -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void report(const char *name, Object *o)
{
    printf("%s box=%d,%d,%d,%d min=%d,%d def=%d,%d max=%d,%d sub=%d,%d\n",
        name, _left(o), _top(o), _width(o), _height(o), _minwidth(o),
        _minheight(o), _defwidth(o), _defheight(o), _maxwidth(o),
        _maxheight(o), _subwidth(o), _subheight(o));
}

int main(void)
{
    Object *app, *win, *hbal, *vbal, *hgroup, *vgroup;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "balancebounds",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "balancebounds",
            WindowContents, VGroup,
                Child, hgroup = HGroup,
                    Child, RectangleObject, TextFrame, End,
                    Child, hbal = BalanceObject, End,
                    Child, RectangleObject, TextFrame, End,
                    End,
                Child, vgroup = VGroup,
                    Child, RectangleObject, TextFrame, End,
                    Child, vbal = BalanceObject, End,
                    Child, RectangleObject, TextFrame, End,
                    End,
                End,
            End,
        End;
    if (!app)
        goto done;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    Delay(25);
    report("hgroup", hgroup);
    report("hbal", hbal);
    report("vgroup", vgroup);
    report("vbal", vbal);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

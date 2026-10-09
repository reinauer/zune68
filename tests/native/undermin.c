/* Group layout must not shrink children below their minimum size.
 * Image buttons with narrow maximum widths are placed in vertical groups
 * with and without stretchable siblings; their boxes are reported.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit undermin.c -o undermin -lamiga
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
    printf("%s at=%d,%d size=%d,%d min=%d,%d def=%d,%d max=%d,%d\n", name,
        _left(o), _top(o), _width(o), _height(o), _minwidth(o),
        _minheight(o), _defwidth(o), _defheight(o), _maxwidth(o),
        _maxheight(o));
}

int main(void)
{
    Object *app, *win, *check, *pop, *rect, *check2, *pop2, *group;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "undermin",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "undermin",
            WindowContents, HGroup,
                Child, group = VGroup,
                    Child, check = CheckMark(FALSE),
                    Child, pop = PopButton(MUII_PopUp),
                    Child, rect = RectangleObject, TextFrame,
                        MUIA_FixHeight, 10, End,
                    End,
                Child, VGroup,
                    Child, check2 = CheckMark(FALSE),
                    Child, pop2 = PopButton(MUII_PopUp),
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
    report("group", group);
    report("check", check);
    report("pop", pop);
    report("rect", rect);
    report("check2", check2);
    report("pop2", pop2);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

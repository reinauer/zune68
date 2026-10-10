/* When is the active object's dotted frame drawn? A window opens with
 * MUIA_Window_ActiveObject set; then the active object changes while the
 * window is open. Prints the pens on a line just outside each button
 * (where Zune draws the frame) for native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit activeframe.c -o activeframe -lamiga
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

/* Pens of the rows just above and inside the top edge of an object */
static void edge(const char *step, const char *name, Object *o)
{
    LONG x, y;

    for (y = _top(o) - 1; y <= _top(o); y++)
    {
        printf("%-10s %-6s y%+ld ", step, name, (long)(y - _top(o)));
        for (x = _left(o) - 1; x <= _right(o) + 1; x++)
            printf("%lx", (long)(ReadPixel(_rp(o), x, y) & 15));
        printf("\n");
    }
}

int main(void)
{
    Object *app, *win, *b1, *b2;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "activeframe",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "activeframe",
            WindowContents, HGroup,
                Child, b1 = SimpleButton("Quit"),
                Child, b2 = SimpleButton("Cancel"),
                End,
            End,
        End;
    if (!app)
        goto done;
    set(b1, MUIA_CycleChain, 1);
    set(b2, MUIA_CycleChain, 1);
    set(win, MUIA_Window_ActiveObject, b2);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    edge("opened", "quit", b1);
    edge("opened", "cancel", b2);
    set(win, MUIA_Window_ActiveObject, b1);
    DoMethod(app, MUIM_Application_InputBuffered);
    edge("switched", "quit", b1);
    edge("switched", "cancel", b2);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

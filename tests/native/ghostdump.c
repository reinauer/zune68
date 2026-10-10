/* Pixel maps of a text button and a popup button, enabled and disabled,
 * to compare the disabled (ghost) pattern of native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit ghostdump.c -o ghostdump -lamiga
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

static void dump(const char *name, Object *o)
{
    LONG x, y;

    printf("%s %dx%d at %d,%d:\n", name, _width(o), _height(o), _left(o),
        _top(o));
    for (y = _top(o); y <= _bottom(o); y++)
    {
        for (x = _left(o); x <= _right(o); x++)
            printf("%lx", (long)(ReadPixel(_rp(o), x, y) & 15));
        printf("\n");
    }
}

int main(void)
{
    Object *app, *win, *btn, *pop;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "ghostdump",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "ghostdump",
            WindowContents, HGroup,
                Child, btn = SimpleButton("Search"),
                Child, pop = PopButton(MUII_PopUp),
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
    dump("button", btn);
    dump("popup", pop);
    set(btn, MUIA_Disabled, TRUE);
    set(pop, MUIA_Disabled, TRUE);
    DoMethod(app, MUIM_Application_InputBuffered);
    dump("button disabled", btn);
    dump("popup disabled", pop);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

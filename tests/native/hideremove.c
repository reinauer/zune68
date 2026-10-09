/* Hidden and removed child repaint comparison.
 * Hides a trailing child with MUIA_ShowMe and removes another through
 * InitChange/OM_REMMEMBER/ExitChange, in groups with and without
 * MUIA_FillArea. Each phase is held so the screen can be sampled.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit hideremove.c -o hideremove -lamiga
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

static Object *row(Object **last, BOOL fill)
{
    return HGroup,
        MUIA_FillArea, fill,
        Child, SimpleButton("One"),
        Child, SimpleButton("Two"),
        Child, *last = SimpleButton("Trailing"),
        Child, HSpace(0),
        End;
}

static void phase(Object *win, const char *name)
{
    struct Window *w = NULL;

    get(win, MUIA_Window_Window, &w);
    printf("phase %s window=%d,%d\n", name, w ? w->LeftEdge : -1,
        w ? w->TopEdge : -1);
    fflush(stdout);
    Delay(150);
}

int main(void)
{
    Object *app, *win, *g1, *g2, *g3, *g4, *t1, *t2, *t3, *t4;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "hideremove",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "hideremove",
            MUIA_Window_LeftEdge, 100,
            MUIA_Window_TopEdge, 200,
            WindowContents, VGroup,
                Child, g1 = row(&t1, TRUE),
                Child, g2 = row(&t2, FALSE),
                Child, g3 = row(&t3, TRUE),
                Child, g4 = row(&t4, FALSE),
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
    printf("rows %d %d %d %d trailing x=%d,%d,%d,%d\n", _top(g1), _top(g2),
        _top(g3), _top(g4), _left(t1), _left(t2), _left(t3), _left(t4));
    phase(win, "initial");

    set(t1, MUIA_ShowMe, FALSE);
    set(t2, MUIA_ShowMe, FALSE);
    DoMethod(app, MUIM_Application_InputBuffered);
    phase(win, "hidden");

    if (DoMethod(g3, MUIM_Group_InitChange))
    {
        DoMethod(g3, OM_REMMEMBER, t3);
        DoMethod(g3, MUIM_Group_ExitChange);
    }
    if (DoMethod(g4, MUIM_Group_InitChange))
    {
        DoMethod(g4, OM_REMMEMBER, t4);
        DoMethod(g4, MUIM_Group_ExitChange);
    }
    DoMethod(app, MUIM_Application_InputBuffered);
    phase(win, "removed");
    MUI_DisposeObject(t3);
    MUI_DisposeObject(t4);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

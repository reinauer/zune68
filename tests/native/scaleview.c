/* Scale objects at fixed sizes for comparing their drawing: horizontal at
 * three widths, a vertical one, and their positions for screenshots.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit scaleview.c -o scaleview -lamiga
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

int main(void)
{
    Object *app, *win, *s[5];
    struct Window *w = NULL;
    ULONG open = FALSE;
    int rc = 20, i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "scaleview",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "scaleview",
            MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 220,
            WindowContents, HGroup,
                Child, VGroup,
                    Child, s[0] = ScaleObject, MUIA_FixWidth, 38, End,
                    Child, s[1] = ScaleObject, MUIA_FixWidth, 100, End,
                    Child, s[2] = ScaleObject, MUIA_FixWidth, 250, End,
                    Child, s[3] = ScaleObject, MUIA_FixWidth, 250,
                        MUIA_Font, MUIV_Font_Big, End,
                    End,
                Child, s[4] = ScaleObject, MUIA_Scale_Horiz, FALSE,
                    MUIA_FixHeight, 100, End,
                End,
            End,
        End;
    if (!app)
        goto done;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    get(win, MUIA_Window_Window, &w);
    printf("window %d,%d\n", w->LeftEdge, w->TopEdge);
    for (i = 0; i < 5; i++)
        printf("scale%d box=%d,%d,%d,%d\n", i, _left(s[i]), _top(s[i]),
            _width(s[i]), _height(s[i]));
    fflush(stdout);
    Delay(50 * 20);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

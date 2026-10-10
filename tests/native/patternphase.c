/* Phase of MUI's pattern backgrounds: rectangles with MUII_SHINEBACK and
 * MUII_FILLBACK2 at different origins, printing each one's origin and its
 * first two pixel rows, to tell absolute from object-relative alignment.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit patternphase.c -o patternphase -lamiga
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

static Object *rect(LONG w, LONG h, ULONG bg)
{
    return RectangleObject, MUIA_FixWidth, w, MUIA_FixHeight, h,
        MUIA_Background, bg, End;
}

static void show(const char *name, Object *o)
{
    LONG x, y;

    printf("%-6s at %3d,%3d:", name, _left(o), _top(o));
    for (y = _top(o); y < _top(o) + 2; y++)
    {
        printf(" ");
        for (x = _left(o); x < _left(o) + 8; x++)
            printf("%lx", (long)(ReadPixel(_rp(o), x, y) & 15));
    }
    printf("\n");
}

int main(void)
{
    Object *app, *win, *o[6], *grp, *child, *child2;
    ULONG open = FALSE;
    int rc = 20, i;
    static const char *names[] = { "a", "b", "c", "d", "e", "f" };

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "patternphase",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "patternphase",
            WindowContents, VGroup,
                Child, HGroup,
                    Child, o[0] = rect(9, 5, MUII_SHINEBACK),
                    Child, o[1] = rect(10, 5, MUII_SHINEBACK),
                    Child, o[2] = rect(10, 6, MUII_FILLBACK2),
                    End,
                Child, grp = HGroup, MUIA_Background, MUII_SHINEBACK,
                    Child, rect(3, 3, MUII_BACKGROUND),
                    Child, child = RectangleObject, MUIA_FixWidth, 9,
                        MUIA_FixHeight, 5, End,
                    Child, rect(2, 3, MUII_BACKGROUND),
                    Child, child2 = RectangleObject, MUIA_FixWidth, 9,
                        MUIA_FixHeight, 5, End,
                    End,
                Child, HGroup,
                    Child, rect(3, 3, MUII_BACKGROUND),
                    Child, o[3] = rect(9, 5, MUII_SHINEBACK),
                    Child, o[4] = rect(9, 5, MUII_FILLBACK2),
                    Child, o[5] = rect(9, 5, MUII_SHADOWBACK),
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
    for (i = 0; i < 6; i++)
        show(names[i], o[i]);
    show("group", grp);
    show("child", child);
    show("child2", child2);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

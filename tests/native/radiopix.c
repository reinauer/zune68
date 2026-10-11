/* Pixels of a three-item radio group (one selected) and a disabled one.
 * Writes radiopix.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit radiopix.c -o radiopix -lamiga
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

static void dump(Object *o)
{
    LONG x, y;

    printf("box %dx%d\n", _width(o), _height(o));
    for (y = _top(o); y <= _bottom(o); y++)
    {
        for (x = _left(o); x <= _right(o); x++)
            printf("%c", "0123456789abcdef"[ReadPixel(_rp(o), x, y) & 15]);
        printf("\n");
    }
}

int main(void)
{
    static const char *items[] = { "One", "Two", "Three", NULL };
    Object *app, *win, *r1, *r2;
    ULONG open = FALSE;

    freopen("radiopix.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, VGroup,
            Child, r1 = RadioObject, MUIA_Radio_Entries, items,
                MUIA_Radio_Active, 1, End,
            Child, r2 = RadioObject, MUIA_Radio_Entries, items,
                MUIA_Disabled, TRUE, End,
            End, End, End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    if (open)
    {
        dump(r1);
        dump(r2);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

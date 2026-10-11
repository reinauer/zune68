/* Pens of a horizontal Gauge's info text over the filled and the empty
 * part (50% filled, text "5000"). Writes gaugetext.txt. Compare native
 * and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit gaugetext.c -o gaugetext -lamiga
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

int main(void)
{
    Object *app, *win, *g;
    ULONG open = FALSE;
    LONG x, y;

    freopen("gaugetext.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Width, 200,
        WindowContents, VGroup,
            Child, g = GaugeObject, GaugeFrame, MUIA_Gauge_Horiz, TRUE,
                MUIA_Gauge_Max, 100, MUIA_Gauge_Current, 45,
                MUIA_Gauge_InfoText, "5000", End,
            End, End, End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    if (open)
    {
        printf("gauge box %dx%d\n", _width(g), _height(g));
        for (y = _top(g); y <= _bottom(g); y++)
        {
            for (x = _left(g); x <= _right(g); x++)
                printf("%c", "0123456789abcdef"[ReadPixel(_rp(g), x, y) & 15]);
            printf("\n");
        }
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

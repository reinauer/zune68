/* Horizontal Gauges 60, 66 and 67 pixels wide, created with an empty
 * InfoText and then given a short text and texts wider than themselves,
 * each next to a Rectangle with a solid background. Dumps the pens of both
 * boxes to gaugeoverflow.txt after each text. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit gaugeoverflow.c -o gaugeoverflow -lmui -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void dump(Object *g, Object *r, const char *label)
{
    LONG x, y;

    printf("== %s gauge %ld,%ld %ldx%ld inner %ld..%ld rect %ld..%ld\n",
        label, (long)_left(g), (long)_top(g), (long)_width(g),
        (long)_height(g), (long)_mleft(g), (long)_mright(g),
        (long)_left(r), (long)_right(r));
    for (y = _top(g); y <= _bottom(g); y++)
    {
        for (x = _left(g) - 2; x <= _right(r); x++)
        {
            if (x == _left(r))
                putchar('|');
            putchar("0123456789abcdef"[ReadPixel(_rp(g), x, y) & 15]);
        }
        putchar('\n');
    }
}

static void settle(Object *app)
{
    int i;

    for (i = 0; i < 10; i++)
    {
        DoMethod(app, MUIM_Application_InputBuffered);
        Delay(2);
    }
}

#define GAUGES 3
static const LONG widths[GAUGES] = { 60, 66, 67 };

static Object *row(Object **g, Object **r, LONG width)
{
    return HGroup,
        Child, *g = GaugeObject, GaugeFrame, MUIA_Gauge_Horiz, TRUE,
            MUIA_FixWidth, width, MUIA_Gauge_Max, 100,
            MUIA_Gauge_Current, 30, MUIA_Gauge_InfoText, "", End,
        Child, *r = RectangleObject, MUIA_FixWidth, 40,
            MUIA_Background, MUII_SHINE, End,
        Child, HSpace(0),
        End;
}

static void step(Object *app, Object **g, Object **r, CONST_STRPTR text,
    const char *label)
{
    char name[32];
    int i;

    for (i = 0; i < GAUGES; i++)
        set(g[i], MUIA_Gauge_InfoText, (IPTR)text);
    settle(app);
    for (i = 0; i < GAUGES; i++)
    {
        sprintf(name, "%s-%ld", label, (long)widths[i]);
        dump(g[i], r[i], name);
    }
}

int main(void)
{
    Object *app, *win, *g[GAUGES], *r[GAUGES];
    ULONG open = FALSE;

    freopen("SYS:gaugeoverflow.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, VGroup,
            Child, row(&g[0], &r[0], widths[0]),
            Child, row(&g[1], &r[1], widths[1]),
            Child, row(&g[2], &r[2], widths[2]),
            End, End, End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
    {
        settle(app);
        step(app, g, r, "50%", "short");
        step(app, g, r, "101.6 kB of 16.0 MB read", "first");
        step(app, g, r, "2203.2 kB of 16.0 MB", "second");
    }
    printf("open=%lu\n", open);
    fclose(stdout);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

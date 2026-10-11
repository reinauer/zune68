/* Pixels of Knob and Levelmeter objects at several values, as pen
 * numbers, with the RGB of every pen used, for reproducing MUI 3.9's
 * artwork. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit knobdump.c -o knobdump -lamiga
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

#define COUNT 8
static UBYTE used[256];

static void dump(Object *o, const char *name)
{
    LONG x, y;

    printf("%s box %d,%d %dx%d inner %d,%d %dx%d\n", name, _left(o), _top(o),
        _width(o), _height(o), _mleft(o), _mtop(o), _mwidth(o),
        _mheight(o));
    for (y = _top(o); y <= _bottom(o); y++)
    {
        printf("%3ld ", (long)(y - _top(o)));
        for (x = _left(o); x <= _right(o); x++)
        {
            LONG p = ReadPixel(_rp(o), x, y);

            used[p & 255] = 1;
            printf("%c", "0123456789abcdefghijklmnopqrstuvwxyz"[p % 36]);
        }
        printf("\n");
    }
}

int main(void)
{
    static const char *names[COUNT] = { "knob 0", "knob 25", "knob 64",
        "knob 100", "level 0", "level 30", "level 100", "level label" };
    static const LONG values[COUNT] = { 0, 25, 64, 100, 0, 30, 100, 50 };
    Object *app, *win, *o[COUNT];
    struct Screen *scr;
    ULONG open = FALSE, rgb[3];
    int rc = 20, i;

    freopen("knobdump.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "knobdump",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "knobdump",
            WindowContents, VGroup,
                Child, HGroup,
                    Child, o[0] = KnobObject, End,
                    Child, o[1] = KnobObject, End,
                    Child, o[2] = KnobObject, End,
                    Child, o[3] = KnobObject, End,
                    End,
                Child, HGroup,
                    Child, o[4] = LevelmeterObject, End,
                    Child, o[5] = LevelmeterObject, End,
                    Child, o[6] = LevelmeterObject, End,
                    Child, o[7] = LevelmeterObject,
                        MUIA_Levelmeter_Label, "Vertical", End,
                    End,
                End,
            End,
        End;
    if (!app)
        goto done;
    for (i = 0; i < COUNT; i++)
        set(o[i], MUIA_Numeric_Value, values[i]);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    for (i = 0; i < COUNT; i++)
    {
        LONG min = 0, max = 0;

        get(o[i], MUIA_Numeric_Min, &min);
        get(o[i], MUIA_Numeric_Max, &max);
        printf("%s range %ld..%ld\n", names[i], (long)min, (long)max);
        dump(o[i], names[i]);
    }
    scr = _screen(o[0]);
    for (i = 0; i < 256; i++)
        if (used[i])
        {
            GetRGB32(scr->ViewPort.ColorMap, i, 1, rgb);
            printf("pen %d = %02lx%02lx%02lx\n", i, rgb[0] >> 24,
                rgb[1] >> 24, rgb[2] >> 24);
        }
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

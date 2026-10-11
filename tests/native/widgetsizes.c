/* Minimum, default and maximum sizes of Gauge, Scale, Slider, Knob,
 * Levelmeter and HBar variants (orientation, info text, font), for
 * deriving MUI 3.9's size rules. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit widgetsizes.c -o widgetsizes -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define MAXW 32
static const char *names[MAXW];
static Object *objs[MAXW];
static int count;

static Object *add(const char *name, Object *o)
{
    if (count < MAXW)
    {
        names[count] = name;
        objs[count++] = o;
    }
    return o;
}

int main(void)
{
    Object *app, *win;
    ULONG open = FALSE;
    int rc = 20, i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "widgetsizes",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "widgetsizes",
            WindowContents, VGroup,
                Child, HGroup,
                    Child, add("gauge h", GaugeObject, GaugeFrame,
                        MUIA_Gauge_Horiz, TRUE, End),
                    Child, add("gauge h noframe", GaugeObject,
                        MUIA_Gauge_Horiz, TRUE, End),
                    Child, add("gauge h info", GaugeObject, GaugeFrame,
                        MUIA_Gauge_Horiz, TRUE,
                        MUIA_Gauge_InfoText, "%ld %%", End),
                    Child, add("gauge h big", GaugeObject, GaugeFrame,
                        MUIA_Gauge_Horiz, TRUE, MUIA_Font, MUIV_Font_Big,
                        End),
                    Child, add("gauge v", GaugeObject, GaugeFrame, End),
                    Child, add("gauge v info", GaugeObject, GaugeFrame,
                        MUIA_Gauge_InfoText, "%ld", End),
                    End,
                Child, HGroup,
                    Child, add("scale h", ScaleObject, End),
                    Child, add("scale v", ScaleObject,
                        MUIA_Scale_Horiz, FALSE, End),
                    Child, add("scale big", ScaleObject,
                        MUIA_Font, MUIV_Font_Big, End),
                    End,
                Child, HGroup,
                    Child, add("slider h", SliderObject, End),
                    Child, add("slider v", SliderObject,
                        MUIA_Slider_Horiz, FALSE, End),
                    Child, add("slider max999", SliderObject,
                        MUIA_Numeric_Max, 999, End),
                    Child, add("slider big", SliderObject,
                        MUIA_Font, MUIV_Font_Big, End),
                    End,
                Child, HGroup,
                    Child, add("knob", KnobObject, End),
                    Child, add("knob big", KnobObject,
                        MUIA_Font, MUIV_Font_Big, End),
                    Child, add("levelmeter", LevelmeterObject, End),
                    Child, add("levelmeter lbl", LevelmeterObject,
                        MUIA_Levelmeter_Label, "Level", End),
                    End,
                Child, add("hbar 2", MUI_MakeObject(MUIO_HBar, 2)),
                Child, add("hbar 0", MUI_MakeObject(MUIO_HBar, 0)),
                Child, add("vbar 2", MUI_MakeObject(MUIO_VBar, 2)),
                End,
            End,
        End;
    if (!app)
        goto done;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    for (i = 0; i < count; i++)
    {
        Object *o = objs[i];

        if (!o)
        {
            printf("%-16s missing\n", names[i]);
            continue;
        }
        printf("%-16s min=%d,%d def=%d,%d max=%d,%d sub=%d,%d\n", names[i],
            _minwidth(o), _minheight(o), _defwidth(o), _defheight(o),
            _maxwidth(o), _maxheight(o), _subwidth(o), _subheight(o));
    }
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

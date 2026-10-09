/* Built-in widget MinMax comparison.
 * Reports box, MinMax and frame size of common MUI classes created with
 * default attributes, for comparison between implementations.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit widgetbounds.c -o widgetbounds -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;

static const char *cycle_entries[] = { "One", "Two", "Three", NULL };
static const char *radio_entries[] = { "Alpha", "Beta", NULL };

#define MAXW 32
static Object *objs[MAXW];
static const char *names[MAXW];
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

int main(int argc, char **argv)
{
    Object *app, *win, *str;
    ULONG open = FALSE;
    int i, rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "widgetbounds",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "widgetbounds",
            WindowContents, VGroup,
                Child, add("string", StringObject, StringFrame, End),
                Child, add("stringbig", StringObject, StringFrame,
                    MUIA_Font, MUIV_Font_Big, End),
                Child, add("stringmax5", StringObject, StringFrame,
                    MUIA_String_MaxLen, 5, End),
                Child, add("stringnoframe", StringObject, End),
                Child, add("popasl", PopaslObject,
                    MUIA_Popstring_String, str = StringObject,
                        StringFrame, End,
                    MUIA_Popstring_Button, add("popbutton",
                        PopButton(MUII_PopFile)),
                    ASLFR_TitleText, "x",
                    End),
                Child, add("popdrawer", PopButton(MUII_PopDrawer)),
                Child, add("popup", PopButton(MUII_PopUp)),
                Child, add("label", Label("Label:")),
                Child, add("label2", Label2("Label:")),
                Child, add("button", SimpleButton("Button")),
                Child, add("checkmark", CheckMark(FALSE)),
                Child, add("checked", CheckMark(TRUE)),
                Child, add("cycle", CycleObject,
                    MUIA_Cycle_Entries, cycle_entries, End),
                Child, add("radio", RadioObject,
                    MUIA_Radio_Entries, radio_entries, End),
                Child, add("slider", SliderObject, End),
                Child, add("gauge", GaugeObject, GaugeFrame,
                    MUIA_Gauge_Horiz, TRUE, End),
                Child, add("scale", ScaleObject, End),
                Child, add("numbutton", NumericbuttonObject, End),
                Child, add("knob", KnobObject, End),
                Child, add("levelmeter", LevelmeterObject, End),
                Child, add("hbar", MUI_MakeObject(MUIO_HBar, 2)),
                Child, add("hspace", HSpace(0)),
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
    Delay(25);
    for (i = 0; i < count; i++)
    {
        Object *o = objs[i];

        if (!o)
        {
            printf("%s missing\n", names[i]);
            continue;
        }
        printf("%s at=%d,%d size=%d,%d min=%d,%d def=%d,%d max=%d,%d "
            "sub=%d,%d\n", names[i], _left(o), _top(o), _width(o), _height(o), _minwidth(o), _minheight(o),
            _defwidth(o), _defheight(o), _maxwidth(o), _maxheight(o),
            _subwidth(o), _subheight(o));
    }
    (void)str;
    (void)argv;
    /* An argument keeps the window open for pixel inspection. */
    if (argc > 1)
    {
        fflush(stdout);
        Delay(1500);
    }
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

/* Compound widget layout comparison.
 * Walks Cycle, Radio, Popasl and Slider objects recursively and reports
 * each part's box, MinMax and frame size.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit compoundwalk.c -o compoundwalk -lamiga
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

static void walk(Object *o, int depth)
{
    struct List *children = NULL;
    Object *child;
    APTR state;

    printf("%*sbox=%d,%d,%d,%d min=%d,%d def=%d,%d max=%d,%d "
        "sub=%d,%d\n", depth * 2, "", _left(o), _top(o), _width(o),
        _height(o), _minwidth(o), _minheight(o), _defwidth(o),
        _defheight(o), _maxwidth(o), _maxheight(o), _subwidth(o),
        _subheight(o));
    if (depth >= 4 || !get(o, MUIA_Group_ChildList, &children) || !children)
        return;
    state = children->lh_Head;
    while ((child = NextObject(&state)) != NULL)
        walk(child, depth + 1);
}

static const char *cycle_entries[] = { "One", "Two", "Three", NULL };
static const char *radio_entries[] = { "Alpha", "Beta", NULL };

int main(int argc, char **argv)
{
    Object *app, *win, *cycle, *radio, *popasl, *slider;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "compoundwalk",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "compoundwalk",
            WindowContents, VGroup,
                Child, cycle = CycleObject,
                    MUIA_Cycle_Entries, cycle_entries, End,
                Child, radio = RadioObject,
                    MUIA_Radio_Entries, radio_entries, End,
                Child, popasl = PopaslObject,
                    MUIA_Popstring_String, StringObject, StringFrame, End,
                    MUIA_Popstring_Button, PopButton(MUII_PopFile),
                    End,
                Child, slider = SliderObject, End,
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
    printf("cycle\n");
    walk(cycle, 1);
    printf("radio\n");
    walk(radio, 1);
    printf("popasl\n");
    walk(popasl, 1);
    printf("slider\n");
    walk(slider, 1);
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

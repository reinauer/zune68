/* Two-column group width distribution comparison.
 * Labels next to stretchable controls, as in YAM's First Steps page;
 * reports each child's box so column widths can be compared.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit colgroup.c -o colgroup -lamiga
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

static const char *zones[] = { "Asia", "Europe", NULL };
static const char *cities[] = { "Karachi", "Berlin", NULL };

#define COUNT 10
static Object *objs[COUNT];
static const char *names[COUNT] = {
    "label1", "string", "label2", "cycles", "space", "text", "group", "wide",
    "space-1", "text-1"
};

static LONG weight(Object *o)
{
    LONG w = 0;

    get(o, MUIA_HorizWeight, &w);
    return w;
}

int main(void)
{
    Object *app, *win;
    ULONG open = FALSE;
    int i, rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "colgroup",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "colgroup",
            WindowContents, VGroup,
                Child, objs[7] = StringObject, StringFrame,
                    MUIA_FixWidth, 500, End,
                Child, objs[6] = ColGroup(2), GroupFrameT("System"),
                    Child, objs[0] = Label2("Charset"),
                    Child, objs[1] = StringObject, StringFrame, End,
                    Child, objs[2] = Label2("Time zone"),
                    Child, objs[3] = HGroup,
                        Child, CycleObject,
                            MUIA_Cycle_Entries, zones, End,
                        Child, CycleObject,
                            MUIA_Cycle_Entries, cities, End,
                        End,
                    Child, objs[4] = HSpace(0),
                    Child, objs[5] = TextObject,
                        MUIA_Text_Contents, "Description: n/a\nNext line",
                        End,
                    End,
                Child, ColGroup(2), GroupFrame,
                    Child, Label2("Charset"),
                    Child, StringObject, StringFrame, End,
                    Child, objs[8] = HSpace(-1),
                    Child, objs[9] = TextObject,
                        MUIA_Text_Contents, "Description: n/a", End,
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
    Delay(10);
    for (i = 0; i < COUNT; i++)
        printf("%s box=%d,%d,%d,%d min=%d def=%d max=%d hw=%ld\n", names[i],
            _left(objs[i]), _top(objs[i]), _width(objs[i]),
            _height(objs[i]), _minwidth(objs[i]), _defwidth(objs[i]),
            _maxwidth(objs[i]), (long)weight(objs[i]));
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

/* Every pixel of a window with the common widgets: bevel, recessed and
 * group frames (titled), buttons in normal, selected and disabled state,
 * radio, cycle, checkmark, slider, string, list with scrollbar arrows,
 * gauge and popup glyphs. Dumps the window after opening, after
 * MUI_Redraw of the root, after an InitChange/ExitChange relayout, after
 * toggling the selected states and while a button is hidden and shown,
 * which moves the frames below it. Titled frames and button columns also
 * sit on a white and on a pattern background. Compare two library builds
 * (or native and MUI 3.9) line by line; the output is about 600 KB.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit pixdump.c -o pixdump -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdarg.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static Object *new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;

    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject new_object

static const char *cycles[] = { "First", "Second", NULL };
static const char *radios[] = { "Alpha", "Beta", "Gamma", NULL };
static const char *entries[] = { "One", "Two", "Three", "Four", "Five",
    "Six", "Seven", "Eight", "Nine", "Ten", "Eleven", "Twelve", NULL };

static void dump(Object *win, const char *label)
{
    struct Window *w = NULL;
    struct RastPort *rp;
    WORD x, y;
    static char line[700];

    get(win, MUIA_Window_Window, &w);
    if (!w)
        return;
    rp = w->RPort;
    printf("state %s %dx%d\n", label, w->Width, w->Height);
    for (y = 0; y < w->Height; y++)
    {
        int n = 0;

        for (x = 0; x < w->Width && n < (int)sizeof(line) - 1; x++)
            line[n++] = "0123456789abcdef"[ReadPixel(rp, x, y) & 15];
        line[n] = 0;
        printf("%3d %s\n", y, line);
    }
}

int main(int argc, char **argv)
{
    Object *app, *win, *root, *sel, *dis, *radio, *check, *list, *gauge;
    struct Screen *scr;
    ULONG open = 0;
    int rc = 20;

    setvbuf(stdout, NULL, _IOFBF, 8192);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_Depth, 4,
        TAG_DONE);
    if (!scr)
        goto close;
    app = ApplicationObject,
        MUIA_Application_Title, "pixdump",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "pixdump",
            MUIA_Window_Screen, scr,
            MUIA_Window_LeftEdge, 10,
            MUIA_Window_TopEdge, 10,
            WindowContents, root = VGroup,
                Child, HGroup, GroupFrameT("Controls"),
                    Child, ColGroup(2),
                        Child, Label2("Name:"),
                        Child, StringObject, StringFrame,
                            MUIA_String_Contents, "text", End,
                        Child, Label1("Mode:"),
                        Child, CycleObject, MUIA_Cycle_Entries, cycles, End,
                        Child, Label1("Check:"),
                        Child, check = CheckMark(TRUE),
                        Child, Label2("Value:"),
                        Child, SliderObject, MUIA_Numeric_Max, 100,
                            MUIA_Numeric_Value, 40, End,
                        End,
                    Child, radio = RadioObject, GroupFrame,
                        MUIA_Radio_Entries, radios, End,
                    End,
                Child, HGroup,
                    Child, ListviewObject,
                        MUIA_Listview_List, list = ListObject,
                            InputListFrame, End,
                        End,
                    Child, VGroup,
                        Child, TextObject, TextFrame,
                            MUIA_Text_Contents, "Status", End,
                        Child, gauge = GaugeObject, GaugeFrame,
                            MUIA_Gauge_Horiz, TRUE, MUIA_Gauge_Current, 30,
                            End,
                        Child, sel = SimpleButton("_Selected"),
                        Child, dis = SimpleButton("Disabled"),
                        Child, PopaslObject,
                            MUIA_Popstring_String, StringObject,
                                StringFrame, End,
                            MUIA_Popstring_Button, PopButton(MUII_PopFile),
                            End,
                        Child, HGroup,
                            Child, ImageObject, ImageButtonFrame,
                                MUIA_Image_Spec, MUII_ArrowLeft, End,
                            Child, ImageObject, ImageButtonFrame,
                                MUIA_Image_Spec, MUII_ArrowRight, End,
                            Child, ImageObject, ImageButtonFrame,
                                MUIA_Image_Spec, MUII_ArrowUp, End,
                            Child, ImageObject, ImageButtonFrame,
                                MUIA_Image_Spec, MUII_ArrowDown, End,
                            End,
                        End,
                    End,
                Child, HGroup,
                    Child, VGroup, MUIA_Background, MUII_SHINE,
                        Child, VGroup, GroupFrameT("White"),
                            Child, SimpleButton("W1"),
                            Child, SimpleButton("W2"),
                            End,
                        Child, SimpleButton("W3"),
                        End,
                    Child, VGroup, MUIA_Background, MUII_SHADOWBACK,
                        Child, VGroup, GroupFrameT("Pattern"),
                            Child, SimpleButton("P1"),
                            Child, SimpleButton("P2"),
                            End,
                        Child, SimpleButton("P3"),
                        End,
                    End,
                Child, HGroup, GroupFrameT("Footer"),
                    Child, TextObject, MUIA_Text_Contents,
                        "\33cCentered", End,
                    Child, SimpleButton("OK"),
                    End,
                End,
            End,
        End;
    if (!app)
        goto screen;
    DoMethod(list, MUIM_List_Insert, entries, -1, MUIV_List_Insert_Bottom);
    set(dis, MUIA_Disabled, TRUE);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto dispose;
    DoMethod(app, MUIM_Application_InputBuffered);
    dump(win, "open");
    MUI_Redraw(root, MADF_DRAWOBJECT);
    dump(win, "redraw");
    DoMethod(root, MUIM_Group_InitChange);
    DoMethod(root, MUIM_Group_ExitChange);
    DoMethod(app, MUIM_Application_InputBuffered);
    dump(win, "relayout");
    set(sel, MUIA_Selected, TRUE);
    set(radio, MUIA_Radio_Active, 2);
    set(check, MUIA_Selected, FALSE);
    set(list, MUIA_List_Active, 3);
    set(gauge, MUIA_Gauge_Current, 70);
    dump(win, "states");
    set(sel, MUIA_Selected, FALSE);
    set(dis, MUIA_Disabled, FALSE);
    dump(win, "restored");
    /* Bevel frames below move up and back: corners must not keep the
     * pixels of whatever was there before */
    set(sel, MUIA_ShowMe, FALSE);
    DoMethod(app, MUIM_Application_InputBuffered);
    dump(win, "hidden");
    set(sel, MUIA_ShowMe, TRUE);
    DoMethod(app, MUIM_Application_InputBuffered);
    dump(win, "shown");
    rc = 0;
dispose:
    MUI_DisposeObject(app);
screen:
    CloseScreen(scr);
close:
    CloseLibrary(MUIMasterBase);
    printf(rc ? "FAIL rc=%d\n" : "PASS rc=%d\n", rc);
    fflush(stdout);
    return rc;
}

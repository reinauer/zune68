/* Cover and uncover a MUI window with a plain Intuition window: do all
 * pixels come back, how long does it take, and what does it cost in chip
 * RAM while covered? Prints the window's refresh type, the pixels that
 * differ after uncovering (one cycle), the EClock ticks for 20 cover and
 * uncover cycles including the toolkit's refresh, and the chip memory
 * used while the window is covered. Run on each toolkit and refresh type.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit overlapprobe.c -o overlapprobe -lamiga
 */
#include <stdio.h>
#include <stdarg.h>
#include <devices/timer.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/timer.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
struct Device *TimerBase;
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

static ULONG now(void)
{
    struct EClockVal ev;

    ReadEClock(&ev);
    return ev.ev_lo;
}

static UBYTE *snap(struct Window *w, UBYTE *buf)
{
    WORD x, y;
    UBYTE *p = buf;

    for (y = 0; y < w->Height; y++)
        for (x = 0; x < w->Width; x++)
            *p++ = ReadPixel(w->RPort, x, y);
    return buf;
}

static void cover(struct Screen *scr, struct Window *w, Object *app,
    ULONG *chipused)
{
    ULONG before = AvailMem(MEMF_CHIP);
    struct Window *c = OpenWindowTags(NULL, WA_CustomScreen, (ULONG) scr,
        WA_Left, w->LeftEdge + w->Width / 3, WA_Top, w->TopEdge + 20,
        WA_Width, w->Width, WA_Height, w->Height - 30,
        WA_SimpleRefresh, TRUE, WA_Borderless, TRUE, TAG_DONE);

    if (!c)
        return;
    SetAPen(c->RPort, 1);
    RectFill(c->RPort, 0, 0, c->Width - 1, c->Height - 1);
    if (chipused)
        *chipused = before - AvailMem(MEMF_CHIP);
    CloseWindow(c);
    /* let the toolkit repair what was covered */
    DoMethod(app, MUIM_Application_InputBuffered);
}

int main(void)
{
    struct timerequest tr;
    Object *app, *win, *list;
    struct Screen *scr;
    struct Window *w = NULL;
    ULONG open = 0, t, chip = 0;
    UBYTE *a, *b;
    LONG n, i, diff = 0;
    int rc = 20;

    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        goto device;
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_Depth, 4,
        TAG_DONE);
    if (!scr)
        goto close;
    app = ApplicationObject,
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "overlap",
            MUIA_Window_Screen, scr,
            MUIA_Window_LeftEdge, 20,
            MUIA_Window_TopEdge, 20,
            WindowContents, VGroup,
                Child, HGroup, GroupFrameT("Controls"),
                    Child, ColGroup(2),
                        Child, Label2("Name:"),
                        Child, StringObject, StringFrame,
                            MUIA_String_Contents, "text", End,
                        Child, Label1("Mode:"),
                        Child, CycleObject, MUIA_Cycle_Entries, cycles, End,
                        Child, Label2("Value:"),
                        Child, SliderObject, MUIA_Numeric_Max, 100,
                            MUIA_Numeric_Value, 40, End,
                        End,
                    Child, RadioObject, GroupFrame,
                        MUIA_Radio_Entries, radios, End,
                    End,
                Child, HGroup,
                    Child, ListviewObject,
                        MUIA_Listview_List, list = ListObject,
                            InputListFrame, End,
                        End,
                    Child, VGroup,
                        Child, GaugeObject, GaugeFrame,
                            MUIA_Gauge_Horiz, TRUE, MUIA_Gauge_Current, 30,
                            End,
                        Child, SimpleButton("_One"),
                        Child, SimpleButton("_Two"),
                        End,
                    End,
                End,
            End,
        End;
    if (!app)
        goto screen;
    DoMethod(list, MUIM_List_Insert, entries, -1, MUIV_List_Insert_Bottom);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &w);
    if (!open || !w)
        goto dispose;
    DoMethod(app, MUIM_Application_InputBuffered);
    n = (LONG) w->Width * w->Height;
    a = AllocVec(n, MEMF_ANY);
    b = AllocVec(n, MEMF_ANY);
    if (!a || !b)
        goto dispose;
    printf("library %d.%d refresh %s window %dx%d\n",
        MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision,
        (w->Flags & WFLG_REFRESHBITS) == WFLG_SMART_REFRESH ? "smart" :
        "simple", w->Width, w->Height);
    snap(w, a);
    cover(scr, w, app, &chip);
    snap(w, b);
    for (i = 0; i < n; i++)
        diff += a[i] != b[i];
    printf("after uncover: %ld of %ld pixels differ\n", diff, n);
    printf("chip RAM used while covered: %lu bytes\n", (unsigned long) chip);
    t = now();
    for (i = 0; i < 20; i++)
        cover(scr, w, app, NULL);
    printf("20 cover/uncover cycles: %lu ticks\n", (unsigned long)(now() - t));
    snap(w, b);
    diff = 0;
    for (i = 0; i < n; i++)
        diff += a[i] != b[i];
    printf("after 20 cycles: %ld pixels differ\n", diff);
    FreeVec(a);
    FreeVec(b);
    rc = diff ? 10 : 0;
dispose:
    MUI_DisposeObject(app);
screen:
    CloseScreen(scr);
close:
    CloseLibrary(MUIMasterBase);
device:
    CloseDevice((struct IORequest *)&tr);
    printf(rc ? "FAIL rc=%d\n" : "PASS rc=%d\n", rc);
    return rc;
}

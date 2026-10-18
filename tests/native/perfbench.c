/* Toolkit operation timing comparison.
 * Times common operations with the EClock: object tree creation and
 * disposal, window open/close, full redraws, relayouts, list inserts and
 * scrolling, notification chains and text updates. Each phase reports
 * the median EClock ticks of its repetitions.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit perfbench.c -o perfbench -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <devices/timer.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <proto/timer.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
/* Use va_start rather than the older SDK's address-of-argument wrapper.
 * AmigaOS/m68k passes the tag/value words consecutively on the stack. */
#include <stdarg.h>
struct Library *MUIMasterBase;
static Object *bench_new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;
    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject bench_new_object

struct Device *TimerBase;
ULONG __stack = 65536;

#ifndef PERFBENCH_REPS
#define PERFBENCH_REPS 15
#endif
#define REPS PERFBENCH_REPS
static ULONG samples[REPS];
static ULONG eclock_freq;

static const char *cycles[] = { "First", "Second", "Third", NULL };
static const char *radios[] = { "Alpha", "Beta", "Gamma", NULL };
static const char *entries[] = {
    "Entry one", "Entry two", "Entry three", "Entry four", "Entry five",
    "Entry six", "Entry seven", "Entry eight", "Entry nine", "Entry ten",
    NULL
};

static Object *app, *win, *root, *list, *slider, *numeric, *text;
static struct Screen *benchmark_screen;
static int fixed_geometry;

static ULONG now(void)
{
    struct EClockVal ev;

    eclock_freq = ReadEClock(&ev);
    return ev.ev_lo;
}

static int cmp(const void *a, const void *b)
{
    ULONG x = *(const ULONG *)a, y = *(const ULONG *)b;

    return x < y ? -1 : x > y;
}

static void report(const char *name, int reps, ULONG ops)
{
    ULONG median;

    qsort(samples, reps, sizeof(samples[0]), cmp);
    median = samples[reps / 2];
    printf("%-14s median=%8lu ticks  %8lu us/rep  ops=%lu\n", name,
        (unsigned long)median,
        (unsigned long)(median * 1000ULL / (eclock_freq / 1000)),
        (unsigned long)ops);
}

static Object *build(void)
{
    return ApplicationObject,
        MUIA_Application_Title, "perfbench",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "perfbench",
            MUIA_Window_Screen, benchmark_screen,
            MUIA_Window_LeftEdge, 20,
            MUIA_Window_TopEdge, 20,
            fixed_geometry ? MUIA_Window_Width : TAG_IGNORE, 600,
            fixed_geometry ? MUIA_Window_Height : TAG_IGNORE, 400,
            WindowContents, root = VGroup,
                fixed_geometry ? MUIA_FixWidth : TAG_IGNORE, 600,
                fixed_geometry ? MUIA_FixHeight : TAG_IGNORE, 400,
                Child, HGroup, GroupFrameT("Controls"),
                    Child, ColGroup(2),
                        Child, Label2("Name:"),
                        Child, StringObject, StringFrame, End,
                        Child, Label1("Mode:"),
                        Child, CycleObject, MUIA_Cycle_Entries, cycles, End,
                        Child, Label1("Check:"),
                        Child, CheckMark(TRUE),
                        Child, Label2("Value:"),
                        Child, numeric = SliderObject,
                            MUIA_Numeric_Max, 1000, End,
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
                        Child, text = TextObject, TextFrame,
                            MUIA_Text_Contents, "Status", End,
                        Child, slider = SliderObject,
                            MUIA_Numeric_Max, 1000, End,
                        Child, GaugeObject, GaugeFrame,
                            MUIA_Gauge_Horiz, TRUE, End,
                        Child, HGroup,
                            Child, SimpleButton("_One"),
                            Child, SimpleButton("_Two"),
                            Child, SimpleButton("_Three"),
                            End,
                        Child, HGroup,
                            Child, SimpleButton("Four"),
                            Child, SimpleButton("Five"),
                            Child, SimpleButton("Six"),
                            End,
                        End,
                    End,
                Child, HGroup, GroupFrameT("Footer"),
                    Child, TextObject, MUIA_Text_Contents,
                        "\33cA centered footer line", End,
                    Child, HSpace(0),
                    Child, SimpleButton("OK"),
                    Child, SimpleButton("Cancel"),
                    End,
                End,
            End,
        End;
}

/* Repeat one phase for profiling: perfbench <phase> <count> */
static void profile_loop(const char *phase, int count)
{
    static char buf[32];
    int j;

    for (j = 0; j < count; j++)
    {
        if (!strcmp(phase, "text"))
        {
            sprintf(buf, "Status %d", j);
            set(text, MUIA_Text_Contents, buf);
        }
        else if (!strcmp(phase, "redraw"))
            MUI_Redraw(root, MADF_DRAWOBJECT);
        else if (!strcmp(phase, "relayout"))
        {
            DoMethod(root, MUIM_Group_InitChange);
            DoMethod(root, MUIM_Group_ExitChange);
        }
        else if (!strcmp(phase, "open"))
        {
            set(win, MUIA_Window_Open, TRUE);
            set(win, MUIA_Window_Open, FALSE);
        }
        else if (!strcmp(phase, "active"))
            set(list, MUIA_List_Active, (j * 7) % 200);
        else if (!strcmp(phase, "create"))
        {
            Object *a = build();

            if (a)
                MUI_DisposeObject(a);
        }
    }
}

int main(int argc, char **argv)
{
    struct timerequest tr;
    ULONG t, open = FALSE;
    int i, j, rc = 20;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        goto nolib;
    printf("library %d.%d eclock=%lu\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision, (unsigned long)(now(), eclock_freq));

    fixed_geometry = argc == 2 && !strcmp(argv[1], "screen-fixed");
    if (argc == 2 && (!strcmp(argv[1], "screen") || fixed_geometry))
    {
        benchmark_screen = OpenScreenTags(NULL, SA_Width, 640,
            SA_Height, fixed_geometry ? 480 : 256, SA_Depth, 2,
            SA_DisplayID, fixed_geometry ? HIRESLACE_KEY : HIRES_KEY,
            TAG_DONE);
        if (!benchmark_screen) goto done;
        printf("screen=%dx%d depth=%d repetitions=%d\n",
            benchmark_screen->Width, benchmark_screen->Height,
            benchmark_screen->RastPort.BitMap->Depth, REPS);
        {
            struct DimensionInfo dims = {0};
            ULONG mode = GetVPModeID(&benchmark_screen->ViewPort);
            if (!GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims),
                                   DTAG_DIMS, mode))
                goto done;
            printf("display-mode=%08lx nominal=%dx%d\n", mode,
                dims.Nominal.MaxX - dims.Nominal.MinX + 1,
                dims.Nominal.MaxY - dims.Nominal.MinY + 1);
            if (benchmark_screen->Width >
                    dims.Nominal.MaxX - dims.Nominal.MinX + 1 ||
                benchmark_screen->Height >
                    dims.Nominal.MaxY - dims.Nominal.MinY + 1)
            {
                puts("screen exceeds display dimensions");
                goto done;
            }
        }
    }

    if (argc > 2)
    {
        if (!(app = build()))
            goto done;
        DoMethod(list, MUIM_List_Insert, entries, -1,
            MUIV_List_Insert_Bottom);
        for (i = 0; i < 19; i++)
            DoMethod(list, MUIM_List_Insert, entries, -1,
                MUIV_List_Insert_Bottom);
        if (strcmp(argv[1], "open"))
            set(win, MUIA_Window_Open, TRUE);
        printf("profiling %s\n", argv[1]);
        fflush(stdout);
        /* Give an attached sampler time to start */
        Delay(250);
        profile_loop(argv[1], atoi(argv[2]));
        rc = 0;
        goto done;
    }

    /* Warm class caches before measuring. */
    if ((app = build()))
        MUI_DisposeObject(app);

    for (i = 0; i < REPS; i++)
    {
        t = now();
        app = build();
        if (!app)
            goto done;
        MUI_DisposeObject(app);
        samples[i] = now() - t;
    }
    report("create+dispose", REPS, 1);

    if (!(app = build()))
        goto done;
    DoMethod(slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
        numeric, 3, MUIM_Set, MUIA_Numeric_Value, MUIV_TriggerValue);

    for (i = 0; i < REPS; i++)
    {
        t = now();
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        set(win, MUIA_Window_Open, FALSE);
        samples[i] = now() - t;
        if (!open)
            goto done;
    }
    report("open+close", REPS, 1);

    set(win, MUIA_Window_Open, TRUE);
    DoMethod(app, MUIM_Application_InputBuffered);
    {
        struct Window *native = NULL;
        ULONG width = 0, height = 0, visible = 0;

        get(win, MUIA_Window_Window, &native);
        get(root, MUIA_Width, &width);
        get(root, MUIA_Height, &height);
        get(list, MUIA_List_Visible, &visible);
        if (!native) goto done;
        printf("window-position=%d,%d\n", native->LeftEdge, native->TopEdge);
        if (fixed_geometry &&
            (native->LeftEdge < 0 || native->TopEdge < 0 ||
             native->LeftEdge + native->Width > benchmark_screen->Width ||
             native->TopEdge + native->Height > benchmark_screen->Height))
        {
            puts("window exceeds screen dimensions");
            goto done;
        }
        printf("geometry window=%dx%d root=%lux%lu list-visible=%lu\n",
            native->Width, native->Height, (unsigned long)width,
            (unsigned long)height, (unsigned long)visible);
        if (fixed_geometry &&
            (width > native->Width - native->BorderLeft - native->BorderRight ||
             height > native->Height - native->BorderTop - native->BorderBottom))
        {
            puts("root exceeds window interior");
            goto done;
        }
    }

    for (i = 0; i < REPS; i++)
    {
        t = now();
        for (j = 0; j < 10; j++)
            MUI_Redraw(root, MADF_DRAWOBJECT);
        samples[i] = now() - t;
    }
    report("redraw x10", REPS, 10);

    for (i = 0; i < REPS; i++)
    {
        t = now();
        for (j = 0; j < 5; j++)
        {
            DoMethod(root, MUIM_Group_InitChange);
            DoMethod(root, MUIM_Group_ExitChange);
        }
        samples[i] = now() - t;
    }
    report("relayout x5", REPS, 5);

    for (i = 0; i < REPS; i++)
    {
        t = now();
        for (j = 0; j < 20; j++)
            DoMethod(list, MUIM_List_Insert, entries, -1,
                MUIV_List_Insert_Bottom);
        samples[i] = now() - t;
        if (i != REPS - 1)
            DoMethod(list, MUIM_List_Clear);
    }
    report("list insert200", REPS, 200);
    {
        ULONG count = 0;

        get(list, MUIA_List_Entries, &count);
        printf("list-entries=%lu\n", (unsigned long)count);
        if (count != 200) goto done;
    }

    for (i = 0; i < REPS; i++)
    {
        t = now();
        for (j = 0; j < 50; j++)
            set(list, MUIA_List_Active, (j * 7) % 200);
        samples[i] = now() - t;
    }
    report("list active x50", REPS, 50);

    for (i = 0; i < REPS; i++)
    {
        t = now();
        for (j = 0; j < 100; j++)
            set(slider, MUIA_Numeric_Value, (i * 100 + j) % 1000);
        samples[i] = now() - t;
    }
    report("notify x100", REPS, 100);

    for (i = 0; i < REPS; i++)
    {
        static char buf[32];

        t = now();
        for (j = 0; j < 100; j++)
        {
            sprintf(buf, "Status %d", j);
            set(text, MUIA_Text_Contents, buf);
        }
        samples[i] = now() - t;
    }
    report("text set x100", REPS, 100);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    if (benchmark_screen) CloseScreen(benchmark_screen);
    CloseLibrary(MUIMasterBase);
nolib:
    CloseDevice((struct IORequest *)&tr);
    printf("rc=%d\n", rc);
    return rc;
}

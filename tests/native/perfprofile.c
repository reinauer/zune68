/* Test-only profiler for a disposable guest. The plain run leaves all class
 * dispatchers untouched. The trace run temporarily wraps built-in dispatchers
 * and restores them before exit; never run it alongside other MUI clients.
 * Reuse the exact widget tree from perfbench, without changing its timings.
 * The create modes stop after construction/disposal: create and create-trace
 * use five warmups; create-cold uses one. create-combined times both operations
 * together after one warmup; create-combined-warm uses five.
 */
#define main perfbench_main
#include "perfbench.c"
#undef main

/* Compile a second binary with -DPROFILE_FOCUS=1 to time only Application
 * and Configdata construction with much less dispatcher instrumentation. */
#ifndef PROFILE_FOCUS
#define PROFILE_FOCUS 0
#endif
#ifndef PROFILE_NOFORWARD
#define PROFILE_NOFORWARD 0
#endif

typedef ULONG (*Dispatch)(struct IClass * __asm("a0"),
    Object * __asm("a2"), Msg __asm("a1"), APTR __asm("a6"));
static const char *class_names[] = {
    "Notify.mui", "Family.mui", "Application.mui", "Window.mui",
    "Area.mui", "Rectangle.mui", "Group.mui", "Text.mui", "String.mui",
    "Cycle.mui", "Radio.mui", "Image.mui", "Numeric.mui", "Slider.mui",
    "List.mui", "Listview.mui", "Prop.mui", "Scrollbar.mui", "Gauge.mui",
    "Configdata.mui", "Dataspace.mui", "Semaphore.mui", "Scrollbutton.mui"
};
#define NCLASS (sizeof(class_names) / sizeof(class_names[0]))
#define NSLOT 2048
static struct IClass *classes[NCLASS];
static Dispatch dispatchers[NCLASS];
static struct Stat {
    ULONG method, calls, inclusive, exclusive;
    int cls;
} stats[NSLOT];
struct Frame { struct Frame *parent; ULONG children; };
static struct Frame *frame;
static struct Task *owner;
static ULONG overflow, underflow;
static int tracing;

static ULONG intercept(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"), APTR base __asm("a6"))
{
    ULONG outer = PROFILE_FOCUS ? 0 : now(), start, end, result, slot, inclusive;
    unsigned int c;
    struct Frame current;
    struct Stat *stat;
    for (c = 0; c < NCLASS && classes[c] != cl; c++);
    if (c == NCLASS) return 0; /* All installed wrappers have a saved entry. */
    if (!tracing || (PROFILE_FOCUS && msg->MethodID != OM_NEW && msg->MethodID != OM_DISPOSE)
        || FindTask(NULL) != owner)
        return dispatchers[c](cl, obj, msg, base);
    if (PROFILE_FOCUS) outer = now();
    slot = (msg->MethodID ^ (msg->MethodID >> 12) ^ (c * 37)) % NSLOT;
    for (unsigned int n = 0;; n++, slot = (slot + 1) % NSLOT) {
        stat = &stats[slot];
        if (!stat->calls || (stat->cls == (int)c && stat->method == msg->MethodID))
            break;
        if (n == NSLOT - 1) {
            overflow++;
            return dispatchers[c](cl, obj, msg, base);
        }
    }
    stat->cls = c;
    stat->method = msg->MethodID;
    stat->calls++;
    current.parent = frame;
    current.children = 0;
    frame = &current;
    start = now();
    result = dispatchers[c](cl, obj, msg, base);
    end = now();
    inclusive = end - start;
    stat->inclusive += inclusive;
    if (inclusive >= current.children)
        stat->exclusive += inclusive - current.children;
    else underflow++;
    frame = current.parent;
    if (frame) frame->children += now() - outer;
    return result;
}

static void trace_begin(void)
{
    memset(stats, 0, sizeof(stats));
    overflow = underflow = 0;
    tracing = 1;
}

static void trace_end(const char *phase)
{
    tracing = 0;
    printf("trace phase=%s overflow=%lu underflow=%lu\n", phase, overflow, underflow);
    for (unsigned int s = 0; s < NSLOT; s++) {
        struct Stat *p = &stats[s];
        if (p->calls)
            printf("stat %s method=%08lx calls=%lu inclusive=%lu exclusive=%lu\n",
                class_names[p->cls], p->method, p->calls, p->inclusive, p->exclusive);
    }
}

static void timing(const char *name, ULONG *values, int count)
{
    qsort(values, count, sizeof(*values), cmp);
    printf("timing %s median=%lu min=%lu max=%lu n=%d\n",
        name, values[count/2], values[0], values[count-1], count);
}

static void active_batch(int mode)
{
    for (int j = 0; j < 50; j++) {
        if (PROFILE_NOFORWARD)
            SetAttrs(list, MUIA_Group_Forward, FALSE,
                MUIA_List_Active, mode == 0 ? (j * 7) % 200 : j % 2, TAG_DONE);
        else
            set(list, MUIA_List_Active, mode == 0 ? (j * 7) % 200 : j % 2);
    }
}

int main(int argc, char **argv)
{
    struct timerequest tr;
    ULONG create[15], dispose[15], ticks[15], t, open = 0;
    int create_only = argc == 2 && !strncmp(argv[1], "create", 6);
    int combined = create_only && strstr(argv[1], "combined");
    int extra_warmups = create_only && (!strcmp(argv[1], "create-cold") ||
        !strcmp(argv[1], "create-combined")) ? 0 : 4;
    int trace = argc == 2 && (!strcmp(argv[1], "trace") ||
        !strcmp(argv[1], "create-trace")), rc = 20;
    setvbuf(stdout, NULL, _IONBF, 0);
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0)) return 20;
    TimerBase = tr.tr_node.io_Device;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) goto device;
    printf("profile library=%u.%u eclock=%lu trace=%d\n",
        MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision,
        (unsigned long)(now(), eclock_freq), trace);
    if (PROFILE_FOCUS) puts("focus=Application,Configdata new/dispose");
    if (create_only) printf("creation-only combined=%d warmups=%d\n",
        combined, extra_warmups + 1);
    if (PROFILE_NOFORWARD) puts("experiment=active sets with Group_Forward=FALSE");
    benchmark_screen = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480,
        SA_Depth, 2, TAG_DONE);
    if (!benchmark_screen) goto done;
    fixed_geometry = 1;
    if (!(app = build())) goto done;
    MUI_DisposeObject(app); app = NULL;
    owner = FindTask(NULL);
    if (trace) {
        for (unsigned int c = 0; c < NCLASS; c++) {
            if (PROFILE_FOCUS && c != 2 && c != 19) continue;
            classes[c] = MUI_GetClass(class_names[c]);
            if (classes[c]) {
                dispatchers[c] = (Dispatch)classes[c]->cl_Dispatcher.h_Entry;
                classes[c]->cl_Dispatcher.h_Entry = (HOOKFUNC)intercept;
            }
        }
    }
    /* The first few application lifetimes can include filesystem/cache
     * activity. Warm this separately from both timings and trace totals. */
    for (int i = 0; i < extra_warmups; i++) {
        if (!(app = build())) goto done;
        MUI_DisposeObject(app); app = NULL;
    }
    for (int i = 0; i < (trace && !PROFILE_FOCUS ? 1 : 15); i++) {
        if (trace) trace_begin();
        t = now(); app = build();
        if (combined) {
            if (!app) goto done;
            MUI_DisposeObject(app); app = NULL;
            create[i] = now() - t;
            continue;
        }
        create[i] = now() - t;
        if (trace) trace_end("create");
        if (!app) goto done;
        if (trace) trace_begin();
        t = now(); MUI_DisposeObject(app); dispose[i] = now() - t; app = NULL;
        if (trace) trace_end("dispose");
    }
    timing(combined ? "create+dispose" : "create", create,
        trace && !PROFILE_FOCUS ? 1 : 15);
    if (!combined) timing("dispose", dispose, trace && !PROFILE_FOCUS ? 1 : 15);
    if (create_only) { rc = 0; goto done; }
    if (!(app = build())) goto done;
    for (int i = 0; i < 20; i++)
        DoMethod(list, MUIM_List_Insert, entries, -1, MUIV_List_Insert_Bottom);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open) goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    {
        ULONG width = 0, height = 0, visible = 0, count = 0;
        get(list, MUIA_Width, &width); get(list, MUIA_Height, &height);
        get(list, MUIA_List_Visible, &visible); get(list, MUIA_List_Entries, &count);
        printf("list width=%lu height=%lu visible=%lu entries=%lu\n",
            width, height, visible, count);
        if (count != 200 || visible < 2) goto done;
    }
    for (int mode = 0; mode < 3; mode++) {
        const char *name = mode == 0 ? "active-scroll50" :
            mode == 1 ? "active-visible50" : "active-closed50";
        if (mode == 2) set(win, MUIA_Window_Open, FALSE);
        for (int i = 0; i < (trace ? 1 : 15); i++) {
            set(list, MUIA_List_Active, 0);
            set(list, MUIA_List_First, 0);
            if (trace) trace_begin();
            t = now(); active_batch(mode); ticks[i] = now() - t;
            if (trace) trace_end(name);
        }
        timing(name, ticks, trace ? 1 : 15);
#if PROFILE_NOFORWARD
        {
            ULONG active = -1, first = -1;
            get(list, MUIA_List_Active, &active);
            get(list, MUIA_List_First, &first);
            printf("experiment-state %s active=%lu first=%lu\n", name, active, first);
            if (active != (mode == 0 ? 143 : 1)) goto done;
        }
#endif
    }
    rc = 0;
done:
    tracing = 0;
    if (app) MUI_DisposeObject(app);
    for (unsigned int c = 0; c < NCLASS; c++)
        if (classes[c]) classes[c]->cl_Dispatcher.h_Entry = (HOOKFUNC)dispatchers[c];
    for (unsigned int c = 0; c < NCLASS; c++)
        if (classes[c]) MUI_FreeClass(classes[c]);
    if (benchmark_screen) CloseScreen(benchmark_screen);
    CloseLibrary(MUIMasterBase);
device:
    CloseDevice((struct IORequest *)&tr);
    printf("rc=%d\n", rc);
    return rc;
}

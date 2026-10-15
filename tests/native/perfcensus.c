/* Test-only OS-call census for a disposable guest. It times the perfbench
 * creation, disposal, window open/close, redraw and relayout phases and,
 * in the count/time modes, wraps every function of perfcensus_lvos.h with
 * SetFunction to count calls and attribute inclusive/exclusive time per OS
 * function. The trace mode wraps the built-in class dispatchers instead,
 * as perfprofile does. Never run it alongside other MUI clients; all
 * patches are removed before exit.
 *
 * Time stamps come from a VBlank frame counter and the beam position
 * (256 units per raster line, about 64 us), which costs a few cycles per
 * read instead of a timer.device call. The calibration line relates
 * those units to EClock ticks.
 *
 * Build: m68k-amigaos-gcc -m68000 -msoft-float -noixemul -Os -std=gnu17 \
 *   -Wall -Wno-pointer-sign -I<SDK>/MUI/C/include -Wl,-u,___stkinit \
 *   perfcensus.c -o perfcensus -lamiga
 */
#define main perfbench_main
#include "perfbench.c"
#undef main
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <hardware/custom.h>
#include <hardware/intbits.h>

struct CensusFn { const char *lib; const char *name; WORD lvo; };
#include "perfcensus_lvos.h"
#define NFN (sizeof(census_fns) / sizeof(census_fns[0]))
#define NSHADOW 256

extern struct ExecBase *SysBase;

static struct Library *fn_base[NFN];
static APTR fn_orig[NFN];
static UBYTE fn_patched[NFN];
static ULONG fn_calls[NFN], fn_incl[NFN], fn_excl[NFN], fn_top[NFN];
/* count mode: return addresses of the callers of selected functions */
#define NCALLER 4096
static struct { ULONG ret; UWORD idx; UWORD count; } callers[NCALLER];
static UBYTE fn_track[NFN];
static ULONG caller_overflow;
static const char *tracked[] = { "AllocMem", "AllocVec", "FreeMem", "FreeVec",
    "NextTagItem", "FindTagItem", "GetTagData", "GetAttr", "SetAttrsA",
    "NewObjectA", "Draw", "WritePixel", "RectFill", "SetAPen", "Move", "Text",
    "TextLength", "InstallClipRegion", "StrToLong", "FilePart", "ParseIX",
    "Open", "StartNotify", "UDivMod32", "SMult32", "SDivMod32", "ObtainSemaphore",
    "CopyMem", "Stricmp", "SetFont", "TypeOfMem", "LockLayer", "GetScreenDrawInfo",
    "SizeWindow", "WindowLimits", "OpenFont", "MapANSI", "RemTailMinList",
    "InsertMinNode", "AddTailMinList", "CallHookPkt", "SetSoftStyle", NULL };
static UWORD *stubs;
static struct { ULONG ret, idx, start, children; } shadow[NSHADOW];
static int depth, mode_time, active, census_overflow;
static struct Task *owner;
static volatile ULONG vb_frames;
static struct Interrupt vb_int;
static struct Library *opened[16];
static int nopened;

extern void census_common(void);
extern void census_exit(void);
extern void vb_server(void);

__asm__("\n"
"    .text\n"
"    .globl _census_common\n"
"_census_common:\n"
"    movem.l d0-d1/a0-a1,-(sp)\n"
"    lea 16(sp),a0\n"
"    jsr _census_enter\n"
"    movem.l (sp)+,d0-d1/a0-a1\n"
"    rts\n"
"    .globl _census_exit\n"
"_census_exit:\n"
"    subq.l #4,sp\n"
"    movem.l d0-d1/a0-a1,-(sp)\n"
"    jsr _census_leave\n"
"    move.l d0,16(sp)\n"
"    movem.l (sp)+,d0-d1/a0-a1\n"
"    rts\n"
"    .globl _vb_server\n"
"_vb_server:\n"
"    addq.l #1,_vb_frames\n"
"    moveq #0,d0\n"
"    rts\n");

static ULONG vticks(void)
{
    volatile struct Custom *custom = (volatile struct Custom *)0xdff000;
    ULONG f1, f2;
    UWORD vp, vhp;

    UWORD pending;

    do {
        f1 = vb_frames;
        vp = custom->vposr;
        vhp = custom->vhposr;
        pending = custom->intreqr & INTF_VERTB;
        f2 = vb_frames;
    } while (f1 != f2);
    /* Exec clears the request bit before running the VERTB servers, so a
     * pending request means the counter has not yet caught up with the
     * beam position of the new field. */
    if (pending)
        f1++;
    return f1 * (313UL * 256) + ((((ULONG)(vp & 7) << 8) | (vhp >> 8)) << 8)
        + (vhp & 0xff);
}

static ULONG clock_check(void)
{
    ULONG last = vticks(), t, violations = 0, small = 0, line = 0, i, worst = 0;

    for (i = 0; i < 400000; i++) {
        t = vticks();
        if ((LONG)(t - last) < 0) {
            violations++;
            if (last - t < 16)
                small++;
            else if (last - t < 4096)
                line++;
            if (last - t > worst)
                worst = last - t;
        }
        last = t;
    }
    printf("clock-check below16=%lu below4096=%lu worst=%lu\n",
        (unsigned long)small, (unsigned long)line, (unsigned long)worst);
    return violations;
}

/* slots[0] holds the stub index and becomes the real entry point; slots[1]
 * is the caller's return address and is redirected to census_exit while
 * timing. Called by census_common with all scratch registers saved. */
void census_enter(ULONG *slots __asm("a0"))
{
    ULONG idx = slots[0];

    slots[0] = (ULONG)fn_orig[idx];
    if (!active || SysBase->ThisTask != owner)
        return;
    fn_calls[idx]++;
    if (!mode_time) {
        ULONG ret, slot, n;

        if (!fn_track[idx])
            return;
        ret = slots[1];
        slot = ((ret >> 1) ^ (ret >> 13) ^ idx) % NCALLER;
        for (n = 0; n < NCALLER; n++, slot = (slot + 1) % NCALLER) {
            if (!callers[slot].count) {
                callers[slot].ret = ret;
                callers[slot].idx = idx;
                callers[slot].count = 1;
                return;
            }
            if (callers[slot].ret == ret && callers[slot].idx == idx) {
                if (callers[slot].count < 65535)
                    callers[slot].count++;
                return;
            }
        }
        caller_overflow++;
        return;
    }
    if (depth >= NSHADOW) {
        census_overflow++;
        return;
    }
    shadow[depth].ret = slots[1];
    shadow[depth].idx = idx;
    shadow[depth].children = 0;
    depth++;
    slots[1] = (ULONG)census_exit;
    shadow[depth - 1].start = vticks();
}

ULONG census_leave(void)
{
    ULONG end = vticks(), t, idx;

    depth--;
    t = (LONG)(end - shadow[depth].start) < 0 ? 0 : end - shadow[depth].start;
    idx = shadow[depth].idx;
    fn_incl[idx] += t;
    fn_excl[idx] += t > shadow[depth].children ? t - shadow[depth].children : 0;
    if (depth)
        shadow[depth - 1].children += t;
    else
        fn_top[idx] += t;
    return shadow[depth].ret;
}

static struct Library *census_base(const char *name)
{
    int i;

    if (!strcmp(name, "exec.library"))
        return (struct Library *)SysBase;
    for (i = 0; i < nopened; i++)
        if (!strcmp(opened[i]->lib_Node.ln_Name, name))
            return opened[i];
    if (nopened < 16 && (opened[nopened] = OpenLibrary(name, 0)))
        return opened[nopened++];
    return NULL;
}

static int census_install(void)
{
    unsigned int i;

    stubs = AllocMem(NFN * 12, MEMF_PUBLIC | MEMF_CLEAR);
    if (!stubs)
        return 0;
    for (i = 0; i < NFN; i++) {
        UWORD *p = stubs + i * 6;

        p[0] = 0x2f3c; /* move.l #i,-(sp) */
        p[1] = i >> 16;
        p[2] = i;
        p[3] = 0x4ef9; /* jmp census_common */
        p[4] = (ULONG)census_common >> 16;
        p[5] = (ULONG)census_common;
    }
    CacheClearU();
    for (i = 0; i < NFN; i++) {
        const char **t;

        for (t = tracked; *t; t++)
            if (!strcmp(*t, census_fns[i].name))
                fn_track[i] = 1;
        fn_base[i] = census_base(census_fns[i].lib);
        if (!fn_base[i])
            continue;
        fn_orig[i] = SetFunction(fn_base[i], census_fns[i].lvo,
            (APTR)(stubs + i * 6));
        fn_patched[i] = 1;
    }
    return 1;
}

static void census_remove(void)
{
    unsigned int i;

    for (i = NFN; i-- > 0;)
        if (fn_patched[i]) {
            SetFunction(fn_base[i], census_fns[i].lvo, fn_orig[i]);
            fn_patched[i] = 0;
        }
    CacheClearU();
    if (stubs)
        FreeMem(stubs, NFN * 12);
    stubs = NULL;
    while (nopened)
        CloseLibrary(opened[--nopened]);
}

/* Dispatcher wrappers as in perfprofile.c */
typedef ULONG (*Dispatch)(struct IClass * __asm("a0"),
    Object * __asm("a2"), Msg __asm("a1"), APTR __asm("a6"));
static const char *class_names[] = {
    "Notify.mui", "Family.mui", "Application.mui", "Window.mui",
    "Area.mui", "Rectangle.mui", "Group.mui", "Text.mui", "String.mui",
    "Cycle.mui", "Radio.mui", "Image.mui", "Numeric.mui", "Slider.mui",
    "List.mui", "Listview.mui", "Prop.mui", "Scrollbar.mui", "Gauge.mui",
    "Configdata.mui", "Dataspace.mui", "Semaphore.mui", "Scrollbutton.mui",
    "Menustrip.mui", "Menu.mui", "Menuitem.mui", "Boopsi.mui", "Frameadjust.mui"
};
#define NCLASS (sizeof(class_names) / sizeof(class_names[0]))
#define NSLOT 2048
static struct IClass *classes[NCLASS];
static Dispatch dispatchers[NCLASS];
static struct Stat { ULONG method, calls, inclusive, exclusive; int cls; } stats[NSLOT];
struct Frame { struct Frame *parent; ULONG children; };
static struct Frame *frame;
static ULONG overflow, underflow;

static ULONG intercept(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"), APTR base __asm("a6"))
{
    ULONG start, end, result, slot, inclusive;
    unsigned int c, n;
    struct Frame current;
    struct Stat *stat;

    for (c = 0; c < NCLASS && classes[c] != cl; c++);
    if (c == NCLASS) return 0;
    if (!active || SysBase->ThisTask != owner)
        return dispatchers[c](cl, obj, msg, base);
    slot = (msg->MethodID ^ (msg->MethodID >> 12) ^ (c * 37)) % NSLOT;
    for (n = 0;; n++, slot = (slot + 1) % NSLOT) {
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
    start = vticks();
    result = dispatchers[c](cl, obj, msg, base);
    end = vticks();
    inclusive = (LONG)(end - start) < 0 ? 0 : end - start;
    stat->inclusive += inclusive;
    if (inclusive >= current.children)
        stat->exclusive += inclusive - current.children;
    else
        underflow++;
    frame = current.parent;
    if (frame)
        frame->children += vticks() - start;
    return result;
}

static void trace_install(void)
{
    unsigned int c;

    for (c = 0; c < NCLASS; c++) {
        classes[c] = MUI_GetClass(class_names[c]);
        if (classes[c]) {
            dispatchers[c] = (Dispatch)classes[c]->cl_Dispatcher.h_Entry;
            classes[c]->cl_Dispatcher.h_Entry = (HOOKFUNC)intercept;
        }
    }
}

static void trace_remove(void)
{
    unsigned int c;

    for (c = 0; c < NCLASS; c++)
        if (classes[c]) {
            classes[c]->cl_Dispatcher.h_Entry = (HOOKFUNC)dispatchers[c];
            MUI_FreeClass(classes[c]);
            classes[c] = NULL;
        }
}

static ULONG phase_eclock, phase_vticks;

static void phase_begin(void)
{
    memset(fn_calls, 0, sizeof(fn_calls));
    memset(fn_incl, 0, sizeof(fn_incl));
    memset(fn_excl, 0, sizeof(fn_excl));
    memset(fn_top, 0, sizeof(fn_top));
    memset(stats, 0, sizeof(stats));
    memset(callers, 0, sizeof(callers));
    overflow = underflow = census_overflow = caller_overflow = 0;
    depth = 0;
    frame = NULL;
    active = 1;
    phase_vticks = vticks();
    phase_eclock = now();
}

static void phase_end(const char *name)
{
    ULONG e = now() - phase_eclock, v = vticks() - phase_vticks;
    unsigned int i;

    active = 0;
    printf("phase %s eclock=%lu vticks=%lu depth=%d overflow=%lu/%lu underflow=%lu\n",
        name, (unsigned long)e, (unsigned long)v, depth,
        (unsigned long)census_overflow, (unsigned long)overflow,
        (unsigned long)underflow);
    for (i = 0; i < NFN; i++)
        if (fn_calls[i])
            printf("fn %s %s calls=%lu incl=%lu excl=%lu top=%lu\n",
                census_fns[i].lib, census_fns[i].name,
                (unsigned long)fn_calls[i], (unsigned long)fn_incl[i],
                (unsigned long)fn_excl[i], (unsigned long)fn_top[i]);
    for (i = 0; i < NCALLER; i++)
        if (callers[i].count)
            printf("caller %s %08lx %u\n", census_fns[callers[i].idx].name,
                (unsigned long)callers[i].ret, callers[i].count);
    if (caller_overflow)
        printf("caller-overflow %lu\n", (unsigned long)caller_overflow);
    for (i = 0; i < NSLOT; i++)
        if (stats[i].calls)
            printf("stat %s method=%08lx calls=%lu inclusive=%lu exclusive=%lu\n",
                class_names[stats[i].cls], (unsigned long)stats[i].method,
                (unsigned long)stats[i].calls, (unsigned long)stats[i].inclusive,
                (unsigned long)stats[i].exclusive);
}

static void timing(const char *name, ULONG *values, int count)
{
    qsort(values, count, sizeof(*values), cmp);
    printf("timing %s median=%lu min=%lu max=%lu n=%d\n", name,
        (unsigned long)values[count / 2], (unsigned long)values[0],
        (unsigned long)values[count - 1], count);
}

static void open_close(void)
{
    ULONG open = 0;

    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    set(win, MUIA_Window_Open, FALSE);
}

static void redraw10(void)
{
    int j;

    for (j = 0; j < 10; j++)
        MUI_Redraw(root, MADF_DRAWOBJECT);
}

static void relayout5(void)
{
    int j;

    for (j = 0; j < 5; j++) {
        DoMethod(root, MUIM_Group_InitChange);
        DoMethod(root, MUIM_Group_ExitChange);
    }
}

int main(int argc, char **argv)
{
    struct timerequest tr;
    ULONG create[15], dispose[15], ticks[15], t, open = 0, e0, v0;
    const char *mode = argc == 2 ? argv[1] : "plain";
    int plain = !strcmp(mode, "plain"), count = !strcmp(mode, "count");
    int trace = !strcmp(mode, "trace"), i, rc = 20;

    int loop = !strncmp(mode, "loop-", 5);

    mode_time = !strcmp(mode, "time");
    if (!plain && !count && !trace && !mode_time && !loop && strcmp(mode, "clock"))
        return 20;
    setvbuf(stdout, NULL, _IONBF, 0);
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        goto device;
    printf("census library=%u.%u eclock=%lu mode=%s functions=%u\n",
        MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision,
        (unsigned long)(now(), eclock_freq), mode, (unsigned int)NFN);
    printf("muimaster base=%08lx layout-vector=%08lx\n", (unsigned long)MUIMasterBase,
        (unsigned long)*(ULONG *)((UBYTE *)MUIMasterBase - 126 + 2));
    vb_int.is_Node.ln_Type = NT_INTERRUPT;
    vb_int.is_Node.ln_Pri = 127;
    vb_int.is_Node.ln_Name = "perfcensus";
    vb_int.is_Code = (void (*)())vb_server;
    AddIntServer(INTB_VERTB, &vb_int);
    e0 = now();
    v0 = vticks();
    Delay(50);
    printf("calibration eclock=%lu vticks=%lu\n",
        (unsigned long)(now() - e0), (unsigned long)(vticks() - v0));
    printf("clock-check violations=%lu\n", (unsigned long)clock_check());
    owner = FindTask(NULL);
    if (!strcmp(mode, "clock")) {
        rc = 0;
        goto done;
    }
    benchmark_screen = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480,
        SA_Depth, 2, TAG_DONE);
    if (!benchmark_screen)
        goto done;
    fixed_geometry = 1;
    /* Warm class caches and first-use resources, as perfbench does. */
    for (i = 0; i < 5; i++) {
        if (!(app = build()))
            goto done;
        MUI_DisposeObject(app);
        app = NULL;
    }
    if (loop) {
        /* Repeat one phase for an external PC sampler: loop-<phase> */
        const char *phase = mode + 5;
        int n = !strcmp(phase, "create") ? 1500 : !strcmp(phase, "open") ? 300 :
            !strcmp(phase, "redraw") ? 600 : !strcmp(phase, "relayout") ? 300 : 0;

        if (!n)
            goto done;
        if (strcmp(phase, "create")) {
            if (!(app = build()))
                goto done;
            DoMethod(slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
                numeric, 3, MUIM_Set, MUIA_Numeric_Value, MUIV_TriggerValue);
            if (strcmp(phase, "open")) {
                set(win, MUIA_Window_Open, TRUE);
                DoMethod(app, MUIM_Application_InputBuffered);
            }
        }
        printf("looping %s x%d\n", phase, n);
        Delay(400);
        t = now();
        for (i = 0; i < n; i++) {
            if (!strcmp(phase, "create")) {
                Object *a = build();
                if (a) MUI_DisposeObject(a);
            } else if (!strcmp(phase, "open"))
                open_close();
            else if (!strcmp(phase, "redraw"))
                redraw10();
            else
                relayout5();
        }
        printf("looped %s eclock=%lu\n", phase, (unsigned long)(now() - t));
        rc = 0;
        goto done;
    }
    if (trace)
        trace_install();
    else if (!plain && !census_install())
        goto done;

    for (i = 0; i < (plain ? 15 : 1); i++) {
        if (!plain) phase_begin();
        t = now();
        app = build();
        create[i] = now() - t;
        if (!plain) phase_end("create");
        if (!app)
            goto done;
        if (!plain) phase_begin();
        t = now();
        MUI_DisposeObject(app);
        dispose[i] = now() - t;
        app = NULL;
        if (!plain) phase_end("dispose");
    }
    if (plain) {
        timing("create", create, 15);
        timing("dispose", dispose, 15);
    }

    if (!(app = build()))
        goto done;
    DoMethod(slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
        numeric, 3, MUIM_Set, MUIA_Numeric_Value, MUIV_TriggerValue);
    if (!plain)
        open_close(); /* first open pays one-time costs; measure the second */
    for (i = 0; i < (plain ? 15 : 1); i++) {
        if (!plain) phase_begin();
        t = now();
        open_close();
        ticks[i] = now() - t;
        if (!plain) phase_end("open+close");
    }
    if (plain)
        timing("open+close", ticks, 15);

    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    {
        struct Window *native = NULL;
        ULONG width = 0, height = 0;

        get(win, MUIA_Window_Window, &native);
        get(root, MUIA_Width, &width);
        get(root, MUIA_Height, &height);
        if (!native)
            goto done;
        printf("geometry window=%dx%d root=%lux%lu\n", native->Width,
            native->Height, (unsigned long)width, (unsigned long)height);
    }
    if (!plain)
        redraw10();
    for (i = 0; i < (plain ? 15 : 1); i++) {
        if (!plain) phase_begin();
        t = now();
        redraw10();
        ticks[i] = now() - t;
        if (!plain) phase_end("redraw10");
    }
    if (plain)
        timing("redraw10", ticks, 15);

    if (!plain)
        relayout5();
    for (i = 0; i < (plain ? 15 : 1); i++) {
        if (!plain) phase_begin();
        t = now();
        relayout5();
        ticks[i] = now() - t;
        if (!plain) phase_end("relayout5");
    }
    if (plain)
        timing("relayout5", ticks, 15);
    rc = 0;
done:
    active = 0;
    if (app)
        MUI_DisposeObject(app);
    trace_remove();
    census_remove();
    if (benchmark_screen)
        CloseScreen(benchmark_screen);
    RemIntServer(INTB_VERTB, &vb_int);
    CloseLibrary(MUIMasterBase);
device:
    CloseDevice((struct IORequest *)&tr);
    printf("rc=%d\n", rc);
    return rc;
}

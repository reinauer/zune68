/* Observe exposure dispatch, including flags modified by Area's Draw method.
 * No timing claim: this compares which custom children receive drawing work.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/layers.h>
#include <graphics/regions.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define MAX_CALLS 64
#ifndef REFRESH_CLASS
#define REFRESH_CLASS MUIC_Area
#endif
#ifndef REFRESH_GEOMETRY
#define REFRESH_GEOMETRY 0
#endif
#ifndef REFRESH_EXTRA_FLAGS
#define REFRESH_EXTRA_FLAGS 0
#endif
#ifndef REFRESH_FORCE_SIMPLE
#define REFRESH_FORCE_SIMPLE 0
#endif
/* Native internal flag, not a named public flag in the original SDK. */
#define PROBE_DRAWALL_BIT 0x80000000UL
struct Instance { ULONG index; };
struct ChildState { LONG left, top, width, height; ULONG count, before, after; };
struct DrawCall { ULONG child, before, after; };
static struct ChildState state[3];
static struct DrawCall trace[MAX_CALLS];
static ULONG trace_count;
struct Geometry {
    ULONG layer, damage, clip, layerflags, mriflags;
    BOOL damage_nonempty, clip_nonempty;
    struct Rectangle bounds, damage_bounds, clip_bounds;
    LONG object_left, object_top;
};
static struct Geometry after_geometry;
static BOOL geometry_saved;

static void capture_geometry(struct Geometry *g, Object *obj)
{
    struct MUI_RenderInfo *mri = muiRenderInfo(obj);
    struct Layer *layer;
    memset(g, 0, sizeof(*g));
    g->object_left = _left(obj);
    g->object_top = _top(obj);
    if (!mri) return;
    g->mriflags = mri->mri_Flags;
    if (!mri->mri_RastPort) return;
    layer = mri->mri_RastPort->Layer;
    g->layer = (ULONG)layer;
    if (!layer) return;
    g->layerflags = layer->Flags;
    g->bounds = layer->bounds;
    g->damage = (ULONG)layer->DamageList;
    g->clip = (ULONG)layer->ClipRegion;
    if (layer->DamageList) {
        g->damage_bounds = layer->DamageList->bounds;
        g->damage_nonempty = layer->DamageList->RegionRectangle != NULL;
    }
    if (layer->ClipRegion) {
        g->clip_bounds = layer->ClipRegion->bounds;
        g->clip_nonempty = layer->ClipRegion->RegionRectangle != NULL;
    }
}

static void print_geometry(const char *label, const struct Geometry *g)
{
    printf("geometry=%s object-left-top=%ld,%ld mri-flags=%08lx "
           "layer=%08lx flags=%08lx bounds=%d,%d,%d,%d\n", label,
           g->object_left, g->object_top, g->mriflags, g->layer,
           g->layerflags, g->bounds.MinX, g->bounds.MinY,
           g->bounds.MaxX, g->bounds.MaxY);
    printf("damage=%08lx nonempty=%d bounds=%d,%d,%d,%d "
           "clip=%08lx nonempty=%d bounds=%d,%d,%d,%d\n",
           g->damage, g->damage_nonempty, g->damage_bounds.MinX,
           g->damage_bounds.MinY, g->damage_bounds.MaxX, g->damage_bounds.MaxY,
           g->clip, g->clip_nonempty, g->clip_bounds.MinX,
           g->clip_bounds.MinY, g->clip_bounds.MaxX, g->clip_bounds.MaxY);
}

static ULONG dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_AskMinMax) {
        struct MUI_MinMax *size;
        ULONG result = DoSuperMethodA(cl, obj, msg);
        /* The superclass can populate MinMaxInfo in the message. */
        size = ((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        size->MinWidth += 64;
        size->DefWidth += 64;
        size->MaxWidth += 64;
        size->MinHeight += 40;
        size->DefHeight += 40;
        size->MaxHeight += 40;
        return result;
    }
    if (msg->MethodID == MUIM_Draw) {
        struct Instance *data = INST_DATA(cl, obj);
        struct MUIP_Draw *draw = (struct MUIP_Draw *)msg;
        ULONG index = data->index, before = draw->flags, result;
        BOOL capture = REFRESH_GEOMETRY && index == 0 && !geometry_saved;
        result = DoSuperMethodA(cl, obj, msg);
        if (capture) {
            capture_geometry(&after_geometry, obj);
            geometry_saved = TRUE;
        }
        if (index < 3) {
            state[index].left = _mleft(obj);
            state[index].top = _mtop(obj);
            state[index].width = _mwidth(obj);
            state[index].height = _mheight(obj);
            state[index].count++;
            state[index].before |= before;
            state[index].after |= draw->flags;
            if (trace_count < MAX_CALLS) {
                trace[trace_count].child = index;
                trace[trace_count].before = before;
                trace[trace_count].after = draw->flags;
            }
            trace_count++;
        }
        return result;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void pump(Object *app, ULONG ticks)
{
    ULONG signals = 0;
    for (ULONG i = 0; i < ticks; i++) {
        DoMethod(app, MUIM_Application_NewInput, &signals);
        Delay(1);
    }
}

static void reset_counts(void)
{
    for (ULONG i = 0; i < 3; i++)
        state[i].count = state[i].before = state[i].after = 0;
    trace_count = 0;
    geometry_saved = FALSE;
}

static void report(const char *phase, struct Window *window)
{
    printf("phase=%s total=%lu window-flags=%08lx layer-flags=%08lx\n",
           phase, trace_count, window->Flags, (ULONG)window->WLayer->Flags);
    for (ULONG i = 0; i < 3; i++)
        printf("child=%lu draws=%lu incoming-or=%08lx after-super-or=%08lx "
               "inner=%ld,%ld,%ld,%ld\n", i, state[i].count,
               state[i].before, state[i].after, state[i].left,
               state[i].top, state[i].width, state[i].height);
    for (ULONG i = 0; i < trace_count && i < MAX_CALLS; i++)
        printf("call=%lu child=%lu incoming=%08lx after-super=%08lx\n",
               i, trace[i].child, trace[i].before, trace[i].after);
    if (trace_count > MAX_CALLS) puts("TRACE TRUNCATED");
    if (REFRESH_GEOMETRY && geometry_saved) {
        print_geometry("first-child-after-super", &after_geometry);
    }
    fflush(stdout);
}

static void draw_phase(Object *app, struct Window *host, Object *target,
    const char *phase, ULONG flags, BOOL direct)
{
    reset_counts();
    printf("request=%s flags=%08lx via=%s\n", phase, flags,
           direct ? "DoMethod-Draw" : "MUI_Redraw");
    fflush(stdout);
    if (direct) DoMethod(target, MUIM_Draw, flags);
    else MUI_Redraw(target, flags);
    pump(app, 5);
    report(phase, host);
}

int main(void)
{
    struct MUI_CustomClass *cc = NULL;
    Object *app = NULL, *window = NULL, *group = NULL, *children[3] = {0};
    Object *root_group = NULL; /* Borrowed after successful Window construction. */
    struct Window *host = NULL, *cover = NULL;
    struct Screen *screen;
    LONG x, y, width = 24, height = 16;
    ULONG opened = 0;
    int result = 20;

    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    printf("refreshdispatch=2 library=%u.%u children=3 geometry=%d class=%s\n",
           MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision,
           REFRESH_GEOMETRY, REFRESH_CLASS);
    puts("SDK38 exposes mri_Flags; mri_ClipRect is private and not sampled.");
    fflush(stdout);
    cc = MUI_CreateCustomClass(NULL, (char *)REFRESH_CLASS, NULL,
                              sizeof(struct Instance), (APTR)dispatch);
    if (!cc) goto done;
    for (ULONG i = 0; i < 3; i++) {
        struct Instance *data;
        children[i] = NewObject(cc->mcc_Class, NULL, TAG_DONE);
        if (!children[i]) goto done;
        data = INST_DATA(cc->mcc_Class, children[i]);
        data->index = i;
    }
    group = MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, TRUE,
        MUIA_Group_HorizSpacing, 8,
        MUIA_Group_Child, (ULONG)children[0],
        MUIA_Group_Child, (ULONG)children[1],
        MUIA_Group_Child, (ULONG)children[2], TAG_DONE);
    if (!group) { memset(children, 0, sizeof(children)); goto done; }
    root_group = group;
    window = MUI_NewObject((char *)MUIC_Window,
        MUIA_Window_Title, (ULONG)"Refresh dispatch probe",
        MUIA_Window_PublicScreen, (ULONG)"Workbench",
        MUIA_Window_LeftEdge, 40, MUIA_Window_TopEdge, 50,
        MUIA_Window_RootObject, (ULONG)group, TAG_DONE);
    group = NULL;
    if (!window) { memset(children, 0, sizeof(children)); goto done; }
    app = MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Title, (ULONG)"Refresh dispatch probe",
        MUIA_Application_Base, (ULONG)"REFRESHDISPATCH",
        MUIA_Application_Window, (ULONG)window, TAG_DONE);
    if (!app) { window = NULL; memset(children, 0, sizeof(children)); goto done; }
    if (REFRESH_FORCE_SIMPLE) {
        ULONG value = 0xffffffffUL, set_result, get_result;
        /* Original-reference private configuration mapping, verified on
         * MUI 3.8/3.9. This is not the native implementation's item ID.
         */
        set_result = DoMethod(app, MUIM_Application_SetConfigItem, 28UL, 1UL);
        get_result = DoMethod(app, MUIM_GetConfigItem, 28UL, &value);
        printf("original-reference-private-setting item=28 requested=1 "
               "set-result=%08lx get-result=%08lx readback=%08lx\n",
               set_result, get_result, value);
        fflush(stdout);
        if (!get_result || value != 1) {
            puts("FAIL: original-reference simple-refresh setting not confirmed");
            goto done;
        }
    }
    puts("stage=objects-created-opening");
    fflush(stdout);
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    puts("stage=open-returned");
    fflush(stdout);
    GetAttr(MUIA_Window_Open, window, &opened);
    GetAttr(MUIA_Window_Window, window, (ULONG *)&host);
    if (!opened || !host || !host->WLayer) goto done;
    pump(app, 20);
    report("initial", host);
    for (ULONG i = 0; i < 3; i++) if (!state[i].count) goto done;
    if (!(host->Flags & WFLG_SIMPLE_REFRESH)) {
        puts("UNSUPPORTED: host is not simple-refresh; zero exposure calls "
             "would not demonstrate damage culling.");
        result = 5;
        goto done;
    }
    screen = host->WScreen;
    x = host->LeftEdge + state[0].left + 8;
    y = host->TopEdge + state[0].top + 8;
    if (state[0].width < width + 16 || state[0].height < height + 16 ||
        x < 0 || y < 0 || x + width > screen->Width ||
        y + height > screen->Height) {
        puts("FAIL: overlay does not fit inside first child and screen");
        goto done;
    }
    printf("host=%ld,%ld,%ld,%ld overlay=%ld,%ld,%ld,%ld refresh=%s\n",
           (LONG)host->LeftEdge, (LONG)host->TopEdge,
           (LONG)host->Width, (LONG)host->Height, x, y, width, height,
           host->Flags & WFLG_SIMPLE_REFRESH ? "simple" : "smart/superbitmap");
    reset_counts();
    cover = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)screen,
        WA_Left, x, WA_Top, y, WA_Width, width, WA_Height, height,
        WA_Borderless, TRUE, WA_Activate, FALSE, WA_SmartRefresh, TRUE,
        WA_IDCMP, 0, WA_RMBTrap, TRUE, TAG_DONE);
    if (!cover) goto done;
    SetAPen(cover->RPort, 0);
    RectFill(cover->RPort, 0, 0, width - 1, height - 1);
    pump(app, 5);
    report("covered", host);
    reset_counts();
    CloseWindow(cover);
    cover = NULL;
    pump(app, 20);
    report("exposure-first-child-only", host);
    reset_counts();
    MUI_Redraw(children[2], MADF_DRAWOBJECT);
    pump(app, 5);
    report("explicit-third-child", host);
    if (!state[2].count) {
        puts("FAIL: explicit child redraw did not reach its dispatcher");
        goto done;
    }
    if (REFRESH_EXTRA_FLAGS) {
        puts("Extra flags: bit31 is native-private, sampled as a raw request; "
             "no original MUI meaning is assumed.");
        draw_phase(app, host, children[2], "child-update", MADF_DRAWUPDATE, FALSE);
        draw_phase(app, host, children[2], "child-bit31", PROBE_DRAWALL_BIT, FALSE);
        draw_phase(app, host, children[2], "child-object-bit31",
                   MADF_DRAWOBJECT | PROBE_DRAWALL_BIT, FALSE);
        draw_phase(app, host, root_group, "parent-object", MADF_DRAWOBJECT, FALSE);
        draw_phase(app, host, root_group, "parent-bit31", PROBE_DRAWALL_BIT, FALSE);
        draw_phase(app, host, root_group, "parent-object-bit31",
                   MADF_DRAWOBJECT | PROBE_DRAWALL_BIT, FALSE);
        /* Direct method calls isolate the superclass's message changes from
         * preparation performed by the public MUI_Redraw entry point.
         */
        draw_phase(app, host, children[2], "direct-child-zero", 0, TRUE);
        draw_phase(app, host, children[2], "direct-child-object", MADF_DRAWOBJECT, TRUE);
        draw_phase(app, host, children[2], "direct-child-update", MADF_DRAWUPDATE, TRUE);
    }
    result = 0;
done:
    if (cover) CloseWindow(cover);
    if (app) MUI_DisposeObject(app);
    else if (window) MUI_DisposeObject(window);
    else if (group) MUI_DisposeObject(group);
    else for (ULONG i = 0; i < 3; i++)
        if (children[i]) MUI_DisposeObject(children[i]);
    if (cc) MUI_DeleteCustomClass(cc);
    CloseLibrary(MUIMasterBase);
    printf("result=%d\n", result);
    return result;
}

/* Public-API workload. Build unchanged against Zune68 or the MUI 3.8 SDK.
 * Run in separate OS installations with the same compiler/CPU settings.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define ABI_CHECK(name, expr) typedef char name[(expr) ? 1 : -1]
ABI_CHECK(long_is_32, sizeof(ULONG) == 4);
ABI_CHECK(tag_is_8, sizeof(struct TagItem) == 8);
ABI_CHECK(minmax_is_12, sizeof(struct MUI_MinMax) == 12);
ABI_CHECK(area_renderinfo, offsetof(struct MUI_AreaData, mad_RenderInfo) == 0);
ABI_CHECK(area_box, offsetof(struct MUI_AreaData, mad_Box) == 24);
ABI_CHECK(render_window, offsetof(struct MUI_RenderInfo, mri_Window) == 16);
ABI_CHECK(render_flags, offsetof(struct MUI_RenderInfo, mri_Flags) == 24);
ABI_CHECK(custom_class, offsetof(struct MUI_CustomClass, mcc_Class) == 24);
ABI_CHECK(notify_params, offsetof(struct MUIP_Notify, FollowParams) == 16);

static ULONG comparisons, events[8], event_count;

static ULONG record(struct Hook *hook __asm("a0"),
    Object *object __asm("a2"), ULONG *args __asm("a1"))
{
    if (event_count < 8) events[event_count++] = args[0];
    return 0;
}

static LONG compare(struct Hook *hook __asm("a0"),
    APTR second __asm("a2"), APTR first __asm("a1"))
{
    comparisons++;
    return (ULONG)first < (ULONG)second ? -1 : (first != second);
}

static ULONG attribute(Object *object, ULONG attr)
{
    ULONG value = 0;
    GetAttr(attr, object, &value);
    return value;
}

static int notifications(void)
{
    struct Hook hook = { {NULL, NULL}, (HOOKFUNC)record, NULL, NULL };
    struct TagItem tags[] = { {TAG_DONE, 0} };
    Object *source = MUI_NewObjectA(MUIC_Notify, tags);
    int ok;
    if (!source) return 0;
    DoMethod(source, MUIM_Notify, MUIA_UserData, MUIV_EveryTime,
        MUIV_Notify_Self, 3, MUIM_CallHook, (ULONG)&hook, MUIV_TriggerValue);
    SetAttrs(source, MUIA_UserData, 1, TAG_DONE);
    SetAttrs(source, MUIA_UserData, 1, TAG_DONE);
    SetAttrs(source, MUIA_NoNotify, TRUE, MUIA_UserData, 2, TAG_DONE);
    SetAttrs(source, MUIA_UserData, 3, TAG_DONE);
    ok = event_count == 3 && events[0] == 1 && events[1] == 1
        && events[2] == 3;
    printf("contract notify %ld %lu %lu %lu %lu\n", (LONG)ok,
        event_count, events[0], events[1], events[2]);
    DoMethod(source, MUIM_KillNotify, MUIA_UserData);
    MUI_DisposeObject(source);
    return ok;
}

static int lists(void)
{
    struct Hook hook = { {NULL, NULL}, (HOOKFUNC)compare, NULL, NULL };
    struct TagItem tags[] = {
        {MUIA_List_CompareHook, (ULONG)&hook},
        {MUIA_List_Quiet, TRUE}, {TAG_DONE, 0}
    };
    Object *list = MUI_NewObjectA(MUIC_List, tags);
    APTR entry = NULL;
    LONG i;
    int ok = 1;
    if (!list) return 0;
    for (i = 256; i > 0; i--)
    {
        DoMethod(list, MUIM_List_InsertSingle, i, MUIV_List_Insert_Sorted);
        if (attribute(list, MUIA_List_InsertPosition) != 0) ok = 0;
    }
    if (attribute(list, MUIA_List_Entries) != 256) ok = 0;
    for (i = 0; i < 256; i++)
    {
        DoMethod(list, MUIM_List_GetEntry, i, (ULONG)&entry);
        if ((ULONG)entry != (ULONG)i + 1) ok = 0;
    }
    printf("contract sorted-list %ld\n", (LONG)ok);
    printf("metric list-comparisons %lu\n", comparisons);
    MUI_DisposeObject(list);
    return ok;
}

static int numeric(void)
{
    static const LONG cases[][8] = {
#include "numeric_cases.h"
    };
    Object *object = MUI_NewObject(MUIC_Numeric, TAG_DONE);
    ULONG i;
    int ok = object != NULL;

    if (object)
    {
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const LONG *c = cases[i];
            LONG result;
            SetAttrs(object, MUIA_Numeric_Max, c[2],
                MUIA_Numeric_Min, c[1], MUIA_Numeric_Reverse, c[5],
                MUIA_Numeric_Value, c[6], TAG_DONE);
            result = c[0] ? DoMethod(object, MUIM_Numeric_ValueToScale,
                c[3], c[4]) : DoMethod(object, MUIM_Numeric_ScaleToValue,
                c[3], c[4], c[6]);
            if (result != c[7])
            {
                printf("numeric mismatch %lu got=%ld expected=%ld\n",
                    i, result, c[7]);
                ok = 0;
            }
        }
        MUI_DisposeObject(object);
    }
    printf("contract numeric %ld\n", (LONG)ok);
    return ok;
}

static int cycle(BOOL gui)
{
    struct TagItem texttags[] = {
        {MUIA_Text_Contents, (ULONG)"Zune68 lifecycle workload"},
        {TAG_DONE, 0}
    };
    struct TagItem grouptags[] = { {MUIA_Group_Child, 0}, {TAG_DONE, 0} };
    struct TagItem wintags[] = {
        {MUIA_Window_Title, (ULONG)"MUI compatibility workload"},
        {MUIA_Window_RootObject, 0}, {TAG_DONE, 0}
    };
    struct TagItem apptags[] = {
        {MUIA_Application_Title, (ULONG)"compatcheck"},
        {MUIA_Application_Base, (ULONG)"COMPATCHECK"},
        {MUIA_Application_UseRexx, FALSE},
        {MUIA_Application_UseCommodities, FALSE},
        {MUIA_Application_Window, 0}, {TAG_DONE, 0}
    };
    Object *text, *group, *win, *app;
    int ok = 1;
    text = MUI_NewObjectA(MUIC_Text, texttags);
    if (!text) return 0;
    grouptags[0].ti_Data = (ULONG)text;
    group = MUI_NewObjectA(MUIC_Group, grouptags);
    if (!group) return 0; /* OM_NEW consumes the supplied children */
    wintags[1].ti_Data = (ULONG)group;
    win = MUI_NewObjectA(MUIC_Window, wintags);
    if (!win) return 0;
    apptags[4].ti_Data = (ULONG)win;
    app = MUI_NewObjectA(MUIC_Application, apptags);
    if (!app) return 0;
    if (gui)
    {
        SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
        ok = attribute(win, MUIA_Window_Open) != 0;
        DoMethod(app, MUIM_Application_InputBuffered);
        DoMethod(group, MUIM_Group_InitChange);
        DoMethod(group, MUIM_Group_ExitChange);
    }
    MUI_DisposeObject(app); /* also exercise disposal of an open window */
    return ok;
}

static void memory(const char *stage)
{
    printf("memory %s chip=%lu fast=%lu largest-chip=%lu largest-fast=%lu\n",
        stage, AvailMem(MEMF_CHIP), AvailMem(MEMF_FAST),
        AvailMem(MEMF_CHIP | MEMF_LARGEST),
        AvailMem(MEMF_FAST | MEMF_LARGEST));
}

int main(int argc, char **argv)
{
    const char *library = argc > 1 ? argv[1] : "muimaster.library";
    LONG loops = argc > 2 ? atol(argv[2]) : 100;
    BOOL gui = argc > 3 && argv[3][0] == 'g';
    struct DateStamp before, after;
    LONG i, ticks;
    int ok, lifecycle_ok;
    if (loops < 1 || loops > 100000)
    {
        puts("Usage: compatcheck [library [loops [gui]]], loops 1..100000");
        return 20;
    }
    MUIMasterBase = OpenLibrary(library, 0);
    if (!MUIMasterBase) { puts("Cannot open requested MUI library"); return 20; }
    printf("library %s %u.%u\n", library,
        MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    ok = notifications();
    if (!numeric()) ok = 0;
    DateStamp(&before);
    if (!lists()) ok = 0;
    DateStamp(&after);
    ticks = ((after.ds_Days - before.ds_Days) * 1440 +
        after.ds_Minute - before.ds_Minute) * 60 * TICKS_PER_SECOND +
        after.ds_Tick - before.ds_Tick;
    printf("metric list-ticks %ld ticks-per-second=%ld\n",
        ticks, (LONG)TICKS_PER_SECOND);
    lifecycle_ok = cycle(gui);
    if (!cycle(gui)) lifecycle_ok = 0;
    memory("warm");
    DateStamp(&before);
    for (i = 0; i < loops; i++)
        if (!cycle(gui)) { lifecycle_ok = 0; break; }
    DateStamp(&after);
    memory("after");
    ticks = ((after.ds_Days - before.ds_Days) * 1440 +
        after.ds_Minute - before.ds_Minute) * 60 * TICKS_PER_SECOND +
        after.ds_Tick - before.ds_Tick;
    printf("metric lifecycle-ticks %ld loops=%ld gui=%ld\n", ticks, i, (LONG)gui);
    printf("contract lifecycle %ld\n", (LONG)lifecycle_ok);
    if (!lifecycle_ok) ok = 0;
    CloseLibrary(MUIMasterBase);
    memory("closed");
    return ok ? 0 : 20;
}

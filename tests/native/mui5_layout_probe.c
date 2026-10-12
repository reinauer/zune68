/* Build with the MUI 5 OS3 SDK. All layout calls run in a dispatcher.
 * m68k-amigaos-gcc -m68000 -msoft-float -noixemul -O2 -Wall \
 *   -I"$MUI5_SDK/C/include" -Wl,-u,___stkinit -o mui5-layout-probe \
 *   tests/native/mui5_layout_probe.c
 * Run: mui5-layout-probe [muimaster.library|zunemaster.library] [case 0..5]
 */
#include <stdio.h>
#include <stdlib.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define PROBE_LAYOUT 0x80001001UL
struct ProbeLayout {
    ULONG MethodID;
    Object *child;
    ULONG absolute;
    LONG width, height;
};

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG layout_calls;
static ULONG hook_calls, hook_result = 1;
static int failed;

static ULONG controller(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == PROBE_LAYOUT) {
        struct ProbeLayout *p = (APTR)msg;
        if (p->absolute)
            return MUI_LayoutObj(p->child, 31, 47, p->width, p->height, 0);
        return MUI_Layout(p->child, 31, 47, p->width, p->height, 0);
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((noinline))
ask_minmax(struct IClass *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
    struct MUI_MinMax *mm;

    DoSuperMethodA(cl, obj, (Msg)msg);
    mm = msg->MinMaxInfo;
    mm->MinWidth += 10; mm->MinHeight += 10;
    mm->DefWidth += 100; mm->DefHeight += 70;
    mm->MaxWidth += MUI_MAXMAX; mm->MaxHeight += MUI_MAXMAX;
    return 0;
}

static ULONG area(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_AskMinMax)
        return ask_minmax(cl, obj, (APTR)msg);
    if (msg->MethodID == MUIM_Layout)
        layout_calls++;
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG layout_hook(struct Hook *hook __asm("a0"),
    Object *obj __asm("a2"), struct MUI_LayoutMsg *lm __asm("a1"))
{
    if (lm->lm_Type == MUILM_MINMAX) {
        lm->lm_MinMax.MinWidth = lm->lm_MinMax.MinHeight = 10;
        lm->lm_MinMax.DefWidth = 100;
        lm->lm_MinMax.DefHeight = 70;
        lm->lm_MinMax.MaxWidth = lm->lm_MinMax.MaxHeight = 1000;
        return 0;
    }
    if (lm->lm_Type == MUILM_LAYOUT) {
        hook_calls++;
        return hook_result;
    }
    return MUILM_UNKNOWN;
}

static void report(const char *kind, const char *stage, Object *parent,
    Object *child, Object *leaf, BOOL ready)
{
    ULONG mode, rc;
    for (mode = 0; mode < 2; mode++) {
        layout_calls = 0;
        rc = DoMethod(parent, PROBE_LAYOUT, child, mode, 100, 70);
        failed += rc != (ULONG)ready || _width(child) != 100 ||
            _height(child) != 70 ||
            _left(child) != 31 + (mode ? 0 : _mleft(parent)) ||
            _top(child) != 47 + (mode ? 0 : _mtop(parent));
        printf("%s %s %s rc=%lu parent=%d,%d inner=%d,%d "
            "child=%d,%d,%d,%d calls=%lu",
            kind, stage, mode ? "absolute" : "relative", rc,
            _left(parent), _top(parent), _mleft(parent), _mtop(parent),
            _left(child), _top(child), _width(child), _height(child),
            layout_calls);
        if (leaf)
            printf(" leaf=%d,%d,%d,%d", _left(leaf), _top(leaf),
                _width(leaf), _height(leaf));
        putchar('\n');
        fflush(stdout);
    }
}

int main(int argc, char **argv)
{
    struct MUI_CustomClass *pc, *vc, *ac;
    const char *names[] = {"rectangle", "area", "group", "virtgroup",
        "virtual-parent", "hook"};
    int i;
    MUIMasterBase = OpenLibrary(argc > 1 ? argv[1] : "muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    pc = MUI_CreateCustomClass(NULL, MUIC_Group, NULL, 0, (APTR)controller);
    vc = MUI_CreateCustomClass(NULL, MUIC_Virtgroup, NULL, 0, (APTR)controller);
    ac = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 0, (APTR)area);
    if (!pc || !vc || !ac) return 20;
    printf("library=%u.%u\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);
    for (i = argc > 2 ? atoi(argv[2]) : 0; i < 6; i++) {
        Object *child, *leaf = NULL, *parent, *win, *app;
        struct Hook hook = {0};
        struct TagItem empty[] = {{TAG_DONE, 0}};
        struct TagItem children[] = {{MUIA_Group_Child, 0}, {TAG_DONE, 0}};
        struct TagItem hooktags[] = {
            {MUIA_Group_LayoutHook, (ULONG)&hook}, {TAG_DONE, 0}};
        struct TagItem tags[] = {{MUIA_Frame, MUIV_Frame_Group},
            {MUIA_Group_Child, 0}, {TAG_DONE, 0}};
        struct TagItem wt[] = {
            {MUIA_Window_Title, (ULONG)"MUI layout probe"},
            {MUIA_Window_RootObject, 0},
            {MUIA_Window_Width, 300}, {MUIA_Window_Height, 180},
            {TAG_DONE, 0}};
        struct TagItem at[] = {
            {MUIA_Application_Title, (ULONG)"LayoutProbe"},
            {MUIA_Application_Window, 0}, {TAG_DONE, 0}};
        hook.h_Entry = (HOOKFUNC)layout_hook;
        if (i == 5)
            child = MUI_NewObjectA(MUIC_Group, hooktags);
        else if (i == 1)
            child = NewObjectA(ac->mcc_Class, NULL, empty);
        else if (i == 2 || i == 3) {
            leaf = MUI_NewObjectA(MUIC_Rectangle, empty);
            children[0].ti_Data = (ULONG)leaf;
            child = MUI_NewObjectA(i == 2 ? MUIC_Group : MUIC_Virtgroup,
                children);
        } else
            child = MUI_NewObjectA(MUIC_Rectangle, empty);
        tags[1].ti_Data = (ULONG)child;
        parent = NewObjectA((i == 4 ? vc : pc)->mcc_Class, NULL, tags);
        if (!child || !parent) return 20;
        MUI_Offset(parent, 13, 19);
        report(names[i], "unopened", parent, child, leaf, FALSE);
        wt[1].ti_Data = (ULONG)parent;
        win = MUI_NewObjectA(MUIC_Window, wt);
        if (!win) return 20;
        at[1].ti_Data = (ULONG)win;
        app = MUI_NewObjectA(MUIC_Application, at);
        if (!win || !app) return 20;
        SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
        {
            ULONG opened = 0;
            GetAttr(MUIA_Window_Open, win, &opened);
            printf("%s opened=%lu minmax=%d,%d,%d,%d\n", names[i],
                opened, _minwidth(child), _minheight(child),
                _maxwidth(child), _maxheight(child));
            fflush(stdout);
            if (!opened) {
                failed++;
                MUI_DisposeObject(app);
                continue;
            }
        }
        report(names[i], "opened", parent, child, leaf, TRUE);
        {
            ULONG rc;
            rc = DoMethod(parent, PROBE_LAYOUT, child, 1, 10001, 70);
            failed += rc != 0 || _width(child) != 10001;
            printf("%s oversize rc=%lu box=%d,%d,%d,%d\n", names[i], rc,
                _left(child), _top(child), _width(child), _height(child));
            rc = DoMethod(parent, PROBE_LAYOUT, child, 1, 0, 0);
            failed += rc != (ULONG)(_minwidth(child) == 0 &&
                _minheight(child) == 0) || _width(child) != 0 ||
                _height(child) != 0;
            printf("%s zero rc=%lu box=%d,%d,%d,%d\n", names[i], rc,
                _left(child), _top(child), _width(child), _height(child));
            if (i == 5) {
                hook_calls = 0;
                hook_result = 0;
                rc = DoMethod(parent, PROBE_LAYOUT, child, 1, 100, 70);
                failed += rc != 0 || hook_calls != 1;
                printf("hook failure rc=%lu calls=%lu\n", rc, hook_calls);
            }
            fflush(stdout);
        }
        SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
        MUI_DisposeObject(app);
        if (argc > 2) break;
    }
    MUI_DeleteCustomClass(ac);
    MUI_DeleteCustomClass(vc);
    MUI_DeleteCustomClass(pc);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failed ? "FAIL" : "PASS", failed);
    puts("DONE");
    return failed ? 20 : 0;
}

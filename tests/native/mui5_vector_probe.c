/* Build against the MUI 5 OS3 SDK, then run with each library variant. */
#include <exec/libraries.h>
#include <intuition/classes.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG show_calls, hide_calls, layout_calls;
#define PROBE_LAYOUT 0x80001001UL
#define PROBE_LEGACY_LAYOUT 0x80001002UL

static ULONG dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    switch (msg->MethodID)
    {
    case PROBE_LAYOUT:
        return MUI_LayoutObj(obj, 10, 20, 50, 30, 0);
    case PROBE_LEGACY_LAYOUT:
        return MUI_Layout(obj, 10, 20, 50, 30, 0);
    case MUIM_Show:
        show_calls++;
        return 42;
    case MUIM_Hide:
        hide_calls++;
        return 43;
    case MUIM_Layout:
        layout_calls++;
        return 44;
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}

static LONG attr(Object *obj, ULONG id)
{
    ULONG value = 0;

    GetAttr(id, obj, &value);
    return (LONG)value;
}

int main(int argc, char **argv)
{
    Object *child;
    Object *group;
    Object *win;
    Object *app;
    struct MUI_CustomClass *cc;
    struct TagItem childtags[] = {
        { MUIA_FixWidth, 50 }, { MUIA_FixHeight, 30 }, { TAG_DONE, 0 }
    };
    struct TagItem grouptags[] = {
        { MUIA_Group_Child, 0 }, { TAG_DONE, 0 }
    };
    struct TagItem wintags[] = {
        { MUIA_Window_Title, (ULONG)"MUI vector probe" },
        { MUIA_Window_RootObject, 0 }, { TAG_DONE, 0 }
    };
    struct TagItem apptags[] = {
        { MUIA_Application_Title, (ULONG)"MUIVectorProbe" },
        { MUIA_Application_Window, 0 }, { TAG_DONE, 0 }
    };
    BOOL layout, legacy_layout;
    ULONG shown, hidden, method_show, method_hide;
    ULONG opened_hidden, opened_shown;
    BOOL opened_layout;

    MUIMasterBase = OpenLibrary(argc > 1 ? argv[1] : "muimaster.library", 0);
    if (!MUIMasterBase)
    {
        puts("open failed");
        return 20;
    }
    cc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 0, (APTR)dispatch);
    child = cc ? NewObjectA(cc->mcc_Class, NULL, childtags) : NULL;
    grouptags[0].ti_Data = (ULONG)child;
    group = child ? MUI_NewObjectA(MUIC_Group, grouptags) : NULL;
    if (!group)
    {
        puts("create failed");
        if (child)
            MUI_DisposeObject(child);
        if (cc)
            MUI_DeleteCustomClass(cc);
        CloseLibrary(MUIMasterBase);
        return 20;
    }

    layout = DoMethod(child, PROBE_LAYOUT);
    legacy_layout = DoMethod(child, PROBE_LEGACY_LAYOUT);
    printf("layout=%lu legacy=%lu calls=%lu child=%ld,%ld,%ld,%ld\n",
        (ULONG)layout, (ULONG)legacy_layout, layout_calls,
        attr(child, MUIA_LeftEdge), attr(child, MUIA_TopEdge),
        attr(child, MUIA_Width), attr(child, MUIA_Height));
    MUI_Offset(group, 7, -3);
    printf("offset group=%ld,%ld child=%ld,%ld\n",
        attr(group, MUIA_LeftEdge), attr(group, MUIA_TopEdge),
        attr(child, MUIA_LeftEdge), attr(child, MUIA_TopEdge));
    shown = MUI_Show(child);
    hidden = MUI_Hide(child);
    method_show = DoMethod(child, MUIM_Show);
    method_hide = DoMethod(child, MUIM_Hide);
    printf("show=%lu hide=%lu methods=%lu,%lu calls=%lu,%lu\n",
        shown, hidden, method_show, method_hide, show_calls, hide_calls);

    wintags[1].ti_Data = (ULONG)group;
    win = MUI_NewObjectA(MUIC_Window, wintags);
    apptags[1].ti_Data = (ULONG)win;
    app = win ? MUI_NewObjectA(MUIC_Application, apptags) : NULL;
    if (app)
    {
        SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
        printf("window=%ld\n", attr(win, MUIA_Window_Open));
        if (attr(win, MUIA_Window_Open))
        {
            opened_hidden = MUI_Hide(child);
            opened_shown = MUI_Show(child);
            opened_layout = DoMethod(child, PROBE_LAYOUT);
            printf("opened hide=%lu show=%lu layout=%lu calls=%lu,%lu,%lu child=%ld,%ld\n",
                opened_hidden, opened_shown, (ULONG)opened_layout,
                show_calls, hide_calls, layout_calls,
                attr(child, MUIA_LeftEdge), attr(child, MUIA_TopEdge));
        }
        SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
        MUI_DisposeObject(app);
    }
    else if (win)
        MUI_DisposeObject(win);
    else
        MUI_DisposeObject(group);
    MUI_DeleteCustomClass(cc);
    CloseLibrary(MUIMasterBase);
    return 0;
}

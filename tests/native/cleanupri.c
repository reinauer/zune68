/* What muiRenderInfo(obj) holds around a subclass's DoSuperMethod of
 * MUIM_Cleanup (HTMLview releases its pens with it after the super call),
 * for subclasses of Rectangle, Group and Virtgroup. */
#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

struct Data { struct MUI_RenderInfo *setup; const char *name; };

static void show(Object *obj, struct Data *d, const char *when)
{
    struct MUI_RenderInfo *ri = muiRenderInfo(obj);
    printf("%s %s ri=%s", d->name, when,
        !ri ? "null" : ri == d->setup ? "same" : "other");
    if (ri)
        printf(" screen=%s", ri->mri_Screen ? "set" : "null");
    putchar('\n');
    fflush(stdout);
}

static ULONG dispatch(struct IClass *cl __asm("a0"),
                      Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    struct Data *d = INST_DATA(cl, obj);
    ULONG result;
    switch (msg->MethodID) {
    case OM_NEW:
        if ((obj = (Object *)DoSuperMethodA(cl, obj, msg))) {
            d = INST_DATA(cl, obj);
            d->setup = NULL;
            d->name = (const char *)cl->cl_UserData;
        }
        return (ULONG)obj;
    case MUIM_Setup:
        result = DoSuperMethodA(cl, obj, msg);
        d->setup = muiRenderInfo(obj);
        show(obj, d, "setup-after");
        return result;
    case MUIM_Hide:
        result = DoSuperMethodA(cl, obj, msg);
        show(obj, d, "hide-after");
        return result;
    case MUIM_Cleanup:
        show(obj, d, "cleanup-before");
        result = DoSuperMethodA(cl, obj, msg);
        show(obj, d, "cleanup-after");
        return result;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static struct MUI_CustomClass *make(const char *super, const char *name)
{
    struct MUI_CustomClass *mcc = MUI_CreateCustomClass(NULL, (char *)super,
        NULL, sizeof(struct Data), (APTR)dispatch);
    if (mcc) mcc->mcc_Class->cl_UserData = (ULONG)name;
    return mcc;
}

int main(void)
{
    struct MUI_CustomClass *rect = NULL, *group = NULL, *virt = NULL;
    Object *app = NULL, *window, *root;
    ULONG opened = 0;
    int result = 20;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    puts("cleanupri=1");
    rect = make(MUIC_Rectangle, "rectangle");
    group = make(MUIC_Group, "group");
    virt = make(MUIC_Virtgroup, "virtgroup");
    if (!rect || !group || !virt) goto done;
    root = NewObject(group->mcc_Class, NULL,
        MUIA_Group_Child, (ULONG)NewObject(rect->mcc_Class, NULL,
            MUIA_FixWidth, 24, MUIA_FixHeight, 16, TAG_DONE),
        MUIA_Group_Child, (ULONG)NewObject(virt->mcc_Class, NULL,
            MUIA_Frame, MUIV_Frame_Virtual,
            MUIA_Group_Child, (ULONG)MUI_NewObject((char *)MUIC_Rectangle,
                MUIA_FixWidth, 40, MUIA_FixHeight, 40, TAG_DONE),
            TAG_DONE),
        TAG_DONE);
    window = MUI_NewObject((char *)MUIC_Window,
        MUIA_Window_Title, (ULONG)"Cleanup render info probe",
        MUIA_Window_RootObject, (ULONG)root, TAG_DONE);
    app = MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Title, (ULONG)"Cleanup render info probe",
        MUIA_Application_Base, (ULONG)"CLEANUPRI",
        MUIA_Application_Window, (ULONG)window, TAG_DONE);
    if (!app) goto done;
    puts("opening");
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, window, &opened);
    printf("opened=%lu\n", opened);
    puts("closing");
    SetAttrs(window, MUIA_Window_Open, FALSE, TAG_DONE);
    puts("disposing");
    MUI_DisposeObject(app);
    if (opened) result = 0;
done:
    if (virt) MUI_DeleteCustomClass(virt);
    if (group) MUI_DeleteCustomClass(group);
    if (rect) MUI_DeleteCustomClass(rect);
    CloseLibrary(MUIMasterBase);
    printf("result=%d\n", result);
    return result;
}

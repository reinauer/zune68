/* Observe Group/Rectangle lifecycle order; no ordering is assumed. */
#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static Object *objects[5];
static const int ids[5] = {1, 2, 3, 20, 10};
static ULONG sequence;

static ULONG dispatch(struct IClass *cl __asm("a0"),
                      Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    const char *name = NULL;
    ULONG result;
    int i, id = 0;
    switch (msg->MethodID) {
    case MUIM_Setup: name = "Setup"; break;
    case MUIM_Cleanup: name = "Cleanup"; break;
    case MUIM_Show: name = "Show"; break;
    case MUIM_Hide: name = "Hide"; break;
    }
    if (!name) return DoSuperMethodA(cl, obj, msg);
    for (i = 0; i < 5; i++) if (objects[i] == obj) id = ids[i];
    printf("%lu id=%d %s before-super\n", ++sequence, id, name);
    fflush(stdout);
    result = DoSuperMethodA(cl, obj, msg);
    printf("%lu id=%d %s after-super result=%08lx\n",
           ++sequence, id, name, result);
    fflush(stdout);
    return result;
}

static void window_screen(Object *window, const char *phase)
{
    ULONG screen = 0xdeadbeef, ok;
    ok = GetAttr(MUIA_Window_Screen, window, &screen);
    printf("window-screen %s get=%lu value=%08lx\n", phase, ok, screen);
    fflush(stdout);
}

static ULONG window_dispatch(struct IClass *cl __asm("a0"),
                             Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    ULONG result;
    if (msg->MethodID != 0x8042c34cUL && msg->MethodID != 0x8042ab26UL)
        return DoSuperMethodA(cl, obj, msg);
    window_screen(obj, msg->MethodID == 0x8042c34cUL ?
        "Setup-before-super" : "Cleanup-before-super");
    result = DoSuperMethodA(cl, obj, msg);
    window_screen(obj, msg->MethodID == 0x8042c34cUL ?
        "Setup-after-super" : "Cleanup-after-super");
    printf("window-method=%08lx result=%08lx\n", msg->MethodID, result);
    return result;
}

static int scenario(struct MUI_CustomClass *leafclass,
                    struct MUI_CustomClass *groupclass,
                    struct MUI_CustomClass *windowclass, int nested)
{
    Object *window = NULL, *app = NULL;
    ULONG opened = 0;
    int i, result = 20;
    for (i = 0; i < 5; i++) objects[i] = NULL;
    sequence = 0;
    printf("case=%s root=10 nested=20 leaves=1,2,3\n",
           nested ? "nested" : "flat");
    for (i = 0; i < 3; i++) {
        objects[i] = NewObject(leafclass->mcc_Class, NULL,
            MUIA_FixWidth, 24, MUIA_FixHeight, 16, TAG_DONE);
        if (!objects[i]) goto children_failed;
    }
    if (nested) {
        objects[3] = NewObject(groupclass->mcc_Class, NULL,
            MUIA_Group_Child, (ULONG)objects[1],
            MUIA_Group_Child, (ULONG)objects[2], TAG_DONE);
        if (!objects[3]) {
            objects[1] = objects[2] = NULL;
            goto children_failed;
        }
        objects[4] = NewObject(groupclass->mcc_Class, NULL,
            MUIA_Group_Child, (ULONG)objects[0],
            MUIA_Group_Child, (ULONG)objects[3], TAG_DONE);
    } else {
        objects[4] = NewObject(groupclass->mcc_Class, NULL,
            MUIA_Group_Child, (ULONG)objects[0],
            MUIA_Group_Child, (ULONG)objects[1],
            MUIA_Group_Child, (ULONG)objects[2], TAG_DONE);
    }
    if (!objects[4]) return 20;
    window = NewObject(windowclass->mcc_Class, NULL,
        MUIA_Window_Title, (ULONG)"Lifecycle order probe",
        MUIA_Window_RootObject, (ULONG)objects[4], TAG_DONE);
    if (!window) return 20;
    app = MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Title, (ULONG)"Lifecycle order probe",
        MUIA_Application_Base, (ULONG)"LIFECYCLEORDER",
        MUIA_Application_Window, (ULONG)window, TAG_DONE);
    if (!app) return 20;
    window_screen(window, "unopened");
    puts("opening");
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, window, &opened);
    printf("opened=%lu\n", opened);
    window_screen(window, "opened");
    puts("closing");
    SetAttrs(window, MUIA_Window_Open, FALSE, TAG_DONE);
    window_screen(window, "closed");
    puts("disposing");
    MUI_DisposeObject(app);
    if (opened) result = 0;
    return result;
children_failed:
    for (i = 0; i < 3; i++)
        if (objects[i]) MUI_DisposeObject(objects[i]);
    return 20;
}

int main(void)
{
    struct MUI_CustomClass *leafclass = NULL, *groupclass = NULL;
    struct MUI_CustomClass *windowclass = NULL;
    int result = 20;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    printf("lifecycleorder=2 library=%u.%u\n",
           MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    leafclass = MUI_CreateCustomClass(NULL, (char *)MUIC_Rectangle,
        NULL, 0, (APTR)dispatch);
    groupclass = MUI_CreateCustomClass(NULL, (char *)MUIC_Group,
        NULL, 0, (APTR)dispatch);
    windowclass = MUI_CreateCustomClass(NULL, (char *)MUIC_Window,
        NULL, 0, (APTR)window_dispatch);
    if (!leafclass || !groupclass || !windowclass) goto done;
    result = scenario(leafclass, groupclass, windowclass, 0);
    if (!result) result = scenario(leafclass, groupclass, windowclass, 1);
done:
    if (windowclass) MUI_DeleteCustomClass(windowclass);
    if (groupclass) MUI_DeleteCustomClass(groupclass);
    if (leafclass) MUI_DeleteCustomClass(leafclass);
    CloseLibrary(MUIMasterBase);
    printf("result=%d\n", result);
    return result;
}

/* Timer callbacks carry { MethodID, signals, originating handler }. */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define TIMER_METHOD 0x81726801UL
static ULONG calls, failures;
static struct MUI_InputHandlerNode *expected_node;

static ULONG dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == TIMER_METHOD) {
        ULONG *words = (ULONG *)msg;
        if (words[1] != 0 || words[2] != (ULONG)expected_node)
            failures++;
        calls++;
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(void)
{
    struct MUI_CustomClass *cc;
    struct MUI_InputHandlerNode node = {0};
    Object *app;
    ULONG sig = 0, completed;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    cc = MUI_CreateCustomClass(NULL, MUIC_Application, NULL, 0,
        (APTR)dispatch);
    if (!cc) { CloseLibrary(MUIMasterBase); return 20; }
    app = NewObject(cc->mcc_Class, NULL,
        MUIA_Application_Title, (ULONG)"InputHandler",
        MUIA_Application_Base, (ULONG)"INPUTHANDLER", TAG_DONE);
    if (!app) {
        MUI_DeleteCustomClass(cc);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    node.ihn_Object = app;
    node.ihn_Millis = 40;
    node.ihn_Flags = MUIIHNF_TIMER;
    node.ihn_Method = TIMER_METHOD;
    expected_node = &node;
    DoMethod(app, MUIM_Application_AddInputHandler, &node);
    for (int i = 0; i < 100 && calls < 3; i++) {
        DoMethod(app, MUIM_Application_NewInput, &sig);
        Delay(1);
    }
    DoMethod(app, MUIM_Application_RemInputHandler, &node);
    completed = calls;
    for (int i = 0; i < 10; i++) {
        DoMethod(app, MUIM_Application_NewInput, &sig);
        Delay(1);
    }
    if (completed < 3 || calls != completed) failures++;
    printf("inputhandler: %lu callbacks, %lu failures\n", calls, failures);
    MUI_DisposeObject(app);
    MUI_DeleteCustomClass(cc);
    CloseLibrary(MUIMasterBase);
    return failures ? 20 : 0;
}

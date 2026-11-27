/* What MUI_Error() reports after failed object creation: a
 * missing external class, a second SingleTask application, a window whose
 * root object failed, a success after a failure, and another task. MUI 3.8
 * and MUI 5 return MUIE_OK throughout; Zune68 returns the cause
 * (ref/zune68/muierror.txt). DA3 shows the value when its window fails. */
#include <exec/types.h>
#include <libraries/mui.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static struct Task *parent;
static volatile LONG child_error = -1;

static void report(const char *what, Object *obj)
{
    printf("%s obj=%s MUI_Error=%ld\n", what, obj ? "set" : "null", MUI_Error());
    fflush(stdout);
}

static void reset(void)
{
    MUI_SetError(0);
}

static Object *app(const char *base)
{
    return MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Base, (ULONG)base,
        MUIA_Application_SingleTask, TRUE, TAG_DONE);
}

static void child(void)
{
    child_error = MUI_Error();
    Forbid();
    Signal(parent, SIGBREAKF_CTRL_F);
}

int main(void)
{
    Object *a, *b, *o;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    puts("muierror=2");

    reset();
    o = MUI_NewObject((char *)MUIC_Rectangle, TAG_DONE);
    report("rectangle", o);
    if (o) MUI_DisposeObject(o);

    reset();
    o = MUI_NewObject("NoSuchClass.mcc", TAG_DONE);
    report("missing-class", o);
    if (o) MUI_DisposeObject(o);

    /* no reset: a success keeps the earlier failure's code */
    o = MUI_NewObject((char *)MUIC_Rectangle, TAG_DONE);
    report("rectangle-after-failure", o);
    if (o) MUI_DisposeObject(o);

    /* another task does not see this task's code */
    parent = FindTask(NULL);
    SetSignal(0, SIGBREAKF_CTRL_F);
    if (CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"muierror child",
            TAG_DONE))
    {
        Wait(SIGBREAKF_CTRL_F);
        printf("other-task MUI_Error=%ld\n", child_error);
    }

    reset();
    a = app("MUIERROR");
    report("first-app", a);
    reset();
    b = app("MUIERROR");
    report("second-singletask-app", b);
    if (b) MUI_DisposeObject(b);
    if (a) MUI_DisposeObject(a);

    reset();
    o = MUI_NewObject((char *)MUIC_Window,
        MUIA_Window_RootObject, (ULONG)MUI_NewObject("NoSuchClass.mcc", TAG_DONE),
        TAG_DONE);
    report("window-missing-root", o);
    if (o) MUI_DisposeObject(o);

    reset();
    MUI_SetError(5);
    report("set-error-5", (Object *)1);
    reset();
    report("set-error-0", (Object *)1);

    CloseLibrary(MUIMasterBase);
    puts("result=0");
    return 0;
}

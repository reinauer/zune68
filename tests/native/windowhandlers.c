/* Observe removal during real RAWKEY dispatch with the original MUI SDK.
 * By default, nodes and targets stay alive throughout every dispatch.
 * WINDOWHANDLERS_RELEASE=1 builds a separate experiment: after removing a
 * client-owned node, overwrite its memory and free it immediately. Targets
 * still stay alive. This observes reference tolerance; the SDK does not
 * explicitly document in-callback removal/free safety.
 * Press and release F1 once for each of the three READY messages.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#ifndef WINDOWHANDLERS_RELEASE
#define WINDOWHANDLERS_RELEASE 0
#endif

static Object *app, *window, *targets[3];
static struct MUI_EventHandlerNode *handlers[3];
#if !WINDOWHANDLERS_RELEASE
static struct MUI_EventHandlerNode retained[3];
#endif
static BOOL registered[3], triggered, overflow;
static ULONG phase, removals, event_seconds, event_micros;
static ULONG order[16], order_count;
static const char *names[] = { "baseline", "remove-self", "remove-next" };

static void remove_handler(ULONG index)
{
    if (registered[index]) {
        DoMethod(window, MUIM_Window_RemEventHandler, handlers[index]);
        registered[index] = FALSE;
    }
#if WINDOWHANDLERS_RELEASE
    if (handlers[index]) {
        volatile UBYTE *bytes = (volatile UBYTE *)handlers[index];
        ULONG i;
        /* This is our own, now unregistered allocation. Volatile keeps
         * the pre-free overwrite observable even under optimization.
         */
        for (i = 0; i < sizeof(*handlers[index]); i++) bytes[i] = 0xa5;
        FreeVec(handlers[index]);
        handlers[index] = NULL;
    }
#endif
}

static ULONG dispatch(struct IClass *cl __asm("a0"),
                      Object *obj __asm("a2"), Msg message __asm("a1"))
{
    struct MUIP_HandleEvent *msg;
    ULONG index;
    BOOL first = FALSE;
    if (message->MethodID != MUIM_HandleEvent)
        return DoSuperMethodA(cl, obj, message);
    msg = (struct MUIP_HandleEvent *)message;
    if (!msg->imsg || msg->imsg->Class != IDCMP_RAWKEY ||
        msg->imsg->Code != 0x50) /* F1 down, not its key-up */
        return 0;
    for (index = 0; index < 3; index++)
        if (targets[index] == obj) break;
    if (index == 3 || phase >= 3) return 0;
    if (!triggered) {
        triggered = TRUE;
        first = TRUE;
        event_seconds = msg->imsg->Seconds;
        event_micros = msg->imsg->Micros;
    }
    /* Ignore repeats while NewInput finishes processing its input queue. */
    if (msg->imsg->Seconds != event_seconds ||
        msg->imsg->Micros != event_micros)
        return 0;
    if (order_count < sizeof(order) / sizeof(order[0]))
        order[order_count++] = index;
    else
        overflow = TRUE;
    if (index == 0 && phase != 0 && !removals) {
        remove_handler(phase == 1 ? 0 : 1);
        removals++;
    }
    if (first) {
        /* The outer loop changes phase only after NewInput returns. */
        DoMethod(app, MUIM_Application_ReturnID, phase + 1);
    }
    return 0; /* Let the remaining registered handlers observe this event. */
}

static ULONG ticks(void)
{
    struct DateStamp stamp;
    DateStamp(&stamp);
    return (ULONG)stamp.ds_Days * 24UL * 60UL * 60UL * 50UL +
           (ULONG)stamp.ds_Minute * 60UL * 50UL + stamp.ds_Tick;
}

static BOOL ready(struct MUI_CustomClass *cc)
{
    ULONG i;
    for (i = 0; i < 3; i++) remove_handler(i);
    triggered = overflow = FALSE;
    order_count = removals = 0;
    for (i = 0; i < 3; i++) {
#if WINDOWHANDLERS_RELEASE
        handlers[i] = AllocVec(sizeof(*handlers[i]), MEMF_PUBLIC | MEMF_CLEAR);
        if (!handlers[i]) return FALSE;
#else
        handlers[i] = &retained[i];
        memset(handlers[i], 0, sizeof(*handlers[i]));
#endif
        handlers[i]->ehn_Priority = 20 - (LONG)i * 10;
        handlers[i]->ehn_Object = targets[i];
        handlers[i]->ehn_Class = cc->mcc_Class;
        handlers[i]->ehn_Events = IDCMP_RAWKEY;
        DoMethod(window, MUIM_Window_AddEventHandler, handlers[i]);
        registered[i] = TRUE;
    }
    SetAttrs(window, MUIA_Window_Title, (ULONG)names[phase], TAG_DONE);
    printf("READY phase=%lu case=%s press-release-F1 priorities=A20,B10,C0\n",
           phase, names[phase]);
    fflush(stdout);
    return TRUE;
}

int main(int argc, char **argv)
{
    struct MUI_CustomClass *cc = NULL;
    Object *group = NULL;
    ULONG i, opened = 0, signals = 0, result, start;
    ULONG timeout = argc > 1 ? strtoul(argv[1], NULL, 10) : 120;
    int status = 20;
    if (timeout < 5 || timeout > 600) return 20;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    printf("windowhandlers=2 library=%u.%u timeout-per-phase=%lu seconds "
           "release-nodes=%u\n", MUIMasterBase->lib_Version,
           MUIMasterBase->lib_Revision, timeout, WINDOWHANDLERS_RELEASE);
    puts("observations only; targets retained; no active/default object");
    fflush(stdout);
    cc = MUI_CreateCustomClass(NULL, (char *)MUIC_Rectangle, NULL, 0,
                              (APTR)dispatch);
    if (!cc) goto done;
    app = MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Title, (ULONG)"Window handlers probe",
        MUIA_Application_Base, (ULONG)"WINDOWHANDLERS", TAG_DONE);
    if (!app) goto done;
    group = MUI_NewObject((char *)MUIC_Group,
        MUIA_Group_Horiz, TRUE, TAG_DONE);
    if (!group) goto done;
    for (i = 0; i < 3; i++) {
        targets[i] = NewObject(cc->mcc_Class, NULL,
            MUIA_FixWidth, 48, MUIA_FixHeight, 24, TAG_DONE);
        if (!targets[i]) goto done;
        DoMethod(group, OM_ADDMEMBER, targets[i]);
    }
    window = MUI_NewObject((char *)MUIC_Window,
        MUIA_Window_Title, (ULONG)"Window handlers probe",
        MUIA_Window_PublicScreen, (ULONG)"Workbench",
        MUIA_Window_RootObject, (ULONG)group, TAG_DONE);
    group = NULL; /* Window constructor takes ownership of its root. */
    if (!window) goto done;
    DoMethod(app, OM_ADDMEMBER, window);
    DoMethod(window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
        app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, window, &opened);
    if (!opened) goto done;
    SetAttrs(window, MUIA_Window_ActiveObject,
        MUIV_Window_ActiveObject_None, MUIA_Window_DefaultObject, 0,
        MUIA_Window_Activate, TRUE, TAG_DONE);
    if (!ready(cc)) goto done;
    start = ticks();
    while (phase < 3) {
        result = DoMethod(app, MUIM_Application_NewInput, &signals);
        if (result == (ULONG)MUIV_Application_ReturnID_Quit ||
            (SetSignal(0, 0) & SIGBREAKF_CTRL_C)) {
            puts("STOP interrupted");
            status = 5;
            goto done;
        }
        if (result == phase + 1) {
            printf("RESULT case=%s order=", names[phase]);
            for (i = 0; i < order_count; i++)
                printf("%s%c", i ? "," : "", (int)('A' + order[i]));
            printf(" removals=%lu overflow=%u\n", removals, overflow);
            fflush(stdout);
            if (overflow) goto done;
            phase++;
            if (phase == 3) break;
            if (!ready(cc)) goto done;
            start = ticks();
        }
        if (ticks() - start >= timeout * 50UL) {
            printf("STOP timeout phase=%lu\n", phase);
            status = 5;
            goto done;
        }
        Delay(1);
    }
    status = 0;
done:
    for (i = 0; i < 3; i++) remove_handler(i);
    if (window) SetAttrs(window, MUIA_Window_Open, FALSE, TAG_DONE);
    if (app) MUI_DisposeObject(app);
    if (group) MUI_DisposeObject(group);
    if (cc) MUI_DeleteCustomClass(cc);
    printf("windowhandlers complete result=%d phases=%lu\n", status, phase);
    CloseLibrary(MUIMasterBase);
    return status;
}

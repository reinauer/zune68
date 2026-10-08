/* Public MUI 3.8 menu-command/focus comparison; Q records, never quits.
 * Queue snapshots use public Intuition structures and do not remove messages.
 * Run with stdout redirected. The probe exits after 300 seconds or Quit.
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG menu_count;

static void dump(const char *phase, Object *window, Object *core, Object *custom)
{
    ULONG active = 0;
    STRPTR a = NULL, b = NULL;
    GetAttr(MUIA_Window_ActiveObject, window, &active);
    GetAttr(MUIA_String_Contents, core, (ULONG *)&a);
    GetAttr(MUIA_String_Contents, custom, (ULONG *)&b);
    printf("%s menu=%lu focus=%s core=[%.127s] newstring=[%.127s]\n",
        phase, menu_count, active == (ULONG)core ? "core" :
        active == (ULONG)custom ? "newstring" : "none/other",
        (const char *)(a ? a : (STRPTR)"<null>"),
        (const char *)(b ? b : (STRPTR)"<null>"));
    fflush(stdout);
}

static void messages(Object *object)
{
    struct Window *window = NULL;
    struct IntuiMessage *msg;
    struct { ULONG type; UWORD code, qualifier; } saved[32];
    unsigned count = 0, scanned = 0, i;
    GetAttr(MUIA_Window_Window, object, (ULONG *)&window);
    if (!window || !window->UserPort) return;
    Forbid();
    for (msg = (struct IntuiMessage *)window->UserPort->mp_MsgList.lh_Head;
         msg->ExecMessage.mn_Node.ln_Succ && count < 32 && scanned++ < 128;
         msg = (struct IntuiMessage *)msg->ExecMessage.mn_Node.ln_Succ) {
        if (msg->IDCMPWindow == window &&
            (msg->Class == IDCMP_RAWKEY || msg->Class == IDCMP_MENUPICK)) {
            saved[count].type = msg->Class;
            saved[count].code = msg->Code;
            saved[count++].qualifier = msg->Qualifier;
        }
    }
    Permit();
    for (i = 0; i < count; i++)
        printf("queued %s code=%04x qualifier=%04x\n",
            saved[i].type == IDCMP_RAWKEY ? "RAWKEY" : "MENUPICK",
            saved[i].code, saved[i].qualifier);
    if (count) fflush(stdout);
}

int main(void)
{
    Object *app, *window, *core, *custom, *root, *buttons;
    Object *none, *focuscore, *focuscustom, *report, *quit;
    Object *qitem, *menu, *strip;
    struct DateStamp start, now;
    ULONG signals = 0, id, opened = 0;
    int expired = 0;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    core = MUI_NewObject(MUIC_String, MUIA_Frame, MUIV_Frame_String,
        MUIA_String_Contents, (ULONG)"Core:", MUIA_String_MaxLen, 128,
        MUIA_CycleChain, 1, TAG_DONE);
    custom = MUI_NewObject("Newstring.mcc", MUIA_Frame, MUIV_Frame_String,
        MUIA_String_Contents, (ULONG)"Newstring:", MUIA_String_MaxLen, 128,
        MUIA_CycleChain, 1, TAG_DONE);
    if (!core || !custom) {
        puts("Could not create both string classes");
        if (core) MUI_DisposeObject(core);
        if (custom) MUI_DisposeObject(custom);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    none = MUI_MakeObject(MUIO_Button, (ULONG)"None");
    focuscore = MUI_MakeObject(MUIO_Button, (ULONG)"Core");
    focuscustom = MUI_MakeObject(MUIO_Button, (ULONG)"Newstring");
    report = MUI_MakeObject(MUIO_Button, (ULONG)"Dump");
    quit = MUI_MakeObject(MUIO_Button, (ULONG)"Quit");
    buttons = MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
        MUIA_Group_Child, (ULONG)none, MUIA_Group_Child, (ULONG)focuscore,
        MUIA_Group_Child, (ULONG)focuscustom, MUIA_Group_Child, (ULONG)report,
        MUIA_Group_Child, (ULONG)quit, TAG_DONE);
    root = MUI_NewObject(MUIC_Group,
        MUIA_Group_Child, (ULONG)MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents,
            (ULONG)"Choose focus, press Right Amiga Q, then Dump.", TAG_DONE),
        MUIA_Group_Child, (ULONG)core,
        MUIA_Group_Child, (ULONG)custom,
        MUIA_Group_Child, (ULONG)MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents,
            (ULONG)"Probe menu / Count Q must increment; it does not quit.",
            TAG_DONE), MUIA_Group_Child, (ULONG)buttons, TAG_DONE);
    qitem = MUI_NewObject(MUIC_Menuitem,
        MUIA_Menuitem_Title, (ULONG)"Count Q",
        MUIA_Menuitem_Shortcut, (ULONG)"Q", TAG_DONE);
    menu = MUI_NewObject(MUIC_Menu,
        MUIA_Menu_Title, (ULONG)"Probe",
        MUIA_Family_Child, (ULONG)qitem, TAG_DONE);
    strip = MUI_NewObject(MUIC_Menustrip,
        MUIA_Family_Child, (ULONG)menu, TAG_DONE);
    window = MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, (ULONG)"Menu key probe (300 second limit)",
        MUIA_Window_RootObject, (ULONG)root,
        MUIA_Window_Menustrip, (ULONG)strip, TAG_DONE);
    app = MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, (ULONG)"Menu key probe",
        MUIA_Application_Base, (ULONG)"MENUKEYPROBE",
        MUIA_Application_Window, (ULONG)window, TAG_DONE);
    if (!app) { CloseLibrary(MUIMasterBase); return 20; }
    DoMethod(none, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, 10);
    DoMethod(focuscore, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, 11);
    DoMethod(focuscustom, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, 12);
    DoMethod(report, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, 13);
    DoMethod(qitem, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
        app, 2, MUIM_Application_ReturnID, 100);
    DoMethod(quit, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
        app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    set(window, MUIA_Window_Open, TRUE);
    GetAttr(MUIA_Window_Open, window, &opened);
    if (!opened) {
        MUI_DisposeObject(app); CloseLibrary(MUIMasterBase); return 20;
    }
    set(window, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
    printf("library=%u.%u\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);
    dump("initial", window, core, custom);
    DateStamp(&start);
    for (;;) {
        messages(window);
        id = DoMethod(app, MUIM_Application_NewInput, &signals);
        if (id == (ULONG)MUIV_Application_ReturnID_Quit) break;
        if (id >= 10 && id <= 12) {
            set(window, MUIA_Window_ActiveObject, id == 10 ?
                MUIV_Window_ActiveObject_None :
                id == 11 ? (ULONG)core : (ULONG)custom);
            dump("focus", window, core, custom);
        } else if (id == 13) dump("dump", window, core, custom);
        else if (id == 100) {
            menu_count++;
            dump("menu Q", window, core, custom);
        }
        DateStamp(&now);
        if ((now.ds_Days - start.ds_Days) * 86400L
            + (now.ds_Minute - start.ds_Minute) * 60L
            + (now.ds_Tick - start.ds_Tick) / TICKS_PER_SECOND >= 300) {
            expired = 1; break;
        }
        Delay(1);
    }
    dump(expired ? "timeout" : "quit", window, core, custom);
    set(window, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

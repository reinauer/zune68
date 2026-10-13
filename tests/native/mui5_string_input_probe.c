/* Compare edit-hook messages and actual keyboard editing on AmigaOS. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <intuition/sghooks.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(name, ...) \
    ({ ULONG tags[] = {__VA_ARGS__}; MUI_NewObjectA(name, (struct TagItem *)tags); })
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG calls, veto_return, lonely, lonely_calls;
static int failures;
static ULONG qualifier;
static ULONG edit_hook(struct Hook *hook __asm("a0"),
    struct SGWork *work __asm("a2"), ULONG *command __asm("a1"))
{
    calls++;
    printf("hook cmd=%lu op=%u code=%u actions=%lu pos=%d chars=%d new=%s\n",
        *command, work->EditOp, work->Code, work->Actions, work->BufferPos,
        work->NumChars, (char *)work->WorkBuffer);
    if (*command == SGH_KEY && lonely)
    {
        lonely_calls++;
        if (work->EditOp != EO_INSERTCHAR || strcmp(work->WorkBuffer,"seeda"))
            failures++;
        strcpy(work->WorkBuffer,"lonely");
        work->NumChars = work->BufferPos = 6;
        work->Actions = SGA_USE | SGA_REDISPLAY;
        return TRUE;
    }
    if (*command == SGH_KEY)
    {
        if (work->EditOp == EO_INSERTCHAR && work->Code == 'a' && work->BufferPos)
            work->WorkBuffer[work->BufferPos - 1] = 'A';
        if (work->Code == 'b') work->Actions &= ~SGA_USE;
        if (veto_return && work->EditOp == EO_ENTER) work->Actions &= ~SGA_END;
    }
    return TRUE;
}
static void pump(Object *app)
{
    for (int i = 0; i < 6; i++)
    { Delay(2); DoMethod(app, MUIM_Application_InputBuffered); }
}
static void key(Object *app, struct IOStdReq *io, ULONG raw)
{
    struct InputEvent event = {0};
    event.ie_Class = IECLASS_RAWKEY; event.ie_Code = raw;
    event.ie_Qualifier = qualifier;
    io->io_Command = IND_WRITEEVENT; io->io_Data = &event; io->io_Length = sizeof(event);
    DoIO((APTR)io); pump(app);
    event.ie_Code |= IECODE_UP_PREFIX;
    DoIO((APTR)io); pump(app);
}
static void contents(Object *obj, const char *expected)
{
    STRPTR text = NULL;
    GetAttr(MUIA_String_Contents, obj, (ULONG *)&text);
    if (!text || strcmp((char *)text, expected))
    { printf("FAIL contents expected=%s actual=%s\n", expected, text ? (char *)text : "NULL"); failures++; }
}
int main(int argc, char **argv)
{
    struct Hook hook = {0};
    Object *obj, *win, *app;
    struct MsgPort *port;
    struct IOStdReq *io;
    struct Screen *screen = NULL;
    ULONG got;
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    printf("library=%u.%u\n", MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    if (argc > 1 && !strcmp(argv[1], "screen"))
        screen = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2, TAG_DONE);
    hook.h_Entry = (HOOKFUNC)edit_hook;
    obj = MO(MUIC_String, 0x8042b06eUL, 2, /* Newstring compatibility mode */
        MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_Contents, "seed", MUIA_String_EditHook, &hook, TAG_DONE);
    win = MO(MUIC_Window, MUIA_Window_Title, "String input probe",
        MUIA_Window_Screen, screen, MUIA_Window_RootObject, obj, TAG_DONE);
    app = MO(MUIC_Application, MUIA_Application_Window, win, TAG_DONE);
    if (!app) return 20;
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    SetAttrs(win, MUIA_Window_ActiveObject, obj, TAG_DONE);
    SetAttrs(obj, MUIA_String_BufferPos, 4, TAG_DONE);
    pump(app);
    port = CreateMsgPort();
    io = port ? (APTR)CreateIORequest(port, sizeof(*io)) : NULL;
    if (!io || OpenDevice("input.device", 0, (APTR)io, 0)) return 20;
    key(app, io, 0x20); contents(obj, "seedA");
    key(app, io, 0x35); contents(obj, "seedA");
    key(app, io, 0x41); contents(obj, "seed");
    veto_return = TRUE;
    key(app, io, 0x44);
    got = 0; GetAttr(MUIA_Window_ActiveObject, win, &got);
    printf("return-veto-active=%d\n", got == (ULONG)obj);
    if (got == (ULONG)obj) failures++;
    veto_return = FALSE;
    SetAttrs(win, MUIA_Window_ActiveObject, obj, TAG_DONE);
    pump(app);
    key(app, io, 0x44);
    got = 0; GetAttr(MUIA_String_Acknowledge, obj, &got);
    if (!got || strcmp((char *)got, "seed")) failures++;
    if (!calls) failures++;
    SetAttrs(obj, MUIA_String_LonelyEditHook, TRUE, TAG_DONE);
    SetAttrs(win, MUIA_Window_ActiveObject, obj, TAG_DONE);
    SetAttrs(obj, MUIA_String_BufferPos, 4, TAG_DONE);
    lonely = TRUE;
    key(app, io, 0x20);
    lonely = FALSE;
    contents(obj, "lonely");
    if (lonely_calls != 1) failures++;
    SetAttrs(obj, MUIA_String_LonelyEditHook, FALSE, TAG_DONE);
    SetAttrs(obj, MUIA_String_EditHook, NULL, MUIA_String_Contents, "ftp://",
        TAG_DONE);
    SetAttrs(win, MUIA_Window_ActiveObject, obj, TAG_DONE);
    SetAttrs(obj, MUIA_String_BufferPos, 6, 0x81cd0025UL, 1, TAG_DONE);
    pump(app);
    key(app, io, 0x17); /* i replaces the autocomplete suffix */
    contents(obj, "fi");
    SetAttrs(obj, MUIA_String_BufferPos, 2, 0x81cd0025UL, 0, TAG_DONE);
    qualifier = IEQUALIFIER_RCOMMAND;
    key(app, io, 0x33); /* copy */
    qualifier = 0;
    SetAttrs(obj, MUIA_String_Contents, "", TAG_DONE);
    qualifier = IEQUALIFIER_RCOMMAND;
    key(app, io, 0x34); /* paste */
    qualifier = 0;
    contents(obj, "fi");
    if (MUIMasterBase->lib_Version == 19)
    {
        Object *menu = (Object *)DoMethod(obj, MUIM_ContextMenuBuild, 0, 0);
        if ((ULONG)menu <= 1) failures++;
        else
        {
            Object *choice = (Object *)DoMethod(menu, MUIM_FindUData, 4);
            DoMethod(obj, MUIM_ContextMenuChoice, choice);
            choice = (Object *)DoMethod(menu, MUIM_FindUData, 1);
            DoMethod(obj, MUIM_ContextMenuChoice, choice);
            contents(obj, "");
            choice = (Object *)DoMethod(menu, MUIM_FindUData, 3);
            DoMethod(obj, MUIM_ContextMenuChoice, choice);
            contents(obj, "fi");
        }
    }
    CloseDevice((APTR)io); DeleteIORequest((APTR)io); DeleteMsgPort(port);
    MUI_DisposeObject(app);
    if (screen) CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d hooks=%lu\n", failures ? "FAIL" : "PASS", failures, calls);
    return failures ? 20 : 0;
}

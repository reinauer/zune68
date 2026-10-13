/* Exercise actual popup input and image drawing, including a tall image. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <intuition/imageclass.h>
#include <intuition/intuitionbase.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(name, ...) \
    ({ ULONG tags[] = {__VA_ARGS__}; MUI_NewObjectA(name, (struct TagItem *)tags); })
struct Library *MUIMasterBase;
extern struct IntuitionBase *IntuitionBase;
ULONG __stack = 65536;
static struct Window *window;
static volatile ULONG draws, disposed, done, select_item;
static LONG px, py;
static LONG boxes[8][4];
static ULONG nboxes, keyqual, chosen;
static ULONG image_dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == OM_DISPOSE) disposed++;
    if (msg->MethodID == IM_DRAW)
    {
        struct impDraw *draw = (APTR)msg;
        draws++;
        SetAPen(draw->imp_RPort, 1);
        RectFill(draw->imp_RPort, draw->imp_Offset.X, draw->imp_Offset.Y,
            draw->imp_Offset.X + 11, draw->imp_Offset.Y + 19);
        return 1;
    }
    return DoSuperMethodA(cl, obj, msg);
}
static void inject(struct IOStdReq *io, ULONG type, ULONG code, LONG x, LONG y)
{
    struct InputEvent ev = {0};
    struct IEPointerPixel pos = {0};
    ev.ie_Class = type;
    ev.ie_Code = code;
    ev.ie_Qualifier = keyqual;
    if (type == IECLASS_NEWPOINTERPOS)
    {
        pos.iepp_Screen = window->WScreen;
        pos.iepp_Position.X = x; pos.iepp_Position.Y = y;
        ev.ie_SubClass = IESUBCLASS_PIXEL;
        ev.ie_EventAddress = &pos;
    }
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &ev; io->io_Length = sizeof(ev);
    DoIO((struct IORequest *)io);
    Delay(10);
}
static void driver(void)
{
    struct MsgPort *port = CreateMsgPort();
    struct IOStdReq *io = port ? (APTR)CreateIORequest(port, sizeof(*io)) : NULL;
    if (io && !OpenDevice("input.device", 0, (APTR)io, 0))
    {
        Delay(30);
        if (select_item)
        {
            struct Window *w;
            inject(io, IECLASS_NEWPOINTERPOS, 0, px + 8, py + 8);
            Delay(20);
            Forbid();
            for (w = window->WScreen->FirstWindow; w && nboxes < 8; w = w->NextWindow)
                if (w != window && (w->Flags & WFLG_BORDERLESS))
                {
                    boxes[nboxes][0] = w->LeftEdge;
                    boxes[nboxes][1] = w->TopEdge;
                    boxes[nboxes][2] = w->Width;
                    boxes[nboxes++][3] = w->Height;
                }
            Permit();
            if (nboxes)
            {
                inject(io, IECLASS_NEWPOINTERPOS, 0,
                    boxes[0][0] + 20, boxes[0][1] + 8);
                inject(io, IECLASS_RAWMOUSE, IECODE_LBUTTON, 0, 0);
                inject(io, IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0);
            }
        }
        inject(io, IECLASS_RAWKEY, 0x45, 0, 0);
        inject(io, IECLASS_RAWKEY, 0xc5, 0, 0);
        CloseDevice((APTR)io);
    }
    if (io) DeleteIORequest((APTR)io);
    if (port) DeleteMsgPort(port);
    done = TRUE;
}
int main(int argc, char **argv)
{
    Object *item, *strip, *root, *win, *app, *lower, *upper, *menu;
    struct IClass *ic;
    Object *image;
    ULONG result, opened = 0;
    int failures = 0;
    struct Screen *screen = NULL;
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    if (argc > 1 && !strcmp(argv[1], "screen"))
        screen = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256,
            SA_Depth, 2, TAG_DONE);
    printf("library=%u.%u\n", MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    fflush(stdout);
    ic = MakeClass(NULL, "imageclass", NULL, 0, 0);
    if (!ic) return 20;
    ic->cl_Dispatcher.h_Entry = (HOOKFUNC)image_dispatch;
    image = NewObject(ic, NULL, IA_Width, 12, IA_Height, 20, TAG_DONE);
    item = MO(MUIC_Menuitem, MUIA_Menuitem_Title, "Pick image",
        MUIA_Menuitem_Image, image, MUIA_UserData, 42,
        MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, TAG_DONE);
    lower = MO(MUIC_Menuitem, MUIA_Menuitem_Title, "Lower",
        MUIA_Menuitem_Shortcut, "a", TAG_DONE);
    upper = MO(MUIC_Menuitem, MUIA_Menuitem_Title, "Upper",
        MUIA_Menuitem_Shortcut, "A", TAG_DONE);
    strip = MO(MUIC_Menustrip, MUIA_Menustrip_CaseSensitive, TRUE, MUIA_Family_Child,
        menu = MO(MUIC_Menu, MUIA_Menu_Title, "Probe", MUIA_Family_Child, item,
            MUIA_Family_Child, MO(MUIC_Menuitem, MUIA_Menuitem_Title, "Disabled",
                MUIA_Menuitem_Enabled, FALSE, TAG_DONE),
            MUIA_Family_Child, lower, MUIA_Family_Child, upper, TAG_DONE), TAG_DONE);
    root = MO(MUIC_Rectangle, TAG_DONE);
    win = MO(MUIC_Window, MUIA_Window_Title, "Menu input probe",
        MUIA_Window_Menustrip, strip, MUIA_Window_Width, 240, MUIA_Window_Height, 100,
        MUIA_Window_Screen, screen, MUIA_Window_RootObject, root, TAG_DONE);
    app = MO(MUIC_Application, MUIA_Application_Window, win, TAG_DONE);
    if (!app) return 20;
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Window, win, (ULONG *)&window);
    GetAttr(MUIA_Window_Open, win, &opened);
    if (!opened || !window) return 20;
    DoMethod(lower, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
        app, 3, MUIM_WriteLong, 1, &chosen);
    DoMethod(upper, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
        app, 3, MUIM_WriteLong, 2, &chosen);
    {
        struct MsgPort *port = CreateMsgPort();
        struct IOStdReq *io = port ? (APTR)CreateIORequest(port, sizeof(*io)) : NULL;
        if (!io || OpenDevice("input.device", 0, (APTR)io, 0)) return 20;
        ActivateWindow(window);
        for (ULONG shift = 0; shift < 2; shift++)
        {
            keyqual = IEQUALIFIER_RCOMMAND | (shift ? IEQUALIFIER_LSHIFT : 0);
            chosen = 0;
            inject(io, IECLASS_RAWKEY, 0x20, 0, 0);
            inject(io, IECLASS_RAWKEY, 0xa0, 0, 0);
            for (int j = 0; j < 6; j++)
            { DoMethod(app, MUIM_Application_InputBuffered); Delay(2); }
            printf("shortcut shift=%lu chosen=%lu\n", shift, chosen);
            if (chosen != shift + 1) failures++;
        }
        keyqual = 0;
        CloseDevice((APTR)io); DeleteIORequest((APTR)io); DeleteMsgPort(port);
    }
    printf("attached init=%lu nested=%lu\n",
        DoMethod(strip, MUIM_Menustrip_InitChange),
        DoMethod(strip, MUIM_Menustrip_InitChange));
    {
        Object *added = MO(MUIC_Menuitem, MUIA_Menuitem_Title, "Added", TAG_DONE);
        DoMethod(menu, OM_ADDMEMBER, added);
        SetAttrs(lower, MUIA_Menuitem_Enabled, FALSE, TAG_DONE);
        SetAttrs(lower, MUIA_Menuitem_Enabled, TRUE, TAG_DONE);
        DoMethod(strip, MUIM_Menustrip_ExitChange);
        DoMethod(menu, OM_REMMEMBER, added);
        MUI_DisposeObject(added);
        DoMethod(strip, MUIM_Menustrip_ExitChange);
    }
    px = window->LeftEdge + _left(root) + 10;
    py = window->TopEdge + _top(root) + 10;
    for (select_item = 0; select_item < 2; select_item++)
    {
        done = FALSE;
        if (!CreateNewProcTags(NP_Entry, driver, NP_Name, "Menu probe input",
            NP_StackSize, 16384, TAG_DONE)) return 20;
        result = DoMethod(strip, MUIM_Menustrip_Popup, root, 0, 10, 10);
        while (!done) Delay(1);
        printf("popup select=%lu result=%lu draws=%lu\n", select_item, result, draws);
        for (ULONG i = 0; i < nboxes; i++)
            printf("popup box %ld,%ld %ldx%ld\n", boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3]);
        if (result != (select_item ? 42 : 0)) failures++;
    }
    if (!draws) failures++;
    if (strstr((char *)MUIMasterBase->lib_IdString, "Zune68") != NULL)
    {
        DoMethod(app, MUIM_Application_PushMethod, win, 3,
            MUIM_Set, MUIA_Window_Open, FALSE);
        result = DoMethod(strip, MUIM_Menustrip_Popup, root, 0, 10, 10);
        GetAttr(MUIA_Window_Open, win, &opened);
        printf("popup parent close result=%lu open=%lu\n", result, opened);
        if (result || opened) failures++;
    }
    MUI_DisposeObject(app);
    if (disposed != 1 || !FreeClass(ic)) failures++;
    if (screen) CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 20 : 0;
}

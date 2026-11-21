/* A window control char (MUIA_ControlChar) typed while the pointer is
 * over a list. The pointer is put over the check mark and then over the
 * list through input.device, and "b" is typed each time; the check mark
 * (control char "b") should toggle both times: a list eats only the
 * mouse wheel's keys.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit \
 *        listkey.c -o listkey -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static struct IOStdReq *io;

static void event(struct InputEvent *ie)
{
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = ie;
    io->io_Length = sizeof(*ie);
    DoIO((struct IORequest *)io);
}

static void pointer(struct Screen *scr, WORD x, WORD y)
{
    struct InputEvent ie;
    struct IEPointerPixel pp;

    memset(&ie, 0, sizeof(ie));
    pp.iepp_Screen = scr;
    pp.iepp_Position.X = x;
    pp.iepp_Position.Y = y;
    ie.ie_Class = IECLASS_NEWPOINTERPOS;
    ie.ie_SubClass = IESUBCLASS_PIXEL;
    ie.ie_Code = IECODE_NOBUTTON;
    ie.ie_EventAddress = &pp;
    event(&ie);
}

static void key(UWORD code)
{
    struct InputEvent ie;

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = code;
    event(&ie);
    ie.ie_Code = code | IECODE_UP_PREFIX;
    event(&ie);
}

static void settle(Object *app)
{
    int i;

    for (i = 0; i < 10; i++)
    {
        Delay(2);
        DoMethod(app, MUIM_Application_InputBuffered);
    }
}

int main(void)
{
    static const char *entries[] = { "one", "two", "three", "four", NULL };
    Object *app, *win, *list, *check;
    struct Window *w = NULL;
    struct MsgPort *port;
    ULONG open = FALSE;
    IPTR sel = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    if (!(port = CreateMsgPort()))
        return 20;
    io = (struct IOStdReq *)CreateIORequest(port, sizeof(*io));
    if (!io || OpenDevice("input.device", 0, (struct IORequest *)io, 0))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Title, "listkey",
        WindowContents, VGroup,
            Child, HGroup,
                Child, check = CheckMark(FALSE),
                Child, LLabel("b toggles"),
                End,
            Child, ListviewObject, MUIA_Listview_List, list = ListObject,
                InputListFrame, MUIA_List_SourceArray, entries, End, End,
            End, End, End;
    if (!app)
        return 20;
    set(check, MUIA_ControlChar, 'b');
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &w);
    printf("open %s\n", open && w ? "yes" : "no");
    if (open && w)
    {
        ActivateWindow(w);
        settle(app);
        pointer(w->WScreen, w->LeftEdge + _mleft(check) + 2,
            w->TopEdge + _mtop(check) + 2);
        settle(app);
        key(0x35);  /* b */
        settle(app);
        get(check, MUIA_Selected, &sel);
        printf("pointer over the check mark: %s\n", sel ? "toggled" : "not toggled");
        pointer(w->WScreen, w->LeftEdge + _mleft(list) + 10,
            w->TopEdge + _mtop(list) + 4);
        settle(app);
        key(0x35);
        settle(app);
        get(check, MUIA_Selected, &sel);
        printf("pointer over the list: %s\n", sel ? "not toggled" : "toggled");
    }
    MUI_DisposeObject(app);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    CloseLibrary(MUIMasterBase);
    return 0;
}

/* Who gets Amiga-C while a String gadget is active in a window whose
 * menu strip has a "C" shortcut: the menu (return id) or the string
 * (copy to the clipboard)? Prints the Intuition menu strip state, the
 * ids received and the clipboard size, for native and MUI 3.9. The
 * keys are injected from the host while the probe waits.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit menukeyfocus.c -o menukeyfocus -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
#include <devices/clipboard.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/iffparse.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase, *IFFParseBase;
ULONG __stack = 65536;

static LONG clip_size(void)
{
    struct IOClipReq *io;
    struct MsgPort *port = CreateMsgPort();
    LONG size = -1;

    if (!port)
        return -1;
    if ((io = (struct IOClipReq *)CreateIORequest(port, sizeof(*io))))
    {
        if (!OpenDevice("clipboard.device", 0, (struct IORequest *)io, 0))
        {
            ULONG hdr[3];

            io->io_Command = CMD_READ;
            io->io_Data = (STRPTR)hdr;
            io->io_Length = 12;
            io->io_Offset = 0;
            io->io_ClipID = 0;
            if (!DoIO((struct IORequest *)io) && io->io_Actual == 12)
                size = hdr[1];
            /* finish the read so the clipboard is released */
            io->io_Command = CMD_READ;
            io->io_Data = NULL;
            io->io_Length = 0x7fffffff;
            DoIO((struct IORequest *)io);
            CloseDevice((struct IORequest *)io);
        }
        DeleteIORequest((struct IORequest *)io);
    }
    DeleteMsgPort(port);
    return size;
}

int main(void)
{
    Object *app, *win, *str, *strip;
    struct Window *w = NULL;
    ULONG sigs = 0, open = FALSE;
    LONG id, ticks;
    int rc = 20;

    if (!freopen("Bench:menukeyfocus.log", "w", stdout))
        return 20;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    strip = MenustripObject,
        Child, MenuObject, MUIA_Menu_Title, "Edit",
            Child, MenuitemObject, MUIA_Menuitem_Title, "Copy",
                MUIA_Menuitem_Shortcut, "C", MUIA_UserData, 77, End,
            Child, MenuitemObject, MUIA_Menuitem_Title, "Quit",
                MUIA_Menuitem_Shortcut, "Q", MUIA_UserData, 78, End,
            End,
        End;
    app = ApplicationObject,
        MUIA_Application_Title, "menukeyfocus",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "menukeyfocus",
            MUIA_Window_Menustrip, strip,
            WindowContents, VGroup,
                Child, str = StringObject, StringFrame,
                    MUIA_String_Contents, "copy me", End,
                End,
            End,
        End;
    if (!app)
        goto done;
    DoMethod(win, MUIM_Notify, MUIA_Window_MenuAction, MUIV_EveryTime,
        app, 2, MUIM_Application_ReturnID, MUIV_TriggerValue);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    set(win, MUIA_Window_ActiveObject, str);
    get(win, MUIA_Window_Window, &w);
    printf("menustrip %s, clipboard %ld\n",
        w && w->MenuStrip ? "attached" : "none", (long)clip_size());
    fflush(stdout);
    /* About 15 seconds for the host to send RAmiga-A and RAmiga-C */
    for (ticks = 0; ticks < 15 * 50; ticks++)
    {
        id = DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (id > 0)
        {
            printf("menu id %ld\n", (long)id);
            fflush(stdout);
        }
        Delay(1);
    }
    printf("menustrip %s, clipboard %ld\n",
        w && w->MenuStrip ? "attached" : "none", (long)clip_size());
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

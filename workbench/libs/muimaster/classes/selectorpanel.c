/* Requester panel lifecycle, separate from AROS's collapsible Panel. */
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "selectorpanel.h"
extern struct Library *MUIMasterBase;
struct PanelData
{
    BOOL running, done, accepted;
    Object *close_notifier, *watched_window;
};
static void unwatch_close(struct PanelData *d)
{
    if (d->watched_window)
    {
        DoMethod(d->watched_window, MUIM_KillNotifyObj,
            MUIA_Window_CloseRequest, d->close_notifier);
        d->watched_window = NULL;
    }
}
BOOPSI_DISPATCHER(IPTR, SelectorPanel_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == OM_NEW) return DoSuperMethodA(cl, obj, msg);
    struct PanelData *d = INST_DATA(cl, obj);
    switch (msg->MethodID)
    {
    case OM_DISPOSE:
        unwatch_close(d);
        if (d->close_notifier) MUI_DisposeObject(d->close_notifier);
        break;
    case MUIM_Cleanup:
        unwatch_close(d);
        d->running = FALSE;
        break;
    case MUIM_HandleEvent:
    {
        struct MUIP_HandleEvent *event = (APTR)msg;
        if (event->imsg && event->imsg->Class == IDCMP_RAWKEY &&
            event->imsg->Code == 0x45)
        { d->done = TRUE; d->accepted = FALSE; return MUI_EventHandlerRC_Eat; }
        return 0;
    }
    case MUIM_Zune_PanelFinish:
        d->accepted = ((IPTR *)msg)[1] != 0;
        d->done = TRUE;
        return TRUE;
    case MUIM_Panel_Run:
    {
        struct MUIP_Panel_Run *run = (APTR)msg;
        struct ZuneWindowGuard guard = {0};
        struct MUI_EventHandlerNode handler = {0};
        Object *app = run->app, *win = run->win;
        ULONG signals = 0;
        BOOL result;
        if (!app || !win || d->running) return FALSE;
        d->running = TRUE;
        d->done = d->accepted = FALSE;
        if (!d->close_notifier)
        {
            struct TagItem tags[] = {{TAG_DONE, 0}};
            d->close_notifier = MUI_NewObjectA(MUIC_Notify, tags);
            if (!d->close_notifier) { d->running = FALSE; return FALSE; }
        }
        set(d->close_notifier, MUIA_UserData, FALSE);
        set(win, MUIA_Window_Open, TRUE);
        if (!XGET(win, MUIA_Window_Open)) { d->running = FALSE; return FALSE; }
        d->watched_window = win;
        DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
            d->close_notifier, 3, MUIM_Set, MUIA_UserData, TRUE);
        handler.ehn_Events = IDCMP_RAWKEY;
        handler.ehn_Priority = 1;
        handler.ehn_Flags = MUI_EHF_GUIMODE;
        handler.ehn_Object = obj;
        handler.ehn_Class = cl;
        guard.handler = &handler;
        DoMethod(win, MUIM_Window_AddEventHandler, &handler);
        ZuneWatchWindow(win, &guard);
        while (!d->done)
        {
            IPTR id = DoMethod(app, MUIM_Application_NewInput, &signals);
            /* A callback may have disposed the entire window and panel. */
            if (guard.closed) return FALSE;
            if (id)
            {
                /* Let the application's own loop handle its return IDs. */
                DoMethod(app, MUIM_Application_ReturnID, id);
                break;
            }
            if (XGET(d->close_notifier, MUIA_UserData)) break;
            if (!d->done && signals)
            {
                signals = Wait(signals | SIGBREAKF_CTRL_C);
                if (signals & SIGBREAKF_CTRL_C) break;
            }
        }
        result = d->done && d->accepted;
        unwatch_close(d);
        ZuneUnwatchWindow(win, &guard);
        DoMethod(win, MUIM_Window_RemEventHandler, &handler);
        d->running = FALSE;
        /* The caller owns the window; leave it open for geometry queries. */
        return result;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END
const struct __MUIBuiltinClass _MUI_SelectorPanel_desc = {
    MUIC_Panel, MUIC_Group, sizeof(struct PanelData),
    (void *)SelectorPanel_Dispatcher
};

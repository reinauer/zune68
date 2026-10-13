/*
    Copyright (C) 2002-2015, The AROS Development Team. All rights reserved.
*/

#include <string.h>
#include <exec/memory.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/inputevent.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>

/*  #define MYDEBUG 1 */
#include "debug.h"
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "menu.h"

extern struct Library *MUIMasterBase;

#define MENUF_CHECKED        (1<<0)
#define MENUF_CHECKIT        (1<<1)
#define MENUF_COMMANDSTRING  (1<<2)
#define MENUF_ENABLED        (1<<3)
#define MENUF_TOGGLE         (1<<4)
#define MENUF_COPYSTRINGS    (1<<5)
#define MENUF_FREEIMAGE      (1<<6)
#define MENUF_CASESENSITIVE  (1<<7)

#define MENUF_MENUSTRIP      (1<<15)
#define MENUF_MENU           (1<<16)
#define MENUF_MENUITEM       (1<<17)

#define MUIA_Menuitem_Type   (MUIB_Menuitem | 0x00000001)

#define MUIV_Menuitem_Type_Menustrip    (-1)
#define MUIV_Menuitem_Type_Menu         (-2)
#define MUIV_Menuitem_Type_Menuitem     (-3)

struct MUI_MenuitemData
{
    ULONG flags;
    ULONG exclude;

    char *shortcut;
    char *title;

    struct NewMenu *newmenu;

    struct MenuItem *trigger;
    struct Image *image;
    CONST_STRPTR aiss;
    ULONG change_depth;
};

/* The private attribute is queried only while walking menu ancestors. */
#define MUIA_Menuitem_ChangeDepth (MUIB_Menuitem | 0x00000002)

static void Menuitem_Changed(Object *obj)
{
    Object *parent = obj;
    IPTR depth;
    while (parent)
    {
        depth = 0;
        if (get(parent, MUIA_Menuitem_ChangeDepth, &depth) && depth)
            return;
        {
            Object *next = NULL;
            get(parent, MUIA_Parent, &next);
            parent = next;
        }
    }
    if (muiNotifyData(obj)->mnd_GlobalInfo && _app(obj))
        DoMethod(_app(obj), MUIM_Application_UpdateMenus);
}

static BOOL Menuitem_SetString(struct MUI_MenuitemData *data,
    char **slot, const char *value)
{
    char *copy = (char *)value;
    if (*slot == value) return TRUE;
    if ((data->flags & MENUF_COPYSTRINGS) && value &&
        value != (char *)NM_BARLABEL && value != (char *)MUIV_Menuitem_Shortcut_Check)
    {
        if (slot == &data->title &&
            data->shortcut == (char *)MUIV_Menuitem_Shortcut_Check)
        {
            ULONG length = strlen(value + 2) + 3;
            copy = AllocVec(length, MEMF_ANY);
            if (copy) memcpy(copy, value, length);
        }
        else
            copy = StrDup(value);
        if (!copy) return FALSE;
    }
    if ((data->flags & MENUF_COPYSTRINGS) && *slot &&
        *slot != (char *)NM_BARLABEL && *slot != (char *)MUIV_Menuitem_Shortcut_Check)
        FreeVec(*slot);
    *slot = copy;
    return TRUE;
}

static BOOL Menuitem_Changing(Object *obj)
{
    struct MinList *children = NULL;
    Object *state, *child;
    IPTR depth = 0;
    get(obj, MUIA_Menuitem_ChangeDepth, &depth);
    if (depth) return TRUE;
    get(obj, MUIA_Family_List, &children);
    state = children ? (Object *)children->mlh_Head : NULL;
    while (state && (child = NextObject(&state)))
        if (Menuitem_Changing(child)) return TRUE;
    return FALSE;
}

static int Menuitem_GetTotalChildren(Object *obj)
{
    Object *cstate;
    Object *child;
    struct MinList *ChildList = NULL;
    int num = 0;

    get(obj, MUIA_Family_List, &ChildList);
    cstate = (Object *) ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        num++;
        num += Menuitem_GetTotalChildren(child);
    }
    return num;
}

static int Menuitem_FillNewMenu(Object *obj, struct NewMenu *menu,
    int depth)
{
    Object *cstate;
    Object *child;
    struct MinList *ChildList = NULL;
    int num = 0;

    if (depth > 2)
        return 0;

    get(obj, MUIA_Family_List, &ChildList);
    cstate = (Object *) ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        int entries;
        IPTR checkit = 0, checked = 0, toggle = 0, enabled = 0, cmdstring = 0, exclude = 0;

        get(child, MUIA_Menuitem_Title, &menu->nm_Label);
        get(child, MUIA_Menuitem_Shortcut, &menu->nm_CommKey);
        if (menu->nm_CommKey == (STRPTR)MUIV_Menuitem_Shortcut_Check)
        {
            menu->nm_CommKey = menu->nm_Label;
            if (menu->nm_Label && menu->nm_Label != NM_BARLABEL)
                menu->nm_Label += 2;
        }
        get(child, MUIA_Menuitem_Checkit, &checkit);
        get(child, MUIA_Menuitem_Checked, &checked);
        get(child, MUIA_Menuitem_Toggle, &toggle);
        get(child, MUIA_Menuitem_Enabled, &enabled);
        {
            IPTR parent_enabled = TRUE;
            get(obj, MUIA_Menustrip_Enabled, &parent_enabled);
            if (!parent_enabled) enabled = FALSE;
        }
        get(child, MUIA_Menuitem_CommandString, &cmdstring);
        if (checkit)
            menu->nm_Flags |= CHECKIT;
        if (checked)
            menu->nm_Flags |= CHECKED;
        if (toggle)
            menu->nm_Flags |= MENUTOGGLE;
        if (cmdstring)
            menu->nm_Flags |= COMMSEQ;
        get(child, MUIA_Menuitem_Exclude, &exclude);
        menu->nm_MutualExclude = exclude;

        if (depth == 0)
        {
            SIPTR type = 0;
            get(child, MUIA_Menuitem_Type, &type);

            if (type == MUIV_Menuitem_Type_Menuitem) {
                /* Depth 0 Menuitems are to become items of current menu,
                 * not menus. MUI behavior */
                menu->nm_Type = NM_ITEM;
            } else
                menu->nm_Type = NM_TITLE;
        }
        else if (depth == 1)
            menu->nm_Type = NM_ITEM;
        else if (depth == 2)
            menu->nm_Type = NM_SUB;

        if (!enabled)
        {
            if (menu->nm_Type == NM_TITLE)
                menu->nm_Flags |= NM_MENUDISABLED;
            else
                menu->nm_Flags |= NM_ITEMDISABLED;
        }

        menu->nm_UserData = child;

        menu++;
        num++;
        entries = Menuitem_FillNewMenu(child, menu, depth + 1);

        menu += entries;
        num += entries;
    }
    return num;
}

/**************************************************************************
 ...
**************************************************************************/
static struct NewMenu *Menuitem_BuildNewMenu(struct MUI_MenuitemData *data,
    Object *obj)
{
    int entries = Menuitem_GetTotalChildren(obj);
    if (data->newmenu)
        FreeVec(data->newmenu);
    data->newmenu = NULL;
    if (!entries || Menuitem_Changing(obj))
        return NULL;

    if ((data->newmenu =
            (struct NewMenu *)AllocVec((entries +
                    1) * sizeof(struct NewMenu), MEMF_CLEAR)))
    {
        Menuitem_FillNewMenu(obj, data->newmenu, 0);
    }
    return data->newmenu;
}

/**************************************************************************
 OM_NEW
**************************************************************************/
IPTR Common__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_MenuitemData *data;
    struct TagItem *tags, *tag;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
        /* We need no tags */
    if (!obj)
        return 0;

    data = INST_DATA(cl, obj);

    data->flags = MENUF_ENABLED | MENUF_FREEIMAGE;
    if (GetTagData(MUIA_Menuitem_CopyStrings, FALSE, msg->ops_AttrList) ||
        GetTagData(MUIA_Menu_CopyStrings, FALSE, msg->ops_AttrList))
        data->flags |= MENUF_COPYSTRINGS;
    if (!GetTagData(MUIA_Menuitem_FreeImage, TRUE, msg->ops_AttrList))
        data->flags &= ~MENUF_FREEIMAGE;
    if (GetTagData(MUIA_Menuitem_Shortcut, 0, msg->ops_AttrList) ==
        (IPTR)MUIV_Menuitem_Shortcut_Check)
        data->shortcut = (char *)MUIV_Menuitem_Shortcut_Check;
    if (GetTagData(MUIA_Menustrip_CaseSensitive, FALSE, msg->ops_AttrList))
        data->flags |= MENUF_CASESENSITIVE;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Menuitem_Image:
            data->image = (struct Image *)tag->ti_Data;
            break;
        case MUIA_Menuitem_AISSName:
            data->aiss = (CONST_STRPTR)tag->ti_Data;
            break;

        case MUIA_Menuitem_Checked:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_CHECKED);
            break;

        case MUIA_Menuitem_Checkit:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_CHECKIT);
            break;

        case MUIA_Menuitem_CommandString:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_COMMANDSTRING);
            break;

        case MUIA_Menustrip_Enabled:
        case MUIA_Menu_Enabled:
        case MUIA_Menuitem_Enabled:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_ENABLED);
            break;

        case MUIA_Menuitem_Toggle:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_TOGGLE);
            break;

        case MUIA_Menuitem_Exclude:
            data->exclude = tag->ti_Data;
            break;

        case MUIA_Menuitem_Shortcut:
            if (!Menuitem_SetString(data, &data->shortcut, (char *)tag->ti_Data))
            {
                CoerceMethod(cl, obj, OM_DISPOSE);
                return 0;
            }
            break;

        case MUIA_Menu_Title:
        case MUIA_Menuitem_Title:
            if (!Menuitem_SetString(data, &data->title, (char *)tag->ti_Data))
            {
                CoerceMethod(cl, obj, OM_DISPOSE);
                return 0;
            }
            break;
        }
    }

    return (IPTR) obj;
}

IPTR Menuitem__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_MenuitemData *data;
    obj = (Object *) Common__OM_NEW(cl, obj, msg);
    if (!obj) return 0;
    data = INST_DATA(cl, obj);

    data->flags |= MENUF_MENUITEM;

    return (IPTR) obj;
}

IPTR Menu__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_MenuitemData *data;
    obj = (Object *) Common__OM_NEW(cl, obj, msg);
    if (!obj) return 0;
    data = INST_DATA(cl, obj);

    data->flags |= MENUF_MENU;

    return (IPTR) obj;
}

IPTR Menustrip__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_MenuitemData *data;
    obj = (Object *) Common__OM_NEW(cl, obj, msg);
    if (!obj) return 0;
    data = INST_DATA(cl, obj);

    data->flags |= MENUF_MENUSTRIP;

    return (IPTR) obj;
}

/**************************************************************************
 OM_SET
**************************************************************************/
IPTR Menuitem__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_MenuitemData *data;
    struct TagItem *tags, *tag;

    data = INST_DATA(cl, obj);

    BOOL rebuild = FALSE;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Menuitem_Image:
            if (data->image != (struct Image *)tag->ti_Data)
            {
                if ((data->flags & MENUF_FREEIMAGE) && data->image)
                    DisposeObject((Object *)data->image);
                data->image = (struct Image *)tag->ti_Data;
                rebuild = TRUE;
            }
            break;
        case MUIA_Menuitem_AISSName:
            data->aiss = (CONST_STRPTR)tag->ti_Data;
            rebuild = TRUE;
            break;

        case MUIA_Menuitem_Checked:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_CHECKED);
            if (data->exclude && (data->flags & MENUF_CHECKED))
            {
                Object *parent = NULL;
                get(obj, MUIA_Parent, &parent);

                if (parent)
                {
                    Object *child;
                    Object *cstate;
                    struct MinList *ChildList = NULL;
                    ULONG i = 1;

                    get(parent, MUIA_Family_List, &ChildList);
                    cstate = (Object *) ChildList->mlh_Head;
                    while ((child = NextObject(&cstate)))
                    {
                        if ((i & data->exclude) && (child != obj))
                        {
                            IPTR checkit = 0, checked = 0;

                            get(child, MUIA_Menuitem_Checkit, &checkit);
                            get(child, MUIA_Menuitem_Checked, &checked);

                            if (checkit && checked)
                            {
                                set(child, MUIA_Menuitem_Checked, FALSE);
                            }
                        }

                        i <<= 1;
                    }
                }

            }
            rebuild = TRUE;
            break;

        case MUIA_Menuitem_Checkit:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_CHECKIT);
            rebuild = TRUE;
            break;

        case MUIA_Menuitem_CommandString:
            _handle_bool_tag(data->flags, tag->ti_Data,
                MENUF_COMMANDSTRING);
            rebuild = TRUE;
            break;

        case MUIA_Menustrip_Enabled:
        case MUIA_Menu_Enabled:
        case MUIA_Menuitem_Enabled:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_ENABLED);
            tag->ti_Tag = TAG_IGNORE;
            rebuild = TRUE;
            break;

        case MUIA_Menuitem_Toggle:
            _handle_bool_tag(data->flags, tag->ti_Data, MENUF_TOGGLE);
            rebuild = TRUE;
            break;

        case MUIA_Menuitem_Exclude:
            data->exclude = tag->ti_Data;
            rebuild = TRUE;
            break;

        case MUIA_Menuitem_Shortcut:
            Menuitem_SetString(data, &data->shortcut, (char *)tag->ti_Data);
            rebuild = TRUE;
            break;

        case MUIA_Menu_Title:
        case MUIA_Menuitem_Title:
            Menuitem_SetString(data, &data->title, (char *)tag->ti_Data);
            tag->ti_Tag = TAG_IGNORE;
            rebuild = TRUE;
            break;

        case MUIA_Menuitem_Trigger:
            data->trigger = (struct MenuItem *)tag->ti_Data;
            break;
        }
    }

    if (rebuild)
    {
        Menuitem_Changed(obj);
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
 OM_GET
**************************************************************************/
#define STORE *(msg->opg_Storage)
IPTR Menuitem__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct MUI_MenuitemData *data = INST_DATA(cl, obj);

    switch (msg->opg_AttrID)
    {
    case MUIA_Menuitem_ChangeDepth:
        STORE = data->change_depth;
        return 1;
    case MUIA_Menustrip_CaseSensitive:
        STORE = !!(data->flags & MENUF_CASESENSITIVE);
        return 1;
    case MUIA_Menuitem_Image:
        STORE = (IPTR)data->image;
        return 1;
    case MUIA_Menuitem_AISSName:
        STORE = (IPTR)data->aiss;
        return 1;

    case MUIA_Menuitem_Checked:
        STORE = ((data->flags & MENUF_CHECKED) != 0);
        return 1;

    case MUIA_Menuitem_Checkit:
        STORE = ((data->flags & MENUF_CHECKIT) != 0);
        return 1;

    case MUIA_Menuitem_CommandString:
        STORE = ((data->flags & MENUF_COMMANDSTRING) != 0);
        return 1;

    case MUIA_Menustrip_Enabled:
    case MUIA_Menu_Enabled:
    case MUIA_Menuitem_Enabled:
        STORE = ((data->flags & MENUF_ENABLED) != 0);
        return 1;

    case MUIA_Menuitem_Toggle:
        STORE = ((data->flags & MENUF_TOGGLE) != 0);
        return 1;

    case MUIA_Menuitem_Exclude:
        STORE = data->exclude;
        return 1;

    case MUIA_Menuitem_Shortcut:
        STORE = (IPTR) data->shortcut;
        return 1;

    case MUIA_Menu_Title:
    case MUIA_Menuitem_Title:
        STORE = (IPTR) data->title;
        return 1;

    case MUIA_Menuitem_NewMenu:
        Menuitem_BuildNewMenu(data, obj);
        STORE = (IPTR) data->newmenu;
        return 1;

    case MUIA_Menuitem_Trigger:
        STORE = (IPTR) data->trigger;
        return 1;

    case MUIA_Menuitem_Type:
        if (data->flags & MENUF_MENUSTRIP)
            STORE = (IPTR) MUIV_Menuitem_Type_Menustrip;
        else if(data->flags & MENUF_MENU)
            STORE = (IPTR) MUIV_Menuitem_Type_Menu;
        else
            STORE = (IPTR) MUIV_Menuitem_Type_Menuitem;
        return 1;

    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}
#undef STORE


/**************************************************************************
 OM_DISPOSE
**************************************************************************/
IPTR Menuitem__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_MenuitemData *data = INST_DATA(cl, obj);

    FreeVec(data->newmenu);
    Menuitem_SetString(data, &data->title, NULL);
    Menuitem_SetString(data, &data->shortcut, NULL);
    if ((data->flags & MENUF_FREEIMAGE) && data->image)
        DisposeObject((Object *)data->image);

    return DoSuperMethodA(cl, obj, msg);
}


/**************************************************************************
 MUIM_ConnectParent
**************************************************************************/
IPTR Menuitem__MUIM_ConnectParent(struct IClass *cl, Object *obj,
    struct MUIP_ConnectParent *msg)
{
    Object *cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    D(bug("Menuitem_ConnectParent(%p) %s\n", obj, OCLASS(obj)->cl_ID));

    DoSuperMethodA(cl, obj, (Msg) msg);

    muiNotifyData(obj)->mnd_ParentObject = msg->parent;

    get(obj, MUIA_Family_List, &ChildList);
    cstate = (Object *) ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        DoMethod(child, MUIM_ConnectParent, (IPTR) obj);
    }
    return TRUE;
}

/**************************************************************************
 MUIM_DisconnectParent
**************************************************************************/
IPTR Menuitem__MUIM_DisconnectParent(struct IClass *cl, Object *obj,
    struct MUIP_ConnectParent *msg)
{
    Object *cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    D(bug("Menuitem_DisconnectParent(%p) %s\n", obj, OCLASS(obj)->cl_ID));

    get(obj, MUIA_Family_List, &ChildList);
    cstate = (Object *) ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        DoMethodA(child, (Msg) msg);
    }
    muiNotifyData(obj)->mnd_ParentObject = NULL;
    DoSuperMethodA(cl, obj, (Msg) msg);
    return TRUE;
}

/**************************************************************************
 MUIM_Family_Insert, MUIM_Family_Remove
**************************************************************************/
IPTR Menuitem__MUIM_Update(struct IClass *cl, Object *obj, Msg msg)
{
    IPTR retval = DoSuperMethodA(cl, obj, msg);
    Menuitem_Changed(obj);
    return retval;
}

static IPTR Menustrip_Change(struct IClass *cl, Object *obj, BOOL begin)
{
    struct MUI_MenuitemData *data = INST_DATA(cl, obj);
    if (begin)
    {
        if (!muiNotifyData(obj)->mnd_GlobalInfo || !_app(obj)) return FALSE;
        if (!data->change_depth++)
            DoMethod(_app(obj), MUIM_Application_UpdateMenus);
        return TRUE;
    }
    if (data->change_depth && !--data->change_depth)
        Menuitem_Changed(obj);
    return 0;
}

struct PopupState
{
    struct ZMenu *menu;
    struct MenuItem selected;
    Object *choice;
    BOOL done;
};

/* A stack-local dispatcher target keeps modal input state out of the
 * application object's lifetime. It is removed before any notifications. */
BOOPSI_DISPATCHER(IPTR, PopupEvent, cl, obj, rawmsg)
{
    struct MUIP_HandleEvent *msg = (APTR)rawmsg;
    struct PopupState *state = (APTR)cl->cl_UserData;
    struct IntuiMessage *event = msg->imsg;
    if (!event) return 0;
    if (event->Class == IDCMP_MOUSEMOVE)
        zune_mouse_update(state->menu, 0);
    else if (event->Class == IDCMP_MOUSEBUTTONS &&
        (event->Code == SELECTUP || event->Code == MENUUP))
    {
        struct MenuItem *item = zune_leave_menu(state->menu);
        if (item)
        {
            state->selected = *item;
            state->choice = (Object *)GTMENUITEM_USERDATA(item);
        }
        state->done = TRUE;
    }
    else if (event->Class == IDCMP_RAWKEY && event->Code == 0x45)
        state->done = TRUE;
    return MUI_EventHandlerRC_Eat;
}
BOOPSI_DISPATCHER_END

static IPTR Menustrip_Popup(struct IClass *cl, Object *obj,
    struct MUIP_Menustrip_Popup *msg)
{
    struct PopupState state = {0};
    struct IClass handler = {0};
    struct MUI_EventHandlerNode ehn = {0};
    struct ZuneWindowGuard guard = {0};
    Object *parent = msg->parent;
    Object *win, *app;
    struct NewMenu *newmenu = NULL;
    ULONG signals = 0;
    IPTR result = 0;
    if (!parent || !(_flags(parent) & MADF_CANDRAW)) return 0;
    win = _win(parent);
    app = _app(parent);
    get(obj, MUIA_Menuitem_NewMenu, &newmenu);
    if (!newmenu) return 0;
    state.menu = zune_open_menu_at(_window(parent), newmenu,
        _window(parent)->LeftEdge + _left(parent) + msg->x,
        _window(parent)->TopEdge + _top(parent) + msg->y);
    if (!state.menu) return 0;
    handler.cl_Dispatcher.h_Entry = (HOOKFUNC)PopupEvent;
    handler.cl_UserData = (IPTR)&state;
    ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY;
    ehn.ehn_Priority = 127;
    ehn.ehn_Flags = MUI_EHF_GUIMODE;
    ehn.ehn_Object = parent;
    ehn.ehn_Class = &handler;
    DoMethod(win, MUIM_Window_AddEventHandler, &ehn);
    guard.handler = &ehn;
    ZuneWatchWindow(win, &guard);
    while (!state.done)
    {
        DoMethod(app, MUIM_Application_NewInput, &signals);
        if (guard.closed) break;
        if (!state.done)
        {
            struct MenuItem *selected = NULL;
            int picked = zune_poll_menu(state.menu, &selected);
            if (picked)
            {
                state.done = TRUE;
                if (selected)
                {
                    state.selected = *selected;
                    state.choice = (Object *)GTMENUITEM_USERDATA(selected);
                }
            }
            if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
            if (!state.done) Delay(1);
        }
    }
    if (!guard.closed)
    {
        ZuneUnwatchWindow(win, &guard);
        DoMethod(win, MUIM_Window_RemEventHandler, &ehn);
    }
    zune_close_menu(state.menu);
    if (state.choice && !guard.closed)
    {
        get(state.choice, MUIA_UserData, &result);
        if (state.selected.Flags & CHECKIT)
            set(state.choice, MUIA_Menuitem_Checked,
                !!(state.selected.Flags & CHECKED));
        set(state.choice, MUIA_Menuitem_Trigger, &state.selected);
    }
    return result;
}

IPTR Common_Dispatcher(Class *cl, Object *obj, Msg msg)
{
    switch (msg->MethodID)
    {
    case MUIM_Menustrip_Popup:
        return Menustrip_Popup(cl, obj, (APTR)msg);
    case MUIM_Menustrip_InitChange:
        return Menustrip_Change(cl, obj, TRUE);
    case MUIM_Menustrip_ExitChange:
        return Menustrip_Change(cl, obj, FALSE);
    case OM_DISPOSE:
        return Menuitem__OM_DISPOSE(cl, obj, msg);
    case OM_SET:
        return Menuitem__OM_SET(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return Menuitem__OM_GET(cl, obj, (struct opGet *)msg);
    case MUIM_ConnectParent:
        return Menuitem__MUIM_ConnectParent(cl, obj, (APTR) msg);
    case MUIM_DisconnectParent:
        return Menuitem__MUIM_DisconnectParent(cl, obj, (APTR) msg);
    case MUIM_Family_Insert:
        return Menuitem__MUIM_Update(cl, obj, (APTR) msg);
    case MUIM_Family_Remove:
        return Menuitem__MUIM_Update(cl, obj, (APTR) msg);
    case MUIM_Family_AddTail:
        return Menuitem__MUIM_Update(cl, obj, (APTR) msg);
    case MUIM_Family_AddHead:
        return Menuitem__MUIM_Update(cl, obj, (APTR) msg);
    case OM_ADDMEMBER:
        return Menuitem__MUIM_Update(cl, obj, (APTR) msg);
    case OM_REMMEMBER:
        return Menuitem__MUIM_Update(cl, obj, (APTR) msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

BOOPSI_DISPATCHER(IPTR, Menuitem_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Menuitem__OM_NEW(cl, obj, (struct opSet *)msg);
    }

    return Common_Dispatcher(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

BOOPSI_DISPATCHER(IPTR, Menu_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Menu__OM_NEW(cl, obj, (struct opSet *)msg);
    }

    return Common_Dispatcher(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

BOOPSI_DISPATCHER(IPTR, Menustrip_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Menustrip__OM_NEW(cl, obj, (struct opSet *)msg);
    }

    return Common_Dispatcher(cl, obj, msg);
}
BOOPSI_DISPATCHER_END
/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_Menuitem_desc =
{
    MUIC_Menuitem,
    MUIC_Family,
    sizeof(struct MUI_MenuitemData),
    (void *) Menuitem_Dispatcher
};

/*
 * Class descriptor.- this class is the same like menuitem
 */
const struct __MUIBuiltinClass _MUI_Menu_desc =
{
    MUIC_Menu,
    MUIC_Family,
    sizeof(struct MUI_MenuitemData),
    (void *) Menu_Dispatcher
};

/*
 * Class descriptor.- this class is the same like menuitem
 */
const struct __MUIBuiltinClass _MUI_Menustrip_desc =
{
    MUIC_Menustrip,
    MUIC_Family,
    sizeof(struct MUI_MenuitemData),
    (void *) Menustrip_Dispatcher
};

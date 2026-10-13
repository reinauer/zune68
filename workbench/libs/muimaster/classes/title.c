/*
    Copyright (C) 2012-2020, The AROS Development Team. All rights reserved.
*/

#define MUIMASTER_YES_INLINE_STDARG

#include <clib/alib_protos.h>
#include <stdio.h>
#include <proto/exec.h>
#include <devices/inputevent.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "support_classes.h"
#include "title_private.h"

/*  #define MYDEBUG 1 */
#include "debug.h"

extern struct Library *MUIMasterBase;

#define _isinobj(x,y,obj) (_between(_left(obj),(x),_right (obj)) \
                          && _between(_top(obj) ,(y),_bottom(obj)))

static struct List *TitleChildren(Object *obj)
{
    struct List *list = NULL;
    get(obj, MUIA_Group_ChildList, &list);
    return list;
}

static Object *TitleChild(Object *obj, LONG index)
{
    struct List *list = TitleChildren(obj);
    APTR state = list ? list->lh_Head : NULL;
    Object *child;
    while (state && (child = NextObject(&state)))
        if (!index--) return child;
    return NULL;
}

static LONG TitleIndex(Object *obj, Object *wanted)
{
    struct List *list = TitleChildren(obj);
    APTR state = list ? list->lh_Head : NULL;
    Object *child;
    LONG index = 0;
    while (state && (child = NextObject(&state)))
    {
        if (child == wanted) return index;
        index++;
    }
    return -1;
}

static LONG TitleCount(Object *obj)
{
    struct List *list = TitleChildren(obj);
    APTR state = list ? list->lh_Head : NULL;
    LONG count = 0;
    while (state && NextObject(&state)) count++;
    return count;
}

ULONG Tabs_Layout_Function(struct Hook *hook, Object *obj,
    struct MUI_LayoutMsg *lm)
{
    struct Title_DATA *data = hook->h_Data;
    APTR state = lm->lm_Children->mlh_Head;
    Object *child;
    LONG count = 0, total = 0, widest = 0, height = 0;
    LONG buttons = data->newable ? 18 : 0;
    while ((child = NextObject(&state)))
    {
        LONG width = _minwidth(child) + 8 + (data->closable ? 12 : 0);
        total += width;
        widest = MAX(widest, width);
        height = MAX(height, _minheight(child));
        count++;
    }
    if (lm->lm_Type == MUILM_MINMAX)
    {
        lm->lm_MinMax.MinWidth = MIN(total, widest + 36) + buttons;
        lm->lm_MinMax.DefWidth = total + buttons;
        lm->lm_MinMax.MaxWidth = MUI_MAXMAX;
        lm->lm_MinMax.MinHeight = lm->lm_MinMax.DefHeight =
            lm->lm_MinMax.MaxHeight = MAX(height + 8, 18);
        return 0;
    }
    if (lm->lm_Type == MUILM_LAYOUT)
    {
        LONG index = 0, left = 0, available = lm->lm_Layout.Width - buttons;
        data->overflow = total > available;
        if (data->overflow) available -= 36;
        else data->first = 0;
        data->first = MAX(0, MIN(data->first, count - 1));
        data->last = data->first - 1;
        state = lm->lm_Children->mlh_Head;
        while ((child = NextObject(&state)))
        {
            LONG width = _minwidth(child) + 8 + (data->closable ? 12 : 0);
            BOOL visible = index >= data->first && left + width <= available;
            if (visible)
            {
                _flags(child) |= MADF_SHOWME;
                if (!MUI_Layout(child, left + 4, 4, _minwidth(child),
                    MIN(_maxheight(child), lm->lm_Layout.Height - 8), 0)) return FALSE;
                if ((_flags(obj) & MADF_CANDRAW) && !(_flags(child) & MADF_CANDRAW))
                    DoShowMethod(child);
                left += width;
                data->last = index;
            }
            else
            {
                if (_flags(child) & MADF_CANDRAW) DoHideMethod(child);
                _flags(child) &= ~MADF_SHOWME;
            }
            index++;
        }
        return TRUE;
    }
    return TRUE;
}

IPTR Title__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Title_DATA *data = NULL;
    struct Hook *layout_hook;

    layout_hook = AllocVec(sizeof(struct Hook), MEMF_ANY | MEMF_CLEAR);
    if (!layout_hook)
        return (IPTR) NULL;

    layout_hook->h_Entry = HookEntry;
    layout_hook->h_SubEntry = (HOOKFUNC) Tabs_Layout_Function;

    obj = (Object *) DoSuperNewTags
        (cl, obj, NULL,
        MUIA_Group_Horiz, TRUE,
        MUIA_ContextMenu, 1,
        MUIA_Group_LayoutHook, (IPTR) layout_hook,
        MUIA_ShowSelState, FALSE, TAG_MORE, (IPTR) msg->ops_AttrList);
    if (!obj)
    {
        FreeVec(layout_hook);
        return FALSE;
    }

    data = INST_DATA(cl, obj);

    layout_hook->h_Data = data;
    data->layout_hook = layout_hook;
    data->activetab = 0;
    data->pressed = -1;
    data->clickable = GetTagData(MUIA_Title_Clickable, TRUE, msg->ops_AttrList);
    data->closable = GetTagData(MUIA_Title_Closable, FALSE, msg->ops_AttrList);
    data->newable = GetTagData(MUIA_Title_Newable, FALSE, msg->ops_AttrList);
    data->sortable = GetTagData(MUIA_Title_Sortable, FALSE, msg->ops_AttrList);
    data->position = GetTagData(MUIA_Title_Position, 0, msg->ops_AttrList);
    data->onlastclose = GetTagData(MUIA_Title_OnLastClose, 0, msg->ops_AttrList);

    /* Applications can place their handler before the tab handler. */
    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    data->ehn.ehn_Priority = GetTagData(MUIA_Title_EventHandlerPriority, 0, msg->ops_AttrList);
    data->ehn.ehn_Flags = MUI_EHF_GUIMODE;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;

    D(bug("muimaster.library/title.c: Title Object created at 0x%p\n",
            obj));

    return (IPTR) obj;
}

IPTR Title__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct Title_DATA *data = INST_DATA(cl, obj);
    if (data->page_menu) MUI_DisposeObject(data->page_menu);
    FreeVec(data->layout_hook);

    return DoSuperMethodA(cl, obj, msg);
}

IPTR Title__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Title_DATA *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *tag;
    BOOL relayout = FALSE;
    IPTR result;

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Title_Closable: data->closable = !!tag->ti_Data; relayout = TRUE; break;
        case MUIA_Title_Newable: data->newable = !!tag->ti_Data; relayout = TRUE; break;
        case MUIA_Title_Sortable: data->sortable = !!tag->ti_Data; break;
        case MUIA_Title_OnLastClose: data->onlastclose = tag->ti_Data; break;
        case MUIA_Title_Position: data->position = tag->ti_Data; relayout = TRUE; break;
        case MUIA_Group_ActivePage:
            {
                if (data->activetab != tag->ti_Data)
                {
                    data->activetab = tag->ti_Data;
                    if (data->activetab < data->first || data->activetab > data->last)
                    {
                        data->first = MAX(0, data->activetab);
                        if (_flags(obj) & MADF_CANDRAW) DoLayoutMethod(obj);
                    }
                    MUI_Redraw(obj, MADF_DRAWOBJECT);
                }
                break;
            }
        }
    }

    result = DoSuperMethodA(cl, obj, (Msg)msg);
    if (relayout && (_flags(obj) & MADF_SETUP) && _parent(obj))
    {
        DoMethod(_parent(obj), MUIM_Group_InitChange);
        DoMethod(_parent(obj), MUIM_Group_ExitChange);
    }
    return result;
}

static void TitleGlyph(Object *obj, LONG x, char glyph)
{
    struct RastPort *rp = _rp(obj);
    LONG y = _mtop(obj) + _mheight(obj) / 2;
    SetAPen(rp, _pens(obj)[MPEN_TEXT]);
    Move(rp, x + 4, y + _font(obj)->tf_Baseline / 2);
    Text(rp, &glyph, 1);
}

IPTR Title__MUIM_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
    struct Title_DATA *data = INST_DATA(cl, obj);
    Object *child;
    LONG index = 0;
    struct List *list = TitleChildren(obj);
    APTR state = list->lh_Head;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) return 0;
    while ((child = NextObject(&state)))
    {
        if (index >= data->first && index <= data->last)
        {
            LONG left = _left(child) - 4;
            LONG right = _right(child) + 4 + (data->closable ? 12 : 0);
            SetAPen(_rp(obj), _pens(obj)[index == data->activetab ? MPEN_SHINE : MPEN_SHADOW]);
            Move(_rp(obj), left, _bottom(obj)); Draw(_rp(obj), left, _top(obj));
            Draw(_rp(obj), right, _top(obj)); Draw(_rp(obj), right, _bottom(obj));
            if (data->closable) TitleGlyph(obj, _right(child) + 3, 'x');
        }
        index++;
    }
    LONG right = _mright(obj) - (data->newable ? 17 : -1);
    if (data->newable) TitleGlyph(obj, _mright(obj) - 17, '+');
    if (data->overflow)
    {
        TitleGlyph(obj, right - 36, '<');
        TitleGlyph(obj, right - 18, '>');
    }
    return TRUE;
}

IPTR Title__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct Title_DATA *data = INST_DATA(cl, obj);

    if (!DoSuperMethodA(cl, obj, (Msg) msg))
        return FALSE;

    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) & data->ehn);

    return TRUE;
}

IPTR Title__MUIM_Cleanup(struct IClass *cl, Object *obj,
    struct MUIP_Cleanup *msg)
{
    struct Title_DATA *data = INST_DATA(cl, obj);

    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) & data->ehn);

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

static IPTR TitleFindPage(Object *obj, Object *tab)
{
    LONG index = TitleIndex(obj, tab);
    return index < 0 || !_parent(obj) ? 0 : (IPTR)TitleChild(_parent(obj), index + 1);
}

static IPTR TitleClose(Object *obj, Object *tab)
{
    Object *parent = _parent(obj);
    Object *page = (Object *)TitleFindPage(obj, tab);
    LONG index = TitleIndex(obj, tab), active;
    if (!parent || !page || index < 0) return FALSE;
    active = XGET(parent, MUIA_Group_ActivePage);
    DoMethod(parent, MUIM_Group_InitChange);
    DoMethod(obj, MUIM_Group_InitChange);
    DoMethod(obj, OM_REMMEMBER, tab);
    DoMethod(parent, OM_REMMEMBER, page);
    MUI_DisposeObject(tab); MUI_DisposeObject(page);
    if (index < active) active--;
    set(parent, MUIA_Group_ActivePage, MAX(0, MIN(active, TitleCount(obj) - 1)));
    DoMethod(obj, MUIM_Group_ExitChange);
    DoMethod(parent, MUIM_Group_ExitChange);
    return TRUE;
}

static void TitleMove(Object *obj, LONG from, LONG to)
{
    LONG count = TitleCount(obj), i;
    Object *parent = _parent(obj), *active;
    IPTR *tabs, *pages;
    if (!parent || from == to || from < 0 || to < 0 || from >= count || to >= count) return;
    tabs = AllocVec((2 * count + 5) * sizeof(IPTR), MEMF_CLEAR);
    if (!tabs) return;
    pages = tabs + count + 2;
    tabs[0] = pages[0] = MUIM_Group_Sort;
    pages[1] = (IPTR)obj;
    active = TitleChild(parent, XGET(parent, MUIA_Group_ActivePage) + 1);
    for (i = 0; i < count; i++)
    {
        LONG original = i;
        if (i == to) original = from;
        else if (from < to && i >= from && i < to) original++;
        else if (from > to && i > to && i <= from) original--;
        tabs[i + 1] = (IPTR)TitleChild(obj, original);
        pages[i + 2] = (IPTR)TitleChild(parent, original + 1);
    }
    DoMethod(parent, MUIM_Group_InitChange); DoMethod(obj, MUIM_Group_InitChange);
    DoMethodA(obj, (Msg)tabs); DoMethodA(parent, (Msg)pages);
    for (i = 0; i < count; i++) if ((Object *)pages[i + 2] == active)
        set(parent, MUIA_Group_ActivePage, i);
    DoMethod(obj, MUIM_Group_ExitChange); DoMethod(parent, MUIM_Group_ExitChange);
    FreeVec(tabs);
}

IPTR Title__MUIM_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct Title_DATA *data = INST_DATA(cl, obj);
    struct IntuiMessage *imsg = msg->imsg;
    LONG index;
    if (!data->clickable || !(_flags(obj) & MADF_CANDRAW)) return 0;
    switch (msg->muikey)
    {
    case MUIKEY_PRESS:
    case MUIKEY_TOGGLE:
    case MUIKEY_RIGHT:
        set(_parent(obj), MUIA_Group_ActivePage, MUIV_Group_ActivePage_Next);
        return MUI_EventHandlerRC_Eat;
    case MUIKEY_LEFT:
        set(_parent(obj), MUIA_Group_ActivePage, MUIV_Group_ActivePage_Prev);
        return MUI_EventHandlerRC_Eat;
    case MUIKEY_WORDLEFT:
    case MUIKEY_LINESTART:
        set(_parent(obj), MUIA_Group_ActivePage, MUIV_Group_ActivePage_First);
        return MUI_EventHandlerRC_Eat;
    case MUIKEY_WORDRIGHT:
    case MUIKEY_LINEEND:
        set(_parent(obj), MUIA_Group_ActivePage, MUIV_Group_ActivePage_Last);
        return MUI_EventHandlerRC_Eat;
    }
    if (!imsg) return 0;
    if (imsg->Class != IDCMP_MOUSEBUTTONS) return 0;
    LONG pressed = data->pressed;
    if (imsg->Code == SELECTUP || imsg->Code == SELECTDOWN)
        data->pressed = -1;
    if (!_isinobj(imsg->MouseX, imsg->MouseY, obj)) return 0;
    if (imsg->Code == SELECTDOWN)
    {
        LONG right = _mright(obj) + 1;
        if (data->newable && imsg->MouseX >= right - 18)
        {
            DoMethod(_app(obj), MUIM_Application_PushMethod, obj, 1, MUIM_Title_New);
            return MUI_EventHandlerRC_Eat;
        }
        right -= data->newable ? 18 : 0;
        if (data->overflow && imsg->MouseX >= right - 36)
        {
            data->first += imsg->MouseX < right - 18 ? -1 : 1;
            DoLayoutMethod(obj); MUI_Redraw(obj, MADF_DRAWOBJECT);
            return MUI_EventHandlerRC_Eat;
        }
    }
    for (index = data->first; index <= data->last; index++)
    {
        Object *child = TitleChild(obj, index);
        if (!child) break;
        if (imsg->MouseX < _left(child) - 4 ||
            imsg->MouseX > _right(child) + 4 + (data->closable ? 12 : 0)) continue;
        if (imsg->Code == SELECTUP && data->sortable)
        {
            LONG from = pressed;
            TitleMove(obj, from, index);
            return MUI_EventHandlerRC_Eat;
        }
        if (imsg->Code != SELECTDOWN) return 0;
        if (data->closable && imsg->MouseX > _right(child) + 2)
        {
            if (TitleCount(obj) == 1 && data->onlastclose == MUIV_Title_OnLastClose_WindowAction)
                set(_win(obj), MUIA_Window_CloseRequest, TRUE);
            else DoMethod(_app(obj), MUIM_Application_PushMethod, obj, 2, MUIM_Title_Close, child);
        }
        else
        {
            data->pressed = index;
            set(_parent(obj), MUIA_Group_ActivePage, index);
        }
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

/* Rebuild the overflow chooser from the current tab/page pairs. Menu labels
 * are copied because arbitrary tab objects need a temporary fallback label. */
static IPTR TitleContextBuild(struct IClass *cl, Object *obj,
    struct MUIP_ContextMenuBuild *msg)
{
    struct Title_DATA *data = INST_DATA(cl, obj);
    Object *menu, *tab;
    LONG count = TitleCount(obj), i;
    LONG right = _mright(obj) - (data->newable ? 18 : 0);
    if (XGET(obj, MUIA_ContextMenu) != 1)
        return DoSuperMethodA(cl, obj, (Msg)msg);
    if (!data->clickable || !data->overflow ||
        msg->mx < right - 35 || msg->mx > right ||
        msg->my < _mtop(obj) || msg->my > _mbottom(obj)) return 0;
    if (data->page_menu) MUI_DisposeObject(data->page_menu);
    data->page_menu = NULL;
    menu = MUI_NewObject(MUIC_Menu, MUIA_Menu_Title, "Pages", TAG_DONE);
    if (!menu) return 0;
    data->page_menu = MUI_NewObject(MUIC_Menustrip,
        MUIA_Family_Child, menu, TAG_DONE);
    if (!data->page_menu) return 0;
    for (i = 0; i < count; i++)
    {
        char fallback[24];
        STRPTR label = NULL;
        tab = TitleChild(obj, i);
        get(tab, MUIA_Text_Contents, &label);
        snprintf(fallback, sizeof(fallback), "Page %ld", i + 1);
        Object *item = MUI_NewObject(MUIC_Menuitem,
            MUIA_Menuitem_Title, label ? label : fallback,
            MUIA_Menuitem_CopyStrings, TRUE, MUIA_UserData, (IPTR)tab,
            MUIA_Menuitem_Checkit, TRUE,
            MUIA_Menuitem_Checked, i == data->activetab, TAG_DONE);
        if (item) DoMethod(menu, OM_ADDMEMBER, item);
    }
    return (IPTR)data->page_menu;
}

BOOPSI_DISPATCHER(IPTR, Title_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case MUIM_ContextMenuBuild:
        return TitleContextBuild(cl, obj, (APTR)msg);
    case MUIM_ContextMenuChoice:
    {
        struct MUIP_ContextMenuChoice *choice = (APTR)msg;
        if (XGET(obj, MUIA_ContextMenu) != 1)
            return DoSuperMethodA(cl, obj, msg);
        LONG index = TitleIndex(obj, (Object *)XGET(choice->item, MUIA_UserData));
        if (index >= 0) set(_parent(obj), MUIA_Group_ActivePage, index);
        return 0;
    }
    case OM_NEW:
        return Title__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_GET:
    {
        struct Title_DATA *data = INST_DATA(cl, obj);
        struct opGet *getmsg = (APTR)msg;
        switch (getmsg->opg_AttrID)
        {
        case MUIA_Title_Position: *getmsg->opg_Storage = data->position; return TRUE;
        case MUIA_Title_Closable: *getmsg->opg_Storage = data->closable; return TRUE;
        case MUIA_Title_Newable: *getmsg->opg_Storage = data->newable; return TRUE;
        case MUIA_Title_Sortable: *getmsg->opg_Storage = data->sortable; return TRUE;
        }
        return DoSuperMethodA(cl, obj, msg);
    }
    case MUIM_Title_FindPage:
        return TitleFindPage(obj, ((struct MUIP_Title_FindPage *)msg)->titlebutton);
    case MUIM_Title_Close:
        return TitleClose(obj, ((struct MUIP_Title_Close *)msg)->tito);
    case MUIM_Title_New:
        return 0;
    case OM_SET:
        return Title__OM_SET(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return Title__OM_DISPOSE(cl, obj, msg);
    case MUIM_Draw:
        return Title__MUIM_Draw(cl, obj, (struct MUIP_Draw *)msg);
    case MUIM_Setup:
        return Title__MUIM_Setup(cl, obj, (struct MUIP_Setup *)msg);
    case MUIM_Cleanup:
        return Title__MUIM_Cleanup(cl, obj, (struct MUIP_Cleanup *)msg);
    case MUIM_HandleEvent:
        return Title__MUIM_HandleEvent(cl, obj,
            (struct MUIP_HandleEvent *)msg);
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Title_desc =
{
    MUIC_Title,
    MUIC_Group,
    sizeof(struct Title_DATA),
    (void *) Title_Dispatcher
};

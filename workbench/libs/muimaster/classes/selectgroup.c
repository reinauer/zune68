/* Radio-style selection over application-supplied objects. */
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"

extern struct Library *MUIMasterBase;
struct SelectgroupData
{
    LONG active, count;
    struct MUI_EventHandlerNode handler;
};
struct SelectionWalk
{
    LONG index, active, hit, x, y;
    BOOL update;
};

static void walk_options(Object *obj, struct SelectionWalk *walk)
{
    struct List *list = NULL;
    ULONG userdata = 0;
    GetAttr(MUIA_UserData, obj, &userdata);
    if (userdata == MUIA_Selectgroup_Active)
    {
        walk->index++;
        if (walk->update)
            DoMethod(obj, MUIM_NoNotifySet, MUIA_Selected,
                walk->index == walk->active);
    }
    GetAttr(MUIA_Group_ChildList, obj, (IPTR *)&list);
    if (list)
    {
        APTR state = list->lh_Head;
        Object *child;
        while ((child = NextObject(&state))) walk_options(child, walk);
    }
    else if (!walk->update && walk->index >= 0 &&
        (_flags(obj) & MADF_CANDRAW) && !muiAreaData(obj)->mad_DisableCount &&
        walk->x >= _left(obj) && walk->x <= _right(obj) &&
        walk->y >= _top(obj) && walk->y <= _bottom(obj))
        walk->hit = walk->index;
}
static LONG scan(Object *obj, LONG active, BOOL update, LONG x, LONG y,
    LONG *count)
{
    struct SelectionWalk walk = {-1, active, -1, x, y, update};
    /* The root's user data belongs to its caller, not an option. */
    struct List *list = NULL;
    GetAttr(MUIA_Group_ChildList, obj, (IPTR *)&list);
    if (list)
    {
        APTR state = list->lh_Head;
        Object *child;
        while ((child = NextObject(&state))) walk_options(child, &walk);
    }
    *count = walk.index + 1;
    return walk.hit;
}
BOOPSI_DISPATCHER(IPTR, Selectgroup_Dispatcher, cl, obj, msg)
{
    struct SelectgroupData *d;
    if (msg->MethodID == OM_NEW)
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList;
        obj = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!obj) return 0;
        d = INST_DATA(cl, obj);
        d->active = GetTagData(MUIA_Selectgroup_Active, 0, tags);
        scan(obj, d->active, TRUE, 0, 0, &d->count);
        if (d->active < 0 || d->active >= d->count)
        {
            d->active = 0;
            scan(obj, 0, TRUE, 0, 0, &d->count);
        }
        d->handler.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
        d->handler.ehn_Priority = 1;
        d->handler.ehn_Flags = MUI_EHF_GUIMODE;
        d->handler.ehn_Object = obj;
        d->handler.ehn_Class = cl;
        return (IPTR)obj;
    }
    d = INST_DATA(cl, obj);
    switch (msg->MethodID)
    {
    case OM_GET:
        if (((struct opGet *)msg)->opg_AttrID == MUIA_Selectgroup_Active)
        {
            *((struct opGet *)msg)->opg_Storage = d->active;
            return TRUE;
        }
        break;
    case OM_SET:
    {
        struct opSet *setmsg = (APTR)msg;
        if (FindTagItem(MUIA_Selectgroup_Active, setmsg->ops_AttrList))
        {
            struct opSet copy = *setmsg;
            struct TagItem *tag, *state;
            IPTR result;
            copy.ops_AttrList = CloneTagItems(setmsg->ops_AttrList);
            if (!copy.ops_AttrList) return FALSE;
            state = copy.ops_AttrList;
            while ((tag = NextTagItem(&state)))
                if (tag->ti_Tag == MUIA_Selectgroup_Active)
                {
                    LONG active = tag->ti_Data;
                    scan(obj, d->active, TRUE, 0, 0, &d->count);
                    if (active == MUIV_Selectgroup_Active_Next)
                        active = d->active + 1 == d->count ? 0 : d->active + 1;
                    else if (active == MUIV_Selectgroup_Active_Prev)
                        active = d->active ? d->active - 1 : d->count - 1;
                    if (active < 0 || active >= d->count || active == d->active)
                        tag->ti_Tag = TAG_IGNORE;
                    else
                    {
                        d->active = active;
                        tag->ti_Data = active;
                        scan(obj, active, TRUE, 0, 0, &d->count);
                    }
                }
            result = DoSuperMethodA(cl, obj, (Msg)&copy);
            FreeTagItems(copy.ops_AttrList);
            return result;
        }
        break;
    }
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg)) return FALSE;
        scan(obj, d->active, TRUE, 0, 0, &d->count);
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, &d->handler);
        return TRUE;
    case MUIM_Cleanup:
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &d->handler);
        break;
    case MUIM_HandleEvent:
    {
        struct MUIP_HandleEvent *event = (APTR)msg;
        if (!(_flags(obj) & MADF_CANDRAW) || muiAreaData(obj)->mad_DisableCount)
            return 0;
        Object *active = NULL;
        get(_win(obj), MUIA_Window_ActiveObject, &active);
        if (active == obj &&
            (event->muikey == MUIKEY_RIGHT || event->muikey == MUIKEY_DOWN ||
             event->muikey == MUIKEY_LEFT || event->muikey == MUIKEY_UP))
        {
            set(obj, MUIA_Selectgroup_Active,
                event->muikey == MUIKEY_RIGHT || event->muikey == MUIKEY_DOWN ?
                MUIV_Selectgroup_Active_Next : MUIV_Selectgroup_Active_Prev);
            return MUI_EventHandlerRC_Eat;
        }
        if (event->imsg && event->imsg->Class == IDCMP_MOUSEBUTTONS &&
            event->imsg->Code == SELECTDOWN)
        {
            LONG hit = scan(obj, d->active, FALSE, event->imsg->MouseX,
                event->imsg->MouseY, &d->count);
            if (hit >= 0)
            {
                set(obj, MUIA_Selectgroup_Active, hit);
                return MUI_EventHandlerRC_Eat;
            }
        }
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END
const struct __MUIBuiltinClass _MUI_Selectgroup_desc = {
    MUIC_Selectgroup, MUIC_Group, sizeof(struct SelectgroupData),
    (void *)Selectgroup_Dispatcher
};

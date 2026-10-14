/*
    Copyright (C) 2002-2023, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.

*/

#include <exec/types.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

extern struct Library *MUIMasterBase;

#include "muimaster_intern.h"
#include <string.h>
#include "mui.h"
#include "support.h"
#include "prefs.h"

/*  #define MYDEBUG 1 */
#include "debug.h"

#define ROUND(x) ((int)(x + 0.5))
#define IS_HIDDEN(obj) (! (_flags(obj) & MADF_SHOWME) \
 || (_flags(obj) & (MADF_BORDERGADGET | MADF_DISAPPEARED)))

/* Attributes filtered out in OM_SET, before OM_SET gets passed to children.
   Tested with MUI under UAE/AOS.

    notifyclass:
    
    MUIA_HelpLine
    MUIA_HelpNode
    MUIA_ObjectID
    MUIA_UserData
   
    areaclass:
     
    MUIA_ContextMenu
    MUIA_ContextMenuTrigger
    MUIA_ControlChar
    MUIA_CycleChain
    MUIA_Draggable
    MUIA_FillArea
    MUIA_Frame
    MUIA_FrameTitle
    MUIA_HorizWeight
    MUIA_Pressed
    MUIA_Selected
    MUIA_ShortHelp
    MUIA_ShowMe
    MUIA_VertWeight
    MUIA_Weight
    
*/

/* Private attribute/method definitions */
#define MUIM_Group_Insert      (MUIB_MUI|0x00424d34)    /* MUI: V20 */
struct MUIP_Group_Insert
{
    STACKED ULONG MethodID;
    STACKED Object *obj;
    STACKED Object *pred;
};



struct layout2d_elem
{
    WORD min;
    WORD max;
    WORD dim;
    ULONG weight;
};


struct MUI_GroupData
{
    Object *family;
    struct Hook *layout_hook;
    ULONG flags;
    ULONG columns;
    ULONG rows;
    struct layout2d_elem *row_infos;
    struct layout2d_elem *col_infos;
    LONG active_page;
    ULONG horiz_spacing;
    ULONG vert_spacing;
    ULONG num_children;
    ULONG num_visible_children; /* for horiz/vert group only */
    ULONG horiz_weight_sum;
    ULONG vert_weight_sum;
    ULONG samesize_maxmin_horiz;
    ULONG samesize_maxmin_vert;
    ULONG update;               /* for MUI_Redraw() 1 - do not redraw the frame
                                 * 2 - the virtual pos has changed */
    struct MUI_EventHandlerNode ehn;
    LONG virt_offx, virt_offy;  /* diplay offsets */
    LONG old_virt_offx, old_virt_offy;  /* Saved virtual positions,
                                         * used for update == 2 */
    LONG virt_mwidth, virt_mheight;     /* The complete width */
    LONG told_mwidth, told_mheight;     /* Size last notified */
    LONG saved_minwidth, saved_minheight;
    LONG dont_forward_get;      /* Set temporarily to 1 so that the get method
                                 * is not forwarded */
    LONG dont_forward_methods;  /* Set temporarily to 1, meaning that the
                                 * methods are not forwarded to the group's
                                 * children */
    /* MUI4 group with tabs */
    Object *titlegroup;
    APTR paint_clip;            /* previous clipping during virtual painting */
};

/* Note:
 * The MUI4 feature of group with tabs is implemented based on behaviour of
 * one application. What this application codes suggest it seems that passing
 * MUIV_Frame_Register together with MUIA_Group_PageMode, TRUE activates this
 * mode.
 * In such mode, the first passed group is used to register tab "titles" and
 * is always visible. The selection of object in this group selects the
 * matching (by position) group to be displayed
 */

#define GROUP_HORIZ       (1<<1)
#define GROUP_SAME_WIDTH  (1<<2)
#define GROUP_SAME_HEIGHT (1<<3)
#define GROUP_CHANGING    (1<<4)
#define GROUP_PAGEMODE    (1<<5)
#define GROUP_VIRTUAL     (1<<6)
#define GROUP_HSPACING    (1<<7)
#define GROUP_VSPACING    (1<<8)
#define GROUP_CHANGED     (1<<9)
#define GROUP_PAINTING    (1<<10)


/* During minmax calculations objects with a weight of 0 shall
   be treated like they had identical min/def/max size, ie. fixed size.
   
   During layout objects with 0 weight must be treated like fixed-sized
   too, but for hgroups only in x direction, and for vgroups only in
   y direction. I think ... */

#define w0_defwidth(x) (_hweight(x) ? _defwidth(x) : _minwidth(x))
#define w0_maxwidth(x) (_hweight(x) ? _maxwidth(x) : _minwidth(x))

#define w0_defheight(x) (_vweight(x) ? _defheight(x) : _minheight(x))
#define w0_maxheight(x) (_vweight(x) ? _maxheight(x) : _minheight(x))

#include "native_version.h"

IPTR Group__MUIM_Show(struct IClass *cl, Object *obj,
    struct MUIP_Show *msg);
IPTR Group__MUIM_Hide(struct IClass *cl, Object *obj,
    struct MUIP_Hide *msg);

/*****************************************************************************/
/*****************************************************************************/


static ULONG Group_DispatchMsg(struct IClass *cl, Object *obj, Msg msg);

static void change_active_page(struct IClass *cl, Object *obj, LONG page)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    LONG newpage, num_children = data->num_children;

    if (!(data->flags & GROUP_PAGEMODE))
        return;

    if (data->titlegroup != NULL)
        num_children--;

    if (num_children <= 0)
    {
        data->active_page = 0;
        return;
    }

    switch (page)
    {
    case MUIV_Group_ActivePage_First:
        newpage = 0;
        break;
    case MUIV_Group_ActivePage_Last:
        newpage = num_children - 1;
        break;
    case MUIV_Group_ActivePage_Prev:
        newpage = data->active_page - 1;
        if (newpage == -1)
            newpage = num_children - 1;
        break;
    case MUIV_Group_ActivePage_Next:
    case MUIV_Group_ActivePage_Advance:
        newpage = (data->active_page + 1) % num_children;
        break;
    default:
        newpage = page;
        break;
    }

    if (newpage != data->active_page)
    {
        if (_flags(obj) & MADF_CANDRAW)
            Group__MUIM_Hide(cl, obj, NULL);

        data->active_page = newpage;

        if (_flags(obj) & MADF_CANDRAW)
        {
            DoLayoutMethod(obj);
            Group__MUIM_Show(cl, obj, NULL);
            data->update = 1;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
        }

        if (data->titlegroup)
            set(data->titlegroup, MUIA_Group_ActivePage, newpage);
    }
}

/**************************************************************************
 Returns the number of visible children. Visible children are all children
 that have MADF_SHOWME and not MADF_BORDERGADGET set.
**************************************************************************/
static int Group_GetNumVisibleChildren(struct MUI_GroupData *data,
    struct MinList *children)
{
    int num_visible_children = data->num_children;
    APTR cstate;
    Object *child;

    /* As there can be invisible children we have to subtract those from
     * the total number of children */
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (IS_HIDDEN(child))
            num_visible_children--;
    }
    return num_visible_children;
}

/**************************************************************************
 Handles insertion of objects - works based on fact that IDs of all methods
 that use it are the same for Group and Family and that struct share obj at
 same offset.
**************************************************************************/
struct MUIP_StructWithObj
{
    STACKED ULONG MethodID;
    STACKED Object *obj;
};

static IPTR Group__MUIM_AddObject(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    struct MUIP_StructWithObj *msgint = (struct MUIP_StructWithObj *)msg;
    struct TagItem nestedDisableAttrs[4] = {
        { MUIA_NoNotify,        TRUE    },
        { MUIA_NestedDisabled,  TRUE    },
        { MUIA_Disabled,        FALSE   },
        { TAG_DONE                      }
    };

    DoMethodA(data->family, (Msg) msg);
    data->num_children++;
    if (data->num_children == 1 &&
        (muiAreaData(msgint->obj)->mad_Flags2 & MADF2_ISTITLE))
    {
        data->titlegroup = msgint->obj;
        data->flags |= GROUP_PAGEMODE;
    }

    if ((data->flags & GROUP_CHANGING) != 0)
        data->flags |= GROUP_CHANGED;

    /* if we are in an application tree, propagate pointers */
    if (muiNotifyData(obj)->mnd_GlobalInfo)
    {
        /* Only children of groups can have parents */

        if ((_flags(obj) & MADF_INVIRTUALGROUP)
            || (data->flags & GROUP_VIRTUAL))
        {
            _flags(msgint->obj) |= MADF_INVIRTUALGROUP;
        }

        DoMethod(msgint->obj, MUIM_ConnectParent, (IPTR) obj);
    }

    /* Ensure new children are disabled if their parent is */
    GET(obj, MUIA_Disabled, &nestedDisableAttrs[2].ti_Data);
    if (nestedDisableAttrs[2].ti_Data == (IPTR)TRUE)
    {
        SetAttrsA(msgint->obj, nestedDisableAttrs);
    }

    /* Some apps (Odyssey) expect _parent() will work before group tree is
     * added to application tree */
    muiNotifyData(msgint->obj)->mnd_ParentObject = obj;

    if (_flags(obj) & MADF_SETUP)
    {
        DoSetupMethod(msgint->obj, muiRenderInfo(obj));
    }
/*      if (_flags(obj) & MADF_CANDRAW) */
/*          DoShowMethod(msg->opam_Object); */

    return TRUE;
}

/**************************************************************************
 OM_NEW - Constructor
**************************************************************************/
IPTR Group__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_GroupData *data;
    struct TagItem *tags, *tag;
    BOOL bad_children = FALSE;
    IPTR frame = MUIV_Frame_None;

    D(bug("[group.mui] OM_NEW, object 0x%p\n", obj));

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
        return 0;

    muiAreaData(obj)->mad_Flags2 |= MADF2_ISGROUP;

    /* Initial local instance data */
    data = INST_DATA(cl, obj);

    data->family = MUI_NewObjectA(MUIC_Family, NULL);
    if (!data->family)
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    data->horiz_spacing = -1;
    data->vert_spacing = -1;
    data->columns = 1;
    data->rows = 1;
    data->active_page = 0;
    get(obj, MUIA_Frame, &frame);

    /* parse initial taglist */
    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Group_Child:
            D(bug("[group.mui] Adding child 0x%p\n", tag->ti_Data));
            if (tag->ti_Data)
            {
                DoMethod(obj, OM_ADDMEMBER, tag->ti_Data);
                /* Set first child as group title */
                if ((frame == MUIV_Frame_Register)
                    && (data->titlegroup == NULL))
                    data->titlegroup = (Object *) tag->ti_Data;
            }
            else
                bad_children = TRUE;
            break;

        case MUIA_Group_ActivePage:
            change_active_page(cl, obj, (LONG) tag->ti_Data);
            break;

        case MUIA_Group_Columns:
            data->columns = (tag->ti_Data) > 1 ? tag->ti_Data : 1;
            data->rows = 0;
            break;

        case MUIA_Group_Horiz:
            _handle_bool_tag(data->flags, tag->ti_Data, GROUP_HORIZ);
            break;

        case MUIA_Group_HorizSpacing:
            data->flags |= GROUP_HSPACING;
            data->horiz_spacing = tag->ti_Data;
            break;

        case MUIA_Group_LayoutHook:
            data->layout_hook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_Group_PageMode:
            _handle_bool_tag(data->flags, tag->ti_Data, GROUP_PAGEMODE);
            break;

        case MUIA_Group_Rows:
            data->rows = MAX((ULONG) tag->ti_Data, 1);
            data->columns = 0;
            break;

        case MUIA_Group_SameHeight:
            _handle_bool_tag(data->flags, tag->ti_Data, GROUP_SAME_HEIGHT);
            break;

        case MUIA_Group_SameSize:
            _handle_bool_tag(data->flags, tag->ti_Data, GROUP_SAME_HEIGHT);
            _handle_bool_tag(data->flags, tag->ti_Data, GROUP_SAME_WIDTH);
            break;

        case MUIA_Group_SameWidth:
            _handle_bool_tag(data->flags, tag->ti_Data, GROUP_SAME_WIDTH);
            break;

        case MUIA_Group_Spacing:
            data->flags |= (GROUP_HSPACING | GROUP_VSPACING);
            data->horiz_spacing = tag->ti_Data;
            data->vert_spacing = tag->ti_Data;
            break;

        case MUIA_Group_VertSpacing:
            data->flags |= GROUP_VSPACING;
            data->vert_spacing = tag->ti_Data;
            break;

        case MUIA_Group_Virtual:
            _handle_bool_tag(data->flags, tag->ti_Data, GROUP_VIRTUAL);
            break;
        }
    }

    if (bad_children)
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

/*      D(bug("Group_New(0x%p)\n",obj)); */

    if (data->flags & GROUP_VIRTUAL)
    {
        /* This is used by MUI_Render() to determine if group is virtual.
         * It then installs a clip region.
         * Also MUI_Layout() uses this. Probably for speed up reason  */
        _flags(obj) |= MADF_ISVIRTUALGROUP;
    }

#if (0)
    // kalamatee : disabled as unnecessary.
    // Object is created before children are added, so MUIA_Disabled is set
    // and OM_ADDMEMBER (called on the children) already does this.

    /* will forward MUIA_Disabled to children */
    get(obj, MUIA_Disabled, &disabled);
    if (disabled)
    {
        set(obj, MUIA_Disabled, TRUE);
    }
#endif

    /* This is only used for virtual groups */
    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;  /* Will be filled on demand */
    data->ehn.ehn_Priority = 10;        /* Will hear the click before all
                                         * other normal objects */
    data->ehn.ehn_Flags = MUI_EHF_GUIMODE;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;
    return (IPTR) obj;
}

/**************************************************************************
 OM_DISPOSE
**************************************************************************/
IPTR Group__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (data->row_infos != NULL)
        mui_free(data->row_infos);
    if (data->col_infos != NULL)
        mui_free(data->col_infos);
    if (data->family != NULL)
        MUI_DisposeObject(data->family);
    return DoSuperMethodA(cl, obj, msg);
}

/* Scrolling changes screen coordinates without re-running client layouts.
 * Walk actual Group subclasses so forwarded/custom ChildList attributes
 * cannot make a non-group look like a container. Hidden children move too. */
static void Group_ShiftChildren(struct IClass *cl, Object *obj,
    LONG dx, LONG dy)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    struct MinList *children = NULL;
    APTR state;
    Object *child;

    get(data->family, MUIA_Family_List, &children);
    if (!children)
        return;
    state = children->mlh_Head;
    while ((child = NextObject(&state)))
    {
        struct IClass *childclass = OCLASS(child);

        _left(child) -= dx;
        _top(child) -= dy;
        muiAreaData(child)->mad_VirtualTop -= dy;
        while (childclass && childclass != cl)
            childclass = childclass->cl_Super;
        if (childclass)
            Group_ShiftChildren(cl, child, dx, dy);
    }
}

/**************************************************************************
 OM_SET
**************************************************************************/
IPTR Group__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *disableTag = NULL;
    struct TagItem *tag;
    BOOL forward = TRUE;
    BOOL need_recalc = FALSE;
    IPTR retval;

    int virt_offx = data->virt_offx, virt_offy = data->virt_offy;

    /* There are many ways to find out what tag items provided by set()
     ** we do know. The best way should be using NextTagItem() and simply
     ** browsing through the list.
     */

    /* Parse group attributes before calling DoSuperMethodA(),
     ** otherwise if an app for example sets up a notification
     ** on MUIA_Group_ActivePage which calls a hook, and then
     ** the hook function calls get(obj, MUIA_Group_ActivePage),
     ** it would get returned the old active page instead of the new
     ** active page
     */

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Disabled:
            disableTag = tag;
            tag->ti_Tag = TAG_IGNORE;
            break;
        case MUIA_Group_Columns:
            data->columns = MAX((ULONG) tag->ti_Data, 1);
            data->rows = 0;
            need_recalc = TRUE;
            break;
        case MUIA_Group_ActivePage:
            change_active_page(cl, obj, (LONG) tag->ti_Data);
            break;
        case MUIA_Group_Forward:
            forward = tag->ti_Data;
            break;
        case MUIA_Group_HorizSpacing:
            data->flags |= GROUP_HSPACING;
            data->horiz_spacing = tag->ti_Data;
            break;
        case MUIA_Group_Rows:
            data->rows = MAX((ULONG) tag->ti_Data, 1);
            data->columns = 0;
            need_recalc = TRUE;
            break;
        case MUIA_Group_Spacing:
            data->flags |= (GROUP_HSPACING | GROUP_VSPACING);
            data->horiz_spacing = tag->ti_Data;
            data->vert_spacing = tag->ti_Data;
            break;
        case MUIA_Group_VertSpacing:
            data->flags |= GROUP_VSPACING;
            data->vert_spacing = tag->ti_Data;
            break;
        case MUIA_Virtgroup_Left:
            //kprintf("set virtgroup_left: %d\n", tag->ti_Data);
            virt_offx = tag->ti_Data;
            break;

        case MUIA_Group_LayoutHook:
            /*
               [ach] Seems like MUI supports setting this attribute after
               initialization, even though the documentation states
               otherwise. At least some programs use it...
             */
            data->layout_hook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_Virtgroup_Top:
            //kprintf("set virtgroup_top: %d\n", tag->ti_Data);
            virt_offy = tag->ti_Data;
            break;
        }

    }

    if (muiRenderInfo(obj) && need_recalc)
        DoMethod(_win(obj), MUIM_Window_RecalcDisplay, (IPTR) obj);

    retval = DoSuperMethodA(cl, obj, (Msg) msg);
    if (disableTag)
        disableTag->ti_Tag = MUIA_Disabled;

    /* seems to be the documented behaviour, however it should be slow! */

    if (forward)
    {
        struct TagItem *attrsOrig = msg->ops_AttrList;
        struct TagItem nestedAttrs[2] = {
            { MUIA_NestedDisabled,  FALSE},
            { TAG_MORE,             (IPTR)msg->ops_AttrList}
        };

        /* Attributes which are to be filtered out, so that they are ignored
           when OM_SET is passed to group's children */

        tags = msg->ops_AttrList;
        while ((tag = NextTagItem(&tags)) != NULL)
        {
            switch (tag->ti_Tag)
            {
            case MUIA_Disabled:
                nestedAttrs[0].ti_Data = tag->ti_Data;
                msg->ops_AttrList = nestedAttrs;
                break;
            case MUIA_HelpLine:
            case MUIA_HelpNode:
            case MUIA_ObjectID:
            case MUIA_UserData:

            case MUIA_ContextMenu:
            case MUIA_ContextMenuTrigger:
            case MUIA_ControlChar:
            case MUIA_CustomBackfill:
            case MUIA_CycleChain:
            case MUIA_Draggable:
            case MUIA_FillArea:
            case MUIA_Group_ActivePage:
            case MUIA_Group_LayoutHook:
            case MUIA_Frame:
            case MUIA_FrameTitle:
            case MUIA_HorizWeight:
            case MUIA_Pressed:
            case MUIA_ShortHelp:
            case MUIA_ShowMe:
            case MUIA_VertWeight:
            case MUIA_Weight:
            case MUIA_Virtgroup_Left:
            case MUIA_Virtgroup_Top:
            case MUIA_AppMessage:
            case MUIA_Timer:
                tag->ti_Tag = TAG_IGNORE;
                break;
            case MUIA_Selected:
/*                D(bug("Group_Set(%p) MUIA_Selected forwarded\n", obj)); */
/*                          tag->ti_Tag = TAG_IGNORE; */
                break;
            }
        }

        Group_DispatchMsg(cl, obj, (Msg) msg);
        msg->ops_AttrList = attrsOrig;
    }

    if (disableTag)
    {
        struct TagItem disableAttrs[2] = {
            { MUIA_Disabled,  disableTag->ti_Data },
            { TAG_DONE, 		0}
        };
        struct TagItem *origTags = msg->ops_AttrList;
        msg->ops_AttrList = disableAttrs;
        DoSuperMethodA(cl, obj, (Msg) msg);
        msg->ops_AttrList = origTags;
    }

    if (data->flags & GROUP_VIRTUAL)
    {
        virt_offx = CLAMP(virt_offx, 0,
            MAX(0, data->virt_mwidth - _mwidth(obj)));
        virt_offy = CLAMP(virt_offy, 0,
            MAX(0, data->virt_mheight - _mheight(obj)));
    }
    if (virt_offx != data->virt_offx || virt_offy != data->virt_offy)
    {
        BOOL shown = (_flags(obj) & MADF_CANDRAW) != 0;
        LONG dx = virt_offx - data->virt_offx;
        LONG dy = virt_offy - data->virt_offy;

        data->virt_offx = virt_offx;
        data->virt_offy = virt_offy;
        Group_ShiftChildren(cl, obj, dx, dy);
        if (shown)
        {
            Group__MUIM_Hide(cl, obj, NULL);
            Group__MUIM_Show(cl, obj, NULL);
            data->update = 2;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
    }

    return retval;
}


/**************************************************************************
 OM_GET
**************************************************************************/
IPTR Group__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
/* small macro to simplify return value storage */
#define STORE *(msg->opg_Storage)

    struct MUI_GroupData *data = INST_DATA(cl, obj);

    switch (msg->opg_AttrID)
    {
    case MUIA_Version:
    case MUIA_Revision:
        /* Notify reports the version of the object's class library. */
        return DoSuperMethodA(cl, obj, (Msg)msg);
    case MUIA_Group_ActivePage:
        STORE = data->active_page;
        return 1;
    case MUIA_Group_ChildList:
        return GetAttr(MUIA_Family_List, data->family, msg->opg_Storage);
    case MUIA_Group_Horiz:
        STORE = (data->flags & GROUP_HORIZ);
        return 1;
    case MUIA_Group_HorizSpacing:
        STORE = data->horiz_spacing;
        return 1;
    case MUIA_Group_VertSpacing:
        STORE = data->vert_spacing;
        return 1;
    case MUIA_Group_ChildCount:
        STORE = data->num_children;
        return 1;
    case MUIA_Virtgroup_Left:
        STORE = data->virt_offx;
        return 1;
    case MUIA_Virtgroup_Top:
        STORE = data->virt_offy;
        return 1;
    case MUIA_Virtgroup_Width:
        STORE = data->virt_mwidth;
        return 1;
    case MUIA_Virtgroup_Height:
        STORE = data->virt_mheight;
        return 1;
    case MUIA_Virtgroup_MinWidth:
        STORE = data->saved_minwidth;
        return 1;
    case MUIA_Virtgroup_MinHeight:
        STORE = data->saved_minheight;
        return 1;
    }

    /* our handler didn't understand the attribute, we simply pass
     ** it to our superclass now
     */
    if (DoSuperMethodA(cl, obj, (Msg) msg))
        return 1;

    /* seems to be the documented behaviour, however it should be slow! */
    if (!data->dont_forward_get && !data->dont_forward_methods)
    {
        APTR cstate;
        Object *child;
        struct MinList *ChildList = NULL;

        get(data->family, MUIA_Family_List, &(ChildList));
        cstate = ChildList->mlh_Head;
        while ((child = NextObject(&cstate)))
            if (DoMethodA(child, (Msg) msg))
                return 1;
    }
    return 0;
#undef STORE
}


/**************************************************************************
 MUIM_AddTail
**************************************************************************/
IPTR Group__MUIM_AddTail(struct IClass *cl, Object *obj,
    struct MUIP_Group_AddTail *msg)
{
    return Group__MUIM_AddObject(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_AddHead
**************************************************************************/
IPTR Group__MUIM_AddHead(struct IClass *cl, Object *obj,
    struct MUIP_Group_AddHead *msg)
{
    return Group__MUIM_AddObject(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_Insert
**************************************************************************/
IPTR Group__MUIM_Insert(struct IClass *cl, Object *obj,
    struct MUIP_Group_Insert *msg)
{
    return Group__MUIM_AddObject(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_Remove
**************************************************************************/
IPTR Group__MUIM_Remove(struct IClass *cl, Object *obj,
    struct MUIP_Group_Remove *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (msg->obj == data->titlegroup) data->titlegroup = NULL;
    if (_flags(obj) & MADF_CANDRAW)
        DoHideMethod(msg->obj);
    if (_flags(obj) & MADF_SETUP)
        DoMethod(msg->obj, MUIM_Cleanup);
    if (muiNotifyData(obj)->mnd_GlobalInfo)
    {
        DoMethod(msg->obj, MUIM_DisconnectParent);
        muiNotifyData(msg->obj)->mnd_ParentObject = NULL;

        _flags(msg->obj) &= ~MADF_INVIRTUALGROUP;
    }

    if ((data->flags & GROUP_CHANGING) != 0)
        data->flags |= GROUP_CHANGED;
    data->num_children--;
    DoMethodA(data->family, (Msg) msg);

    return TRUE;
}

/**************************************************************************
 MUIM_Family_GetChild
**************************************************************************/
IPTR Group__MUIM_Family_GetChild(struct IClass *cl, Object *obj,
    struct MUIP_Group_Remove *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    return DoMethodA(data->family, (APTR) msg);
}

/**************************************************************************
 MUIM_ConnectParent
**************************************************************************/
IPTR Group__MUIM_ConnectParent(struct IClass *cl, Object *obj,
    struct MUIP_ConnectParent *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    DoSuperMethodA(cl, obj, (Msg) msg);

    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if ((_flags(obj) & MADF_INVIRTUALGROUP)
            || (data->flags & GROUP_VIRTUAL))
        {
            _flags(child) |= MADF_INVIRTUALGROUP;
        }

        /* Only children of groups can have parents */
        muiNotifyData(child)->mnd_ParentObject = obj;

        DoMethod(child, MUIM_ConnectParent, (IPTR) obj);
    }
    return TRUE;
}

/**************************************************************************
 MUIM_GlobalSetup (MUI private): the children become ours and get the
 global info too, as they do through MUIM_ConnectParent
**************************************************************************/
static IPTR Group__MUIM_GlobalSetup(struct IClass *cl, Object *obj,
    struct MUIP_GlobalSetup *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    DoSuperMethodA(cl, obj, (Msg) msg);

    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if ((_flags(obj) & MADF_INVIRTUALGROUP)
            || (data->flags & GROUP_VIRTUAL))
            _flags(child) |= MADF_INVIRTUALGROUP;
        muiNotifyData(child)->mnd_ParentObject = obj;
        DoMethodA(child, (Msg) msg);
    }
    return TRUE;
}

/**************************************************************************
 MUIM_DisconnectParent
**************************************************************************/
IPTR Group__MUIM_DisconnectParent(struct IClass *cl, Object *obj,
    struct MUIP_ConnectParent *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        DoMethodA(child, (Msg) msg);
        muiNotifyData(child)->mnd_ParentObject = NULL;
        _flags(child) &= ~MADF_INVIRTUALGROUP;
    }
    DoSuperMethodA(cl, obj, (Msg) msg);
    return TRUE;
}

/*
 * Put group in exchange state
 */
IPTR Group__MUIM_InitChange(struct IClass *cl, Object *obj,
    struct MUIP_Group_InitChange *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    data->flags &= ~GROUP_CHANGED;
    data->flags |= GROUP_CHANGING;
    return TRUE;
}


/*
 * Will recalculate display after dynamic adding/removing
 */
IPTR Group__MUIM_ExitChange(struct IClass *cl, Object *obj,
    struct MUIP_Group_ExitChange *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    data->flags &= ~GROUP_CHANGING;

#if 0
    /* Code is invalid. ExitChange needs to re-layout each time (tested
     * with MUI 3.8 and MUI 4.0 on m68k).
     * This is temporary change until proper implementaion is in place. */
    if (data->flags & GROUP_CHANGED)
#endif
    {
        data->flags &= ~GROUP_CHANGED;

        if ((_flags(obj) & MADF_SETUP) && _win(obj))
        {
            Object *win = _win(obj);
            Object *parent = obj;

            /* CHECKME: Don't call RecalcDisplay if one of our parents is
               in GROUP_CHANGING state to prevent  crash with Zune prefs
               program NListtree page because NList/NListtree when
               killing tree images in MUIM_Cleanup uses InitChange/
               ExitChange. Zune prefs program uses InitChange/ExitChange
               when switching page -> nesting -> mess. */

            while ((parent = _parent(parent)))
            {
                struct MUI_GroupData *pdata = INST_DATA(cl, parent);

                if (parent == win)
                    break;

                if (pdata->flags & GROUP_CHANGING)
                {
                    return TRUE;
                }

            }

            DoMethod(win, MUIM_Window_RecalcDisplay, (IPTR) obj);
        }
    }

    return TRUE;
}


/*
 * Will recalculate display after dynamic adding/removing
 */
IPTR Group__MUIM_ExitChange2(struct IClass *cl, Object *obj,
    struct MUIP_Group_ExitChange2 *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (data->flags & GROUP_CHANGING)
    {
        data->flags &= ~(GROUP_CHANGING | GROUP_CHANGED);

        if ((_flags(obj) & MADF_SETUP) && _win(obj))
        {
            Object *win = _win(obj);
            Object *parent = obj;

            /* CHECKME: Don't call RecalcDisplay if one of our parents is
               in GROUP_CHANGING state to prevent  crash with Zune prefs
               program NListtree page because NList/NListtree when
               killing tree images in MUIM_Cleanup uses InitChange/
               ExitChange. Zune prefs program uses InitChange/ExitChange
               when switching page -> nesting -> mess. */

            while ((parent = _parent(parent)))
            {
                struct MUI_GroupData *pdata = INST_DATA(cl, parent);

                if (parent == win)
                    break;

                if (pdata->flags & GROUP_CHANGING)
                {
                    return TRUE;
                }

            }

            DoMethod(win, MUIM_Window_RecalcDisplay, (IPTR) obj);
        }
    }

    return TRUE;
}


/*
 * Sort the family
 */
static IPTR GroupMoveMember(struct IClass *cl, Object *obj,
    struct MUIP_Group_MoveMember *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    struct List *list;
    Object *state, *child, *after = NULL;
    LONG remaining = 0, pos;
    BOOL found = FALSE;
    get(data->family, MUIA_Family_List, &list);
    state = (Object *)list->lh_Head;
    while ((child = NextObject(&state)))
        if (child == msg->o) found = TRUE;
        else remaining++;
    if (!found) return FALSE;
    pos = msg->pos;
    if (pos < 0) pos = remaining + pos + 1;
    if (pos < 0) pos = 0;
    if (pos > remaining) pos = remaining;
    state = (Object *)list->lh_Head;
    while (pos && (child = NextObject(&state)))
        if (child != msg->o) { after = child; pos--; }
    return DoMethod(data->family, MUIM_Family_Reorder, after, msg->o, NULL);
}
static IPTR GroupReorder(struct IClass *cl, Object *obj,
    struct MUIP_Group_Reorder *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    IPTR result;
    msg->MethodID = MUIM_Family_Reorder;
    result = DoMethodA(data->family, (Msg)msg);
    msg->MethodID = MUIM_Group_Reorder;
    return result;
}

IPTR Group__MUIM_Sort(struct IClass *cl, Object *obj,
    struct MUIP_Group_Sort *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    /* modify message */
    msg->MethodID = MUIM_Family_Sort;

    DoMethodA(data->family, (APTR) msg);

    /* restore original message */
    msg->MethodID = MUIM_Group_Sort;
    return TRUE;
}

/**************************************************************************
 MUIM_Group_DoMethodNoForward

 Executes the given method but does not forward it to the children
**************************************************************************/
IPTR Group__MUIM_DoMethodNoForward(struct IClass *cl, Object *obj,
    struct MUIP_Group_DoMethodNoForward *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    IPTR rc;
    data->dont_forward_methods = 1;     /* disable forwarding */
    rc = DoMethodA(obj, (Msg) & msg->DoMethodID);
        /* Probably doesn't work correctly on AROS? */

    data->dont_forward_methods = 0;
    return rc;
}

/*
 * Propagate a method to group children.
 */
static ULONG Group_DispatchMsg(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    if (data->dont_forward_methods)
        return TRUE;

    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        DoMethodA(child, (Msg) msg);
    }
    return TRUE;
}


/**************************************************************************
 MUIM_Setup
**************************************************************************/
IPTR Group__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    Object *childFailed;
    struct MinList *ChildList = NULL;

    if (!DoSuperMethodA(cl, obj, (Msg) msg))
        return FALSE;

    ASSERT_VALID_PTR(muiGlobalInfo(obj));

    if (!(data->flags & GROUP_HSPACING))
        data->horiz_spacing = muiGlobalInfo(obj)->mgi_Prefs->group_hspacing;
    if (!(data->flags & GROUP_VSPACING))
        data->vert_spacing = muiGlobalInfo(obj)->mgi_Prefs->group_vspacing;
    /* MUI 3.9 halves the spacing from the third stage of fitting a window
     * into the screen (tests/native/fontfallback.c) */
    if (muiRenderInfo(obj)->mri_Fallback >= 3)
    {
        if (!(data->flags & GROUP_HSPACING))
            data->horiz_spacing = MIN(data->horiz_spacing, 2);
        if (!(data->flags & GROUP_VSPACING))
            data->vert_spacing = MIN(data->vert_spacing, 2);
    }
    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
#if 0                           /* SHOWME affects only show/hide */
        if (!(_flags(child) & MADF_SHOWME))
            continue;
#endif

        if (!DoSetupMethod(child, msg->RenderInfo))
        {
            /* Send MUIM_Cleanup to all objects that received MUIM_Setup.
             */
            childFailed = child;
            cstate = ChildList->mlh_Head;
            while ((child = NextObject(&cstate)) && (child != childFailed))
            {
#if 0                           /* SHOWME affects only show/hide */
                if (!(_flags(child) & MADF_SHOWME))
                    continue;
#endif
                DoMethod(child, MUIM_Cleanup);
            }
            return FALSE;
        }
    }

    if (data->flags & GROUP_VIRTUAL)
    {
        DoMethod(_win(obj), MUIM_Window_AddEventHandler,
            (IPTR) & data->ehn);
    }

    return TRUE;
}


/**************************************************************************
 MUIM_Cleanup
**************************************************************************/
IPTR Group__MUIM_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    if (data->flags & GROUP_VIRTUAL)
    {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler,
            (IPTR) & data->ehn);
    }

    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
#if 0                           /* SHOWME affects only show/hide */
        if (!(_flags(child) & MADF_SHOWME))
            continue;
#endif
        DoMethodA(child, (Msg) msg);
    }
    return DoSuperMethodA(cl, obj, (Msg) msg);
}

static struct Region *group_children_clip_region(struct IClass *cl,
    Object *obj)
{
    struct Region *region = NULL;

    region = NewRegion();
    if (region)
    {
        struct MUI_GroupData *data = INST_DATA(cl, obj);
        struct Rectangle rect;
        LONG page = -1;
        struct MinList *ChildList = NULL;
        APTR cstate;
        Object *child;

        rect.MinX = _left(obj);
        rect.MinY = _top(obj);
        rect.MaxX = _right(obj);
        rect.MaxY = _bottom(obj);

        OrRectRegion(region, &rect);
        get(data->family, MUIA_Family_List, &ChildList);
        cstate = ChildList->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            if (child != data->titlegroup)
                ++page;

            if ((data->flags & GROUP_PAGEMODE) && (page != data->active_page)
                && (child != data->titlegroup))
                continue;

            if ((muiAreaData(child)->mad_Flags & MADF_CANDRAW)
                && (_width(child) > 0) && (_height(child) > 0))
            {
                rect.MinX = MAX(_left(child), _mleft(obj));
                rect.MinY = MAX(_top(child), _mtop(obj));
                rect.MaxX = MIN(_right(child), _mright(obj));
                rect.MaxY = MIN(_bottom(child), _mbottom(obj));

                if ((rect.MaxX >= rect.MinX) && (rect.MaxY >= rect.MinY))
                {
                    ClearRectRegion(region, &rect);
                }
            }
        }
    }

    return region;
}

#define GROUP_GAP_CHILDREN 32

/* Collect the visible children of a group clipped to its inner box.
 * Returns the count, or -1 when there are too many for the gap painter. */
static int group_collect_children(struct IClass *cl, Object *obj,
    struct Rectangle *kids)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    struct MinList *ChildList = NULL;
    APTR cstate;
    Object *child;
    LONG page = -1;
    int n = 0;

    get(data->family, MUIA_Family_List, &ChildList);
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        struct Rectangle rect;

        if (child != data->titlegroup)
            ++page;
        if ((data->flags & GROUP_PAGEMODE) && (page != data->active_page)
            && (child != data->titlegroup))
            continue;
        if (!(muiAreaData(child)->mad_Flags & MADF_CANDRAW)
            || _width(child) <= 0 || _height(child) <= 0)
            continue;

        rect.MinX = MAX(_left(child), _mleft(obj));
        rect.MinY = MAX(_top(child), _mtop(obj));
        rect.MaxX = MIN(_right(child), _mright(obj));
        rect.MaxY = MIN(_bottom(child), _mbottom(obj));
        if (rect.MaxX < rect.MinX || rect.MaxY < rect.MinY)
            continue;
        if (n == GROUP_GAP_CHILDREN)
            return -1;
        kids[n++] = rect;
    }
    return n;
}

/* Paint the group background only where no child covers the inner box,
 * using a band sweep instead of a clip region. Region clipping allocated
 * temporary regions and rebuilt the layer clip for every group drawn. */
static void group_fill_gaps(Object *obj, const struct Rectangle *kids,
    int n)
{
    WORD ys[2 * GROUP_GAP_CHILDREN + 2], xs[GROUP_GAP_CHILDREN][2];
    WORD left = _mleft(obj), right = _mright(obj);
    int ny = 0, i, j, k;

    ys[ny++] = _mtop(obj);
    ys[ny++] = _mbottom(obj) + 1;
    for (i = 0; i < n; i++)
    {
        ys[ny++] = kids[i].MinY;
        ys[ny++] = kids[i].MaxY + 1;
    }
    for (i = 1; i < ny; i++)        /* insertion sort, small n */
        for (j = i; j > 0 && ys[j - 1] > ys[j]; j--)
        {
            WORD t = ys[j]; ys[j] = ys[j - 1]; ys[j - 1] = t;
        }

    for (i = 0; i + 1 < ny; i++)
    {
        WORD y0 = ys[i], y1 = ys[i + 1] - 1, x = left;
        int nx = 0;

        if (y0 > y1 || y0 < _mtop(obj) || y1 > _mbottom(obj))
            continue;
        /* Bands split at every child edge, so a child covers a band fully
         * or not at all */
        for (j = 0; j < n; j++)
            if (kids[j].MinY <= y0 && kids[j].MaxY >= y1)
            {
                for (k = nx++; k > 0 && xs[k - 1][0] > kids[j].MinX; k--)
                {
                    xs[k][0] = xs[k - 1][0];
                    xs[k][1] = xs[k - 1][1];
                }
                xs[k][0] = kids[j].MinX;
                xs[k][1] = kids[j].MaxX;
            }
        for (j = 0; j <= nx; j++)
        {
            WORD gap_end = (j < nx) ? xs[j][0] - 1 : right;

            if (gap_end >= x)
                DoMethod(obj, MUIM_DrawBackground, x, y0, gap_end - x + 1,
                    y1 - y0 + 1, x, y0, _flags(obj));
            if (j < nx && xs[j][1] + 1 > x)
                x = xs[j][1] + 1;
        }
    }
}

/* Private MUI virtual-group painting protocol. These method IDs are used
 * by existing subclasses to prepare and release their content clipping.
 * MUI 3.9 calls neither method for an ordinary Group or Area.
 */
#define MUIM_Virtgroup_BeginPaint 0x80425b5cUL
#define MUIM_Virtgroup_EndPaint   0x8042da52UL

/* Establish the outer virtual clip before a subclass installs its own.
 * MUI clipping handles represent the previous region, which can be NULL.
 * Existing subclasses rely on this outer clip when tracking their handle.
 */
IPTR Group__MUIM_Virtgroup_BeginPaint(struct IClass *cl, Object *obj)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    struct Region *region;
    struct Rectangle rect;

    if (data->flags & GROUP_PAINTING)
        return FALSE;
    region = NewRegion();
    if (!region)
        return FALSE;
    rect.MinX = _mleft(obj);
    rect.MinY = _mtop(obj);
    rect.MaxX = _mright(obj);
    rect.MaxY = _mbottom(obj);
    if (data->update == 2 && _mwidth(obj) > 0 && _mheight(obj) > 0)
    {
        LONG dx = data->virt_offx - data->old_virt_offx;
        LONG dy = data->virt_offy - data->old_virt_offy;
        struct Rectangle exposed = rect;
        BOOL okay = TRUE;

        if (dx)
        {
            if (dx > 0)
                exposed.MinX = MAX(rect.MinX, rect.MaxX - dx + 1);
            else
                exposed.MaxX = MIN(rect.MaxX, rect.MinX - dx - 1);
            okay = OrRectRegion(region, &exposed);
        }
        if (okay && dy)
        {
            exposed = rect;
            if (dy > 0)
                exposed.MinY = MAX(rect.MinY, rect.MaxY - dy + 1);
            else
                exposed.MaxY = MIN(rect.MaxY, rect.MinY - dy - 1);
            okay = OrRectRegion(region, &exposed);
        }
        if (!okay)
        {
            DisposeRegion(region);
            return FALSE;
        }
    }
    else if (_mwidth(obj) > 0 && _mheight(obj) > 0 &&
        !OrRectRegion(region, &rect))
    {
        DisposeRegion(region);
        return FALSE;
    }
    data->paint_clip = MUI_AddClipRegion(muiRenderInfo(obj), region);
    if (data->paint_clip == (APTR)-1)
        return FALSE;
    data->flags |= GROUP_PAINTING;
    return TRUE;
}

IPTR Group__MUIM_Virtgroup_EndPaint(struct IClass *cl, Object *obj)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (data->flags & GROUP_PAINTING)
    {
        MUI_RemoveClipping(muiRenderInfo(obj), data->paint_clip);
        data->paint_clip = (APTR)-1;
        data->flags &= ~GROUP_PAINTING;
    }
    return 0;
}

/**************************************************************************
 MUIM_Draw - draw the group
**************************************************************************/
/* Paint the background of an object's box outside its inner box */
static void group_clear_ring(Object *obj)
{
    LONG l = _left(obj), t = _top(obj), r = _right(obj), b = _bottom(obj);
    LONG ml = _mleft(obj), mt = _mtop(obj), mr = _mright(obj),
        mb = _mbottom(obj);

    if (mt > t)
        DoMethod(obj, MUIM_DrawBackground, l, t, r - l + 1, mt - t, l, t, 0);
    if (mb < b)
        DoMethod(obj, MUIM_DrawBackground, l, mb + 1, r - l + 1, b - mb,
            l, mb + 1, 0);
    if (ml > l && mb >= mt)
        DoMethod(obj, MUIM_DrawBackground, l, mt, ml - l, mb - mt + 1,
            l, mt, 0);
    if (mr < r && mb >= mt)
        DoMethod(obj, MUIM_DrawBackground, mr + 1, mt, r - mr, mb - mt + 1,
            mr + 1, mt, 0);
}

IPTR Group__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;
    struct Rectangle group_rect;        /* child_rect; */
    int page;
    BOOL virtual = (data->flags & GROUP_VIRTUAL) != 0;
    ULONG fill = _flags(obj) & MADF_FILLAREA;

    if (data->flags & GROUP_CHANGING)
        return FALSE;

    /* Move existing pixels before BeginPaint lets a subclass restrict the
     * drawing region. Layout hooks must retain their previous scroll state
     * until that callback computes the newly exposed content. */
    if (virtual && data->update == 2)
    {
        LONG dx = data->virt_offx - data->old_virt_offx;
        LONG dy = data->virt_offy - data->old_virt_offy;
        struct Rectangle *bounds = &muiRenderInfo(obj)->mri_ClipRect;
        LONG left = MAX(_mleft(obj), bounds->MinX);
        LONG top = MAX(_mtop(obj), bounds->MinY);
        LONG right = MIN(_mright(obj), bounds->MaxX);
        LONG bottom = MIN(_mbottom(obj), bounds->MaxY);

        if (!dx && !dy)
            return TRUE;
        if (right < left || bottom < top)
            return TRUE;

        ScrollWindowRaster(_window(obj), dx, dy, left, top, right, bottom);
    }

    /* Area draws the frame; virtual contents belong inside the paint pair. */
    if (virtual)
    {
        _flags(obj) &= ~MADF_FILLAREA;
        /* The spacing between the frame and the contents is not part of
         * the paint pair, so clear it before Area puts the frame on top;
         * a page switch otherwise left the previous page's pixels there
         * (xSysInfo's Hardware page) */
        /* Area moves the draw flags into the message only below */
        if ((_flags(obj) & MADF_DRAWOBJECT)
            && !(_flags(obj) & MADF_DRAWUPDATE))
            group_clear_ring(obj);
    }

    if (muiGlobalInfo(obj)->mgi_Prefs->window_redraw
        == WINDOW_REDRAW_WITHOUT_CLEAR)
    {
        struct Rectangle kids[GROUP_GAP_CHILDREN];
        int nkids = -1;

        /* Custom backfill hooks keep one call for the whole inner box */
        if (!virtual && (_flags(obj) & MADF_FILLAREA)
            && !(muiAreaData(obj)->mad_Flags2 & MADF2_CUSTOMBACKFILL))
            nkids = group_collect_children(cl, obj, kids);

        if (nkids >= 0)
        {
            /* Area paints the frame and its surround; the group fills
             * the uncovered parts of its inner box itself */
            _flags(obj) &= ~MADF_FILLAREA;
            DoSuperMethodA(cl, obj, (Msg) msg);
            _flags(obj) |= MADF_FILLAREA;
            if (msg->flags & MADF_DRAWOBJECT)
                group_fill_gaps(obj, kids, nkids);
        }
        else
        {
            /* Without FILLAREA, Area paints only outside the inner box,
             * where the clip excludes nothing. In refresh mode each clip
             * costs two refresh cycles, so skip it. */
            struct Region *r = (_flags(obj) & MADF_FILLAREA)
                ? group_children_clip_region(cl, obj) : NULL;
            APTR c = (APTR)-1;
            if (r)
                c = MUI_AddClipRegion(muiRenderInfo(obj), r);

            DoSuperMethodA(cl, obj, (Msg) msg);

            if (r)
                MUI_RemoveClipRegion(muiRenderInfo(obj), c);
        }
    }
    else
    {
        DoSuperMethodA(cl, obj, (Msg) msg);
    }

    if (virtual)
    {
        _flags(obj) |= fill;
        if (!DoMethod(obj, MUIM_Virtgroup_BeginPaint))
        {
            /* The raster has already moved, even if the client declines
             * painting. Never apply that same scroll delta twice. */
            data->old_virt_offx = data->virt_offx;
            data->old_virt_offy = data->virt_offy;
            data->update = 0;
            return FALSE;
        }
    }

    /* Custom backfills receive the full logical viewport. The exposed
     * region limits actual painting, including on partial scrolls. */
    if (virtual && (msg->flags & MADF_DRAWOBJECT) &&
        !(msg->flags & MADF_DRAWUPDATE))
        DoMethod(obj, MUIM_DrawBackground,
            _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj),
            _mleft(obj), _mtop(obj), 0);

    if ((msg->flags & MADF_DRAWUPDATE) && data->update == 1)
    {
        struct Region *r = NULL;
        APTR c = (APTR)-1;

        if (muiGlobalInfo(obj)->mgi_Prefs->window_redraw
            == WINDOW_REDRAW_WITHOUT_CLEAR)
            r = group_children_clip_region(cl, obj);

        if (r)
            c = MUI_AddClipRegion(muiRenderInfo(obj), r);

        /*
         * update is set when changing active page of a page group
         * need to redraw background ourself
         */
        DoMethod(obj, MUIM_DrawBackground,
            _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj),
            _mleft(obj), _mtop(obj), 0);

        if (r)
            MUI_RemoveClipRegion(muiRenderInfo(obj), c);
    }
    else if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWALL)))
        goto finish;

    group_rect = muiRenderInfo(obj)->mri_ClipRect;
    page = -1;
    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (!(_flags(child) & MADF_SHOWME))
            continue;

        if (child != data->titlegroup)
            ++page;

        if (_flags(child) & MADF_DISAPPEARED)
            continue;

        if ((data->flags & GROUP_PAGEMODE) && ((page != data->active_page)
            && (child != data->titlegroup)))
        {
            continue;
        }

        if ((data->flags & GROUP_PAGEMODE) && (child == data->titlegroup)
            && (msg->flags & MADF_DRAWUPDATE) && (data->update == 1))
        {
            /* Do not issue a re-draw to title group during page switch.
             * The group will re-draw itself due to setting of
             * MUIA_Group_ActivePage attribute.
             */
            continue;
        }

        MUI_Redraw(child, MADF_DRAWOBJECT | MADF_DRAWFRAME);
        muiRenderInfo(obj)->mri_ClipRect = group_rect;
    }

finish:
    if (virtual)
        DoMethod(obj, MUIM_Virtgroup_EndPaint);

    data->old_virt_offx = data->virt_offx;
    data->old_virt_offy = data->virt_offy;
    data->update = 0;

    return TRUE;
}


#define END_MINMAX() \
    tmp.MaxHeight = MAX(tmp.MaxHeight, tmp.MinHeight); \
    tmp.MaxWidth = MAX(tmp.MaxWidth, tmp.MinWidth); \
    tmp.DefHeight = CLAMP(tmp.DefHeight, tmp.MinHeight, tmp.MaxHeight); \
    tmp.DefWidth = CLAMP(tmp.DefWidth, tmp.MinWidth, tmp.MaxWidth); \
    msg->MinMaxInfo->MinWidth += tmp.MinWidth; \
    msg->MinMaxInfo->MinHeight += tmp.MinHeight; \
    msg->MinMaxInfo->MaxWidth += tmp.MaxWidth; \
    msg->MinMaxInfo->MaxHeight += tmp.MaxHeight; \
    msg->MinMaxInfo->DefWidth += tmp.DefWidth; \
    msg->MinMaxInfo->DefHeight += tmp.DefHeight;

/*
 * MinMax calculation function. When this is called,
 * the children of your group have already been asked
 * about their min/max dimension so you can use their
 * dimensions to calculate yours.
 *
 * Notes:
 * - Init minwidth and maxwidth with size needed for total child spacing.
 * - 1st pass to find maximum minimum width, to set minwidth of each child
 * if they should have the same width (for a row of buttons ...)
 * - Adjust minwidth w/o making object bigger than their max size.
 */
static void group_minmax_horiz(struct IClass *cl, Object *obj,
    struct MinList *children, struct MUIP_AskMinMax *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MUI_MinMax tmp;
    WORD maxminwidth = 0;
    BOOL found_nonzero_vweight = FALSE;

    tmp.MinHeight = 0;
    tmp.DefHeight = 0;
    tmp.MaxHeight = MUI_MAXMAX;
    if (data->num_visible_children > 0)
    {
        tmp.MinWidth = tmp.DefWidth =
            (data->num_visible_children - 1) * data->horiz_spacing;
        /* MUI 3.9 adds spacing and each child's maximum, itself at most
         * MUI_MAXMAX, until the sum reaches MUI_MAXMAX, without clamping
         * the sum (tests/native/demolayout.c, widgetsizes.c) */
        tmp.MaxWidth = -data->horiz_spacing;
    }
    else
    {
        tmp.MinWidth = tmp.DefWidth = 0;
        tmp.MaxWidth = MUI_MAXMAX;
    }

    if (data->flags & GROUP_SAME_WIDTH)
    {
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            if (IS_HIDDEN(child))
                continue;
            maxminwidth = MAX(maxminwidth, _minwidth(child));
        }
    }

    data->samesize_maxmin_horiz = maxminwidth;
/*    D(bug("group_minmax_horiz(%p) : maxminwidth=%d\n", obj, maxminwidth)); */

    data->horiz_weight_sum = 0;
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        WORD minwidth;

        if (IS_HIDDEN(child))
            continue;
        if (data->flags & GROUP_SAME_WIDTH)
        {
            minwidth = MAX(maxminwidth, _minwidth(child));
            minwidth = MIN(minwidth, _maxwidth(child));
        }
        else
            minwidth = _minwidth(child);
        /* MUI 3.9 leaves children that may disappear, and their spacing,
         * out of the minimum and default (tests/native/disappear.c) */
        if (muiAreaData(child)->mad_HorizDisappear > 0)
        {
            tmp.MinWidth -= data->horiz_spacing;
            tmp.DefWidth -= data->horiz_spacing;
        }
        else
        {
            /* a child whose own children may all disappear has a
             * negative minimum; MUI 3.9 counts it as 0 here */
            tmp.MinWidth += MAX(minwidth, 0);
            tmp.DefWidth += MAX(w0_defwidth(child), 0);
        }
        if (tmp.MaxWidth < MUI_MAXMAX)
            tmp.MaxWidth += data->horiz_spacing
                + MIN(w0_maxwidth(child), MUI_MAXMAX);
        tmp.MinHeight = MAX(tmp.MinHeight, _minheight(child));
        tmp.DefHeight = MAX(tmp.DefHeight, _defheight(child));
        /*
           if all children have null weight then maxheight=minheight
           if all but some children have null weights, the maxheight
           is the min of all maxheights
         */
        tmp.MaxHeight = MIN(tmp.MaxHeight, _maxheight(child));
        data->horiz_weight_sum += _hweight(child);
        if (_vweight(child) > 0)
        {
            found_nonzero_vweight = TRUE;
        }
    }
    if (!found_nonzero_vweight)
    {
        tmp.MaxHeight = tmp.MinHeight;
    }

    //if (data->flags & GROUP_VIRTUAL)
    //{
    //kprintf("# min %d x %d  def %d x %d  max %d x %d\n",
    //                tmp.MinWidth, tmp.MinHeight,
    //                tmp.DefWidth, tmp.DefHeight,
    //                tmp.MaxWidth, tmp.MaxHeight);
    //}

    END_MINMAX();
}

/* minmax calculation for vertical groups (see group_minmax_horiz)
*/
static void group_minmax_vert(struct IClass *cl, Object *obj,
    struct MinList *children, struct MUIP_AskMinMax *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MUI_MinMax tmp;
    WORD maxminheight = 0;
    BOOL found_nonzero_hweight = FALSE;

    tmp.MinWidth = 0;
    tmp.DefWidth = 0;
    tmp.MaxWidth = MUI_MAXMAX;
    if (data->num_visible_children > 0)
    {
        tmp.MinHeight = tmp.DefHeight =
            (data->num_visible_children - 1) * data->vert_spacing;
        tmp.MaxHeight = -data->vert_spacing;
    }
    else
    {
        tmp.MinHeight = tmp.DefHeight = 0;
        tmp.MaxHeight = MUI_MAXMAX;
    }

    if (data->flags & GROUP_SAME_HEIGHT)
    {
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            if (IS_HIDDEN(child))
                continue;
            maxminheight = MAX(maxminheight, _minheight(child));
        }
    }

    data->samesize_maxmin_vert = maxminheight;
    data->vert_weight_sum = 0;
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (IS_HIDDEN(child))
            continue;

        if (data->flags & GROUP_SAME_HEIGHT)
            _minheight(child) = MIN(maxminheight, w0_maxheight(child));
        if (muiAreaData(child)->mad_VertDisappear > 0)
        {
            tmp.MinHeight -= data->vert_spacing;
            tmp.DefHeight -= data->vert_spacing;
        }
        else
        {
            tmp.MinHeight += MAX(_minheight(child), 0);
            tmp.DefHeight += MAX(w0_defheight(child), 0);
        }
        if (tmp.MaxHeight < MUI_MAXMAX)
            tmp.MaxHeight += data->vert_spacing
                + MIN(w0_maxheight(child), MUI_MAXMAX);
        tmp.MinWidth = MAX(tmp.MinWidth, _minwidth(child));
        tmp.DefWidth = MAX(tmp.DefWidth, _defwidth(child));
        tmp.MaxWidth = MIN(tmp.MaxWidth, _maxwidth(child));
        data->vert_weight_sum += _vweight(child);
        if (_hweight(child) > 0)
        {
            found_nonzero_hweight = TRUE;
        }
    }
    if (!found_nonzero_hweight)
    {
        tmp.MaxWidth = tmp.MinWidth;
    }

    END_MINMAX();
}


static void
minmax_2d_rows_pass(struct MUI_GroupData *data, struct MinList *children,
    struct MUI_MinMax *req, WORD maxmin_height, WORD maxdef_height)
{
    int i, j;
    APTR cstate;
    Object *child;

    /* do not rewind after the while, to process line by line */
    cstate = children->mlh_Head;
    /* for each row */
    for (i = 0; i < data->rows; i++)
    {
        /* calculate min and max height of this row */
        int min_h = 0, def_h = 0, max_h = MUI_MAXMAX;
        BOOL found_nonzero_vweight = FALSE;

        data->row_infos[i].weight = 0;

        j = 0;
        while ((child = NextObject(&cstate)))
        {
            if (IS_HIDDEN(child))
                continue;
            if (data->flags & GROUP_SAME_HEIGHT)
            {
                _minheight(child) = MIN(maxmin_height, w0_maxheight(child));
                _defheight(child) = MIN(maxdef_height, w0_maxheight(child));
            }
            min_h = MAX(min_h, _minheight(child));
            def_h = MAX(def_h, w0_defheight(child));
            max_h = MIN(max_h, _maxheight(child));
            if (_vweight(child) > 0)
            {
                found_nonzero_vweight = TRUE;
                data->row_infos[i].weight += _vweight(child);
            }
            ++j;
            if ((j % data->columns) == 0)
                break;
        }
        if (!found_nonzero_vweight)
            max_h = min_h;
        else
            max_h = MAX(max_h, min_h);
        /* ... and a row by the average over its columns */
        if (data->columns > 0)
            data->row_infos[i].weight /= data->columns;
/*          D(bug("row %d : min_h=%d max_h=%d\n", i, min_h, max_h)); */

        data->row_infos[i].min = min_h;
        data->row_infos[i].max = max_h;
        data->vert_weight_sum += data->row_infos[i].weight;

        req->MinHeight += min_h;
        req->DefHeight += def_h;
        req->MaxHeight += max_h;
        if (req->MaxHeight > MUI_MAXMAX)
            req->MaxHeight = MUI_MAXMAX;
    }
}


static void
minmax_2d_columns_pass(struct MUI_GroupData *data, struct MinList *children,
    struct MUI_MinMax *req, WORD maxmin_width, WORD maxdef_width)
{
    int i, j;
    APTR cstate;
    Object *child;

    for (i = 0; i < data->columns; i++)
    {
        /* calculate min and max width of this column */
        int min_w = 0, def_w = 0, max_w = MUI_MAXMAX;
        BOOL found_nonzero_hweight = FALSE;

        data->col_infos[i].weight = 0;

        j = 0;
        /* process all children to get children on a column */
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            if (IS_HIDDEN(child))
                continue;
            ++j;
            if (((j - 1) % data->columns) != i)
                continue;
            if (data->flags & GROUP_SAME_WIDTH)
            {
                _minwidth(child) = MIN(maxmin_width, w0_maxwidth(child));
                _defwidth(child) = MIN(maxdef_width, w0_maxwidth(child));
            }
            min_w = MAX(min_w, _minwidth(child));
            def_w = MAX(def_w, w0_defwidth(child));

            /* this handles the case of null weight children, which limit
             *  the max size if they're alone, but not if they are with
             *  non-null weight obj
             */
            max_w = MIN(max_w, _maxwidth(child));
            if (_hweight(child) > 0)
            {
                found_nonzero_hweight = TRUE;
                data->col_infos[i].weight += _hweight(child);
            }
        }
        if (!found_nonzero_hweight)
            max_w = min_w;
        else
            max_w = MAX(max_w, min_w);
        /* MUI 3.9 weighs a column by its children's average weight */
        if (data->rows > 0)
            data->col_infos[i].weight /= data->rows;
/*          D(bug("col %d : min_w=%d max_w=%d\n", i, min_w, max_w)); */

        data->col_infos[i].min = min_w;
        data->col_infos[i].max = max_w;
        data->horiz_weight_sum += data->col_infos[i].weight;

        req->MinWidth += min_w;
        req->DefWidth += def_w;
        req->MaxWidth += max_w;
        if (req->MaxWidth > MUI_MAXMAX)
            req->MaxWidth = MUI_MAXMAX;
    }
}

static void
group_minmax_2d(struct IClass *cl, Object *obj,
    struct MinList *children, struct MUIP_AskMinMax *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MUI_MinMax tmp;
    WORD maxmin_width;
    WORD maxmin_height;
    WORD maxdef_width;
    WORD maxdef_height;

    if (!data->columns)
    {
        if (data->num_children % data->rows)
        {
            data->columns = 1;
            data->rows = data->num_children;
        }
        else
            data->columns = data->num_children / data->rows;
    }
    else
    {
        if (data->num_children % data->columns)
        {
            data->rows = 1;
            data->columns = data->num_children;
        }
        else
            data->rows = data->num_children / data->columns;
    }

    if (data->columns < 1)
        data->columns = 1;
    if (data->rows < 1)
        data->rows = 1;

    if (data->row_infos != NULL)
        mui_free(data->row_infos);

    data->row_infos = mui_alloc(data->rows * sizeof(struct layout2d_elem));
    if (NULL == data->row_infos)
        return;

    if (data->col_infos != NULL)
        mui_free(data->col_infos);

    data->col_infos =
        mui_alloc(data->columns * sizeof(struct layout2d_elem));
    if (NULL == data->col_infos)
        return;

    data->horiz_weight_sum = 0;
    data->vert_weight_sum = 0;

    tmp.MinHeight = tmp.DefHeight = tmp.MaxHeight =
        (data->rows - 1) * data->vert_spacing;
    tmp.MinWidth = tmp.DefWidth = tmp.MaxWidth =
        (data->columns - 1) * data->horiz_spacing;
    /* get minimum dims if same dims for all children are needed */
    maxmin_width = 0;
    maxmin_height = 0;
    maxdef_width = 0;
    maxdef_height = 0;

    if ((data->flags & GROUP_SAME_WIDTH)
        || (data->flags & GROUP_SAME_HEIGHT))
    {
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            if (!(_flags(child) & MADF_SHOWME))
                continue;
            maxmin_width = MAX(maxmin_width, _minwidth(child));
            maxmin_height = MAX(maxmin_height, _minheight(child));
            maxdef_width = MAX(maxdef_width, w0_defwidth(child));
            maxdef_height = MAX(maxdef_height, w0_defheight(child));
        }
/*        g_print("2d group: mminw=%d mminh=%d\n", */
/*            maxmin_width, maxmin_height); */
    }
    if (data->flags & GROUP_SAME_HEIGHT)
        data->samesize_maxmin_vert = maxmin_height;
    else
        data->samesize_maxmin_vert = 0;

    if (data->flags & GROUP_SAME_WIDTH)
        data->samesize_maxmin_horiz = maxmin_width;
    else
        data->samesize_maxmin_horiz = 0;

    minmax_2d_rows_pass(data, children, &tmp, maxmin_height, maxdef_height);
    minmax_2d_columns_pass(data, children, &tmp, maxmin_width,
        maxdef_width);

    END_MINMAX();
}


static void
group_minmax_pagemode(struct IClass *cl, Object *obj,
    struct MinList *children, struct MUIP_AskMinMax *msg)
{
    APTR cstate;
    Object *child;
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    /* MUI 3.9: a page group may grow as far as its largest page may,
     * so a page of fixed height does not cap the others (xSysInfo) */
    struct MUI_MinMax tmp = { 0, 0, 0, 0, 0, 0 };

    cstate = children->mlh_Head;

    D(bug("minmax_pagemode(0x%p) defw = %ld\n", obj, tmp.DefWidth));

    while ((child = NextObject(&cstate)))
    {
        if (!(_flags(child) & MADF_SHOWME))
            continue;

        if (child == data->titlegroup)
            continue;

        tmp.MinHeight = MAX(tmp.MinHeight, _minheight(child));
        D(bug("minmax_pagemode(0x%p) minh child = %d tmpmin=%d\n", obj,
                _minheight(child), tmp.MinHeight));
        tmp.MinWidth = MAX(tmp.MinWidth, _minwidth(child));
        tmp.MaxHeight = MAX(tmp.MaxHeight, w0_maxheight(child));
        tmp.MaxWidth = MAX(tmp.MaxWidth, w0_maxwidth(child));
        tmp.DefHeight = MAX(tmp.DefHeight,
            ((w0_defheight(child) <
                    MUI_MAXMAX) ? w0_defheight(child) : tmp.DefHeight));
        tmp.DefWidth =
            MAX(tmp.DefWidth,
            ((w0_defwidth(child) <
                    MUI_MAXMAX) ? w0_defwidth(child) : tmp.DefWidth));
        D(bug("minmax_pagemode(0x%p) defw = %ld\n", obj, tmp.DefWidth));
    }

    if (tmp.MaxWidth == 0 && tmp.MaxHeight == 0)
        tmp.MaxWidth = tmp.MaxHeight = MUI_MAXMAX;

    if (data->titlegroup)
    {
        tmp.MinWidth = MAX(tmp.MinWidth, _minwidth(data->titlegroup));
        tmp.DefWidth = MAX(tmp.DefWidth, _defwidth(data->titlegroup));
        tmp.MaxWidth = MAX(tmp.MaxWidth, _maxwidth(data->titlegroup));
        tmp.MinHeight += _minheight(data->titlegroup);
        tmp.MaxHeight += w0_maxheight(data->titlegroup);
        tmp.DefHeight += w0_defheight(data->titlegroup);
    }

    END_MINMAX();
}

/**************************************************************************
 MUIM_AskMinMax : ask children about min/max sizes, then
 either call a hook, or the builtin method, to calculate our minmax
**************************************************************************/
IPTR Group__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    struct MUI_LayoutMsg lm;
    struct MUIP_AskMinMax childMsg;
    struct MUI_MinMax childMinMax;
    APTR cstate;
    Object *child;
    LONG super_minwidth, super_minheight;

    /*
     * let our superclass first fill in its size with frame, inner spc etc ...
     */
    DoSuperMethodA(cl, obj, (Msg) msg);
    super_minwidth = msg->MinMaxInfo->MinWidth;
    super_minheight = msg->MinMaxInfo->MinHeight;

    /*
     * Ask children
     */
    childMsg.MethodID = msg->MethodID;
    childMsg.MinMaxInfo = &childMinMax;
    get(data->family, MUIA_Family_List, &(lm.lm_Children));

    /* every child counts again; the layout decides which disappear */
    cstate = lm.lm_Children->mlh_Head;
    while ((child = NextObject(&cstate)))
        _flags(child) &= ~MADF_DISAPPEARED;

    cstate = lm.lm_Children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        /* Custom layout hooks see every child, as in MUI 3.8/3.9, so
         * hidden children need valid dimensions too */
        if (!(_flags(child) & MADF_SHOWME) && !data->layout_hook)
            /* BORDERGADGETs should handle this itself */
            continue;
        /*  Ask child; Area points the message at the child's minmax  */
        childMsg.MinMaxInfo = &childMinMax;
        DoMethodA(child, (Msg) & childMsg);
        /*  D(bug("*** group 0x%p, child 0x%p min=%ld,%ld\n", */
        /*      obj, child, childMinMax.MinWidth, childMinMax.MinHeight)); */
        __area_finish_minmax(child, childMsg.MinMaxInfo);
    }

    /*
     * Use children infos to calculate group size
     */
    if (data->flags & GROUP_PAGEMODE)
    {
        D(bug("minmax_pagemode(%p) minh initial = %d\n", obj,
                msg->MinMaxInfo->MinHeight));
        group_minmax_pagemode(cl, obj, lm.lm_Children, msg);
        D(bug("minmax_pagemode(%p) minh = %d\n", obj,
                msg->MinMaxInfo->MinHeight));
    }
    else if (data->layout_hook)
    {
        lm.lm_Type = MUILM_MINMAX;
        CallHookPkt(data->layout_hook, obj, &lm);

        if (lm.lm_MinMax.MaxHeight < lm.lm_MinMax.MinHeight)
            lm.lm_MinMax.MaxHeight = lm.lm_MinMax.MinHeight;
        if (lm.lm_MinMax.DefHeight < lm.lm_MinMax.MinHeight)
            lm.lm_MinMax.DefHeight = lm.lm_MinMax.MinHeight;
        if (lm.lm_MinMax.MaxWidth < lm.lm_MinMax.MinWidth)
            lm.lm_MinMax.MaxWidth = lm.lm_MinMax.MinWidth;
        if (lm.lm_MinMax.DefWidth < lm.lm_MinMax.MinWidth)
            lm.lm_MinMax.DefWidth = lm.lm_MinMax.MinWidth;

        //kprintf("### min %d x %d   def %d x %d  max %d x %d\n",
        //    msg->MinMaxInfo->MinWidth,
        //    msg->MinMaxInfo->MinHeight,
        //    msg->MinMaxInfo->DefWidth,
        //    msg->MinMaxInfo->DefHeight,
        //    msg->MinMaxInfo->MaxWidth,
        //    msg->MinMaxInfo->MaxHeight);

        msg->MinMaxInfo->MinWidth += lm.lm_MinMax.MinWidth;
        msg->MinMaxInfo->MinHeight += lm.lm_MinMax.MinHeight;
        msg->MinMaxInfo->MaxWidth += lm.lm_MinMax.MaxWidth;
        if (msg->MinMaxInfo->MaxWidth > MUI_MAXMAX)
            msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
        msg->MinMaxInfo->MaxHeight += lm.lm_MinMax.MaxHeight;
        if (msg->MinMaxInfo->MaxHeight > MUI_MAXMAX)
            msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
        msg->MinMaxInfo->DefWidth += lm.lm_MinMax.DefWidth;
        msg->MinMaxInfo->DefHeight += lm.lm_MinMax.DefHeight;

        //kprintf("#### min %d x %d   def %d x %d  max %d x %d\n",
        //    msg->MinMaxInfo->MinWidth,
        //    msg->MinMaxInfo->MinHeight,
        //    msg->MinMaxInfo->DefWidth,
        //    msg->MinMaxInfo->DefHeight,
        //    msg->MinMaxInfo->MaxWidth,
        //    msg->MinMaxInfo->MaxHeight);

    }
    else
    {
        if ((data->rows == 1) && (data->columns == 1))
        {
            data->num_visible_children =
                Group_GetNumVisibleChildren(data, lm.lm_Children);
            if (data->flags & GROUP_HORIZ)
                group_minmax_horiz(cl, obj, lm.lm_Children, msg);
            else
                group_minmax_vert(cl, obj, lm.lm_Children, msg);
        }
        else
        {
            group_minmax_2d(cl, obj, lm.lm_Children, msg);
        }
    }

    if (data->flags & GROUP_VIRTUAL)
    {
        data->saved_minwidth = msg->MinMaxInfo->MinWidth;
        data->saved_minheight = msg->MinMaxInfo->MinHeight;
        /* MUI 3.8/3.9 keep eight pixels of virtual content visible */
        msg->MinMaxInfo->MinWidth = super_minwidth + 8;
        msg->MinMaxInfo->MinHeight = super_minheight + 8;

        //kprintf("## min %d x %d   def %d x %d  max %d x %d\n",
        //    msg->MinMaxInfo->MinWidth,
        //    msg->MinMaxInfo->MinHeight,
        //    msg->MinMaxInfo->DefWidth,
        //    msg->MinMaxInfo->DefHeight,
        //    msg->MinMaxInfo->MaxWidth,
        //    msg->MinMaxInfo->MaxHeight);

    }

    return 0;
}



// enforce minmax constraint, but also update total growable/shrinkable weights
// while we're at it
static void Layout1D_minmax_constraint(WORD *sizep, WORD minsize,
    WORD maxsize, WORD *remainp, WORD *sizegrowp, WORD *sizeshrinkp,
    ULONG *weightgrowp, ULONG *weightshrinkp, UWORD weight, WORD samesize)
{
    WORD size = *sizep, remain = *remainp,
        sizegrow = *sizegrowp, sizeshrink = *sizeshrinkp;
    ULONG weightgrow = *weightgrowp, weightshrink = *weightshrinkp;

/*      D(bug("L1D_minmax_c size=%d min=%d max=%d w=%d ss=%d\n", */
/*          size, minsize, maxsize, */
/*          weight, samesize)); */

    if ((samesize > 0) && (weight == 0))
    {
        remain += size - samesize;
        size = samesize;
        sizeshrink -= size;
        sizegrow -= size;
    }

    if (size <= minsize)        // too little
    {
        remain += size - minsize;
        size = minsize;
        sizeshrink -= size;
        if (size == maxsize)
            sizegrow -= size;
    }
    else if (size >= maxsize)   // too big
    {
        remain += size - maxsize;
        size = maxsize;
        sizegrow -= size;
    }

    if (!((samesize > 0) && (weight == 0)))
    {
        if (size < maxsize)
            weightgrow += weight;
        if (size > minsize)
            weightshrink += weight;
    }

    *sizep = size;
    *remainp = remain;
    *sizegrowp = sizegrow;
    *sizeshrinkp = sizeshrink;
    *weightgrowp = weightgrow;
    *weightshrinkp = weightshrink;
}


// redistribute excess size to growable child, or reduce size of a shrinkable
// child
static void Layout1D_redistribution(WORD *sizep, WORD minsize,
    WORD maxsize, WORD *remainp, WORD *sizegrowp, WORD *sizeshrinkp,
    ULONG *weightgrowp, ULONG *weightshrinkp, UWORD weight)
{
    WORD size = *sizep, remain = *remainp,
        sizegrow = *sizegrowp, sizeshrink = *sizeshrinkp;
    ULONG weightgrow = *weightgrowp, weightshrink = *weightshrinkp;

    if (weight == 0)
        return;

    if ((remain > 0) && (size < maxsize))
    {
        LONG newsize;

        /* rounded down like MUI 3.9; the last child takes the rest */
        newsize = (sizegrow * weight) / weightgrow;

/*        D(bug("newsize=%ld == size_growa=%ld * w=%ld / weight_grow=%d\n", */
/*                newsize, sizegrow, weight, weightgrow)); */

        /* take care of off-by-1 errors that may toggle remainder sign
         * by ensuring convergence to 0
         */
        if (remain - newsize + size < 0)
        {
/*              D(bug("adding remainder=%d => size = %d\n", */
/*                    remain, size + remain)); */
            size += remain;
            remain = 0;
        }
        else
        {
            remain -= newsize - size;
            size = newsize;
            sizegrow -= size;
            weightgrow -= weight;
        }
    }
    else if ((remain < 0) && (size > minsize))
    {
        LONG newsize;

        newsize = (sizeshrink * weight) / weightshrink;

/*        D(bug("newsize=%ld == size_shrinkables=%ld * w=%ld " */
/*            "/ weight_shrinkables=%d\n", */
/*            newsize, sizeshrink, weight, weightshrink)); */

        if (remain - newsize + size > 0)
        {
/*              D(bug("adding remainder=%d => size = %d\n", */
/*                    remain, size + remain)); */
            size += remain;
            remain = 0;
        }
        else
        {
            remain -= newsize - size;
            size = newsize;
            sizeshrink -= size;
            weightshrink -= weight;
        }
    }

    *sizep = size;
    *remainp = remain;
    *sizegrowp = sizegrow;
    *sizeshrinkp = sizeshrink;
    *weightgrowp = weightgrow;
    *weightshrinkp = weightshrink;
}


// 2 passes at most, less on average (0.5 or 1.5), each does
// - a minmax clamping, evenutally adding to a remainder
// (remainder = missing (underflow) or remaining (overflow) space compared
// to ideal sizes where children fill the whole group)
// - a redistribution of the remainder, by growing (pos. remainder) or
// shrinking (neg. remainder) children able to support it.
//
// Occasionnaly the first time the redistribution is done, the minmax
// constraint can be broken, thus the extra pass to check and eventually
// redistribute. The second redistribution never breaks minmax constraint
// (there should be a mathematical proof, but feel free to prove me wrong
// with an example)
/* Final settling pass: the two redistribution passes above can leave a
 * child outside its min/max range while others absorb the difference.
 * Clamp every child, then share any remainder by weight among children
 * that can still move, until no space is left or nothing can move. */
static void Layout1D_settle(struct MinList *children, WORD remainder,
    BOOL group_horiz)
{
    APTR cstate;
    Object *child;
    int round;

    for (round = 0; round < 32; round++)
    {
        ULONG weights = 0, movable = 0;
        LONG share = remainder;

        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            WORD *size, min, max;

            if (IS_HIDDEN(child))
                continue;
            size = group_horiz ? &_width(child) : &_height(child);
            min = group_horiz ? _minwidth(child) : _minheight(child);
            max = group_horiz ? _maxwidth(child) : _maxheight(child);
            if (*size < min)
            {
                remainder -= min - *size;
                *size = min;
            }
            else if (*size > max)
            {
                remainder += *size - max;
                *size = max;
            }
        }
        if (remainder == 0)
            return;

        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            WORD size, min, max;

            if (IS_HIDDEN(child))
                continue;
            size = group_horiz ? _width(child) : _height(child);
            min = group_horiz ? _minwidth(child) : _minheight(child);
            max = group_horiz ? _maxwidth(child) : _maxheight(child);
            if ((remainder > 0 && size < max) || (remainder < 0 && size > min))
            {
                movable++;
                weights += group_horiz ? _hweight(child) : _vweight(child);
            }
        }
        if (movable == 0)
            return;

        share = remainder;
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)) && remainder != 0)
        {
            WORD *size, min, max;
            LONG delta, weight;

            if (IS_HIDDEN(child))
                continue;
            size = group_horiz ? &_width(child) : &_height(child);
            min = group_horiz ? _minwidth(child) : _minheight(child);
            max = group_horiz ? _maxwidth(child) : _maxheight(child);
            if (!((remainder > 0 && *size < max)
                    || (remainder < 0 && *size > min)))
                continue;

            weight = group_horiz ? _hweight(child) : _vweight(child);
            delta = weights ? share * weight / (LONG)weights
                : share / (LONG)movable;
            if (delta == 0)
                delta = remainder > 0 ? 1 : -1;
            if ((delta > 0 && delta > remainder)
                || (delta < 0 && delta < remainder))
                delta = remainder;
            if (*size + delta > max)
                delta = max - *size;
            if (*size + delta < min)
                delta = min - *size;
            *size += delta;
            remainder -= delta;
        }
    }
}

static void Layout1D_minmax_constraints_and_redistrib(struct MinList
    *children, WORD total_size, WORD remainder, WORD samesize,
    BOOL group_horiz)
{
    APTR cstate;
    Object *child;
    int i;

    for (i = 0; i < 2; i++)
    {
        WORD size_growables = total_size;
        WORD size_shrinkables = total_size;
        ULONG weight_growables = 0;
        ULONG weight_shrinkables = 0;

/*          D(bug("start : rem=%ld, A=%ld, size_growables=%ld, " */
/*              "size_shrinkables=%ld\n", */
/*              remainder, total_size, size_growables, size_shrinkables)); */

        // minmax constraints
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            /* WORD old_size; */

            if (IS_HIDDEN(child))
                continue;

            if (group_horiz)
            {
                /* old_size = _width(child); */

                Layout1D_minmax_constraint(&_width(child), _minwidth(child),
                    _maxwidth(child), &remainder, &size_growables,
                    &size_shrinkables, &weight_growables,
                    &weight_shrinkables, _hweight(child), samesize);

/*                D(bug("loop1 on %p : width=%d was %d, rem=%d, A=%d, " */
/*                    "sizegrow=%d, sizeshrink=%d w=%d min=%d max=%d\n", */
/*                    child, _width(child), old_size, remainder, total_size, */
/*                        size_growables, size_shrinkables, _hweight(child), */
/*                        _minwidth(child), _maxwidth(child))); */
            }
            else                // ! group_horiz
            {
/*                old_size = _height(child); */

                Layout1D_minmax_constraint(&_height(child),
                    _minheight(child), _maxheight(child), &remainder,
                    &size_growables, &size_shrinkables, &weight_growables,
                    &weight_shrinkables, _vweight(child), samesize);

/*                D(bug("loop1 on %p : h=%ld was %d, rem=%d, A=%d, " */
/*                    "sizegrow=%d, sizeshrink=%d w=%d min=%d max=%d\n", */
/*                    child, _height(child), old_size, remainder, total_size,*/
/*                    size_growables, size_shrinkables, _vweight(child), */
/*                    _minheight(child), _maxheight(child))); */
            }                   // if (group_horiz)
        }                       // while child, minmax constraints

        // mid-pass break
        if (remainder == 0)
            break;

/*          D(bug("mid : rem=%d, A=%d, size_grow=%d, size_shrink=%d, " */
/*                "wg=%ld, ws=%ld\n", remainder, total_size, size_growables, */
/*                size_shrinkables, weight_growables, weight_shrinkables)); */

        // distribute remaining space to possible candidates
        cstate = children->mlh_Head;
        while (((child = NextObject(&cstate)) != NULL) && (remainder != 0))
        {
/*            WORD old_size; */

            if (IS_HIDDEN(child))
                continue;

            if (group_horiz)
            {
/*                old_size = _width(child); */

                Layout1D_redistribution(&_width(child), _minwidth(child),
                    _maxwidth(child), &remainder, &size_growables,
                    &size_shrinkables, &weight_growables,
                    &weight_shrinkables, _hweight(child));

/*                  D(bug("loop2 on %p : h=%d was %d, rem=%d, A=%d, " */
/*                        "size_grow=%d, size_shrink=%d\n", child, */
/*                        _width(child), old_size, remainder, total_size, */
/*                        size_growables, size_shrinkables)); */
            }
            else                // ! group_horiz
            {
/*                old_size = _height(child); */

                Layout1D_redistribution(&_height(child), _minheight(child),
                    _maxheight(child), &remainder, &size_growables,
                    &size_shrinkables, &weight_growables,
                    &weight_shrinkables, _vweight(child));

/*                  D(bug("loop2 on %p : h=%d was %d, rem=%d, A=%d, " */
/*                        "size_grow=%d, size_shrink=%d\n", child, */
/*                        _height(child), old_size, remainder, total_size, */
/*                        size_growables, size_shrinkables)); */
            }                   // if (group_horiz)

        }                       // while child, redistribution

/*          if (remainder != 0) */
/*          { */
/*              D(bug("end : rem=%ld, A=%ld, size_grow=%ld, size_shrink=%ld\n", */
/*                    remainder, total_size, size_growables, size_shrinkables)); */
/*          } */
        // dont break here if remainder == 0, some minmax constraints
        // may not be respected
    }                           // end for

    Layout1D_settle(children, remainder, group_horiz);

    // to easily spot layout bugs, nothing like a (division by zero) exception
/*      if (remainder != 0) */
/*      { */
/*          ASSERT(remainder != 0); */
/*          D(bug("gonna crash, remainder = %d\n", remainder)); */
/*          remainder /= 0; */
/*      } */

}

/* MUI 3.9's redistribution (tests/native/hdist.c): after the weighted
 * first split, each pass clamps every child to its range and then adds
 * to each child that can still move its weight's share of that pass's
 * remainder, taken from the weight of all children and truncated, so
 * clamped children keep their weight in the divisor. After eight
 * passes the rest goes one pixel at a time to the movable children from
 * the last one back. */
static void Layout1D_mui_redistrib(struct MinList *children, WORD total_size,
    ULONG total_weight, BOOL group_horiz)
{
    APTR cstate;
    Object *child;
    LONG remainder;
    int pass;

    for (pass = 0; pass <= 8; pass++)
    {
        LONG start;
        BOOL moved;

        remainder = total_size;
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            WORD *size;

            if (IS_HIDDEN(child))
                continue;
            size = group_horiz ? &_width(child) : &_height(child);
            *size = CLAMP(*size,
                group_horiz ? _minwidth(child) : _minheight(child),
                group_horiz ? _maxwidth(child) : _maxheight(child));
            remainder -= *size;
        }
        if (remainder == 0)
            return;
        if (pass == 8)
            break;

        start = remainder;
        moved = FALSE;
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            WORD *size, min, max;
            LONG weight, delta;

            if (IS_HIDDEN(child))
                continue;
            size = group_horiz ? &_width(child) : &_height(child);
            min = group_horiz ? _minwidth(child) : _minheight(child);
            max = group_horiz ? _maxwidth(child) : _maxheight(child);
            weight = group_horiz ? _hweight(child) : _vweight(child);
            if ((start > 0 && *size >= max) || (start < 0 && *size <= min))
                continue;
            delta = start * weight / (LONG)total_weight;
            if (delta)
            {
                *size += delta;
                moved = TRUE;
            }
        }
        if (!moved)
            break;
    }

    /* the rest, a pixel at a time from the last child back, weighted
     * children first */
    while (remainder != 0)
    {
        BOOL moved = FALSE;
        int round;

        for (round = 0; round < 2 && remainder != 0 && !moved; round++)
        {
            struct MinNode *node;

            for (node = children->mlh_TailPred;
                node->mln_Pred && remainder != 0; node = node->mln_Pred)
            {
                Object *kid = BASEOBJECT((struct _Object *)node);
                WORD *size, min, max;
                LONG weight;

                if (IS_HIDDEN(kid))
                    continue;
                size = group_horiz ? &_width(kid) : &_height(kid);
                min = group_horiz ? _minwidth(kid) : _minheight(kid);
                max = group_horiz ? _maxwidth(kid) : _maxheight(kid);
                weight = group_horiz ? _hweight(kid) : _vweight(kid);
                if (round == 0 && weight == 0)
                    continue;
                if (remainder > 0 && *size < max)
                {
                    ++*size;
                    --remainder;
                    moved = TRUE;
                }
                else if (remainder < 0 && *size > min)
                {
                    --*size;
                    ++remainder;
                    moved = TRUE;
                }
            }
        }
        if (!moved)
            return;
    }
}

static void Layout1D_weight_constraint(WORD *total_sizep,
    WORD *total_init_sizep,
    ULONG *total_weightp, WORD *sizep, UWORD weight, WORD minsize)
{
    if (*total_weightp > 0)
        *sizep = (*total_sizep * weight) / *total_weightp;
    else
        *sizep = minsize;

    *total_weightp -= weight;
    *total_sizep -= *sizep;
    *total_init_sizep += *sizep;
}


/* MUIA_Horiz/VertDisappear, like MUI 3.9 (tests/native/disappear.c):
 * while the minimum sizes of the remaining children do not fit, all
 * children of the lowest disappear level still present disappear. They
 * get their minimum size and are hidden; the others are laid out. */
static void group_disappear(Object *obj, struct MUI_GroupData *data,
    struct MinList *children, BOOL horiz, WORD available)
{
    WORD spacing = horiz ? data->horiz_spacing : data->vert_spacing;
    BOOL any = FALSE;
    APTR cstate;
    Object *child;

    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        _flags(child) &= ~MADF_DISAPPEARED;
        if ((horiz ? muiAreaData(child)->mad_HorizDisappear
            : muiAreaData(child)->mad_VertDisappear) > 0)
            any = TRUE;
    }
    if (!any)
        return;

    for (;;)
    {
        LONG need = 0, level = 0, pos;
        WORD count = 0;

        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            LONG l;

            if (IS_HIDDEN(child))
                continue;
            need += horiz ? _minwidth(child) : _minheight(child);
            count++;
            l = horiz ? muiAreaData(child)->mad_HorizDisappear
                : muiAreaData(child)->mad_VertDisappear;
            if (l > 0 && (level == 0 || l < level))
                level = l;
        }
        if (count > 0)
            need += (count - 1) * spacing;
        if (need <= available || level == 0)
            break;
        /* like MUI 3.9, the batch keeps boxes at its minimum size, one
         * after the other behind the children still present */
        pos = 0;
        cstate = children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            if (IS_HIDDEN(child))
                continue;
            if ((horiz ? muiAreaData(child)->mad_HorizDisappear
                : muiAreaData(child)->mad_VertDisappear) == level)
            {
                _flags(child) |= MADF_DISAPPEARED;
                if (_flags(child) & MADF_CANDRAW)
                    DoHideMethod(child);
                MUI_Layout(child, horiz ? pos : 0, horiz ? 0 : pos,
                    _minwidth(child), _minheight(child), 0);
            }
            pos += (horiz ? _minwidth(child) : _minheight(child)) + spacing;
        }
    }

    /* the layout counts and weighs only what is left */
    data->num_visible_children = 0;
    if (horiz)
        data->horiz_weight_sum = 0;
    else
        data->vert_weight_sum = 0;
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (IS_HIDDEN(child))
            continue;
        data->num_visible_children++;
        if (horiz)
            data->horiz_weight_sum += _hweight(child);
        else
            data->vert_weight_sum += _vweight(child);
    }
}

/* Show children that reappeared in a shown group, after their layout */
static void group_reappear(Object *obj, struct MinList *children)
{
    APTR cstate;
    Object *child;

    if (!(_flags(obj) & MADF_CANDRAW))
        return;
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
        if (!IS_HIDDEN(child) && !(_flags(child) & MADF_CANDRAW))
            DoShowMethod(child);
}

static void group_layout_vert(struct IClass *cl, Object *obj,
    struct MinList *children)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    ULONG total_weight;
    WORD remainder;    /* must converge to 0 to successfully end layout */
    WORD total_size;
    WORD total_size_backup;
    WORD total_init_size;       /* total size of the ideally sized children */
    WORD width;
    WORD left = 0;
    WORD top = 0;
    WORD layout_width;
    WORD layout_height;

    //kprintf("group_layout_vert: virtoff = %d,%d\n",
    //    data->virt_offx, data->virt_offy);

    if (data->flags & GROUP_VIRTUAL)
    {
        data->virt_mwidth =
            CLAMP(_mwidth(obj), data->saved_minwidth - _subwidth(obj),
            _maxwidth(obj) - _subwidth(obj));
        data->virt_mheight =
            CLAMP(_mheight(obj), data->saved_minheight - _subheight(obj),
            _maxheight(obj) - _subheight(obj));

        layout_width = data->virt_mwidth;
        layout_height = data->virt_mheight;
    }
    else
    {
        layout_width = _mwidth(obj);
        layout_height = _mheight(obj);
    }

    group_disappear(obj, data, children, FALSE, layout_height);
    total_weight = data->vert_weight_sum;
    total_init_size = 0;
    total_size =
        layout_height - (data->num_visible_children -
        1) * data->vert_spacing;
    total_size_backup = total_size;

/*      D(bug("\nvert layout for %p, A=%d W=%ld\n", */
/*          obj, total_size, total_weight)); */

    // weight constraints
    // calculate ideal size for each object, and total ideal size
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (IS_HIDDEN(child))
            continue;

        Layout1D_weight_constraint(&total_size, &total_init_size,
            &total_weight, &_height(child), _vweight(child),
            _minheight(child));
/*          D(bug("child %p : ideal=%d w=%ld\n", */
/*                child, _height(child), _vweight(child))); */
    }                           // while child, weight constraints

    total_size = total_size_backup;
    remainder = total_size - total_init_size;

    if (data->flags & GROUP_VIRTUAL)
    {
        /* This is also true for non virtual groups, but if this would be the
         ** case then there is a bug in the layout function
         */
        if (total_size < 0)
            total_size = 0;
    }

    if (!(data->flags & GROUP_SAME_HEIGHT) && data->vert_weight_sum > 0)
        Layout1D_mui_redistrib(children, total_size,
            data->vert_weight_sum, FALSE);
    else
        Layout1D_minmax_constraints_and_redistrib(children,
            total_size,
            remainder,
            (data->flags & GROUP_SAME_HEIGHT) ?
                data->samesize_maxmin_vert : 0,
            FALSE);

    // do the layout
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (IS_HIDDEN(child))
            continue;

        /* the maximum wins over a larger minimum, as in MUI 3.9
         * (tests/native/defclamp.c) */
        width = MIN(MAX(layout_width, _minwidth(child)), _maxwidth(child));
        left = (layout_width - width) / 2;

/*          D(bug("child %p -> layout %d x %d\n", */
/*              child, width, _height(child))); */
        if (!MUI_Layout(child, left, top, width, _height(child), 0))
            return;
        top += data->vert_spacing + _height(child);
    }

    if (!(data->flags & GROUP_VIRTUAL))
        group_reappear(obj, children);
}


static void group_layout_horiz(struct IClass *cl, Object *obj,
    struct MinList *children)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    ULONG total_weight;
    WORD remainder;    /* must converge to 0 to successfully end layout */
    WORD total_size;
    WORD total_size_backup;
    WORD total_init_size;       /* total size of the ideally sized children */
    WORD height;
    WORD top = 0;
    WORD left = 0;
    WORD layout_width;
    WORD layout_height;

    //kprintf("group_layout_horiz: virtoff = %d,%d\n",
    //    data->virt_offx, data->virt_offy);

    if (data->flags & GROUP_VIRTUAL)
    {
        data->virt_mwidth =
            CLAMP(_mwidth(obj), data->saved_minwidth - _subwidth(obj),
            _maxwidth(obj) - _subwidth(obj));
        data->virt_mheight =
            CLAMP(_mheight(obj), data->saved_minheight - _subheight(obj),
            _maxheight(obj) - _subheight(obj));

        layout_width = data->virt_mwidth;
        layout_height = data->virt_mheight;

        //kprintf("group_layout_horiz: layoutsize %d x %d "
        //    " virtsize %d x %d  msize %d x %d\n",
        //    layout_width, layout_height, data->virt_mwidth,
        //    data->virt_mheight,
        //    _mwidth(obj), _mheight(obj));
    }
    else
    {
        layout_width = _mwidth(obj);
        layout_height = _mheight(obj);
    }

    group_disappear(obj, data, children, TRUE, layout_width);
    total_weight = data->horiz_weight_sum;
    total_init_size = 0;
    total_size =
        layout_width - (data->num_visible_children -
        1) * data->horiz_spacing;
    total_size_backup = total_size;

/*      D(bug("\nhoriz layout for %p, A=%d W=%ld\n", */
/*          obj, total_size, total_weight)); */

    // weight constraints
    // calculate ideal size for each object, and total ideal size
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (IS_HIDDEN(child))
            continue;

        Layout1D_weight_constraint(&total_size, &total_init_size,
            &total_weight, &_width(child), _hweight(child),
            _minwidth(child));
/*          D(bug("child %p : ideal=%d w=%ld\n", */
/*                child, _width(child), _hweight(child))); */
    }                           // while child, weight constraints

    total_size = total_size_backup;
    if (data->horiz_weight_sum > 0)
        remainder = total_size - total_init_size;
    else
        remainder = 0;

    if (data->flags & GROUP_VIRTUAL)
    {
        /* This is also true for non virtual groups, but if this would be the
         ** case then there is a bug in the layout function
         */
        if (total_size < 0)
            total_size = 0;
    }

    if (!(data->flags & GROUP_SAME_WIDTH) && data->horiz_weight_sum > 0)
        Layout1D_mui_redistrib(children, total_size,
            data->horiz_weight_sum, TRUE);
    else
        Layout1D_minmax_constraints_and_redistrib(children,
            total_size,
            remainder,
            (data->flags & GROUP_SAME_WIDTH) ?
                data->samesize_maxmin_horiz : 0,
            TRUE);

    // do the layout
    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (IS_HIDDEN(child))
            continue;

        /* the maximum wins over a larger minimum, as in MUI 3.9 */
        height = MIN(MAX(layout_height, _minheight(child)),
            _maxheight(child));
        top = (layout_height - height) / 2;

/*          D(bug("child %p -> layout %d x %d\n", */
/*              child, _width(child), height)); */
        if (!MUI_Layout(child, left, top, _width(child), height, 0))
            return;
        left += data->horiz_spacing + _width(child);
    }

    if (!(data->flags & GROUP_VIRTUAL))
        group_reappear(obj, children);
}


static void Layout2D_weight_constraint(struct MUI_GroupData *data,
    struct layout2d_elem *row_infos,
    struct layout2d_elem *col_infos,
    WORD total_size_height, WORD total_size_width,
    WORD *total_init_height, WORD *total_init_width)
{
    int i;
    ULONG total_weight_vert = data->vert_weight_sum;
    ULONG total_weight_horiz = data->horiz_weight_sum;

    *total_init_height = 0;
    *total_init_width = 0;

    /* calc row heights */
    for (i = 0; i < data->rows; i++)
    {
        if (total_weight_vert > 0)
            row_infos[i].dim =
                (total_size_height * row_infos[i].weight)
                / total_weight_vert;
        else
            row_infos[i].dim = row_infos[i].min;

/*          D(bug("l2 row %d : ideal = %d with w=%d, A=%d, W=%d\n", */
/*              i, row_infos[i].dim, */
/*              row_infos[i].weight, total_size_height, total_weight_vert)); */

        total_weight_vert -= row_infos[i].weight;
        total_size_height -= row_infos[i].dim;
        *total_init_height += row_infos[i].dim;
    }

    /* calc columns widths */
    for (i = 0; i < data->columns; i++)
    {
        if (total_weight_horiz)
            col_infos[i].dim =
                (total_size_width * col_infos[i].weight)
                / total_weight_horiz;
        else
            col_infos[i].dim = col_infos[i].min;

/*          D(bug("l2 col %d : ideal = %d with w=%d, A=%d, W=%d\n", */
/*              i, col_infos[i].dim, */
/*              col_infos[i].weight, total_size_width, total_weight_horiz)); */

        total_weight_horiz -= col_infos[i].weight;
        total_size_width -= col_infos[i].dim;
        *total_init_width += col_infos[i].dim;
    }
}



static void Layout2D_minmax_constraints_and_redistrib(struct layout2d_elem
    *infos, WORD nitems, WORD total_size, WORD samesize, WORD remainder)
{
    int j;

/*      D(bug("L2D mc&r n=%d A=%d ss=%d rem=%d\n", */
/*          nitems, total_size, samesize, remainder)); */

    for (j = 0; j < 2; j++)
    {
        WORD size_growables = total_size;
        WORD size_shrinkables = total_size;
        ULONG weight_growables = 0;
        ULONG weight_shrinkables = 0;
/*        WORD old_size; */
        int i;

        // minmax constraints
        for (i = 0; i < nitems; i++)
        {
/*                old_size = infos[i].dim; */

/*              D(bug("bef loop1 on %d : size=%d, rem=%d, A=%d, " */
/*                        "sizegrow=%d, sizeshrink=%d w=%d min=%d max=%d\n", */
/*                        i, infos[i].dim, remainder, total_size, */
/*                        size_growables, size_shrinkables, infos[i].weight, */
/*                        infos[i].min, infos[i].max)); */

            Layout1D_minmax_constraint(&infos[i].dim, infos[i].min,
                infos[i].max, &remainder, &size_growables,
                &size_shrinkables, &weight_growables, &weight_shrinkables,
                infos[i].weight, samesize);

/*              D(bug("loop1 on %d : size=%d was %d, rem=%d, A=%d, " */
/*                        "sizegrow=%d, sizeshrink=%d w=%d min=%d max=%d\n", */
/*                        i, infos[i].dim, old_size, remainder, total_size, */
/*                        size_growables, size_shrinkables, infos[i].weight, */
/*                        infos[i].min, infos[i].max)); */
        }

        if (remainder == 0)
            break;

        for (i = 0; i < nitems; i++)
        {
/*            old_size = infos[i].dim; */

/*              D(bug("bef loop2 on %d : size=%d, rem=%d, A=%d, " */
/*                        "size_grow=%d, size_shrink=%d\n", i, */
/*                        infos[i].dim, remainder, total_size, */
/*                        size_growables, size_shrinkables)); */

            Layout1D_redistribution(&infos[i].dim, infos[i].min,
                infos[i].max, &remainder, &size_growables,
                &size_shrinkables, &weight_growables, &weight_shrinkables,
                infos[i].weight);

/*              D(bug("loop2 on %d : size=%d was %d, rem=%d, A=%d, " */
/*                        "size_grow=%d, size_shrink=%d\n", i, */
/*                        infos[i].dim, old_size, remainder, total_size, */
/*                        size_growables, size_shrinkables)); */
        }
    }
}

static void
layout_2d_distribute_space(struct MUI_GroupData *data,
    struct layout2d_elem *row_infos,
    struct layout2d_elem *col_infos,
    struct MinList *children, LONG left_start, LONG top_start)
{
    APTR cstate;
    Object *child;
    LONG left;
    LONG top;
    LONG col_width;
    LONG row_height;
    int i, j;

    /*
     * pass 2 : distribute space
     */
    cstate = children->mlh_Head;
    top = top_start;
    /* for each row */
    for (i = 0; i < data->rows; i++)
    {
        /* left start for child layout in this row */
        left = left_start;

        /* max height for children in this row */
        row_height = row_infos[i].dim;
        j = 0;
        /* for each column */
        while ((child = NextObject(&cstate)))
        {
            LONG cleft;
            LONG ctop;
            LONG cwidth;
            LONG cheight;

            if (IS_HIDDEN(child))
                continue;
            /* max width for children in this column */
            col_width = col_infos[j].dim;

            /* center child if col width is bigger than child maxwidth */
            cwidth = MIN(_maxwidth(child), col_width);
            cwidth = MAX(cwidth, _minwidth(child));
            cleft = left + (col_width - cwidth) / 2;

            /* center child if row height is bigger than child maxheight */
            cheight = MIN(_maxheight(child), row_height);
            cheight = MAX(cheight, _minheight(child));
            ctop = top + (row_height - cheight) / 2;

/*          g_print("layout %d %d  %d %d\n", cleft, ctop, cwidth, cheight); */
/*          D(bug("2DL/child %p -> layout %d x %d\n", */
/*              child, cwidth, cheight)); */
            if (!MUI_Layout(child, cleft, ctop, cwidth, cheight, 0))
                return;

            left += data->horiz_spacing + col_width;

            ++j;
            if ((j % data->columns) == 0)
                break;
        }

        top += data->vert_spacing + row_height;
    }

}


/*
 * all children in the same row have the same maximum height
 * all children in the same column have the same maximum height
 * if a child maximum size is smaller than the biggest minimum size,
 * the chid will be centered in the remaining space.
 *
 * for each row, determine its height allocation
 * weight ? the vertical weight of a row, if no fixed-height child
 *  in the row, is the sum of all vertical weights of children
 * all row members will have the same height
 *
 * for each column, determine its width allocation
 * all column members will have the same width
 */
/* Write a proper hook function */
static void
group_layout_2d(struct IClass *cl, Object *obj, struct MinList *children)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    WORD left_start = 0;
    WORD top_start = 0;
    WORD total_size_height =
        _mheight(obj) - (data->rows - 1) * data->vert_spacing;
    WORD total_size_width =
        _mwidth(obj) - (data->columns - 1) * data->horiz_spacing;
    WORD total_init_height;
    WORD total_init_width;
    WORD remainder_height;
    WORD remainder_width;
    WORD layout_width;
    WORD layout_height;

    if (data->rows == 0 || data->columns == 0)
        return;
    if (data->num_children % data->rows
        || data->num_children % data->columns)
        return;
    if (data->row_infos == NULL || data->col_infos == NULL)
        return;

    //kprintf("group_layout_horiz: virtoff = %d,%d\n",
    //    data->virt_offx, data->virt_offy);

    if (data->flags & GROUP_VIRTUAL)
    {
        data->virt_mwidth =
            CLAMP(_mwidth(obj), data->saved_minwidth - _subwidth(obj),
            _maxwidth(obj) - _subwidth(obj));
        data->virt_mheight =
            CLAMP(_mheight(obj), data->saved_minheight - _subheight(obj),
            _maxheight(obj) - _subheight(obj));

        layout_width = data->virt_mwidth;
        layout_height = data->virt_mheight;
    }
    else
    {
        layout_width = _mwidth(obj);
        layout_height = _mheight(obj);
    }

    total_size_height =
        layout_height - (data->rows - 1) * data->vert_spacing;
    total_size_width =
        layout_width - (data->columns - 1) * data->horiz_spacing;

    // fix left/top ?

    // weight constraints
    Layout2D_weight_constraint(data, data->row_infos, data->col_infos,
        total_size_height, total_size_width,
        &total_init_height, &total_init_width);

    remainder_height = total_size_height - total_init_height;
    remainder_width = total_size_width - total_init_width;

    Layout2D_minmax_constraints_and_redistrib(data->row_infos,
        data->rows, total_size_height,
/*      (data->flags & GROUP_SAME_HEIGHT) ? data->samesize_maxmin_vert : 0, */
        0, remainder_height);

    Layout2D_minmax_constraints_and_redistrib(data->col_infos,
        data->columns, total_size_width,
/*      (data->flags & GROUP_SAME_WIDTH) ? data->samesize_maxmin_horiz : 0, */
        0, remainder_width);

    layout_2d_distribute_space(data, data->row_infos, data->col_infos,
        children, left_start, top_start);
}


/* Write a proper hook function */
static void group_layout_pagemode(struct IClass *cl, Object *obj,
    struct MinList *children)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    WORD layout_width;
    WORD layout_height;
    int w, h, yoffset = 0;

    if (data->flags & GROUP_VIRTUAL)
    {
        data->virt_mwidth =
            CLAMP(_mwidth(obj), data->saved_minwidth - _subwidth(obj),
            _maxwidth(obj) - _subwidth(obj));
        data->virt_mheight =
            CLAMP(_mheight(obj), data->saved_minheight - _subheight(obj),
            _maxheight(obj) - _subheight(obj));

        layout_width = data->virt_mwidth;
        layout_height = data->virt_mheight;
    }
    else
    {
        layout_width = _mwidth(obj);
        layout_height = _mheight(obj);
    }

    if (data->titlegroup)
    {
        yoffset = _minheight(data->titlegroup);
        layout_height -= yoffset;
    }

    cstate = children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        w = MIN(layout_width, _maxwidth(child));
        h = MIN(layout_height, _maxheight(child));

        if (child == data->titlegroup)
        {
            IPTR position = 0;
            get(child, MUIA_Title_Position, &position);
            MUI_Layout(child, (layout_width - w) / 2,
                position == MUIV_Title_Position_Bottom ? layout_height : 0,
                w, yoffset, 0);
        }
        else
        {
            D(bug("PM/child %p -> layout %d x %d\n", child, w, h));
            MUI_Layout(child, (layout_width - w) / 2,
                (data->titlegroup && XGET(data->titlegroup, MUIA_Title_Position) ==
                    MUIV_Title_Position_Bottom ? 0 : yoffset) +
                (layout_height - h) / 2, w, h, 0);
        }
    }
}


/**************************************************************************
 MUIM_Zune_Layout
 Either use a given layout hook, or the builtin method.
**************************************************************************/
IPTR Group__MUIM_Layout(struct IClass *cl, Object *obj,
    struct MUIP_Zune_Layout *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    struct MUI_LayoutMsg lm = { 0 };
    IPTR result = TRUE;

    get(data->family, MUIA_Family_List, &(lm.lm_Children));
    if (data->flags & GROUP_PAGEMODE)
    {
        group_layout_pagemode(cl, obj, lm.lm_Children);
    }
    else if (data->layout_hook)
    {
        lm.lm_Type = MUILM_LAYOUT;
        lm.lm_Layout.Width = _mwidth(obj);
        lm.lm_Layout.Height = _mheight(obj);

        result = CallHookPkt(data->layout_hook, obj, &lm);

        if (data->flags & GROUP_VIRTUAL)
        {
            /* A layout hook may need less space than the viewport. Keep
             * the virtual extent large enough to cover its visible area. */
            data->virt_mwidth = MAX(lm.lm_Layout.Width, _mwidth(obj));
            data->virt_mheight = MAX(lm.lm_Layout.Height, _mheight(obj));
        }
    }
    else
    {
        if ((data->rows == 1) && (data->columns == 1))
        {
            if (data->flags & GROUP_HORIZ)
                group_layout_horiz(cl, obj, lm.lm_Children);
            else
                group_layout_vert(cl, obj, lm.lm_Children);
        }
        else
            group_layout_2d(cl, obj, lm.lm_Children);
    }

    if (data->flags & GROUP_VIRTUAL)
    {
        WORD new_virt_offx, new_virt_offy;

        new_virt_offx = data->virt_offx;
        new_virt_offy = data->virt_offy;

        if (new_virt_offx + _mwidth(obj) > data->virt_mwidth)
        {
            new_virt_offx = data->virt_mwidth - _mwidth(obj);
        }
        if (new_virt_offx < 0)
            new_virt_offx = 0;

        if (new_virt_offy + _mheight(obj) > data->virt_mheight)
        {
            new_virt_offy = data->virt_mheight - _mheight(obj);
        }
        if (new_virt_offy < 0)
            new_virt_offy = 0;

        if (new_virt_offx != data->virt_offx)
        {
            nfset(obj, MUIA_Virtgroup_Left, new_virt_offx);
        }

        if (new_virt_offy != data->virt_offy)
        {
            nfset(obj, MUIA_Virtgroup_Top, new_virt_offy);
        }

        /* A relayout of the virtual group alone (a layout hook reporting
         * a new page size) must still reach a Scrollgroup's scrollers. */
        if (data->virt_mwidth != data->told_mwidth ||
            data->virt_mheight != data->told_mheight)
        {
            data->told_mwidth = data->virt_mwidth;
            data->told_mheight = data->virt_mheight;
            SetAttrs(obj, MUIA_Group_Forward, FALSE,
                MUIA_Virtgroup_Width, data->virt_mwidth,
                MUIA_Virtgroup_Height, data->virt_mheight, TAG_DONE);
        }
    }

    return result;
}

/**************************************************************************
 MUIM_Show
**************************************************************************/
IPTR Group__MUIM_Show(struct IClass *cl, Object *obj,
    struct MUIP_Show *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    /* If msg is NULL, we won't want that the super method actually gets
     * this call */
    if (msg)
        DoSuperMethodA(cl, obj, (Msg) msg);

    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;

    if (data->flags & GROUP_PAGEMODE)
    {
        int page = 0;
        while ((child = NextObject(&cstate)))
        {
            if (child == data->titlegroup)
            {
                DoShowMethod(child);
                continue;       /* Title group is not counted as page */
            }

            if (page == data->active_page)
            {
                DoShowMethod(child);
                break;
            }
            page++;
        }
    }
    else
    {
        while ((child = NextObject(&cstate)))
        {
            if (!(data->flags & GROUP_VIRTUAL) ||
                IsObjectVisible(child, MUIMasterBase))
            {
                if ((_flags(child) & MADF_SHOWME)
                    && !(_flags(child) & MADF_DISAPPEARED))
                    DoShowMethod(child);
            }
        }
    }
    return TRUE;
}

/**************************************************************************
 MUIM_Hide
**************************************************************************/
IPTR Group__MUIM_Hide(struct IClass *cl, Object *obj,
    struct MUIP_Hide *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;

    if (data->flags & GROUP_PAGEMODE)
    {
        int page = 0;
        while ((child = NextObject(&cstate)))
        {
            if (child == data->titlegroup)
            {
                DoHideMethod(child);
                continue;       /* Title group is not counted as page */
            }

            if (page == data->active_page)
            {
                DoHideMethod(child);
                break;
            }
            page++;
        }
    }
    else
    {
        while ((child = NextObject(&cstate)))
        {
            if (_flags(child) & MADF_CANDRAW)
                DoHideMethod(child);
        }
    }

    /* If msg is NULL, we won't want that the super method actually gets
     * this call */
    if (msg)
        return DoSuperMethodA(cl, obj, (Msg) msg);
    return 1;
}

/*
 * MUIM_FindUData : tests if the MUIA_UserData of the object
 * contains the given <udata> and returns the object pointer in this case.
 */
IPTR Group__MUIM_FindUData(struct IClass *cl, Object *obj,
    struct MUIP_FindUData *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
        return (IPTR) obj;

    return DoMethodA(data->family, (Msg) msg);
}


/*
 * MUIM_GetUData : This method tests if the MUIA_UserData of the object
 * contains the given <udata> and gets <attr> to <storage> for itself
 * in this case.
 */
IPTR Group__MUIM_GetUData(struct IClass *cl, Object *obj,
    struct MUIP_GetUData *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
    {
        get(obj, msg->attr, msg->storage);
        return TRUE;
    }

    return DoMethodA(data->family, (Msg) msg);
}


/*
 * MUIM_SetUData : This method tests if the MUIA_UserData of the object
 * contains the given <udata> and sets <attr> to <val> for itself in this case.
 */
IPTR Group__MUIM_SetUData(struct IClass *cl, Object *obj,
    struct MUIP_SetUData *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
        set(obj, msg->attr, msg->val);

    DoMethodA(data->family, (Msg) msg);
    return TRUE;
}


/*
 * MUIM_SetUDataOnce : This method tests if the MUIA_UserData of the object
 * contains the given <udata> and sets <attr> to <val> for itself in this case.
 * Stop after the first udata found.
 */
IPTR Group__MUIM_SetUDataOnce(struct IClass *cl, Object *obj,
    struct MUIP_SetUData *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
    {
        set(obj, msg->attr, msg->val);
        return TRUE;
    }
    return DoMethodA(data->family, (Msg) msg);
}

/**************************************************************************
 MUIM_DragQueryExtented
**************************************************************************/
IPTR Group__MUIM_DragQueryExtended(struct IClass *cl, Object *obj,
    struct MUIP_DragQueryExtended *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    Object *found_obj;
    struct MinList *ChildList = NULL;

    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (!(_flags(child) & MADF_CANDRAW))
            continue;

        if ((found_obj = (Object *) DoMethodA(child, (Msg) msg)))
            return (IPTR) found_obj;
    }
    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_HandleEvent
**************************************************************************/
IPTR Group__MUIM_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    /* check this, otherwise a superclass who has IDCMP_MOUSEBUTTONS
       eventhandler might call DoSuperMethod, and this function gets
       called even when he have not added any eventhandler */

    if ((data->flags & GROUP_VIRTUAL) && msg->imsg)
    {
        switch (msg->imsg->Class)
        {
        case IDCMP_MOUSEBUTTONS:
            /* For virtual groups */
            if (msg->imsg->Code == SELECTDOWN)
            {
                if (_between(_mleft(obj), msg->imsg->MouseX, _mright(obj))
                    && _between(_mtop(obj), msg->imsg->MouseY,
                        _mbottom(obj)))
                {
                    DoMethod(_win(obj), MUIM_Window_RemEventHandler,
                        (IPTR) & data->ehn);
                    data->ehn.ehn_Events |= IDCMP_INTUITICKS;
                    DoMethod(_win(obj), MUIM_Window_AddEventHandler,
                        (IPTR) & data->ehn);
                }
            }
            else
            {
                if (data->ehn.ehn_Events & IDCMP_INTUITICKS)
                {
                    DoMethod(_win(obj), MUIM_Window_RemEventHandler,
                        (IPTR) & data->ehn);
                    data->ehn.ehn_Events &= ~IDCMP_INTUITICKS;
                    DoMethod(_win(obj), MUIM_Window_AddEventHandler,
                        (IPTR) & data->ehn);
                }
            }
            break;

        case IDCMP_INTUITICKS:
            if (!(data->ehn.ehn_Events & IDCMP_INTUITICKS))
                break;

            if (!(_between(_mleft(obj), msg->imsg->MouseX, _mright(obj))
                    && _between(_mtop(obj), msg->imsg->MouseY,
                        _mbottom(obj))))
            {
                LONG new_virt_offx = data->virt_offx;
                LONG new_virt_offy = data->virt_offy;

                if (msg->imsg->MouseX < _mleft(obj))
                {
                    /* scroll left */
                    if (new_virt_offx >= 4)
                        new_virt_offx -= 4;
                    else
                        new_virt_offx = 0;
                }
                else if (msg->imsg->MouseX > _mright(obj))
                {
                    /* scroll right */
                    new_virt_offx += 4;
                    if (new_virt_offx > data->virt_mwidth - _mwidth(obj))
                        new_virt_offx = data->virt_mwidth - _mwidth(obj);
                    if (new_virt_offx < 0)
                        new_virt_offx = 0;
                }

                if (msg->imsg->MouseY < _mtop(obj))
                {
                    /* scroll top */
                    if (new_virt_offy >= 4)
                        new_virt_offy -= 4;
                    else
                        new_virt_offy = 0;
                }
                else if (msg->imsg->MouseY > _mbottom(obj))
                {
                    /* scroll bottom */
                    new_virt_offy += 4;
                    if (new_virt_offy > data->virt_mheight - _mheight(obj))
                        new_virt_offy = data->virt_mheight - _mheight(obj);
                    if (new_virt_offy < 0)
                        new_virt_offy = 0;
                }

                if (new_virt_offx != data->virt_offx
                    || new_virt_offy != data->virt_offy)
                {
                    SetAttrs(obj,
                        MUIA_Virtgroup_Left, new_virt_offx,
                        MUIA_Virtgroup_Top, new_virt_offy,
                        MUIA_Group_Forward, FALSE, TAG_DONE);
                }
            }
            break;
        }
    }

    return 0;
}

/**************************************************************************
 MUIM_DrawBackground
**************************************************************************/
IPTR Group__MUIM_DrawBackground(struct IClass *cl, Object *obj,
    struct MUIP_DrawBackground *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);

    if (data->flags & GROUP_VIRTUAL)
    {
        struct MUIP_DrawBackground msg2 = *msg;

        msg2.xoffset += data->virt_offx;
        msg2.yoffset += data->virt_offy;

        return DoSuperMethodA(cl, obj, (Msg) & msg2);
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/* TRUE when obj is a plain built-in Group whose MUIM_DrawBackground would
 * only hand the request on to its parent. Objects inheriting a background
 * skip such levels instead of making two method calls per level. */
BOOL Group_PassesBackground(Object *obj)
{
    struct IClass *cl = OCLASS(obj);
    struct MUI_AreaData *ad;

#if defined(__MAXON__) || defined(__amigaos4__)
    if (cl->cl_Dispatcher.h_Entry != (HOOKFUNC)_MUI_Group_desc.dispatcher)
#else
    if (cl->cl_Dispatcher.h_SubEntry != (HOOKFUNC)_MUI_Group_desc.dispatcher)
#endif
        return FALSE;
    ad = muiAreaData(obj);
    return (BOOL)(!(((struct MUI_GroupData *)INST_DATA(cl, obj))->flags
        & GROUP_VIRTUAL) && (ad->mad_Flags & MADF_CANDRAW) &&
        !ad->mad_Background && !ad->mad_SelBack &&
        !(ad->mad_Flags2 & MADF2_CUSTOMBACKFILL));
}

/**************************************************************************
 MUIM_FindAreaObject
 Find the given object or return NULL
**************************************************************************/
IPTR Group__MUIM_FindAreaObject(struct IClass *cl, Object *obj,
    struct MUIP_FindAreaObject *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    // it's me ?
    if (msg->obj == obj)
        return (IPTR) obj;

    // it's one of my children ?
    get(data->family, MUIA_Family_List, &(ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (msg->obj == child)
            return (IPTR) child;
    }

    // let the children find it
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        Object *res = (Object *) DoMethodA(child, (Msg) msg);
        if (res != NULL)
            return (IPTR) res;
    }

    return (IPTR) NULL;
}

/**************************************************************************
MUIM_Export : to export an object's "contents" to a dataspace object.
**************************************************************************/
static IPTR Group__MUIM_Export(struct IClass *cl, Object *obj,
    struct MUIP_Export *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    get(data->family, MUIA_Family_List, &(ChildList));
    if (!ChildList)
        return 0;

    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        DoMethodA(child, (Msg) msg);
    }

    return 0;
}


/**************************************************************************
MUIM_Import : to import an object's "contents" from a dataspace object.
**************************************************************************/
static IPTR Group__MUIM_Import(struct IClass *cl, Object *obj,
    struct MUIP_Import *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    get(data->family, MUIA_Family_List, &(ChildList));
    if (!ChildList)
        return 0;

    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        DoMethodA(child, (Msg) msg);
    }

    return 0;
}

/**************************************************************************
 MUIM_Notify - disabled now because previous Zune versions had a OM_GET
 check in MUIM_Notify which is no longer the case
**************************************************************************/
#if 0
STATIC IPTR Group_Notify(struct IClass *cl, Object *obj,
    struct MUIP_Notify *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList;

    /* Try at first if understand the message our self
     ** We disable the forwarding of the OM_GET message
     ** as the MUIM_Notify otherwise would "think" that
     ** the group class actually understands the attribute
     ** although a child does this only
     */
    data->dont_forward_get = 1;
    if (DoSuperMethodA(cl, obj, (Msg) msg))
    {
        data->dont_forward_get = 0;
        return 1;
    }

    /* We ourselves didn't understand the notify tag so we try the
     * children now */
    data->dont_forward_get = 0;

    get(data->family, MUIA_Family_List, (IPTR *) & (ChildList));
    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (DoMethodA(child, (Msg) msg))
            return 1;
    }
    return 0;
}
#endif

#if 1
/* Notes about Group_Notify() and echo notification problem:
It was discovered that MUI seems to have some special handling for group class
which will drop notifications on the children which are found to not
understand the attribute.

This is done by checking if an OM_GET on the child returns TRUE.
There's a little problem here because it is not known how big the storage
needed for the attribute in question will be. Almost no class uses anything
bigger than one IPTR. For "big" attributes those return a pointer to the data,
not the data itself. Unfortuntely there are some exceptions like colorwheel
class which does not return a pointer, but the data itself. So it's not
enough to use one single IPTR variable (4 Bytes on 32bit machines, 8 bytes
on 64 bit machines) to store the result of the test-OM_Get.

There is no general way to query the size needed so if one wants to change
Zune to work like MUI one needs to choose a size which one hopes will be
big enough to hold all possible attributes of all classes, old, present
and future ones.
*/
STATIC IPTR Group_Notify(struct IClass *cl, Object *obj,
    struct MUIP_Notify *msg)
{
    struct MUI_GroupData *data = INST_DATA(cl, obj);
    APTR cstate;
    Object *child;
    struct MinList *ChildList = NULL;
    IPTR attr[30];

    data->dont_forward_get = 1;

    if (GetAttr(msg->TrigAttr, obj, attr))
    {
        data->dont_forward_get = 0;
        return DoSuperMethodA(cl, obj, (Msg) msg);
    }
    data->dont_forward_get = 0;

    get(data->family, MUIA_Family_List, &(ChildList));
    if (!ChildList)
        return TRUE;

    cstate = ChildList->mlh_Head;
    while ((child = NextObject(&cstate)))
    {

        if (GetAttr(msg->TrigAttr, child, attr))
        {
            DoMethodA(child, (Msg) msg);
            /* No return here! */
        }
    }
    return TRUE;
}
#endif

BOOPSI_DISPATCHER(IPTR, Group_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Group__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return Group__OM_DISPOSE(cl, obj, msg);
    case OM_SET:
        return Group__OM_SET(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return Group__OM_GET(cl, obj, (struct opGet *)msg);
    case OM_ADDMEMBER:         /* Fall through */
    case MUIM_Group_AddTail:
        return Group__MUIM_AddTail(cl, obj, (APTR) msg);
    case MUIM_Group_AddHead:
        return Group__MUIM_AddHead(cl, obj, (APTR) msg);
    case MUIM_Group_Insert:
        return Group__MUIM_Insert(cl, obj, (APTR) msg);
    case OM_REMMEMBER:         /* Fall through */
    case MUIM_Group_Remove:
        return Group__MUIM_Remove(cl, obj, (APTR) msg);
    case MUIM_Family_GetChild:
        return Group__MUIM_Family_GetChild(cl, obj, (APTR) msg);
    case MUIM_AskMinMax:
        return Group__MUIM_AskMinMax(cl, obj, (APTR) msg);
    case MUIM_Group_ExitChange:
        return Group__MUIM_ExitChange(cl, obj, (APTR) msg);
    case MUIM_Group_ExitChange2:
        return Group__MUIM_ExitChange2(cl, obj, (APTR) msg);
    case MUIM_Group_InitChange:
        return Group__MUIM_InitChange(cl, obj, (APTR) msg);
    case MUIM_Group_MoveMember:
        return GroupMoveMember(cl, obj, (APTR)msg);
    case MUIM_Group_Reorder:
        return GroupReorder(cl, obj, (APTR)msg);
    case MUIM_Group_Sort:
        return Group__MUIM_Sort(cl, obj, (APTR) msg);
    case MUIM_Group_DoMethodNoForward:
        return Group__MUIM_DoMethodNoForward(cl, obj, (APTR) msg);
    case MUIM_ConnectParent:
        return Group__MUIM_ConnectParent(cl, obj, (APTR) msg);
    case MUIM_GlobalSetup:
        return Group__MUIM_GlobalSetup(cl, obj, (APTR) msg);
    case MUIM_DisconnectParent:
        return Group__MUIM_DisconnectParent(cl, obj, (APTR) msg);
    case MUIM_Zune_Layout:
        return Group__MUIM_Layout(cl, obj, (APTR) msg);
    case MUIM_Setup:
        return Group__MUIM_Setup(cl, obj, (APTR) msg);
    case MUIM_Cleanup:
        return Group__MUIM_Cleanup(cl, obj, (APTR) msg);
    case MUIM_Virtgroup_BeginPaint:
        return Group__MUIM_Virtgroup_BeginPaint(cl, obj);
    case MUIM_Virtgroup_EndPaint:
        return Group__MUIM_Virtgroup_EndPaint(cl, obj);
    case MUIM_Draw:
        return Group__MUIM_Draw(cl, obj, (APTR) msg);

    case MUIM_FindUData:
        return Group__MUIM_FindUData(cl, obj, (APTR) msg);
    case MUIM_GetUData:
        return Group__MUIM_GetUData(cl, obj, (APTR) msg);
    case MUIM_SetUData:
        return Group__MUIM_SetUData(cl, obj, (APTR) msg);
    case MUIM_SetUDataOnce:
        return Group__MUIM_SetUDataOnce(cl, obj, (APTR) msg);
    case MUIM_Show:
        return Group__MUIM_Show(cl, obj, (APTR) msg);
    case MUIM_Hide:
        return Group__MUIM_Hide(cl, obj, (APTR) msg);
    case MUIM_HandleEvent:
        return Group__MUIM_HandleEvent(cl, obj, (APTR) msg);
    case MUIM_DrawBackground:
        return Group__MUIM_DrawBackground(cl, obj, (APTR) msg);
    case MUIM_DragQueryExtended:
        return Group__MUIM_DragQueryExtended(cl, obj, (APTR) msg);
    case MUIM_FindAreaObject:
        return Group__MUIM_FindAreaObject(cl, obj, (APTR) msg);
    case MUIM_Export:
        return Group__MUIM_Export(cl, obj, (APTR) msg);
    case MUIM_Import:
        return Group__MUIM_Import(cl, obj, (APTR) msg);

//#if 0
#if 1
        /* Disabled. See above */
    case MUIM_Notify:
        return Group_Notify(cl, obj, (APTR) msg);
#endif
    /* Geometry belongs to this group, not to every child. */
    case MUIM_Layout:
    case MUIM_Set:
    case MUIM_MultiSet:
    case MUIM_CallHook:
    case MUIM_DrawParentBackground:
    case MUIM_DragBegin:
    case MUIM_DragDrop:
    case MUIM_DragQuery:
    case MUIM_DragFinish:
    case MUIM_DoDrag:
    case MUIM_CreateDragImage:
    case MUIM_DeleteDragImage:
    case MUIM_GoActive:
    case MUIM_GoInactive:
    case MUIM_CreateBubble:
    case MUIM_DeleteBubble:
    case MUIM_CreateShortHelp:
    case MUIM_DeleteShortHelp:
    case OM_ADDTAIL:
    case OM_REMOVE:
        return DoSuperMethodA(cl, obj, (APTR) msg);
            /* Needs not to be forwarded? */
    }

    /* sometimes you want to call a superclass method,
     * but not dispatching to child.
     * But what to do with list methods in a listview ?
     */
    Group_DispatchMsg(cl, obj, (APTR) msg);

    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_Group_desc =
{
    MUIC_Group,
    MUIC_Area,
    sizeof(struct MUI_GroupData),
    (void *) Group_Dispatcher
};

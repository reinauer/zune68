/*
    Copyright (C) 2002-2003, The AROS Development Team. All rights reserved.
*/

#define MUIMASTER_YES_INLINE_STDARG

#include <exec/memory.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "support_classes.h"

extern struct Library *MUIMasterBase;

/* Classic MUI's private protocol for custom scroll containers. */
#define MUIA_Virtgroup_ScrollParent 0x80420e88UL
#define MUIM_Scrollgroup_Inform    0x804218a6UL

struct Virtgroup_DATA
{
    Object *scroll_parent;
};

static IPTR Virtgroup_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Virtgroup_DATA *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList, *tag;

    while ((tag = NextTagItem(&tags)))
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Virtgroup_ScrollParent:
            data->scroll_parent = (Object *)tag->ti_Data;
            /* Registration belongs to this group, never its children. */
            tag->ti_Tag = TAG_IGNORE;
            break;
        }
    }

    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/* Keep the notification message off the ordinary forwarding path's stack. */
#ifdef ZUNE68_GCC_NATIVE
static IPTR __attribute__((noinline))
#else
static IPTR
#endif
Virtgroup_Layout(struct IClass *cl, Object *obj, Msg msg)
{
    struct Virtgroup_DATA *data = INST_DATA(cl, obj);
    IPTR result = DoSuperMethodA(cl, obj, msg);

    if (data->scroll_parent)
    {
        struct
        {
            ULONG MethodID;
            IPTR left, top, visible_width, visible_height;
            IPTR virtual_width, virtual_height;
        } inform = { MUIM_Scrollgroup_Inform, 0, 0, 0, 0, 0, 0 };

        get(obj, MUIA_Virtgroup_Left, &inform.left);
        get(obj, MUIA_Virtgroup_Top, &inform.top);
        inform.visible_width = _mwidth(obj);
        inform.visible_height = _mheight(obj);
        get(obj, MUIA_Virtgroup_Width, &inform.virtual_width);
        get(obj, MUIA_Virtgroup_Height, &inform.virtual_height);
        DoMethodA(data->scroll_parent, (Msg)&inform);
    }

    return result;
}

IPTR Virtgroup__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    return (IPTR)DoSuperNewTags
        (cl, obj, NULL,
        MUIA_Group_Virtual, TRUE, TAG_MORE, (IPTR) msg->ops_AttrList);
}

#if ZUNE_BUILTIN_VIRTGROUP
BOOPSI_DISPATCHER(IPTR, Virtgroup_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Virtgroup__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_SET:
        return Virtgroup_Set(cl, obj, (struct opSet *)msg);
    case MUIM_Layout:
        return Virtgroup_Layout(cl, obj, msg);
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Virtgroup_desc =
{
    MUIC_Virtgroup,
    MUIC_Group,
    sizeof(struct Virtgroup_DATA),
    (void *) Virtgroup_Dispatcher
};
#endif /* ZUNE_BUILTIN_VIRTGROUP */

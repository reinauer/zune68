#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <string.h>

#include "mui.h"
#include "muimaster_intern.h"

static BOOL is_group(Object *obj)
{
    struct IClass *cl = OCLASS(obj);

    while (cl)
    {
        if (cl->cl_ID && strcmp(cl->cl_ID, MUIC_Group) == 0)
            return TRUE;
        cl = cl->cl_Super;
    }
    return FALSE;
}

static void offset_object(Object *obj, LONG x, LONG y)
{
    struct MinList *children = NULL;
    APTR state;
    Object *child;

    _left(obj) += x;
    _top(obj) += y;
    muiAreaData(obj)->mad_VirtualTop += y;

    if (!is_group(obj))
        return;

    get(obj, MUIA_Group_ChildList, &children);
    if (!children)
        return;
    state = children->mlh_Head;
    while ((child = NextObject(&state)))
        offset_object(child, x, y);
}

AROS_LH3(VOID, MUI_Offset,
    AROS_LHA(Object *, obj, A0),
    AROS_LHA(LONG, x, D0),
    AROS_LHA(LONG, y, D1),
    struct Library *, MUIMasterBase, 39, MUIMaster)
{
    AROS_LIBFUNC_INIT

    if (obj && (x || y))
        offset_object(obj, x, y);

    AROS_LIBFUNC_EXIT
}

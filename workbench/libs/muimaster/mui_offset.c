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

/* Private in MUI, and called by Voyager's Show method. Fills left, top,
 * right, bottom with what the virtual groups around obj leave visible. */
void MUIP_GetVirtualRect(Object *obj __asm("a0"), LONG *rect __asm("a1"))
{
    rect[0] = rect[1] = 0;
    rect[2] = rect[3] = 0x40000000;

    while (obj)
    {
        struct MUI_AreaData *area = muiAreaData(obj);

        if (area->mad_Flags & MADF_ISVIRTUALGROUP)
        {
            LONG left = area->mad_Box.Left + area->mad_addleft;
            LONG top = area->mad_VirtualTop + area->mad_addtop;
            LONG right = left + area->mad_Box.Width - area->mad_subwidth - 1;
            LONG bottom = top + area->mad_Box.Height - area->mad_subheight - 1;

            if (left > rect[0]) rect[0] = left;
            if (top > rect[1]) rect[1] = top;
            if (right < rect[2]) rect[2] = right;
            if (bottom < rect[3]) rect[3] = bottom;
        }
        if (!(area->mad_Flags & MADF_INVIRTUALGROUP))
            break;
        obj = _parent(obj);
    }
}

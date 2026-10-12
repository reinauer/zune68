#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include "mui.h"

AROS_LH6(BOOL, MUI_LayoutObj,
    AROS_LHA(Object *, obj, A0),
    AROS_LHA(LONG, left, D0),
    AROS_LHA(LONG, top, D1),
    AROS_LHA(LONG, width, D2),
    AROS_LHA(LONG, height, D3),
    AROS_LHA(ULONG, flags, D4),
    struct Library *, MUIMasterBase, 38, MUIMaster)
{
    AROS_LIBFUNC_INIT

    static const struct MUIP_Layout method = { MUIM_Layout };

    /* Unlike MUI_Layout, these coordinates are already window-relative. */
    _left(obj) = left;
    _top(obj) = top;
    muiAreaData(obj)->mad_VirtualTop = top;
    _width(obj) = width;
    _height(obj) = height;

    /* MUI keeps the requested box even when its size is unacceptable. */
    if (width < _minwidth(obj) || width > _maxwidth(obj) ||
        height < _minheight(obj) || height > _maxheight(obj))
        return FALSE;

    return DoMethodA(obj, (Msg)&method);

    AROS_LIBFUNC_EXIT
}

#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include "mui.h"
#include "support.h"
#include "support_classes.h"

#include "layout_intern.h"

IPTR ZuneLayout(Object *obj, LONG left, LONG top, LONG width, LONG height,
    ULONG flags)
{
    return LayoutBuiltin(obj, left, top, width, height, flags);
}

IPTR ZuneLayoutObject(Object *obj, LONG left, LONG top, LONG width,
    LONG height, ULONG flags)
{
    return ZuneLayoutInline(obj, left, top, width, height, flags);
}

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
    return ZuneLayoutInline(obj, left, top, width, height, flags);
    AROS_LIBFUNC_EXIT
}

IPTR DoLayoutMethod(Object *obj)
{
    /* Internal callers already installed the window-relative box. Built-in
     * groups inherit Area's public wrapper unchanged; enter their private
     * layout method directly. Custom groups must see public dispatch. */
    if (NeedsPublicLayout(obj))
    {
        struct MUIP_Layout method = {
            MUIM_Layout, _left(obj), _top(obj), _width(obj), _height(obj), 0
        };
        return DoMethodA(obj, (Msg)&method) != 0;
    }
    muiAreaData(obj)->mad_VirtualTop = _top(obj);
    if (_width(obj) < _minwidth(obj) || _width(obj) > _maxwidth(obj) ||
        _height(obj) < _minheight(obj) || _height(obj) > _maxheight(obj))
        return FALSE;
    static const struct MUIP_Zune_Layout method = {MUIM_Zune_Layout};
    return DoMethodA(obj, (Msg)&method);
}

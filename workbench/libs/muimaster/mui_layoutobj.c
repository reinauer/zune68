#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include "mui.h"
#include "support.h"
#include "support_classes.h"

/* Public methods enter here after subclass dispatch, so the old callback
 * can run without re-entering the public method on the same object. */
IPTR ZuneLayout(Object *obj, LONG left, LONG top, LONG width, LONG height,
    ULONG flags)
{
    static const struct MUIP_Zune_Layout method = { MUIM_Zune_Layout };

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

    if (muiAreaData(obj)->mad_Flags2 & MADF2_ISGROUP)
    {
        struct MUIP_Layout method = {
            MUIM_Layout, left, top, width, height, flags
        };

        return DoMethodA(obj, (Msg)&method) != 0;
    }

    return ZuneLayout(obj, left, top, width, height, flags);

    AROS_LIBFUNC_EXIT
}

IPTR DoLayoutMethod(Object *obj)
{
    /* Internal callers already installed the window-relative box. Built-in
     * groups inherit Area's public wrapper unchanged; enter their private
     * layout method directly. Custom groups must see public dispatch. */
    if ((muiAreaData(obj)->mad_Flags2 & MADF2_ISGROUP)
#if !defined(__AROS__) && !defined(__amigaos4__) && !defined(__MAXON__)
        && OCLASS(obj)->cl_Dispatcher.h_Entry != (HOOKFUNC)builtinDispatcher
#endif
        )
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

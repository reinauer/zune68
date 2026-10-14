/* Shared layout checks for relative and absolute library entry points. */
#ifndef ZUNE_LAYOUT_INTERN_H
#define ZUNE_LAYOUT_INTERN_H
#include "support_classes.h"

static inline __attribute__((always_inline)) IPTR LayoutBuiltin(
    Object *obj, LONG left, LONG top, LONG width, LONG height,
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

static inline BOOL NeedsPublicLayout(Object *obj)
{
    return (muiAreaData(obj)->mad_Flags2 & MADF2_ISGROUP)
#if !defined(__AROS__) && !defined(__amigaos4__) && !defined(__MAXON__)
        && OCLASS(obj)->cl_Dispatcher.h_Entry != (HOOKFUNC)builtinDispatcher
#endif
        ;
}

static inline __attribute__((always_inline)) IPTR ZuneLayoutInline(
    Object *obj, LONG left, LONG top, LONG width, LONG height, ULONG flags)
{
    if (NeedsPublicLayout(obj))
    {
        struct MUIP_Layout method = {
            MUIM_Layout, left, top, width, height, flags
        };

        return DoMethodA(obj, (Msg)&method) != 0;
    }

    return LayoutBuiltin(obj, left, top, width, height, flags);
}

#endif

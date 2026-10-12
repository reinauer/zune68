#include <proto/muimaster.h>

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

    return MUI_Layout(obj, left, top, width, height, flags);

    AROS_LIBFUNC_EXIT
}

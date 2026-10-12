#include <clib/alib_protos.h>
#include <proto/muimaster.h>

#include "mui.h"

AROS_LH1(ULONG, MUI_Show,
    AROS_LHA(Object *, obj, A0),
    struct Library *, MUIMasterBase, 36, MUIMaster)
{
    AROS_LIBFUNC_INIT

    /* MUI 5 only dispatches Show after the area has been set up. */
    if (!obj || !(_flags(obj) & MADF_SETUP))
        return FALSE;

    return DoMethod(obj, MUIM_Show) != 0;

    AROS_LIBFUNC_EXIT
}

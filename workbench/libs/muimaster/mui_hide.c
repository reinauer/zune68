#include <clib/alib_protos.h>
#include <proto/muimaster.h>

#include "mui.h"

AROS_LH1(ULONG, MUI_Hide,
    AROS_LHA(Object *, obj, A0),
    struct Library *, MUIMasterBase, 37, MUIMaster)
{
    AROS_LIBFUNC_INIT

    /* The library call returns zero even when MUIM_Hide succeeds. */
    if (obj && (_flags(obj) & MADF_SETUP))
        DoMethod(obj, MUIM_Hide);

    return FALSE;

    AROS_LIBFUNC_EXIT
}

/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
*/

#include <proto/muimaster.h>
#include "muimaster_intern.h"

/*****************************************************************************

    NAME */
        AROS_LH1(LONG, MUI_SetError,

/*  SYNOPSIS */
        AROS_LHA(LONG, num, D0),

/*  LOCATION */
        struct Library *, MUIMasterBase, 12, MUIMaster)

/*  FUNCTION
        Set the calling task's MUI_Error() value. The process's DOS error
        value is not changed.

    INPUTS
        num - A MUIE_* code; MUIE_OK clears it.

    RESULT
        Always zero. This is not the previous error value.

    NOTES
        MUI 3.8 declares the function VOID and ignores num.

    SEE ALSO
        MUI_Error(), dos.library/SetIoErr()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    zune_set_error(num);
    return 0;

    AROS_LIBFUNC_EXIT

} /* MUIA_SetError */

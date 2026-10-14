/*
    Copyright (C) 2002-2007, The AROS Development Team. All rights reserved.
*/

#include <proto/muimaster.h>
#include <proto/asl.h>

#include "muimaster_intern.h"
#ifdef ZUNE68_GCC_NATIVE
#include "requesters.h"
#endif

/*****************************************************************************

    NAME */
        AROS_LH1(VOID, MUI_FreeAslRequest,

/*  SYNOPSIS */
        AROS_LHA(APTR, requester, A0),

/*  LOCATION */
        struct Library *, MUIMasterBase, 10, MUIMaster)

/*  FUNCTION
        Interface to asl.library

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        asl.library/FreeAslRequest()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

#ifdef ZUNE68_GCC_NATIVE
    ZuneFreeAslRequest(requester);
#else
    FreeAslRequest(requester);
#endif

    AROS_LIBFUNC_EXIT

} /* MUIA_FreeAslRequestA */

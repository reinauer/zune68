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
        AROS_LH2(BOOL, MUI_AslRequest,

/*  SYNOPSIS */
        AROS_LHA(APTR, requester, A0),
        AROS_LHA(struct TagItem *, tagList, A1),

/*  LOCATION */
        struct Library *, MUIMasterBase, 9, MUIMaster)

/*  FUNCTION
        Interface to asl.library.

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        asl.library/AslRequest()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

#ifdef ZUNE68_GCC_NATIVE
    return ZuneAslRequest(requester, tagList);
#else
    return AslRequest(requester, tagList);
#endif

    AROS_LIBFUNC_EXIT

} /* MUIA_AslRequest */

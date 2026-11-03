/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
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
        Run an allocated ASL requester and wait for it to finish. A
        MUI-managed requester from MUI_AllocAslRequest() opens as a MUI
        file or font panel in a separate process, unless tagList adds tags
        the panel does not support. Other requesters run through
        asl.library/AslRequest().

    INPUTS
        requester - Requester allocated by MUI_AllocAslRequest() or
                    asl.library/AllocAslRequest().
        tagList - Additional requester-specific ASL tags terminated by
                  TAG_DONE, or NULL to retain the existing settings.

    RESULT
        TRUE on successful completion, FALSE on cancellation or failure.

    NOTES
        MUI_AslRequestTags() is the variadic tag-list wrapper.

        Call from a process context suitable for an ASL requester. Result
        fields and detailed error behavior belong to the ASL requester
        type. The requester remains allocated after this call.

    SEE ALSO
        MUI_AllocAslRequest(), MUI_FreeAslRequest(),
        asl.library/AslRequest()

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

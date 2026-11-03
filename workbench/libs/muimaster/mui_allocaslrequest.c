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
        AROS_LH2(APTR, MUI_AllocAslRequest,

/*  SYNOPSIS */
        AROS_LHA(ULONG, reqType, D0),
        AROS_LHA(struct TagItem *, tagList, A0),

/*  LOCATION */
        struct Library *, MUIMasterBase, 8, MUIMaster)

/*  FUNCTION
        Allocate an ASL requester through asl.library/AllocAslRequest().
        For a file or font requester whose tags are all supported by the
        MUI file or font panel, return a MUI-managed requester instead,
        which MUI_AslRequest() runs as a MUI panel. Other requesters are
        returned as allocated by ASL.

    INPUTS
        reqType - ASL_FileRequest, ASL_FontRequest, or ASL_ScreenModeRequest.
        tagList - Requester-specific ASL tags terminated by TAG_DONE, or
                  NULL to use defaults.

    RESULT
        The allocated requester, or NULL.

    NOTES
        MUI_AllocAslRequestTags() is the variadic tag-list wrapper.

        Run the requester with MUI_AslRequest() and release it with
        MUI_FreeAslRequest(). Refer to ASL for the lifetime of tag payloads
        and requester-specific result fields.

    SEE ALSO
        MUI_AslRequest(), MUI_FreeAslRequest(),
        asl.library/AllocAslRequest()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

#ifdef ZUNE68_GCC_NATIVE
    return ZuneAllocAslRequest(reqType, tagList);
#else
    return AllocAslRequest(reqType, tagList);
#endif

    AROS_LIBFUNC_EXIT

} /* MUIA_AllocAslRequest */

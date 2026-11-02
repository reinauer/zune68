/* Copyright (C) 2002-2026, The AROS Development Team. */
#include <stdarg.h>
#define NO_INLINE_STDARG
#include <proto/muimaster.h>

/*****************************************************************************

    NAME
        BOOL MUI_AslRequestTags (

    SYNOPSIS
        APTR requester,
        Tag tag1,
        ...)

    FUNCTION
        Run an ASL requester using a variable-length tag list.
        The call forwards its arguments to MUI_AslRequest().

    INPUTS
        requester - ASL requester allocated with MUI_AllocAslRequest()
                    or AllocAslRequest().
        tag1      - First tag in a sequence of tag/value pairs terminated
                    by TAG_DONE.

    RESULT
        Returns the BOOL result of MUI_AslRequest(): TRUE on success,
        FALSE otherwise.

*****************************************************************************/
BOOL MUI_AslRequestTags(APTR requester, ...)
{
    va_list args;
    BOOL result;
    va_start(args, requester);
    result = MUI_AslRequest(requester, (struct TagItem *)args);
    va_end(args);
    return result;
}

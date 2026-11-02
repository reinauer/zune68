/* Copyright (C) 2002-2026, The AROS Development Team. */
#include <stdarg.h>
#define NO_INLINE_STDARG
#include <proto/muimaster.h>

/*****************************************************************************

    NAME
        APTR MUI_AllocAslRequestTags (

    SYNOPSIS
        ULONG reqType,
        Tag tag1,
        ...)

    FUNCTION
        Allocate an ASL requester using a variable-length tag list.
        This is the varargs form of MUI_AllocAslRequest().

    INPUTS
        reqType - Requester type: ASL_FileRequest, ASL_FontRequest or
                  ASL_ScreenModeRequest.
        tag1    - First tag in a sequence of tag/value pairs terminated
                  by TAG_DONE.

    RESULT
        Pointer to the allocated requester, or NULL if allocation fails.

    NOTES
        Release the requester with MUI_FreeAslRequest().

*****************************************************************************/
APTR MUI_AllocAslRequestTags(ULONG type, ...)
{
    va_list args;
    APTR result;
    va_start(args, type);
    result = MUI_AllocAslRequest(type, (struct TagItem *)args);
    va_end(args);
    return result;
}

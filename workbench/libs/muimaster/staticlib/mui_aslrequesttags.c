/* Copyright (C) 2002, The AROS Development Team. */
#include <stdarg.h>
#define NO_INLINE_STDARG
#include <proto/muimaster.h>
BOOL MUI_AslRequestTags(APTR requester, ...)
{
    va_list args;
    BOOL result;
    va_start(args, requester);
    result = MUI_AslRequest(requester, (struct TagItem *)args);
    va_end(args);
    return result;
}

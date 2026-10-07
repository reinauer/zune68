/* Copyright (C) 2002, The AROS Development Team. */
#include <stdarg.h>
#define NO_INLINE_STDARG
#include <proto/muimaster.h>
APTR MUI_AllocAslRequestTags(ULONG type, ...)
{
    va_list args;
    APTR result;
    va_start(args, type);
    result = MUI_AllocAslRequest(type, (struct TagItem *)args);
    va_end(args);
    return result;
}

/* Copyright (C) 2002, The AROS Development Team. */
#include <stdarg.h>
#define NO_INLINE_STDARG
#include <proto/muimaster.h>
Object * MUI_NewObject(CONST_STRPTR classid, ...)
{
    va_list args;
    Object * result;
    va_start(args, classid);
    result = MUI_NewObjectA(classid, (struct TagItem *)args);
    va_end(args);
    return result;
}

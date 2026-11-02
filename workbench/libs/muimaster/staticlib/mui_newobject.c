/* Copyright (C) 2002-2026, The AROS Development Team. */
#include <stdarg.h>
#define NO_INLINE_STDARG
#include <proto/muimaster.h>

/*****************************************************************************

    NAME
        Object * MUI_NewObject (

    SYNOPSIS
        const char * classname,
        Tag tag1,
        ...)

    FUNCTION
        Create an object of the named MUI class using tag/value pairs.
        This is the varargs form of MUI_NewObjectA().

    INPUTS
        classname - Case-sensitive MUI class identifier.
        tag1      - First tag in a sequence of tag/value pairs terminated
                    by TAG_DONE.

    RESULT
        Pointer to the object, or NULL if the class cannot be found
        or the object cannot be created.

    NOTES
        Dispose of the returned object with MUI_DisposeObject().

*****************************************************************************/
Object * MUI_NewObject(CONST_STRPTR classid, ...)
{
    va_list args;
    Object * result;
    va_start(args, classid);
    result = MUI_NewObjectA(classid, (struct TagItem *)args);
    va_end(args);
    return result;
}

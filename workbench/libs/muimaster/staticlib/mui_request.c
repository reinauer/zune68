#include <stdarg.h>
#define NO_INLINE_STDARG
#include <proto/muimaster.h>
LONG MUI_Request(APTR app, APTR win, LONGBITS flags, CONST_STRPTR title,
    CONST_STRPTR gadgets, CONST_STRPTR format, ...)
{
    va_list args;
    LONG result;
    va_start(args, format);
    result = MUI_RequestA(app, win, flags, title, gadgets, format, args);
    va_end(args);
    return result;
}

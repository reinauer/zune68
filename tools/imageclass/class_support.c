/* Helpers shared by the classes built from their .conf files. */
#include <stdarg.h>
#include <exec/types.h>
#include <graphics/rastport.h>
#include <intuition/classes.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <string.h>

Object *DoSuperNewTags(struct IClass *cl, Object *obj, void *unused, ...)
{
    va_list args;
    struct opSet message;
    Object *result;

    va_start(args, unused);
    message.MethodID = OM_NEW;
    message.ops_AttrList = (struct TagItem *)args;
    message.ops_GInfo = NULL;
    result = (Object *)DoSuperMethodA(cl, obj, (Msg)&message);
    va_end(args);
    return result;
}

STRPTR StrDup(CONST_STRPTR source)
{
    ULONG size = strlen(source) + 1;
    STRPTR copy = AllocVec(size, MEMF_ANY);

    if (copy)
        CopyMem((APTR)source, copy, size);
    return copy;
}

struct RastPort *ZuneCloneRastPort(const struct RastPort *rp)
{
    struct RastPort *copy = AllocVec(sizeof(*copy), MEMF_PUBLIC);

    if (copy)
        *copy = *rp;
    return copy;
}

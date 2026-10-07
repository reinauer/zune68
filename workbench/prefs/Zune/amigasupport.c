#include <stdarg.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
Object *DoSuperNewTagList(Class *cl, Object *obj, void *unused, struct TagItem *tags)
{
    struct opSet msg = { OM_NEW, tags, NULL };
    return (Object *)DoSuperMethodA(cl, obj, (Msg)&msg);
}
Object *DoSuperNewTags(Class *cl, Object *obj, void *unused, ...)
{
    va_list args;
    va_start(args, unused);
    obj = DoSuperNewTagList(cl, obj, unused, (struct TagItem *)args);
    va_end(args);
    return obj;
}

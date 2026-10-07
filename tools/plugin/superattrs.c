/* Work around alib's invalid superclass load without executable startup. */
#include <clib/alib_protos.h>
#include <intuition/classusr.h>

ULONG __stdargs SetSuperAttrs(struct IClass *cl, Object *obj, ULONG tag1, ...)
{
    struct opSet msg;
    msg.MethodID = OM_SET;
    msg.ops_AttrList = (struct TagItem *)&tag1;
    msg.ops_GInfo = NULL;
    return DoSuperMethodA(cl, obj, (Msg)&msg);
}

/* Resources normally opened by executable startup, owned by this library. */
#include <proto/exec.h>
#include <clib/alib_protos.h>
#include <intuition/classusr.h>

/* The soft-float runtime calls these; opened on first use (lazylib.h). */
struct Library *MathIeeeDoubBasBase;
struct Library *MathIeeeDoubTransBase;

/*
 * The installed alib SetSuperAttrs helper loads sp@(24) where it needs
 * cl->cl_Super (a0@(24)), then jumps through object data as a dispatcher.
 * Build the message here and use the working DoSuperMethodA helper.
 * __stdargs keeps tag1 and the remaining tag words contiguous on m68k.
 */
ULONG __stdargs SetSuperAttrs(struct IClass *cl, Object *obj,
    ULONG tag1, ...)
{
    struct opSet msg;

    msg.MethodID = OM_SET;
    msg.ops_AttrList = (struct TagItem *)&tag1;
    msg.ops_GInfo = NULL;
    return DoSuperMethodA(cl, obj, (Msg)&msg);
}

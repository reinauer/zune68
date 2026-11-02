/* Native library envelope for the AROS Calltips class. */
#include <libraries/mui.h>
#include <mcc_common.h>
#include <SDI_hook.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <stdarg.h>

#define DEBUG 0
#define D(...) ((void)0)
#define E(...) ((void)0)
#include "shellstart.c"

ULONG __stdargs SetSuperAttrs(struct IClass *cl, Object *obj, ULONG tag1, ...);
Object *DoSuperNewTags(struct IClass *cl, Object *obj, void *unused, ...);

#include "Calltips.c"

#define CLASS MUIC_Calltips
#define SUPERCLASS MUIC_Window
#define INSTDATA Data
#define USERLIBID CLASS " 20.12 (02.11.2026) AROS native AmigaOS port"
#define MASTERVERSION 19
#define MIN_STACKSIZE 16384
#define SetupDebug() ((void)0)
#define CleanupDebug() ((void)0)
#include "mccinit.c"

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

DISPATCHER(_Dispatcher)
{
    switch (msg->MethodID)
    {
    case OM_NEW: return Calltips__OM_NEW(cl, obj, (APTR)msg);
    case OM_SET: return Calltips__OM_SET(cl, obj, (APTR)msg);
    case OM_GET: return Calltips__OM_GET(cl, obj, (APTR)msg);
    case MUIM_Calltips_SetRectangle:
        return Calltips__MUIM_Calltips_SetRectangle(cl, obj, (APTR)msg);
    case MUIM_Calltips_ParentSetup:
        return Calltips__MUIM_Calltips_ParentSetup(cl, obj, msg);
    case MUIM_Calltips_ParentCleanup:
        return Calltips__MUIM_Calltips_ParentCleanup(cl, obj, msg);
    case MUIM_Calltips_ParentShow:
        return Calltips__MUIM_Calltips_ParentShow(cl, obj, msg);
    case MUIM_Calltips_ParentHide:
        return Calltips__MUIM_Calltips_ParentHide(cl, obj, msg);
    case MUIM_Calltips_ParentWindowArranged:
        return Calltips__MUIM_Calltips_ParentWindowArranged(cl, obj, msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Native library envelope for the AROS Busy class. */
#include <libraries/mui.h>
#include <mcc_common.h>
#include <SDI_hook.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

#define DEBUG 0
#define D(...) ((void)0)
#define E(...) ((void)0)
#include "shellstart.c"

#include "busy.c"

#define CLASS MUIC_Busy
#define SUPERCLASS MUIC_Area
#define INSTDATA Busy_DATA
#define VERSION 20
#define REVISION 1
#define USERLIBID CLASS " 20.1 (10.10.2026) AROS native AmigaOS port"
#define MASTERVERSION 19
#define MIN_STACKSIZE 16384
#define SetupDebug() ((void)0)
#define CleanupDebug() ((void)0)
#include "mccinit.c"

DISPATCHER(_Dispatcher)
{
    switch (msg->MethodID)
    {
    case OM_NEW: return Busy__OM_NEW(cl, obj, (APTR)msg);
    case OM_SET: return Busy__OM_SET(cl, obj, (APTR)msg);
    case OM_GET: return Busy__OM_GET(cl, obj, (APTR)msg);
    case MUIM_Setup: return Busy__MUIM_Setup(cl, obj, (APTR)msg);
    case MUIM_Cleanup: return Busy__MUIM_Cleanup(cl, obj, (APTR)msg);
    case MUIM_Show: return Busy__MUIM_Show(cl, obj, (APTR)msg);
    case MUIM_Hide: return Busy__MUIM_Hide(cl, obj, (APTR)msg);
    case MUIM_AskMinMax: return Busy__MUIM_AskMinMax(cl, obj, (APTR)msg);
    case MUIM_Draw: return Busy__MUIM_Draw(cl, obj, (APTR)msg);
    case MUIM_Busy_Move: return Busy__MUIM_Busy_Move(cl, obj, msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

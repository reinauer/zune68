/* Native library envelope for the AROS Listtree class built on NListtree. */
#include <libraries/mui.h>
#include <mcc_common.h>
#include <SDI_hook.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>

#define DEBUG 0
#define D(...) ((void)0)
#define E(...) ((void)0)
#include "shellstart.c"

Object *DoSuperNewTags(struct IClass *cl, Object *obj, void *unused, ...);

#include "support.c"
#include "listtree.c"

#define CLASS MUIC_Listtree
#define SUPERCLASS MUIC_List
#define INSTDATA Listtree_DATA
#define VERSION 20
#define REVISION 1
#define USERLIBID CLASS " 20.1 (10.10.2026) AROS native AmigaOS port"
#define MASTERVERSION 19
#define MIN_STACKSIZE 16384
#define CLASSINIT
#define CLASSEXPUNGE
#define SetupDebug() ((void)0)
#define CleanupDebug() ((void)0)
static BOOL ClassInit(struct Library *base);
static void ClassExpunge(struct Library *base);
#include "mccinit.c"

static BOOL ClassInit(struct Library *base)
{
    return MCC_NListtreeInt_Startup(base);
}

static void ClassExpunge(struct Library *base)
{
    if (CL_NListtreeInt)
        MCC_NListtreeInt_Shutdown(base);
    CL_NListtreeInt = NULL;
}

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
    case OM_NEW: return (IPTR)Listtree__OM_NEW(cl, obj, (APTR)msg);
    case OM_SET: return Listtree__OM_SET(cl, obj, (APTR)msg);
    case OM_GET: return Listtree__OM_GET(cl, obj, (APTR)msg);
    case OM_DISPOSE: return Listtree__OM_DISPOSE(cl, obj, msg);
    case MUIM_Notify: return Listtree__MUIM_Notify(cl, obj, (APTR)msg);
    case MUIM_Listtree_Insert:
        return Listtree__MUIM_Listtree_Insert(cl, obj, (APTR)msg);
    case MUIM_Listtree_GetEntry:
        return Listtree__MUIM_Listtree_GetEntry(cl, obj, (APTR)msg);
    case MUIM_Listtree_GetNr:
        return Listtree__MUIM_Listtree_GetNr(cl, obj, (APTR)msg);
    case MUIM_Listtree_Remove:
        return Listtree__MUIM_Listtree_Remove(cl, obj, (APTR)msg);
    case MUIM_Listtree_Rename:
        return Listtree__MUIM_Listtree_Rename(cl, obj, (APTR)msg);
    case MUIM_Listtree_Open:
        return Listtree__MUIM_Listtree_Open(cl, obj, (APTR)msg);
    case MUIM_Listtree_Close:
        return Listtree__MUIM_Listtree_Close(cl, obj, (APTR)msg);
    case MUIM_Listtree_TestPos:
        return Listtree__MUIM_Listtree_TestPos(cl, obj, (APTR)msg);
    case MUIM_Listtree_SetDropMark:
        return Listtree__MUIM_Listtree_SetDropMark(cl, obj, msg);
    case MUIM_Listtree_FindName:
        return Listtree__MUIM_Listtree_FindName(cl, obj, (APTR)msg);
    case MUIM_List_TestPos:
        return Listtree__MUIM_List_TestPos(cl, obj, (APTR)msg);
    case MUIM_List_Redraw:
        return Listtree__MUIM_List_Redraw(cl, obj, (APTR)msg);
    case MUIM_List_CreateImage:
        return Listtree__MUIM_List_CreateImage(cl, obj, (APTR)msg);
    case MUIM_List_DeleteImage:
        return Listtree__MUIM_List_DeleteImage(cl, obj, (APTR)msg);
    case MUIM_CreateDragImage:
        return Listtree__MUIM_CreateDragImage(cl, obj, msg);
    case MUIM_DeleteDragImage:
        return Listtree__MUIM_DeleteDragImage(cl, obj, msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

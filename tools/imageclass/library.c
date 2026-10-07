/* Native library envelope for the imported AROS image classes. */
#include <libraries/mui.h>
#include <mcc_common.h>
#include <SDI_hook.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#define DEBUG 0
#define D(...) ((void)0)
#define E(...) ((void)0)
#include "shellstart.c"

#ifdef BUILD_RAWIMAGE
#include "Rawimage.c"
#include "Rawimage_private.h"
#define CLASS "Rawimage.mcc"
#define SUPERCLASS MUIC_Pixmap
#define INSTDATA Data
#define VERSION 20
#define REVISION 19
#define USERLIBID CLASS " 20.19 (7.10.2026) AROS native AmigaOS port"
#else
#include "pixmap.c"
#define CLASS "Pixmap.mui"
#define SUPERCLASS MUIC_Area
#define INSTDATA Pixmap_DATA
#define VERSION 20
#define REVISION 29
#define USERLIBID CLASS " 20.29 (7.10.2026) AROS native AmigaOS port"
struct Library *CyberGfxBase;
#define CLASSINIT
#define CLASSEXPUNGE
static BOOL ClassInit(struct Library *base);
static void ClassExpunge(struct Library *base);
#endif
#define MASTERVERSION 19
#define MIN_STACKSIZE 16384
#define SetupDebug() ((void)0)
#define CleanupDebug() ((void)0)
#include "mccinit.c"

#ifndef BUILD_RAWIMAGE
static BOOL ClassInit(struct Library *base)
{
    CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    return TRUE;
}
static void ClassExpunge(struct Library *base)
{
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    CyberGfxBase = NULL;
}
#endif

DISPATCHER(_Dispatcher)
{
    switch (msg->MethodID)
    {
#ifdef BUILD_RAWIMAGE
    case OM_NEW: return Rawimage__OM_NEW(cl, obj, (APTR)msg);
    case OM_SET: return Rawimage__OM_SET(cl, obj, (APTR)msg);
#else
    case OM_NEW: return Pixmap__OM_NEW(cl, obj, (APTR)msg);
    case OM_DISPOSE: return Pixmap__OM_DISPOSE(cl, obj, msg);
    case OM_SET: return Pixmap__OM_SET(cl, obj, (APTR)msg);
    case OM_GET: return Pixmap__OM_GET(cl, obj, (APTR)msg);
    case MUIM_Setup: return Pixmap__MUIM_Setup(cl, obj, (APTR)msg);
    case MUIM_Cleanup: return Pixmap__MUIM_Cleanup(cl, obj, msg);
    case MUIM_Draw: return Pixmap__MUIM_Draw(cl, obj, (APTR)msg);
    case MUIM_AskMinMax: return Pixmap__MUIM_AskMinMax(cl, obj, (APTR)msg);
    case MUIM_Layout: return Pixmap__MUIM_Layout(cl, obj, msg);
    case MUIM_Pixmap_DrawSection:
        return Pixmap__MUIM_Pixmap_DrawSection(cl, obj, (APTR)msg);
#endif
    }
    return DoSuperMethodA(cl, obj, msg);
}

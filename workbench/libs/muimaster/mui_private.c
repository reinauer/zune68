/* MUI's private library functions from -132 to -306. MUI 3.8's own classes
 * (Coloradjust, Floattext, Virtgroup, Settings, Aboutmui) and preferences
 * program call them, and MUI 5 keeps most of them. Their meanings come
 * from MUI 3.8 (19.35) and MUI 5 (21.227). MUI_Show, MUI_Hide,
 * MUI_LayoutObj, MUI_Offset and MUIP_GetVirtualRect live elsewhere.
 */
#include <exec/memory.h>
#include <proto/exec.h>

#include "mui.h"
#include "muimaster_intern.h"

APTR MUIGfx_GetImage(struct MUI_RenderInfo *mri __asm("a0"),
    STRPTR spec __asm("a1"), ULONG flags __asm("d0"));
void MUIGfx_ReleaseImage(APTR image __asm("a0"));

/* MUI 3.8 has no restrictions to lift; its unregistered level is 0 */
#define MUI_LEVEL_REGISTERED 3

static UWORD ForceRegistered;

/* -132 and -144: memory from MUI's pool. MUI 3.8's -144 also records the
 * size so that -150 can free without one; AllocVec does that for all. */
APTR MUIP_AllocPooled(ULONG size __asm("d0"))
{
    return AllocVec(size, MEMF_ANY | MEMF_CLEAR);
}

/* -138 and -150. MUI 3.8 passes the size in d0 for -138; MUI 5 does not. */
void MUIP_FreePooled(APTR mem __asm("a1"))
{
    FreeVec(mem);
}

/* -204 and -210: MUI 3.8 hands these to muigfx.library's image functions.
 * MUI 5 takes the same registers. */
APTR MUIP_ObtainImage(struct MUI_RenderInfo *mri __asm("a0"),
    STRPTR spec __asm("a1"), ULONG flags __asm("d0"))
{
    return MUIGfx_GetImage(mri, spec, flags);
}

void MUIP_ReleaseImage(APTR image __asm("a1"))
{
    MUIGfx_ReleaseImage(image);
}

/* -246: whether the object's box overlaps rect (left, top, right, bottom),
 * in the virtual coordinates that MUIP_GetVirtualRect returns. */
ULONG MUIP_ObjectInRect(Object *obj __asm("a0"), LONG *rect __asm("a1"))
{
    struct MUI_AreaData *area = muiAreaData(obj);
    LONG left = area->mad_Box.Left;
    LONG top = area->mad_VirtualTop;
    LONG right = left + area->mad_Box.Width - 1;
    LONG bottom = top + area->mad_Box.Height - 1;

    return right >= rect[0] && left <= rect[2]
        && bottom >= rect[1] && top <= rect[3];
}

/* -264: MUI's application list object, a Semaphore subclass. AmIRC 3.6
 * asks it for a free ARexx base name (classes/application.c). */
Object *MUIP_Applist(void)
{
    return zune_applist_get();
}

/* -270: whether the user may change a configuration item, which MUI 3.8
 * limited by registration. */
ULONG MUIP_ConfigItemAllowed(void)
{
    return TRUE;
}

/* -282: the registration level. MUI 5 reports 3 when -294 forced it. */
ULONG MUIP_RegistrationLevel(void)
{
    return MUI_LEVEL_REGISTERED;
}

/* -294 */
void MUIP_ForceRegistered(UWORD force __asm("d0"))
{
    ForceRegistered = force;
}

/* The FD calls -132 to -150, -204 and -210 MUI_Priv1 to MUI_Priv6, and sfdc
 * makes register gates for them that the vector table does not use. */
__asm__(".globl _MUI_Priv1\n.set _MUI_Priv1,_MUIP_AllocPooled\n"
    ".globl _MUI_Priv2\n.set _MUI_Priv2,_MUIP_FreePooled\n"
    ".globl _MUI_Priv3\n.set _MUI_Priv3,_MUIP_AllocPooled\n"
    ".globl _MUI_Priv4\n.set _MUI_Priv4,_MUIP_FreePooled\n"
    ".globl _MUI_Priv5\n.set _MUI_Priv5,_MUIP_ObtainImage\n"
    ".globl _MUI_Priv6\n.set _MUI_Priv6,_MUIP_ReleaseImage\n");

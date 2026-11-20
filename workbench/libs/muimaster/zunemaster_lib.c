/* Copyright (C) 2002-2020, The AROS Development Team.
 * Native library loader, based on Dirk Stoecker's library example.
 */
#include <proto/exec.h>
#include <exec/resident.h>
#include <exec/execbase.h>
#include "muimaster_intern.h"
#include "native_version.h"
#include "gates.h"
struct ExecBase *SysBase;
ULONG L_InitLib(struct Library *base);
void L_ExpungeLib(struct Library *base);
#ifdef MUIMASTER_DROPIN
#define LIBNAME "muimaster.library"
#else
#define LIBNAME "zunemaster.library"
#endif
#define IDSTRING "$VER: " LIBNAME " " ZUNE68_LIBRARY_VERSION_STRING \
    " (" ZUNE68_LIBRARY_DATE ") Zune68 " ZUNE68_RELEASE_STAGE "\r\n"

ULONG LibReserved(void) { return 0; }
void MUIP_GetVirtualRect(Object *obj __asm("a0"), LONG *rect __asm("a1"));
/* mui_private.c */
void MUIP_AllocPooled(void);
void MUIP_FreePooled(void);
void MUIP_ObtainImage(void);
void MUIP_ReleaseImage(void);
void MUIP_ObjectInRect(void);
void MUIP_Applist(void);
void MUIP_ConfigItemAllowed(void);
void MUIP_RegistrationLevel(void);
void MUIP_ForceRegistered(void);
struct Library *LibOpen(struct MUIMasterBase_intern *base __asm("a6"))
{
    base->library.lib_Flags &= ~LIBF_DELEXP;
    ++base->library.lib_OpenCnt;
    return &base->library;
}
BPTR LibExpunge(struct MUIMasterBase_intern *base __asm("a6"))
{
    BPTR segment;
    if (!base->library.lib_OpenCnt)
        zune_applist_free(); /* it holds a Semaphore subclass */
    if (base->library.lib_OpenCnt || !ZUNE_FreeBuiltinClasses(&base->library)) {
        base->library.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    segment = base->seglist;
    Remove((struct Node *)base);
    L_ExpungeLib(&base->library);
    FreeMem((UBYTE *)base - base->library.lib_NegSize,
        base->library.lib_NegSize + base->library.lib_PosSize);
    return segment;
}
BPTR LibClose(struct MUIMasterBase_intern *base __asm("a6"))
{
    if (!--base->library.lib_OpenCnt &&
        (base->library.lib_Flags & LIBF_DELEXP))
        return LibExpunge(base);
    return 0;
}
struct Library *LibInit(BPTR segment __asm("a0"),
    struct MUIMasterBase_intern *base __asm("d0"),
    struct ExecBase *exec __asm("a6"))
{
    SysBase = exec;
    base->sysbase = exec;
    base->seglist = segment;
    base->library.lib_Node.ln_Type = NT_LIBRARY;
    base->library.lib_Node.ln_Name = LIBNAME;
    base->library.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    base->library.lib_Version = ZUNE68_LIBRARY_VERSION;
    base->library.lib_Revision = ZUNE68_LIBRARY_REVISION;
    base->library.lib_IdString = IDSTRING;
    if (L_InitLib(&base->library)) return &base->library;
    FreeMem((UBYTE *)base - base->library.lib_NegSize,
        base->library.lib_NegSize + base->library.lib_PosSize);
    return NULL;
}
#define MUI_VECTOR(name) (APTR)Gate_##name
#define RESERVED (APTR)LibReserved
static const APTR LibVectors[] = {
  (APTR) LibOpen,
  (APTR) LibClose,
  (APTR) LibExpunge,
  (APTR) LibReserved,
  MUI_VECTOR(MUI_NewObjectA),
  MUI_VECTOR(MUI_DisposeObject),
  MUI_VECTOR(MUI_RequestA),
  MUI_VECTOR(MUI_AllocAslRequest),
  MUI_VECTOR(MUI_AslRequest),
  MUI_VECTOR(MUI_FreeAslRequest),
  MUI_VECTOR(MUI_Error),
  MUI_VECTOR(MUI_SetError),
  MUI_VECTOR(MUI_GetClass),
  MUI_VECTOR(MUI_FreeClass),
  MUI_VECTOR(MUI_RequestIDCMP),
  MUI_VECTOR(MUI_RejectIDCMP),
  MUI_VECTOR(MUI_Redraw),
  MUI_VECTOR(MUI_CreateCustomClass),
  MUI_VECTOR(MUI_DeleteCustomClass),
  MUI_VECTOR(MUI_MakeObjectA),
  MUI_VECTOR(MUI_Layout),
  (APTR) MUIP_AllocPooled,
  (APTR) MUIP_FreePooled,
  (APTR) MUIP_AllocPooled,
  (APTR) MUIP_FreePooled,
  MUI_VECTOR(MUI_ObtainPen),
  MUI_VECTOR(MUI_ReleasePen),
  MUI_VECTOR(MUI_AddClipping),
  MUI_VECTOR(MUI_RemoveClipping),
  MUI_VECTOR(MUI_AddClipRegion),
  MUI_VECTOR(MUI_RemoveClipRegion),
  MUI_VECTOR(MUI_BeginRefresh),
  MUI_VECTOR(MUI_EndRefresh),
  (APTR) MUIP_ObtainImage,
  (APTR) MUIP_ReleaseImage,
  MUI_VECTOR(MUI_Show),
  MUI_VECTOR(MUI_Hide),
  MUI_VECTOR(MUI_LayoutObj),
  MUI_VECTOR(MUI_Offset),
  (APTR) MUIP_GetVirtualRect,
  (APTR) MUIP_ObjectInRect,
  /* -252 to -306: MUI 3.8's configuration item table, class list and
   * keyfile data, which Zune does not have, and its application list. */
  RESERVED,
  RESERVED,
  (APTR) MUIP_Applist,
  (APTR) MUIP_ConfigItemAllowed,
  RESERVED,
  (APTR) MUIP_RegistrationLevel,
  RESERVED,
  (APTR) MUIP_ForceRegistered,
  RESERVED,
  RESERVED,
  /* MUI 5 has 127 vectors. Its other entries are private image and string
   * functions for its own classes; they return 0 here instead of running
   * whatever follows the table. */
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED,
  RESERVED, RESERVED, RESERVED, RESERVED,
  (APTR) -1
};


const ULONG LibInitTable[] = {
    sizeof(struct MUIMasterBase_intern), (ULONG)LibVectors, 0, (ULONG)LibInit
};
const struct Resident RomTag = {
    RTC_MATCHWORD, (struct Resident *)&RomTag,
    (struct Resident *)(&RomTag + 1), RTF_AUTOINIT,
    ZUNE68_LIBRARY_VERSION, NT_LIBRARY, 0, LIBNAME, IDSTRING,
    (APTR)LibInitTable
};

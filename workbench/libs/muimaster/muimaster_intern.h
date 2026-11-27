/*
    Copyright (C) 2002-2025, The AROS Development Team. All rights reserved.
*/

#ifndef MUIMASTER_INTERN_H
#define MUIMASTER_INTERN_H

#ifndef EXEC_TYPES_H
#   include <exec/types.h>
#endif
#ifndef EXEC_LIBRARIES_H
#   include <exec/libraries.h>
#endif
#ifndef EXEC_MEMORY_H
#   include <exec/memory.h>
#endif
#ifndef INTUITION_CLASSES_H
#   include <intuition/classes.h>
#endif
#ifndef INTUITION_INTUITIONBASE_H
#   include <intuition/intuitionbase.h>
#endif
#ifndef GRAPHICS_GFXBASE_H
#   include <graphics/gfxbase.h>
#endif
#ifndef DOS_DOS_H
#   include <dos/dos.h>
#endif
#ifndef UTILITY_UTILITY_H
#   include <utility/utility.h>
#endif
#ifndef EXEC_SEMAPHORES_H
#   include <exec/semaphores.h>
#endif

#ifdef __AROS__
#   ifndef AROS_ASMCALL_H
#       include <aros/asmcall.h>
#   endif
#else
#   include "support_amigaos.h"
#endif

#include <dos/notify.h>
#include "mui.h"
#include "textengine.h"
#include "prefs.h"
#include "penspec.h"


struct MUIMasterBase_intern
{
    struct Library              library;
#ifndef __AROS__
    /* On AROS these fields are handled by the system */
    struct ExecBase             *sysbase;
    BPTR                        seglist;

    /* On AROS autoopened libraries are used */
    struct DosLibrary           *dosbase;
    struct UtilityBase          *utilitybase;
    struct Library              *aslbase;
    struct GfxBase              *gfxbase;
    struct Library              *layersbase;
    struct IntuitionBase        *intuibase;
    struct Library              *cxbase;
    struct RxsLib               *rxsbase;
    struct Library              *keymapbase;
    struct Library              *gadtoolsbase;
    struct Library              *iffparsebase;
    struct Library              *diskfontbase;
    struct Library              *iconbase;
    struct Library              *cybergfxbase;
    struct Library              *workbenchbase;
#ifdef HAVE_COOLIMAGES
    struct Library              *coolimagesbase;
#endif
    
/*  struct Library              *datatypesbase; */
#endif /* __AROS__ */

    struct TextFont             *topaz8font;
    struct SignalSemaphore      ZuneSemaphore; /* Used when accessing global data */
    APTR                        SpecialMemory;

#ifndef __AROS__
    /* The MUI version each program sees (identity.c) */
    struct Library              *IdCopy;    /* the base with MUI 3.8's version */
    UBYTE                       *IdList;    /* per-program versions */
    LONG                        IdLoader;   /* ZUNE_ID_* + 1 of a program loading a class */
    UBYTE                       IdDefault;
    UBYTE                       IdChanged;
    UBYTE                       IdNotifying;
    struct MsgPort              IdPort;
    struct NotifyRequest        IdNotify;
#endif
    struct SignalSemaphore      LazyLibSemaphore; /* zune_lazylib_open() */
    ULONG                       LazyLibFailed;    /* 1 << ZUNE_LIB_* */
    /* What MUI's private LVO -264 returns (classes/application.c) */
    Object                      *Applist;
    struct MUI_CustomClass      *ApplistClass;
    /* The MUIE_* code MUI_Error() returns, per task (mui_error.c) */
    struct { struct Task *task; LONG error; } TaskErrors[16];
    UBYTE                       TaskErrorNext;
    /* Borrowed built-in classes, cleared before any expunge attempt.
     * BuiltinClasses, Applications and defaultPens end the base: the
     * tests/check_*.py harnesses find them from its end. */
    Class                       *BuiltinClassCache[32];
    struct MinList              BuiltinClasses;
    struct MinList              Applications;
    struct MUI_PenSpec          *defaultPens;
};

void zune_set_error(LONG error); /* mui_error.c */
void zune_clear_error(void);


#undef MUIMB
void zune_vector_free_templates(void); /* imspec_vector.c, at expunge */
#define MUIMB(b)	((struct MUIMasterBase_intern *)b)

#ifndef __AROS__
extern struct Library *MUIMasterBase;
extern struct Library *MUIScreenBase;
extern struct Library *CyberGfxBase;
BOOL ZUNE_FreeBuiltinClasses(struct Library *base);
/* classes/application.c: MUI's private application list object */
Object *zune_applist_get(void);
void zune_applist_free(void);
/* mui_newobject.c: the child objects of a tag list nothing has read yet */
void zune_dispose_children(struct TagItem *tags);
void zune_dispose_children_but(struct TagItem *tags, Tag owned);
BOOL ZUNE_IsBuiltinClass(CONST_STRPTR id);
/* support_classes.c: MakeClass() that undoes OS 3.1's raised subclass
 * count when it fails */
struct IClass *zune_make_class(CONST_STRPTR id, CONST_STRPTR superid,
    struct IClass *super, ULONG size);
/* identity.c: the MUI version each program sees */
#define ZUNE_ID_MUI5  0
#define ZUNE_ID_MUI38 1
void zune_identity_init(struct MUIMasterBase_intern *base);
void zune_identity_exit(struct MUIMasterBase_intern *base);
LONG zune_identity_of(struct MUIMasterBase_intern *base, struct Task *task);
struct Library *zune_identity_open(struct MUIMasterBase_intern *base,
    ULONG version);
#endif

#endif /* MUIMASTER_INTERN_H */

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

    struct MinList              BuiltinClasses;
    struct MinList              Applications;
    struct MUI_PenSpec          *defaultPens;
};


#undef MUIMB
#define MUIMB(b)	((struct MUIMasterBase_intern *)b)

#ifndef __AROS__
extern struct Library *MUIMasterBase;
extern struct Library *MUIScreenBase;
extern struct Library *CyberGfxBase;
BOOL ZUNE_FreeBuiltinClasses(struct Library *base);
#endif

#endif /* MUIMASTER_INTERN_H */

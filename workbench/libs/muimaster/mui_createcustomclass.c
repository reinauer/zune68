/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
*/

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "support_classes.h"

/*****************************************************************************

    NAME */
        AROS_LH5(struct MUI_CustomClass *, MUI_CreateCustomClass,

/*  SYNOPSIS */
        AROS_LHA(struct Library *,         base,       A0),
        AROS_LHA(CONST_STRPTR,                  supername,  A1),
        AROS_LHA(struct MUI_CustomClass *, supermcc,   A2),
        AROS_LHA(ULONG,                    datasize,   D0),
        AROS_LHA(APTR,                     dispatcher, A3),

/*  LOCATION */
        struct Library *, MUIMasterBase, 18, MUIMaster)

/*  FUNCTION
        Create a BOOPSI class derived from a named MUI class or an existing
        custom class, and return a descriptor for the new class.
        If supername is supplied, it takes precedence over supermcc (as
        on MUI 3.8 and MUI 5, measured with tests/native/ccprio.c).

    INPUTS
        base - Library base for a class module, or NULL for a private class.
               With a non-NULL base, its node name supplies the class ID
               through dos.library/FilePart(). The base is also passed to
               the dispatcher through the class dispatcher's data field.
        supername - NUL-terminated superclass identifier, or NULL to use
                    supermcc.
        supermcc - Existing superclass descriptor, used only when
                   supername is NULL. Its mcc_Class must be valid.
        datasize - Number of instance-data bytes added by the new class.
        dispatcher - Non-NULL dispatcher entrypoint taking the class in A0,
                     the object in A2, and the message in A1.

    RESULT
        A newly allocated MUI_CustomClass descriptor, or NULL if neither
        superclass argument is supplied, the named superclass cannot be
        obtained, descriptor allocation fails, or MakeClass() fails.

    NOTES
        Keep the dispatcher and any supplied module base valid while the
        class is in use. Dispose of all instances and subclasses before
        releasing the descriptor with MUI_DeleteCustomClass().
        The supplied superclass descriptor is not copied or freed here.

    BUGS
        A supplied superclass descriptor is not separately acquired:
        delete the subclass before it.

    SEE ALSO
        MUI_DeleteCustomClass(), MUI_GetClass(), intuition.library/MakeClass()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct MUI_CustomClass *mcc;
    struct IClass        *cl, *super;
    ClassID                 id = NULL;

    if ((supername == NULL) && (supermcc == NULL))
        return NULL;

    /* The name wins when both are given, as on MUI 3.8 and MUI 5: some
     * programs pass a supername with garbage in A2 (LumiZip 1.0 does, for
     * an Area subclass; taking A2 as an mcc made a class whose superclass
     * was its name string, and its MUI_DeleteCustomClass() closed a
     * garbage "library" and reset the machine at quit) */
    if (supername)
    {
        super = MUI_GetClass(supername);
        if (!super) return NULL;
        supermcc = NULL;
    }
    else super = supermcc->mcc_Class;

    if (!(mcc = mui_alloc_struct(struct MUI_CustomClass)))
    {
        if (!supermcc)
            MUI_FreeClass(super);
        return NULL;
    }

    if (base)
        id = FilePart(((struct Node *)base)->ln_Name);

    if (!(cl = zune_make_class(id, NULL, super, datasize)))
    {
        mui_free(mcc);
        if (!supermcc)
            MUI_FreeClass(super);
        return NULL;
    }

    mcc->mcc_UtilityBase   = (struct Library *)UtilityBase;
    mcc->mcc_DOSBase       = (struct Library *)DOSBase;
    mcc->mcc_GfxBase       = (struct Library *)GfxBase;
    mcc->mcc_IntuitionBase = (struct Library *)IntuitionBase;

    mcc->mcc_Class  = cl;
    mcc->mcc_Super  = super;
    mcc->mcc_SuperByName = !supermcc;
    mcc->mcc_Module = NULL; /* _zune_class_load() will set this */

#if defined(__MAXON__) || defined(__amigaos4__)
    cl->cl_Dispatcher.h_Entry    = (HOOKFUNC)dispatcher;
#else
    cl->cl_Dispatcher.h_Entry    = (HOOKFUNC)metaDispatcher;
    cl->cl_Dispatcher.h_SubEntry = (HOOKFUNC)dispatcher;
#endif
    cl->cl_Dispatcher.h_Data     = base;

    return mcc;
    
    AROS_LIBFUNC_EXIT
} /* MUIA_CreateCustomClass */

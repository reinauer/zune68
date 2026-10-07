/*
    Copyright (C) 2002-2007, The AROS Development Team. All rights reserved.
*/

#include <string.h>

#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/exec.h>

#include "muimaster_intern.h"
#include "support_classes.h"
#include "debug.h"

/*****************************************************************************

    NAME */
        AROS_LH1(VOID, MUI_FreeClass,

/*  SYNOPSIS */
        AROS_LHA(Class *, cl, A0),

/*  LOCATION */
        struct Library *, MUIMasterBase, 14, MUIMaster)

/*  FUNCTION
        Frees a class returned by MUI_GetClass(). This function is
        obsolete. Use MUI_DeleteCustomClass() instead.

    INPUTS
        cl - The pointer to the class.

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        MUI_GetClass(), MUI_CreateCustomClass(), MUI_DeleteCustomClass()

    INTERNALS

*****************************************************************************/
{
    if (!cl) return;
    ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    if (cl->cl_Flags & CLF_INLIST) {
        /* A dispatcher can still be returning through a superclass. Keep
           cached classes until expunge proves the entire chain idle. */
        if (cl->cl_UserData) --cl->cl_UserData;
        ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    } else {
        struct Library *module = cl->cl_Dispatcher.h_Data;
        ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
        if (module && module != MUIMasterBase) CloseLibrary(module);
    }
}

/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
*/

#include <proto/exec.h>
#include <proto/muimaster.h>
#include "mui.h"
#include "muimaster_intern.h"

/* Remember error as the calling task's MUI_Error() value. A task keeps
 * its slot until it closes the library or the slot is given away to a
 * sixteenth other task. */
void zune_set_error(LONG error)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct Task *me = FindTask(NULL);
    int i, slot = -1;

    Forbid();
    for (i = 0; i < 16; i++)
    {
        if (base->TaskErrors[i].task == me)
        {
            slot = i;
            break;
        }
        if (slot < 0 && !base->TaskErrors[i].task)
            slot = i;
    }
    if (slot < 0)
    {
        slot = base->TaskErrorNext;
        base->TaskErrorNext = (slot + 1) % 16;
    }
    base->TaskErrors[slot].task = me;
    base->TaskErrors[slot].error = error;
    Permit();
}

/* Forget the calling task's code, so a later task at its address does not
 * inherit it. */
void zune_clear_error(void)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct Task *me = FindTask(NULL);
    int i;

    Forbid();
    for (i = 0; i < 16; i++)
        if (base->TaskErrors[i].task == me)
            base->TaskErrors[i].task = NULL;
    Permit();
}

/*****************************************************************************

    NAME */
        AROS_LH0(LONG, MUI_Error,

/*  SYNOPSIS */

/*  LOCATION */
        struct Library *, MUIMasterBase, 11, MUIMaster)

/*  FUNCTION
        Return the MUIE_* code of the calling task's last failure whose
        cause is known, or one set with MUI_SetError().

    INPUTS
        None.

    RESULT
        MUIE_OK when nothing failed so far. MUI_NewObject() sets
        MUIE_MissingLibrary for a class it cannot find and
        MUIE_OutOfMemory for a built-in class it cannot make; an
        application sets MUIE_SingleTask when another one with its base
        runs and MUIA_Application_SingleTask is set, and MUIE_OutOfMemory
        when it lacks memory or signals. An object that fails because a
        child failed leaves the child's code. Success does not reset the
        code.

    NOTES
        MUI 3.8 and MUI 5 always return MUIE_OK, so programs show "MUI
        Error code 0" or, like Digital Almanac III, MUIE_OK with a note
        that MUI has a bug here. Only call it after a failure.

    SEE ALSO
        MUI_SetError(), dos.library/IoErr()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct Task *me = FindTask(NULL);
    LONG error = MUIE_OK;
    int i;

    Forbid();
    for (i = 0; i < 16; i++)
        if (base->TaskErrors[i].task == me)
        {
            error = base->TaskErrors[i].error;
            break;
        }
    Permit();
    return error;

    AROS_LIBFUNC_EXIT
} /* MUIA_Error */

/*
    Copyright (C) 2002-2020, The AROS Development Team. All rights reserved.
*/

#include <intuition/classusr.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"

//#define MYDEBUG 1
#include "debug.h"

/*****************************************************************************

    NAME */
        AROS_LH6(BOOL, MUI_Layout,

/*  SYNOPSIS */
        AROS_LHA(Object *, obj, A0),
        AROS_LHA(LONG, left, D0),
        AROS_LHA(LONG, top, D1),
        AROS_LHA(LONG, width, D2),
        AROS_LHA(LONG, height, D3),
        AROS_LHA(ULONG, flags, D4),

/*  LOCATION */
        struct Library *, MUIMasterBase, 21, MUIMaster)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    Object *parent = _parent(obj);

/*
 * Called only by groups, never by windows
 */
//    ASSERT(parent != NULL);

    if (_flags(parent) & MADF_ISVIRTUALGROUP)
    {
        /* I'm not yet sure what to do by virtual groups in virtual groups,
         * eighter add their offsets too or not, will be tested soon */
        IPTR val = 0;
        get(parent,MUIA_Virtgroup_Left,&val);
        left -= val;
        get(parent,MUIA_Virtgroup_Top,&val);
        top -= val;
    }

    return MUI_LayoutObj(obj, left + _mleft(parent), top + _mtop(parent),
        width, height, flags);

    AROS_LIBFUNC_EXIT

} /* MUIA_Layout */

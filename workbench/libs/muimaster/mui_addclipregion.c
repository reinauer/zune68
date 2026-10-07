/*
    Copyright (C) 2002-2012, The AROS Development Team. All rights reserved.
*/

#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>

#define MUIMASTER_DEFINING_CLIPPING
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"

#include "debug.h"

/*
 * C-callable body.  Library code must not call the asm LVO entry as a
 * stack C function (wrong register setup).  External apps still enter via
 * the __asm MUI_AddClipRegion LVO below.
 */
APTR ZuneAddClipRegion(struct MUI_RenderInfo *mri, struct Region *r)
{
    struct Window *w;
    struct Layer  *l;
    APTR result;
    BOOL refreshmode;
    BOOL lock_layerinfo;

    w = mri->mri_Window;
    if (w != NULL)
        l = w->WLayer;
    else
        l = mri->mri_RastPort->Layer;

    if ((l == NULL) || (r == NULL) || (mri->mri_rCount == MRI_RARRAY_SIZE))
    {
        if (r)
            DisposeRegion(r);
        return (APTR)-1;
    }

    /*
     * InstallClipRegion is illegal while LAYERUPDATING is set unless we
     * already hold MUI_BeginRefresh's LayerInfo lock and wrap the install
     * with EndRefresh(FALSE)/BeginRefresh.  A nested LockLayerInfo here
     * deadlocks classic Amiga (the lock is not recursive).
     */
    if ((w != NULL)
        && (l->Flags & LAYERUPDATING)
        && !(mri->mri_Flags & MUIMRI_REFRESHMODE))
    {
        DisposeRegion(r);
        return (APTR)-1;
    }

    if (mri->mri_rCount != 0
        && !AndRegionRegion(mri->mri_rArray[mri->mri_rCount-1], r))
    {
        DisposeRegion(r);
        return (APTR)-1;
    }

    refreshmode = (BOOL)((w != NULL) && (mri->mri_Flags & MUIMRI_REFRESHMODE));
    lock_layerinfo = (BOOL)((w != NULL) && !refreshmode);

    if (refreshmode)
        EndRefresh(w, FALSE);
    else if (lock_layerinfo)
        LockLayerInfo(&w->WScreen->LayerInfo);

    result = InstallClipRegion(l, r);

    if (refreshmode)
        BeginRefresh(w);
    else if (lock_layerinfo)
        UnlockLayerInfo(&w->WScreen->LayerInfo);

    mri->mri_rArray[mri->mri_rCount++] = r;

    return result;
}

/*****************************************************************************

    NAME */
        MUI_LIB_ENTRY APTR MUI_AddClipRegion(MUI_LIB_ARG(a0, struct MUI_RenderInfo *mri), MUI_LIB_ARG(a1, struct Region *r))

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
    return ZuneAddClipRegion(mri, r);
} /* MUI_AddClipRegion */

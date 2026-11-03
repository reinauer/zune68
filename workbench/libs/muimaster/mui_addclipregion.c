/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
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
        Push a region onto the rendering context's clipping stack and install
        it on the window layer, or the RastPort layer when no window exists.
        If another MUI clip is active, intersect r with that region first.

    INPUTS
        mri - Non-NULL initialized rendering context. When mri_Window is
              NULL, mri_RastPort must be valid.
        r - Region whose ownership is transferred to this call, or NULL.

    RESULT
        The previous layer clip-region pointer as a removal token, including
        NULL when no previous region exists. Returns (APTR)-1 if r or the
        target layer is NULL, the 20-entry MUI clipping stack is full, the
        region intersection fails, or the window layer is being refreshed
        outside MUI_BeginRefresh().

    NOTES
        The function modifies r and disposes of it on rejection. After a
        successful push, MUI_RemoveClipRegion() disposes of it. Do not free
        or modify the region after passing it here.
        Remove successful pushes in reverse order using the same rendering
        context. Preserve the layer and context until all clips are removed.
        Refresh processing is temporarily suspended and resumed when needed.

    SEE ALSO
        MUI_RemoveClipRegion(), MUI_AddClipping(), layers.library/InstallClipRegion()

*****************************************************************************/
{
    return ZuneAddClipRegion(mri, r);
} /* MUI_AddClipRegion */

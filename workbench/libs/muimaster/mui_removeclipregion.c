/*
    Copyright (C) 2002-2012, The AROS Development Team.
    All rights reserved.
    
*/

#include <proto/layers.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#define MUIMASTER_DEFINING_CLIPPING
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"

VOID ZuneRemoveClipRegion(struct MUI_RenderInfo *mri, APTR handle)
{
    struct Window *w;
    struct Layer  *l;
    BOOL refreshmode;
    BOOL lock_layerinfo;

    if (handle == (APTR)-1 || mri->mri_rCount == 0)
        return;

    w = mri->mri_Window;
    if (w != NULL)
        l = w->WLayer;
    else
        l = mri->mri_RastPort->Layer;

    if (l == NULL)
        return;

    mri->mri_rCount--;

    refreshmode = (BOOL)((w != NULL) && (mri->mri_Flags & MUIMRI_REFRESHMODE));
    lock_layerinfo = (BOOL)((w != NULL) && !refreshmode);

    if (refreshmode)
        EndRefresh(w, FALSE);
    else if (lock_layerinfo)
        LockLayerInfo(&w->WScreen->LayerInfo);

    /* The handle can belong to an outer non-MUI clip. Restore it too. */
    InstallClipRegion(l, (struct Region *)handle);

    if (refreshmode)
        BeginRefresh(w);
    else if (lock_layerinfo)
        UnlockLayerInfo(&w->WScreen->LayerInfo);

    /*
     * Frees the region given in MUI_AddClipRegion (upstream does this too;
     * duplicating the region at add time would be cleaner).
     */
    DisposeRegion(mri->mri_rArray[mri->mri_rCount]);
    mri->mri_rArray[mri->mri_rCount] = NULL;
}

/*****************************************************************************

    NAME */
        MUI_LIB_ENTRY VOID MUI_RemoveClipRegion(MUI_LIB_ARG(a0, struct MUI_RenderInfo *mri), MUI_LIB_ARG(a1, APTR handle))

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
    ZuneRemoveClipRegion(mri, handle);
} /* MUI_RemoveClipRegion */

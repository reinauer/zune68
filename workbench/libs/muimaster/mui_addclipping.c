/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
*/

#include <proto/graphics.h>
#include <proto/layers.h>

#define MUIMASTER_DEFINING_CLIPPING
#include "support.h"

#include "mui.h"
#include "muimaster_intern.h"

APTR ZuneAddClipping(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height)
{
    struct Region *r;
    struct Rectangle rect;
    APTR handle;

    if ((width >= MUI_MAXMAX) || (height >= MUI_MAXMAX))
        return (APTR)-1;

    if (mri->mri_rCount > 0)
    {
        if (isRegionWithinBounds(mri->mri_rArray[mri->mri_rCount-1],
            (WORD)left, (WORD)top, (WORD)width, (WORD)height))
            return (APTR)-1;
    }

    if ((r = NewRegion()) == NULL)
        return (APTR)-1;

    rect.MinX = (WORD)left;
    rect.MinY = (WORD)top;
    rect.MaxX = (WORD)(left + width  - 1);
    rect.MaxY = (WORD)(top  + height - 1);
    /* Empty clips are valid; do not hand inverted rectangles to Layers. */
    if (width > 0 && height > 0 && !OrRectRegion(r, &rect))
    {
        DisposeRegion(r);
        return (APTR)-1;
    }

    /* Always the C body â never the asm LVO via a stack call. */
    handle = ZuneAddClipRegion(mri, r);

    return handle;
}

/*****************************************************************************

    NAME */
        MUI_LIB_ENTRY APTR MUI_AddClipping(MUI_LIB_ARG(a0, struct MUI_RenderInfo *mri), MUI_LIB_ARG(d0, WORD left), MUI_LIB_ARG(d1, WORD top), MUI_LIB_ARG(d2, WORD width), MUI_LIB_ARG(d3, WORD height))

/*  FUNCTION
        Restrict drawing to a rectangle by creating a region and passing it
        to MUI_AddClipRegion(). Existing MUI clipping remains in effect.

    INPUTS
        mri - Valid rendering context for the target layer.
        left, top - Rectangle origin in the layer's drawing coordinates.
        width, height - Positive rectangle dimensions in pixels. Values
                        greater than or equal to MUI_MAXMAX skip clipping.

    RESULT
        A removal token, which can be NULL, or (APTR)-1 if no region was
        pushed. The latter includes allocation failure, a rejected region,
        a skipped dimension, and a rectangle containing the current clip
        region's entire bounds.

    NOTES
        Pair this call with MUI_RemoveClipping() using the same context
        and the exact token. Remove nested clips in reverse order. NULL
        is a valid success token; do not test the token as a Boolean.
        Failure and an unnecessary clip share the same sentinel, so this
        interface cannot guarantee that a new restriction was installed.

    BUGS
        The result of OrRectRegion() is not checked before the region is
        passed to MUI_AddClipRegion().

    SEE ALSO
        MUI_RemoveClipping(), MUI_AddClipRegion(), MUI_RemoveClipRegion()

*****************************************************************************/
{
    return ZuneAddClipping(mri, (LONG)left, (LONG)top, (LONG)width,
        (LONG)height);
} /* MUI_AddClipping */

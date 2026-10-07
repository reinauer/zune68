/*
    Copyright (C) 2002-2007, The AROS Development Team. All rights reserved.
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

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    return ZuneAddClipping(mri, (LONG)left, (LONG)top, (LONG)width,
        (LONG)height);
} /* MUI_AddClipping */

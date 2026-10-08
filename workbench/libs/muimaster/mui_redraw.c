/*
    Copyright (C) 2003-2023, The AROS Development Team. All rights reserved.
*/

#define MUIMASTER_DEFINING_REDRAW

#include <string.h>
#include <clib/alib_protos.h>
#include <intuition/classusr.h>
#include <graphics/gfxmacros.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#include "muimaster_intern.h"
#include "mui.h"
#include "classes/area.h"
#include "area_macros.h"

/* Ensure MADF_ISVIRTUALGROUP is defined */
#ifndef MADF_ISVIRTUALGROUP
#define MADF_ISVIRTUALGROUP (1<<30)
#endif
#include "mui.h"
#include "support.h"

#include "debug.h"

/* Built-in dispatchers finish their disabled appearance before an external
 * subclass resumes drawing. Custom classes can then supply their own style. */
void ZuneDrawDisabled(Object *obj)
{
    IPTR disabled = 0;

    if (get(obj, MUIA_Disabled, &disabled))
    {
#if 0
        /*
          Commented out, because group children were drawn wrongly
          when they have been disabled while window is open.
        */
        if (_parent(obj))
        {
            IPTR parentDisabled;
            if (get(_parent(obj), MUIA_Disabled, &parentDisabled))
            {
                /* Let the parent draw the pattern... */
                if (parentDisabled) disabled = FALSE;
            }
        }
#endif

        if ((disabled) && (XGET(obj, MUIA_NestedDisabled) != TRUE))
        {
#ifdef __AROS__
#if 0
            /*
              This aproach might be faster *provided* that the buffer is
              allocated and filled *once* at startup of muimaster.library.

              In reality, the WritePixelArray() call has quite a big
              overhead, so you should only use this buffer if the gadget
              completely fits inside, and fall back to allocating a new
              buffer if the gadget is too big.

              Perhaps a future optimization...
            */
            LONG  width  = 200;
            LONG  height = 100;
            LONG *buffer = AllocVec(width * height * sizeof(LONG), MEMF_ANY);
            LONG  x, y;

            memset(buffer, 0xAA, width * height * sizeof(LONG));

            for (y = 0; y < ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height; y += height)
            {
                for (x = 0; x < ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width; x += width)
                {
                    WritePixelArrayAlpha
                    (
                        buffer, 0, 0, width * sizeof(LONG),
                        ((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left + x, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top + y,
                        x + width  > ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width  ? ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width  - x : width,
                        y + height > ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height ? ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height - y : height,
                        0xffffffff
                    );
                }
            }
#else
            LONG  width  = ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width;
            LONG  height = ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height;
            LONG *buffer = NULL;

            if (GetBitMapAttr(((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort->BitMap, BMA_DEPTH) >= 15)
            {
                buffer = AllocVec(width * sizeof(LONG), MEMF_ANY);
            }

            if (buffer != NULL)
            {
                memset(buffer, 0xAA, width * sizeof(LONG));

                WritePixelArrayAlpha
                (
                    buffer, 0, 0, 0,
                    ((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top, width, height,
                    0xffffffff
                );
                FreeVec(buffer);
            }   else
#endif
#endif
            {
                /* fallback */
                const static UWORD pattern[] = { 0x8888, 0x2222, };
                LONG fg = ((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_Pens[MPEN_SHADOW];

                SetDrMd(((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort, JAM1);
                SetAPen(((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort, fg);
                SetAfPt(((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort, pattern, 1);
                RectFill(((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left + ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width - 1,
                    ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top + ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height - 1);
                SetAfPt(((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort, NULL, 0);
            }
        }
    } /* if (object is disabled) */

}

/*****************************************************************************

    NAME */
        MUI_LIB_ENTRY VOID MUI_Redraw(MUI_LIB_ARG(a0, Object *objin), MUI_LIB_ARG(d0, ULONG flagsin))

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
    Object *obj;
    ULONG flags;
    APTR clip;
    struct MUIP_Draw dmsg;

    obj = objin;
    flags = flagsin;
    clip = (APTR)-1;

    if (!(((struct __dummyAreaData__ *)(obj))->mad.mad_Flags & MADF_CANDRAW)) return;

#if ZUNE68_TRACE
    {
        Class *cl;
        CONST_STRPTR cid;

        cl = OCLASS(obj);
        cid = (cl != NULL && cl->cl_ID != NULL) ? (CONST_STRPTR)cl->cl_ID : (CONST_STRPTR)"?";
        ZuneTrace(("zune: MUI_Redraw enter obj=%lx class=%s flags=%lx invirt=%ld\n",
            (ULONG)obj, cid, flags,
            (LONG)((((struct __dummyAreaData__ *)(obj))->mad.mad_Flags
                & MADF_INVIRTUALGROUP) ? 1 : 0)));
    }
#endif

    if (((struct __dummyAreaData__ *)(obj))->mad.mad_Flags & MADF_INVIRTUALGROUP)
    {
        Object *wnd = NULL;
        Object *parent;
        struct Rectangle bounds;
        BOOL have_bounds = FALSE;
        struct Region *region;

        get(obj, MUIA_WindowObject, &wnd);
        parent = obj;
        while (get(parent, MUIA_Parent, &parent))
        {
            struct Rectangle rect;
            if (!parent || parent == wnd) break;
            if (!(_flags(parent) & MADF_ISVIRTUALGROUP)) continue;

            rect.MinX = _mleft(parent);
            rect.MinY = _mtop(parent);
            rect.MaxX = _mright(parent);
            rect.MaxY = _mbottom(parent);
            if (have_bounds)
            {
                bounds.MinX = MAX(bounds.MinX, rect.MinX);
                bounds.MinY = MAX(bounds.MinY, rect.MinY);
                bounds.MaxX = MIN(bounds.MaxX, rect.MaxX);
                bounds.MaxY = MIN(bounds.MaxY, rect.MaxY);
            }
            else
            {
                bounds = rect;
                have_bounds = TRUE;
            }
            /* No drawing is visible through an empty ancestor. */
            if (bounds.MaxX < bounds.MinX || bounds.MaxY < bounds.MinY)
                return;
        }
        if (have_bounds)
        {
            /* All ancestors clip to rectangles: intersect on stack, then
             * construct one Region. No cache or geometry invalidation. */
            region = NewRegion();
            if (!region) return;
            if (!OrRectRegion(region, &bounds))
            {
                DisposeRegion(region);
                return;
            }
            clip = MUI_AddClipRegion(muiRenderInfo(obj), region);
            if (clip == (APTR)-1) return;
        }
    } /* if object is in a virtual group */

    if (1)
    {
            struct Region *region;
        struct Rectangle *clip_rect;
        struct Layer *l;
        
        clip_rect = &((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_ClipRect;

            if (((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_Window)
        {
            l = ((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_Window->WLayer;
        }
        else
        {
            l = ((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_RastPort->Layer;
        }
        
        if (l && (region = l->ClipRegion))
        {
            /* Maybe this should went to MUI_AddClipRegion() */
            clip_rect->MinX = MAX(((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left,region->bounds.MinX);
            clip_rect->MinY = MAX(((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top,region->bounds.MinY);
            clip_rect->MaxX = MIN(((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left + ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width - 1,region->bounds.MaxX);
            clip_rect->MaxY = MIN(((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top + ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height - 1,region->bounds.MaxY);

        } else
        {
            clip_rect->MinX = ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left;
            clip_rect->MinY = ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top;
            clip_rect->MaxX = ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left + ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width - 1;
            clip_rect->MaxY = ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top + ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height - 1;
        }
    }
    
    ((struct __dummyAreaData__ *)(obj))->mad.mad_Flags = (((struct __dummyAreaData__ *)(obj))->mad.mad_Flags & ~MADF_DRAWFLAGS) | (flags & MADF_DRAWFLAGS);

    dmsg.MethodID = MUIM_Draw;
    dmsg.flags = 0;
    ZuneTrace(("zune: MUI_Redraw DoMethodA obj=%lx flags=%lx\n",
        (ULONG)obj, flags));
    DoMethodA(obj, (Msg)&dmsg);
    ZuneTrace(("zune: MUI_Redraw DoMethodA done\n"));

#if defined(__AROS__) || defined(__amigaos4__) || defined(__MAXON__)
    ZuneDrawDisabled(obj);
#endif

    /* copy buffer to window */
    if (((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_BufferBM
        && ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width >= 1
        && ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height >= 1)
    {
        /* ClipBlit autodoc: XSize and YSize must be at least 1. */
        ClipBlit(&((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_BufferRP, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top,
                 ((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo->mri_Window->RPort, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Left, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Top,
                 ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Width, ((struct __dummyAreaData__ *)(obj))->mad.mad_Box.Height, 0xc0);
    }

    if (clip != (APTR)-1)
    {
        /* This call actually also frees the region */
        MUI_RemoveClipRegion(((struct __dummyAreaData__ *)(obj))->mad.mad_RenderInfo, clip);
    }

} /* MUI_Redraw */

void ZuneRedraw(ULONG obju, ULONG flags)
{
    Object *obj;

    obj = (Object *) obju;
    if (obj == NULL)
        return;
    MUI_Redraw(obj, flags);
}

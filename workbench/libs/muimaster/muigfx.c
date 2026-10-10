/* muigfx.library: the private MUI graphics helpers that older custom
 * classes (TearOffPanel.mcc) and Voyager call. MUI stopped shipping the
 * library; muimaster adds this in-memory version when it starts, so no
 * file is needed in LIBS:. Only the functions those callers use are
 * implemented; the others return zero.
 */
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include "muimaster_intern.h"
#include "frame.h"
#include "imspec.h"
#include "imspec_intern.h"
#include "mui.h"

#define MUIGFX_NAME "muigfx.library"
#define MUIGFX_VERSION 20
#define MUIGFX_REVISION 1
#define MUIGFX_SLOTS 40         /* LVOs -30 to -264 */

/* MUI's struct Frame starts with the border widths; callers read them. */
struct MUIGfxFrame
{
    UBYTE left, right, top, bottom;
    UBYTE type;
};

struct MUIGfxImage
{
    struct MUI_ImageSpec_intern *spec;
    struct MUI_MinMax minmax;
};

static struct Library *MUIGfxBase;
static struct Library *Owner;
static struct MUIGfxFrame Frames[FST_CUSTOM1];

static struct Library *Gfx_Open(struct Library *base __asm("a6"))
{
    /* Keep muimaster, which holds this code, loaded while open. */
    base->lib_OpenCnt++;
    Owner->lib_OpenCnt++;
    return base;
}

static BPTR Gfx_Close(struct Library *base __asm("a6"))
{
    base->lib_OpenCnt--;
    Owner->lib_OpenCnt--;
    return 0;
}

static BPTR Gfx_Expunge(void)
{
    return 0;
}

static ULONG Gfx_Null(void)
{
    return 0;
}

static struct MUIGfxFrame *Gfx_GetFrame(LONG nr __asm("d0"),
    BOOL thin __asm("d1"))
{
    if (nr < 0 || nr >= FST_CUSTOM1)
        nr = FST_RECT;
    return &Frames[nr];
}

static void Gfx_DrawFrame(struct MUI_RenderInfo *mri __asm("a0"),
    struct MUIGfxFrame *frame __asm("a1"), LONG left __asm("d0"),
    LONG top __asm("d1"), LONG width __asm("d2"), LONG height __asm("d3"),
    LONG state __asm("d4"))
{
    struct MUI_FrameSpec_intern spec = { 0 };
    const struct ZuneFrameGfx *zframe;

    if (!mri || !frame || width <= 0 || height <= 0)
        return;
    spec.type = frame->type;
    zframe = zune_zframe_get_with_state(NULL, &spec, state ? 1 : 0);
    zframe->draw(zframe->customframe, mri, left, top, width, height,
        left, top, width, height);
}

/* Like MUI's muigfx, always return an image: scalable images, unknown
 * specs and missing files report 0x0 to 10000x10000 with an 8x8 default
 * (TearOffPanel sizes its handles from that), fixed-size images their
 * own size. */
static struct MUIGfxImage *Gfx_GetImage(struct MUI_RenderInfo *mri __asm("a0"),
    STRPTR spec __asm("a1"), ULONG flags __asm("d0"))
{
    static const struct MUI_MinMax scalable = {
        0, 0, MUI_MAXMAX, MUI_MAXMAX, 8, 8
    };
    struct MUIGfxImage *image;

    if (!(image = AllocVec(sizeof(*image), MEMF_ANY | MEMF_CLEAR)))
        return NULL;
    image->minmax = scalable;
    if (mri && spec)
        image->spec = zune_imspec_setup((IPTR)spec, mri);
    if (image->spec)
    {
        switch (image->spec->type)
        {
        case IST_PATTERN:
        case IST_COLOR:
        case IST_SCALED_GRADIENT:
        case IST_TILED_GRADIENT:
        case IST_BOOPSI:
            break;
        case IST_BRUSH:
            if (!image->spec->u.brush.dt[0])
                break;
            /* fall through */
        default:
            zune_imspec_askminmax(image->spec, &image->minmax);
        }
    }
    return image;
}

static void Gfx_ReleaseImage(struct MUIGfxImage *image __asm("a0"))
{
    if (!image)
        return;
    if (image->spec)
        zune_imspec_cleanup(image->spec);
    FreeVec(image);
}

static ULONG Gfx_GetImageAttr(struct MUIGfxImage *image __asm("a0"),
    ULONG attr __asm("d0"), struct MUI_RenderInfo *mri __asm("a1"))
{
    if (!image)
        return 0;
    switch (attr)
    {
    case 1: return image->minmax.MinWidth;
    case 2: return image->minmax.MinHeight;
    case 3: return image->minmax.MaxWidth;
    case 4: return image->minmax.MaxHeight;
    case 5: return image->minmax.DefWidth;
    case 6: return image->minmax.DefHeight;
    }
    return 0;
}

static void Gfx_GetImageSize(struct MUIGfxImage *image __asm("a0"),
    struct MUI_MinMax *minmax __asm("a1"))
{
    if (image && minmax)
        *minmax = image->minmax;
}

static void Gfx_DrawImage(struct MUI_RenderInfo *mri __asm("a0"),
    struct MUIGfxImage *image __asm("a1"), LONG left __asm("d0"),
    LONG top __asm("d1"), LONG width __asm("d2"), LONG height __asm("d3"),
    ULONG flags __asm("d4"), ULONG flags2 __asm("d5"))
{
    if (!mri || !image || !image->spec || width <= 0 || height <= 0)
        return;
    /* MUI draws nothing for a brush whose file is missing. */
    if (image->spec->type == IST_BRUSH && !image->spec->u.brush.dt[0])
        return;
    zune_imspec_draw(image->spec, mri, left, top, width, height,
        0, 0, flags & 1);
}

/* As the MorphOS reimplementation does: scroll with ScrollWindowRaster,
 * which fills the exposed part with the backfill and reports damage as
 * IDCMP_REFRESHWINDOW; -1/-2 for an empty width/height, 0 without a
 * scroll, 1 otherwise. */
static LONG Gfx_ScrollRaster(struct RastPort *rp __asm("a0"),
    WORD dx __asm("d0"), WORD dy __asm("d1"), WORD left __asm("d2"),
    WORD top __asm("d3"), WORD right __asm("d4"), WORD bottom __asm("d5"))
{
    if (right - left + 1 <= 0)
        return -1;
    if (bottom - top + 1 <= 0)
        return -2;
    if (!dx && !dy)
        return 0;
    if (rp && rp->Layer && rp->Layer->Window)
        ScrollWindowRaster((struct Window *)rp->Layer->Window, dx, dy,
            left, top, right, bottom);
    return 1;
}

static APTR Vectors[4 + MUIGFX_SLOTS + 1];

static void SetSlot(LONG offset, APTR function)
{
    Vectors[offset / 6 - 1] = function;
}

void MUIGfx_Install(struct Library *owner)
{
    struct MUI_FrameSpec_intern spec = { 0 };
    struct Library *base;
    LONG i;

    for (i = 0; i < FST_CUSTOM1; ++i)
    {
        const struct ZuneFrameGfx *zframe;

        spec.type = i;
        zframe = zune_zframe_get_with_state(NULL, &spec, 0);
        Frames[i].left = zframe->ileft;
        Frames[i].right = zframe->iright;
        Frames[i].top = zframe->itop;
        Frames[i].bottom = zframe->ibottom;
        Frames[i].type = i;
    }
    for (i = 0; i < 4 + MUIGFX_SLOTS; ++i)
        Vectors[i] = (APTR)Gfx_Null;
    Vectors[4 + MUIGFX_SLOTS] = (APTR)-1;
    SetSlot(6, Gfx_Open);
    SetSlot(12, Gfx_Close);
    SetSlot(18, Gfx_Expunge);
    SetSlot(114, Gfx_GetImage);
    SetSlot(120, Gfx_ReleaseImage);
    SetSlot(126, Gfx_GetImageAttr);
    SetSlot(132, Gfx_DrawImage);
    SetSlot(138, Gfx_GetImageSize);
    SetSlot(180, Gfx_GetFrame);
    SetSlot(186, Gfx_DrawFrame);
    SetSlot(240, Gfx_ScrollRaster);

    Forbid();
    /* A real or earlier muigfx.library takes precedence. */
    if (!FindName(&SysBase->LibList, MUIGFX_NAME)
        && (base = MakeLibrary(Vectors, NULL, NULL, sizeof(struct Library), 0)))
    {
        base->lib_Node.ln_Type = NT_LIBRARY;
        base->lib_Node.ln_Name = MUIGFX_NAME;
        base->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
        base->lib_Version = MUIGFX_VERSION;
        base->lib_Revision = MUIGFX_REVISION;
        base->lib_IdString = "$VER: muigfx.library 20.1 (10.10.2026) Zune68";
        AddLibrary(base);
        MUIGfxBase = base;
        Owner = owner;
    }
    Permit();
}

void MUIGfx_Remove(void)
{
    struct Library *base = MUIGfxBase;

    if (!base)
        return;
    /* The owner is only expunged with no opener left, so neither is
     * this library. */
    Forbid();
    Remove(&base->lib_Node);
    Permit();
    FreeMem((UBYTE *)base - base->lib_NegSize,
        base->lib_NegSize + base->lib_PosSize);
    MUIGfxBase = NULL;
    Owner = NULL;
}

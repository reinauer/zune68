/* Optional CyberGraphX V41 rendering, also provided by Picasso96 on OS3.
 * Use the original pixel-array API: the newer alpha/processing vectors
 * belong to other platforms and cannot be called on classic systems. */
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>
#include "muimaster_intern.h"
#include "support.h"

BOOL ZuneOpenCyberGfx(void)
{
    ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    if (!CyberGfxBase) CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    return CyberGfxBase != NULL;
}
/* This primitive is also used by the linked 68000 arithmetic tests. */
ULONG ZuneBlendARGB(ULONG source, ULONG destination, ULONG opacity)
{
    ULONG alpha = ((source >> 24) * opacity + 127) / 255;
    if (!alpha) return destination;
    if (alpha == 255) return source;
    ULONG result = 0xff000000;
    for (ULONG shift = 0; shift <= 16; shift += 8)
    {
        ULONG a = (source >> shift) & 255, b = (destination >> shift) & 255;
        result |= ((a * alpha + b * (255 - alpha) + 127) / 255) << shift;
    }
    return result;
}
void ZuneWritePixelArrayAlpha(APTR source, LONG sx, LONG sy, ULONG stride,
    struct RastPort *rp, LONG x, LONG y, LONG width, LONG height, ULONG opacity)
{
    if (!CyberGfxBase || !source || !rp || !rp->BitMap || width <= 0 || height <= 0 ||
        !GetCyberMapAttr(rp->BitMap, CYBRMATTR_ISCYBERGFX)) return;
    if (x < 0) { sx -= x; width += x; x = 0; }
    if (y < 0) { sy -= y; height += y; y = 0; }
    if (width <= 0 || height <= 0 || width > 16383 || sx < 0 || sy < 0) return;
    ULONG *row = AllocVec(width * 4, MEMF_ANY);
    if (!row) return;
    ULONG alpha = opacity >> 24;
    for (LONG iy = 0; iy < height; iy++)
    {
        const ULONG *src = (APTR)((UBYTE *)source + (sy + iy) * stride + sx * 4);
        if (!ReadPixelArray(row, 0, 0, width * 4, rp, x, y + iy, width, 1, RECTFMT_ARGB)) continue;
        for (LONG ix = 0; ix < width; ix++) row[ix] = ZuneBlendARGB(src[ix], row[ix], alpha);
        WritePixelArray(row, 0, 0, width * 4, rp, x, y + iy, width, 1, RECTFMT_ARGB);
    }
    FreeVec(row);
}
void ZuneProcessPixelArray(struct RastPort *rp, LONG x, LONG y,
    LONG width, LONG height, ULONG operation, ULONG value, struct TagItem *tags)
{
    if (!CyberGfxBase || !rp || !rp->BitMap || width <= 0 || height <= 0 || width > 16383 ||
        !GetCyberMapAttr(rp->BitMap, CYBRMATTR_ISCYBERGFX)) return;
    ULONG *row = AllocVec(width * 4, MEMF_ANY);
    if (!row) return;
    value = MIN(value, 255);
    for (LONG iy = 0; iy < height; iy++)
    {
        if (!ReadPixelArray(row, 0, 0, width * 4, rp, x, y + iy, width, 1, RECTFMT_ARGB)) continue;
        for (LONG ix = 0; ix < width; ix++)
        {
            ULONG pixel = row[ix] & 0xff000000;
            for (ULONG shift = 0; shift <= 16; shift += 8)
            {
                LONG channel = (row[ix] >> shift) & 255;
                channel += operation == POP_DARKEN ? -(LONG)value : (LONG)value;
                pixel |= (ULONG)MAX(0, MIN(255, channel)) << shift;
            }
            row[ix] = pixel;
        }
        WritePixelArray(row, 0, 0, width * 4, rp, x, y + iy, width, 1, RECTFMT_ARGB);
    }
    FreeVec(row);
}

/* Render only explicit RGB/alpha text through a one-bit glyph mask. Plain
 * text keeps graphics.library's direct path and needs no scratch bitmap. */
BOOL ZuneTextARGB(struct RastPort *rp, CONST_STRPTR text, ULONG length,
    ULONG foreground, ULONG background)
{
    struct TextExtent extent;
    struct RastPort mask;
    struct BitMap *bitmap;
    ULONG *pixels;
    LONG x = rp->cp_x, y = rp->cp_y, width, height, advance;
    if (!CyberGfxBase || !rp->BitMap ||
        !GetCyberMapAttr(rp->BitMap, CYBRMATTR_ISCYBERGFX)) return FALSE;
    TextExtent(rp, text, length, &extent);
    width = extent.te_Extent.MaxX - extent.te_Extent.MinX + 1;
    height = extent.te_Extent.MaxY - extent.te_Extent.MinY + 1;
    advance = extent.te_Width;
    if (width <= 0 || height <= 0) return TRUE;
    if (width > 16383 || height > 32767 ||
        (ULONG)width * height > 0x1fffffffUL) return FALSE;
    bitmap = AllocBitMap(width, height, 1, BMF_CLEAR, NULL);
    if (!bitmap) return FALSE;
    pixels = AllocVec(width * height * 4, MEMF_ANY);
    if (!pixels) { FreeBitMap(bitmap); return FALSE; }
    InitRastPort(&mask);
    mask.BitMap = bitmap;
    SetFont(&mask, rp->Font);
    SetSoftStyle(&mask, rp->AlgoStyle, 0xff);
    SetAPen(&mask, 1); SetBPen(&mask, 0);
    SetDrMd(&mask, JAM2 | (rp->DrawMode & INVERSVID));
    Move(&mask, -extent.te_Extent.MinX, -extent.te_Extent.MinY);
    Text(&mask, text, length);
    WaitBlit();
    for (LONG iy = 0; iy < height; iy++)
        for (LONG ix = 0; ix < width; ix++)
        {
            UBYTE bits = bitmap->Planes[0][iy * bitmap->BytesPerRow + ix / 8];
            pixels[iy * width + ix] = (bits & (0x80 >> (ix & 7))) ?
                foreground : (rp->DrawMode & JAM2) ? background : 0;
        }
    ZuneWritePixelArrayAlpha(pixels, 0, 0, width * 4, rp,
        x + extent.te_Extent.MinX, y + extent.te_Extent.MinY,
        width, height, 0xff000000);
    Move(rp, x + advance, y);
    FreeVec(pixels); FreeBitMap(bitmap);
    return TRUE;
}

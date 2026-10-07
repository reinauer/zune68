/*
    Copyright (C) 2011, Thore Böckelmann. All rights reserved.
    Copyright (C) 2012, The AROS Development Team. All rights reserved.
*/

#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>

#include <libraries/mui.h>
#include <cybergraphx/cybergraphics.h>

#include <exec/memory.h>
#include <graphics/gfxmacros.h>
#include <strings.h>
#include <string.h>
#include <bzlib.h>

#include "pixmap.h"
#include "pixmap_private.h"

#include <aros/debug.h>

#ifndef MEMF_SHARED
#define MEMF_SHARED MEMF_ANY
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

// libbz2_nostdio needs this
void free(void *memory)
{
    FreeVec(memory);
}

void *malloc(size_t size)
{
    return AllocVec(size, MEMF_ANY);
}

void bz_internal_error(int errcode)
{
    bug("[Pixmap.mui/bz_internal_error] errcode %d\n", errcode);
}

/* ------------------------------------------------------------------------- */

/// default color map
const ULONG defaultColorMap[256] = {
    0x00000000, 0x00000055, 0x000000aa, 0x000000ff,
    0x00002400, 0x00002455, 0x000024aa, 0x000024ff,
    0x00004900, 0x00004955, 0x000049aa, 0x000049ff,
    0x00006d00, 0x00006d55, 0x00006daa, 0x00006dff,
    0x00009200, 0x00009255, 0x000092aa, 0x000092ff,
    0x0000b600, 0x0000b655, 0x0000b6aa, 0x0000b6ff,
    0x0000db00, 0x0000db55, 0x0000dbaa, 0x0000dbff,
    0x0000ff00, 0x0000ff55, 0x0000ffaa, 0x0000ffff,
    0x00240000, 0x00240055, 0x002400aa, 0x002400ff,
    0x00242400, 0x00242455, 0x002424aa, 0x002424ff,
    0x00244900, 0x00244955, 0x002449aa, 0x002449ff,
    0x00246d00, 0x00246d55, 0x00246daa, 0x00246dff,
    0x00249200, 0x00249255, 0x002492aa, 0x002492ff,
    0x0024b600, 0x0024b655, 0x0024b6aa, 0x0024b6ff,
    0x0024db00, 0x0024db55, 0x0024dbaa, 0x0024dbff,
    0x0024ff00, 0x0024ff55, 0x0024ffaa, 0x0024ffff,
    0x00490000, 0x00490055, 0x004900aa, 0x004900ff,
    0x00492400, 0x00492455, 0x004924aa, 0x004924ff,
    0x00494900, 0x00494955, 0x004949aa, 0x004949ff,
    0x00496d00, 0x00496d55, 0x00496daa, 0x00496dff,
    0x00499200, 0x00499255, 0x004992aa, 0x004992ff,
    0x0049b600, 0x0049b655, 0x0049b6aa, 0x0049b6ff,
    0x0049db00, 0x0049db55, 0x0049dbaa, 0x0049dbff,
    0x0049ff00, 0x0049ff55, 0x0049ffaa, 0x0049ffff,
    0x006d0000, 0x006d0055, 0x006d00aa, 0x006d00ff,
    0x006d2400, 0x006d2455, 0x006d24aa, 0x006d24ff,
    0x006d4900, 0x006d4955, 0x006d49aa, 0x006d49ff,
    0x006d6d00, 0x006d6d55, 0x006d6daa, 0x006d6dff,
    0x006d9200, 0x006d9255, 0x006d92aa, 0x006d92ff,
    0x006db600, 0x006db655, 0x006db6aa, 0x006db6ff,
    0x006ddb00, 0x006ddb55, 0x006ddbaa, 0x006ddbff,
    0x006dff00, 0x006dff55, 0x006dffaa, 0x006dffff,
    0x00920000, 0x00920055, 0x009200aa, 0x009200ff,
    0x00922400, 0x00922455, 0x009224aa, 0x009224ff,
    0x00924900, 0x00924955, 0x009249aa, 0x009249ff,
    0x00926d00, 0x00926d55, 0x00926daa, 0x00926dff,
    0x00929200, 0x00929255, 0x009292aa, 0x009292ff,
    0x0092b600, 0x0092b655, 0x0092b6aa, 0x0092b6ff,
    0x0092db00, 0x0092db55, 0x0092dbaa, 0x0092dbff,
    0x0092ff00, 0x0092ff55, 0x0092ffaa, 0x0092ffff,
    0x00b60000, 0x00b60055, 0x00b600aa, 0x00b600ff,
    0x00b62400, 0x00b62455, 0x00b624aa, 0x00b624ff,
    0x00b64900, 0x00b64955, 0x00b649aa, 0x00b649ff,
    0x00b66d00, 0x00b66d55, 0x00b66daa, 0x00b66dff,
    0x00b69200, 0x00b69255, 0x00b692aa, 0x00b692ff,
    0x00b6b600, 0x00b6b655, 0x00b6b6aa, 0x00b6b6ff,
    0x00b6db00, 0x00b6db55, 0x00b6dbaa, 0x00b6dbff,
    0x00b6ff00, 0x00b6ff55, 0x00b6ffaa, 0x00b6ffff,
    0x00db0000, 0x00db0055, 0x00db00aa, 0x00db00ff,
    0x00db2400, 0x00db2455, 0x00db24aa, 0x00db24ff,
    0x00db4900, 0x00db4955, 0x00db49aa, 0x00db49ff,
    0x00db6d00, 0x00db6d55, 0x00db6daa, 0x00db6dff,
    0x00db9200, 0x00db9255, 0x00db92aa, 0x00db92ff,
    0x00dbb600, 0x00dbb655, 0x00dbb6aa, 0x00dbb6ff,
    0x00dbdb00, 0x00dbdb55, 0x00dbdbaa, 0x00dbdbff,
    0x00dbff00, 0x00dbff55, 0x00dbffaa, 0x00dbffff,
    0x00ff0000, 0x00ff0055, 0x00ff00aa, 0x00ff00ff,
    0x00ff2400, 0x00ff2455, 0x00ff24aa, 0x00ff24ff,
    0x00ff4900, 0x00ff4955, 0x00ff49aa, 0x00ff49ff,
    0x00ff6d00, 0x00ff6d55, 0x00ff6daa, 0x00ff6dff,
    0x00ff9200, 0x00ff9255, 0x00ff92aa, 0x00ff92ff,
    0x00ffb600, 0x00ffb655, 0x00ffb6aa, 0x00ffb6ff,
    0x00ffdb00, 0x00ffdb55, 0x00ffdbaa, 0x00ffdbff,
    0x00ffff00, 0x00ffff55, 0x00ffffaa, 0x00ffffff
};

///

IPTR Pixmap__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct TagItem *tag, *tags;

    if ((obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg)) != NULL)
    {
        struct Pixmap_DATA *data = INST_DATA(cl, obj);

        memset(data->ditheredPenMap, 0xff, sizeof(data->ditheredPenMap));
        data->format = MUIV_Pixmap_Format_ARGB32;
        data->alpha = 0xffffffffUL;
        data->compression = MUIV_Pixmap_Compression_None;

        for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
        {
            switch (tag->ti_Tag)
            {
            case MUIA_LeftEdge:
                data->leftOffset = tag->ti_Data;
                break;
            case MUIA_TopEdge:
                data->topOffset = tag->ti_Data;
                break;
            case MUIA_Pixmap_Data:
                data->data = (APTR) tag->ti_Data;
                break;
            case MUIA_Pixmap_Format:
                data->format = tag->ti_Data;
                break;
            case MUIA_Pixmap_Width:
                data->width = tag->ti_Data;
                break;
            case MUIA_Pixmap_Height:
                data->height = tag->ti_Data;
                break;
            case MUIA_Pixmap_CLUT:
                data->clut = (APTR) tag->ti_Data;
                break;
            case MUIA_Pixmap_Alpha:
                data->alpha = tag->ti_Data;
                break;
            case MUIA_Pixmap_Compression:
                data->compression = tag->ti_Data;
                break;
            case MUIA_Pixmap_CompressedSize:
                data->compressedSize = tag->ti_Data;
                break;
            }
        }
    }

    return (IPTR) obj;
}


static void FreeImage(struct IClass *cl, Object *obj)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    if (data->ownsData) FreeVec(data->uncompressedData);
    data->uncompressedData = NULL;
    data->ownsData = FALSE;
}

static void FreeRendered(struct Pixmap_DATA *data);

IPTR Pixmap__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    FreeRendered(INST_DATA(cl, obj));
    FreeImage(cl, obj);
    return DoSuperMethodA(cl, obj, msg);
}


#define RAWIDTH(w) ((((UWORD)(w))+15)>>3 & 0xFFFE)

static void FreeRendered(struct Pixmap_DATA *data)
{
    ULONG i;
    if (data->ditheredBitmap) { WaitBlit(); FreeBitMap(data->ditheredBitmap); }
    if (data->ditheredMask) FreeVec(data->ditheredMask);
    data->ditheredBitmap = NULL;
    data->ditheredMask = NULL;
    for (i = 0; i < 256; i++)
    {
        if (data->ditheredPenMap[i] >= 0 && data->colorMap)
            ReleasePen(data->colorMap, data->ditheredPenMap[i]);
        data->ditheredPenMap[i] = -1;
    }
    data->colorMap = NULL;
}

static BOOL DitherImage(struct IClass *cl, Object *obj)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    const ULONG *colors = data->clut ? data->clut : defaultColorMap;
    static const UBYTE coverage[16] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    const UBYTE *source = data->uncompressedData;
    struct RastPort rp;
    LONG x, y;
    ULONG stride = RAWIDTH(data->width);
    struct TagItem tags[] = {{OBP_Precision, PRECISION_IMAGE}, {TAG_DONE, 0}};

    data->colorMap = _screen(obj)->ViewPort.ColorMap;
    data->ditheredBitmap = AllocBitMap(data->width, data->height,
        data->screenDepth, BMF_CLEAR, NULL);
    if (!data->ditheredBitmap) goto failed;
    if (data->format == MUIV_Pixmap_Format_ARGB32 || data->alpha != 0xffffffffUL)
    {
        data->ditheredMask = AllocVec(stride * data->height,
            MEMF_CHIP | MEMF_CLEAR);
        if (!data->ditheredMask) goto failed;
    }
    InitRastPort(&rp);
    rp.BitMap = data->ditheredBitmap;
    for (y = 0; y < data->height; y++)
    {
        UBYTE *mask = data->ditheredMask ?
            (UBYTE *)data->ditheredMask + y * stride : NULL;
        for (x = 0; x < data->width; x++)
        {
            ULONG index, rgb, alpha = 255;
            if (data->format == MUIV_Pixmap_Format_CLUT8)
            {
                index = *source++;
                rgb = colors[index];
            }
            else
            {
                ULONG red, green, blue;
                if (data->format == MUIV_Pixmap_Format_ARGB32) alpha = *source++;
                red = *source++; green = *source++; blue = *source++;
                if (data->clut)
                {
                    ULONG i, best = ~0UL;
                    index = 0;
                    for (i = 0; i < 256; i++)
                    {
                        LONG dr = (LONG)((colors[i] >> 16) & 255) - red;
                        LONG dg = (LONG)((colors[i] >> 8) & 255) - green;
                        LONG db = (LONG)(colors[i] & 255) - blue;
                        ULONG error = dr * dr + dg * dg + db * db;
                        if (error < best) { best = error; index = i; }
                        if (!error) break;
                    }
                }
                else
                    index = ((red * 7 + 127) / 255 << 5) |
                            ((green * 7 + 127) / 255 << 2) |
                            ((blue * 3 + 127) / 255);
                rgb = colors[index];
            }
            alpha = (alpha * (data->alpha >> 24) + 127) / 255;
            if (mask)
            {
                if (alpha <= coverage[(y & 3) * 4 + (x & 3)] * 16 + 7)
                    continue;
                mask[x >> 3] |= 0x80 >> (x & 7);
            }
            if (data->ditheredPenMap[index] < 0)
            {
                data->ditheredPenMap[index] = ObtainBestPenA(data->colorMap,
                    ((rgb >> 16) & 255) * 0x01010101UL,
                    ((rgb >> 8) & 255) * 0x01010101UL,
                    (rgb & 255) * 0x01010101UL, tags);
                if (data->ditheredPenMap[index] < 0) goto failed;
            }
            SetAPen(&rp, data->ditheredPenMap[index]);
            WritePixel(&rp, x, y);
        }
    }
    return TRUE;
failed:
    FreeRendered(data);
    return FALSE;
}

static BOOL DecompressRLE(struct IClass *cl, Object *obj,
    ULONG uncompressedSize)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    const UBYTE *source = data->data;
    ULONG input = data->compressedSize, output = uncompressedSize;
    UBYTE *buffer = AllocVec(output, MEMF_SHARED), *dest = buffer;
    if (!buffer) return FALSE;
    while (input && output)
    {
        ULONG count;
        UBYTE control = *source++;
        input--;
        count = (control & 0x80) ? (control & 0x7f) + 2 : control + 1;
        if (count > output) break;
        if (control & 0x80)
        {
            if (!input) break;
            memset(dest, *source++, count);
            input--;
        }
        else
        {
            if (count > input) break;
            memcpy(dest, source, count);
            source += count;
            input -= count;
        }
        dest += count;
        output -= count;
    }
    if (input || output) { FreeVec(buffer); return FALSE; }
    data->uncompressedData = buffer;
    data->ownsData = TRUE;
    return TRUE;
}

static BOOL DecompressBZip2(struct IClass *cl, Object *obj,
    ULONG uncompressedSize)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    APTR buffer = AllocVec(uncompressedSize, MEMF_SHARED);
    bz_stream stream;
    int result;
    if (!buffer) return FALSE;
    memset(&stream, 0, sizeof(stream));
    #ifdef ZUNE68_GCC_NATIVE
    result = BZ2_bzDecompressInitBounded(&stream, 0, 1, uncompressedSize);
#else
    result = BZ2_bzDecompressInit(&stream, 0, 1);
#endif
    if (result == BZ_OK)
    {
        stream.next_in = data->data;
        stream.avail_in = data->compressedSize;
        stream.next_out = buffer;
        stream.avail_out = uncompressedSize;
        result = BZ2_bzDecompress(&stream);
        if (result != BZ_STREAM_END || stream.avail_out || stream.avail_in)
            result = BZ_DATA_ERROR;
        BZ2_bzDecompressEnd(&stream);
    }
    if (result != BZ_STREAM_END) { FreeVec(buffer); return FALSE; }
    data->uncompressedData = buffer;
    data->ownsData = TRUE;
    return TRUE;
}

static BOOL DecompressImage(struct IClass *cl, Object *obj)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    ULONG bytes, size;
    if (data->uncompressedData) return TRUE;
    if (!data->data || data->width <= 0 || data->height <= 0 ||
        data->width > 32767 || data->height > 32767) return FALSE;
    switch (data->format)
    {
    case MUIV_Pixmap_Format_CLUT8: bytes = 1; break;
    case MUIV_Pixmap_Format_RGB24: bytes = 3; break;
    case MUIV_Pixmap_Format_ARGB32: bytes = 4; break;
    default: return FALSE;
    }
    size = (ULONG)data->width * data->height;
    if (size > 0x7fffffffUL / bytes) return FALSE;
    size *= bytes;
    if (data->compression == MUIV_Pixmap_Compression_None)
    {
        data->uncompressedData = data->data;
        return TRUE;
    }
    if (!data->compressedSize) return FALSE;
    switch (data->compression)
    {
    case MUIV_Pixmap_Compression_RLE: return DecompressRLE(cl, obj, size);
    case MUIV_Pixmap_Compression_BZip2: return DecompressBZip2(cl, obj, size);
    default: return FALSE;
    }
}

IPTR Pixmap__MUIM_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, (Msg)msg)) return FALSE;
    data->setup = TRUE;
    data->screenDepth = GetBitMapAttr(_screen(obj)->RastPort.BitMap, BMA_DEPTH);
    if (DecompressImage(cl, obj) && data->screenDepth <= 8)
        DitherImage(cl, obj);
    SetSuperAttrs(cl, obj, MUIA_FillArea, TRUE, TAG_DONE);
    return TRUE;
}

IPTR Pixmap__MUIM_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    FreeRendered(data);
    data->setup = FALSE;
    return DoSuperMethodA(cl, obj, msg);
}

IPTR Pixmap__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);

    DoSuperMethodA(cl, obj, (Msg) msg);

    msg->MinMaxInfo->MinWidth += data->width;
    msg->MinMaxInfo->MinHeight += data->height;
    msg->MinMaxInfo->DefWidth += data->width;
    msg->MinMaxInfo->DefHeight += data->height;
    msg->MinMaxInfo->MaxWidth += data->width;
    msg->MinMaxInfo->MaxHeight += data->height;

    return 0;
}


#ifdef ZUNE68_GCC_NATIVE
/* Classic CyberGraphX has no alpha-blending vector and uses 16-bit
 * strides. Keep source offsets wide and bound temporary storage even
 * when drawing a small section of a very wide image.
 */
static void DrawTrueColor(struct Pixmap_DATA *data, struct RastPort *rp,
    LONG sx, LONG sy, LONG width, LONG height, LONG dx, LONG dy)
{
    const ULONG *colors = data->clut ? data->clut : defaultColorMap;
    ULONG bytes = data->format == MUIV_Pixmap_Format_CLUT8 ? 1 :
        data->format == MUIV_Pixmap_Format_RGB24 ? 3 : 4;
    ULONG stride = (ULONG)data->width * bytes;
    ULONG globalAlpha = data->alpha >> 24;
    ULONG x, y, count, i, channel;
    UBYTE *row;
    if (!globalAlpha) return;
    if (globalAlpha == 255 && data->format == MUIV_Pixmap_Format_CLUT8)
    {
        WriteLUTPixelArray(data->uncompressedData, sx, sy, stride, rp,
            (APTR)colors, dx, dy, width, height, CTABFMT_XRGB8);
        return;
    }
    if (globalAlpha == 255 && data->format == MUIV_Pixmap_Format_RGB24)
    {
        if (stride <= 65535)
            WritePixelArray(data->uncompressedData, sx, sy, stride, rp,
                dx, dy, width, height, RECTFMT_RGB);
        else
            for (y = 0; y < height; y++)
                for (x = 0; x < width; x += count)
                {
                    count = MIN((ULONG)width - x, 16383);
                    WritePixelArray((UBYTE *)data->uncompressedData +
                        ((ULONG)sy + y) * stride + ((ULONG)sx + x) * 3,
                        0, 0, count * 3, rp, dx + x, dy + y,
                        count, 1, RECTFMT_RGB);
                }
        return;
    }
    row = AllocVec(MIN((ULONG)width, 256) * 4, MEMF_ANY);
    if (!row) return;
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x += count)
        {
            const UBYTE *pixel = (UBYTE *)data->uncompressedData +
                ((ULONG)sy + y) * stride + ((ULONG)sx + x) * bytes;
            count = MIN((ULONG)width - x, 256);
            /* Clipped pixels may not be read; keep their scratch bytes
             * initialized. The matching write uses the same clipping.
             */
            memset(row, 0, count * 4);
            ReadPixelArray(row, 0, 0, count * 4, rp, dx + x, dy + y,
                count, 1, RECTFMT_ARGB);
            for (i = 0; i < count; i++, pixel += bytes)
            {
                ULONG alpha = globalAlpha;
                ULONG rgb;
                if (bytes == 1) rgb = colors[*pixel];
                else
                {
                    const UBYTE *color = pixel + (bytes == 4);
                    rgb = ((ULONG)color[0] << 16) |
                          ((ULONG)color[1] << 8) | color[2];
                    if (bytes == 4)
                        alpha = (pixel[0] * alpha + 127) / 255;
                }
                for (channel = 1; channel < 4; channel++)
                    row[i * 4 + channel] =
                        (((rgb >> ((3 - channel) * 8)) & 255) * alpha +
                         row[i * 4 + channel] * (255 - alpha) + 127) / 255;
                row[i * 4] = 255;
            }
            WritePixelArray(row, 0, 0, count * 4, rp, dx + x, dy + y,
                count, 1, RECTFMT_ARGB);
        }
    FreeVec(row);
}
#endif

static void DrawPixmapSection(struct IClass *cl, Object *obj, LONG sx,
    LONG sy, LONG sw, LONG sh, struct MUI_RenderInfo *mri, LONG dx, LONG dy)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    struct RastPort *rp;
    if (!mri || !mri->mri_RastPort || sx < 0 || sy < 0 ||
        sx >= data->width || sy >= data->height || sw <= 0 || sh <= 0)
        return;
    sw = MIN(sw, data->width - sx);
    sh = MIN(sh, data->height - sy);
    rp = mri->mri_RastPort;

    if (data->screenDepth <= 8 && data->ditheredBitmap != NULL)
    {
        // CyberGraphics cannot blit raw data through a mask, therefore we
        // have to take this ugly workaround and take the detour using a
        // bitmap.
        if (data->ditheredMask != NULL)
        {
            BltMaskBitMapRastPort(data->ditheredBitmap, sx, sy, rp, dx, dy,
                sw, sh, (ABC | ABNC | ANBC), data->ditheredMask);
        }
        else
        {
            BltBitMapRastPort(data->ditheredBitmap, sx, sy, rp, dx, dy, sw,
                sh, (ABC | ABNC));
        }
    }
    else if (data->screenDepth > 8 && CyberGfxBase && data->uncompressedData != NULL)
    {
#ifdef ZUNE68_GCC_NATIVE
        DrawTrueColor(data, rp, sx, sy, sw, sh, dx, dy);
#else
        switch (data->format)
        {
        case MUIV_Pixmap_Format_CLUT8:
            WriteLUTPixelArray(data->uncompressedData, sx, sy, data->width,
                rp, data->clut ? data->clut : (APTR)defaultColorMap,
                dx, dy, sw, sh, CTABFMT_XRGB8);
            break;

        case MUIV_Pixmap_Format_RGB24:
            WritePixelArray(data->uncompressedData, sx, sy, data->width * 3,
                rp, dx, dy, sw, sh, RECTFMT_RGB);
            break;

        case MUIV_Pixmap_Format_ARGB32:
            WritePixelArrayAlpha(data->uncompressedData, sx, sy,
                data->width * 4, rp, dx, dy, sw, sh, data->alpha);
            break;
        }
#endif
    }
    else
    {
        // just draw a black cross in case we got no valid pixmap
        SetAPen(rp, _pens(obj)[MPEN_TEXT]);
        Move(rp, _mleft(obj), _mtop(obj));
        Draw(rp, _mright(obj), _mbottom(obj));
        Move(rp, _mleft(obj), _mbottom(obj));
        Draw(rp, _mright(obj), _mtop(obj));
    }
}

IPTR Pixmap__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);

    DoSuperMethodA(cl, obj, (Msg) msg);

    if (msg->flags & MADF_DRAWOBJECT)
    {
        if (data->data != NULL)
        {
            int w = MIN(_mwidth(obj), data->width);
            int h = MIN(_mheight(obj), data->height);

            if (w > 0 && h > 0)
                DrawPixmapSection(cl, obj, 0, 0, w, h, muiRenderInfo(obj),
                    _mleft(obj), _mtop(obj));
        }


    }

    return 0;
}


IPTR Pixmap__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    BOOL refresh = FALSE;
    BOOL decompress = FALSE;
    struct TagItem *tag, *tags;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Pixmap_Data:
            data->data = (APTR) tag->ti_Data;
            decompress = TRUE;
            refresh = TRUE;
            break;
        case MUIA_Pixmap_Format:
            data->format = tag->ti_Data;
            decompress = TRUE;
            refresh = TRUE;
            break;
        case MUIA_Pixmap_Width:
            data->width = tag->ti_Data;
            decompress = TRUE;
            refresh = TRUE;
            break;
        case MUIA_Pixmap_Height:
            data->height = tag->ti_Data;
            decompress = TRUE;
            refresh = TRUE;
            break;
        case MUIA_Pixmap_CLUT:
            data->clut = (APTR) tag->ti_Data;
            refresh = TRUE;
            break;
        case MUIA_Pixmap_Alpha:
            data->alpha = tag->ti_Data;
            refresh = TRUE;
            break;
        case MUIA_Pixmap_Compression:
            data->compression = tag->ti_Data;
            decompress = TRUE;
            refresh = TRUE;
            break;
        case MUIA_Pixmap_CompressedSize:
            data->compressedSize = tag->ti_Data;
            decompress = TRUE;
            refresh = TRUE;
            break;
        }
    }

    if (refresh == TRUE)
    {
        // obtain the new image data
        FreeRendered(data);
        if (decompress) FreeImage(cl, obj);
        if (!DecompressImage(cl, obj)) return FALSE;
    }

    if (refresh == TRUE)
    {
        if (data->setup && data->screenDepth <= 8) DitherImage(cl, obj);
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }

    DoSuperMethodA(cl, obj, (Msg) msg);

    // signal success, this is checked by Rawimage.mcc
    return TRUE;
}


IPTR Pixmap__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    IPTR *store = msg->opg_Storage;

    switch (((struct opGet *)msg)->opg_AttrID)
    {
    case MUIA_Pixmap_Data:
        *store = (IPTR) data->data;
        return TRUE;

    case MUIA_Pixmap_Format:
        *store = data->format;
        return TRUE;

    case MUIA_Pixmap_Width:
        *store = (IPTR) data->width;
        return TRUE;

    case MUIA_Pixmap_Height:
        *store = (IPTR) data->height;
        return TRUE;

    case MUIA_Pixmap_CLUT:
        *store = (IPTR) data->clut;
        return TRUE;

    case MUIA_Pixmap_Alpha:
        *store = data->alpha;
        return TRUE;

    case MUIA_Pixmap_Compression:
        *store = data->compression;
        return TRUE;

    case MUIA_Pixmap_CompressedSize:
        *store = data->compressedSize;
        return TRUE;

    case MUIA_Pixmap_UncompressedData:
        DecompressImage(cl, obj);
        *store = (IPTR) data->uncompressedData;
        return TRUE;
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}


IPTR Pixmap__MUIM_Layout(struct IClass *cl, Object *obj, Msg msg)
{
    struct Pixmap_DATA *data = INST_DATA(cl, obj);
    ULONG rc = DoSuperMethodA(cl, obj, (Msg) msg);

    if (data->leftOffset < 0)
        _left(obj) =
            _right(_parent(obj)) - _width(obj) + 1 + 1 + data->leftOffset;
    else
        _left(obj) += data->leftOffset;

    if (data->topOffset < 0)
        _top(obj) =
            _bottom(_parent(obj)) - _height(obj) + 1 + 1 + data->topOffset;
    else
        _top(obj) += data->topOffset;

    return rc;
}


IPTR Pixmap__MUIM_Pixmap_DrawSection(struct IClass *cl, Object *obj,
    struct MUIP_Pixmap_DrawSection *msg)
{
    BOOL success;

    if ((success = DecompressImage(cl, obj)) == TRUE)
    {
        DrawPixmapSection(cl, obj, msg->sx, msg->sy, msg->sw, msg->sh,
            msg->mri, msg->dx, msg->dy);
    }

    return 0;
}


#if ZUNE_BUILTIN_PIXMAP
BOOPSI_DISPATCHER(IPTR, Pixmap_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Pixmap__OM_NEW(cl, obj, (APTR) msg);
    case OM_DISPOSE:
        return Pixmap__OM_DISPOSE(cl, obj, (APTR) msg);
    case OM_SET:
        return Pixmap__OM_SET(cl, obj, (APTR) msg);
    case OM_GET:
        return Pixmap__OM_GET(cl, obj, (APTR) msg);
    case MUIM_Draw:
        return Pixmap__MUIM_Draw(cl, obj, (APTR) msg);
    case MUIM_Setup:
        return Pixmap__MUIM_Setup(cl, obj, (APTR) msg);
    case MUIM_Cleanup:
        return Pixmap__MUIM_Cleanup(cl, obj, (APTR) msg);
    case MUIM_AskMinMax:
        return Pixmap__MUIM_AskMinMax(cl, obj, (APTR) msg);
    case MUIM_Layout:
        return Pixmap__MUIM_Layout(cl, obj, (APTR) msg);
    case MUIM_Pixmap_DrawSection:
        return Pixmap__MUIM_Pixmap_DrawSection(cl, obj, (APTR) msg);
    }

    return DoSuperMethodA(cl, obj, (APTR) msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Pixmap_desc =
{
    MUIC_Pixmap,
    MUIC_Area,
    sizeof(struct Pixmap_DATA),
    (void *)Pixmap_Dispatcher
};
#endif

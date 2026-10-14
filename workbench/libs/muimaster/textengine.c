/*
    Copyright (C) 2002-2026, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.

*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <exec/types.h>
#include <exec/memory.h>

#include <clib/alib_protos.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/cybergraphics.h>

#include "mui.h"
#include "textengine.h"
#include "imspec.h"
#include "support.h"
#include "listimage.h"

#include "muimaster_intern.h"

//#define MYDEBUG 1
#include "debug.h"

extern struct Library *MUIMasterBase;

/* A bit of explanation:
* The most important thing, after the datastructure, is the bounds
* calculation (especially chunk width).
* It will determine where to draw. Drawing is then straightforward.
* From the beginning:
* the input string is parsed line by line. Each line is formed of chunks
* of symbols having the same rendering style. As a special case,
* HiChar is a single letter chunk underlined like MUI 3.9 (ZTEXT_STYLE_HICHAR);
* a HiIndex character has the underlined soft style.
* Size calculation is done by calculating for each line the width and
* height of each chunk, then line width is the sum of widths and line height
* is the max height. *Text width will be max line width, and text height
* will be the sum of line heights.* Remember group layout.
* Chunk width is affected by soft style of text, and maybe other params.
* Drawing is done line by line, chunk by chunk, incrementing each time
* the offsets with chunk width or line height.
*
* Italic is looking ugly, no ?
*/

#define ZTL_LEFT    1
#define ZTL_CENTER  2
#define ZTL_RIGHT   3

struct zune_context
{
    ZText *ztext;               /* owner of new lines and chunks */
    LONG dripen;
    LONG pen;
    ULONG style;
    UBYTE align;

    ZTextLine *line;
    CONST_STRPTR text_start;
    CONST_STRPTR text;
    CONST_STRPTR imspec;
    Object *obj;                /* Area subclass, see List_CreateImage */
    struct MUI_AlphaData *alpha_data;
};

ZText *zune_text_new(CONST_STRPTR preparse, CONST_STRPTR content,
    int argtype, TEXT argbyte);
char *zune_text_iso_string(ZText * text);
void zune_text_destroy(ZText * text);
void zune_text_chunk_new(struct zune_context *zc);
static int strlenlf(const char *str);
static CONST_STRPTR parse_escape_code(ZTextLine * ztl,
    struct zune_context *zc, CONST_STRPTR s);
static ZTextLine *zune_text_parse_line(CONST_STRPTR * s,
    struct zune_context *zc, int *argtype, int arg);
void zune_text_get_bounds(ZText * text, Object * obj);
void zune_text_draw(ZText * text, Object * obj, WORD left, WORD right,
    WORD top);
void zune_text_draw_cursor(ZText * text, Object * obj, WORD left,
    WORD right, WORD top, LONG cursorx, LONG cursory);
void zune_text_draw_single(ZText * text, Object * obj, WORD left,
    WORD right, WORD top, LONG xpos, LONG ypos, BOOL cursor);
int zune_text_get_char_pos(ZText * text, Object * obj, LONG x, LONG y,
    struct ZTextLine **line_ptr, struct ZTextChunk **chunk_ptr,
    int *offset_ptr, int *len_ptr);
int zune_get_xpos_of_line(ZText * text, Object * obj, LONG y, LONG xpixel);
int zune_text_get_lines(ZText * text);
int zune_make_cursor_visible(ZText * text, Object * obj, LONG cursorx,
    LONG cursory, LONG left, LONG top, LONG right, LONG bottom);
int zune_text_merge(ZText * text, Object * obj, int x, int y,
    ZText * tomerge);


/* Most texts are one line with a chunk or two. Lines and chunks come from
 * space inline after the ZText, so parsing one usually takes a single
 * allocation; texts are re-parsed on every change and list entry draw. */
#define ZTEXT_ARENA_SIZE 192

static APTR ztext_alloc(ZText *text, ULONG size)
{
    APTR mem;

    size = (size + 3) & ~3UL;
    if (text->arena_next + size <= text->arena_end)
    {
        mem = text->arena_next;
        text->arena_next += size;
        memset(mem, 0, size);
        return mem;
    }
    return mui_alloc(size);
}

static void ztext_free(ZText *text, APTR mem)
{
    if ((UBYTE *)mem < (UBYTE *)(text + 1) || (UBYTE *)mem >= text->arena_end)
        mui_free(mem);
}

/**************************************************************************
...
**************************************************************************/
ZText *zune_text_new(CONST_STRPTR preparse, CONST_STRPTR content,
    int argtype, TEXT argbyte)
{
    ZText *text;
    /* STRPTR *lines; */
    /* int i; */
    STRPTR dup_content;
    char local_content[128];
    CONST_STRPTR buf;
    int preparse_len;
    struct zune_context zc;
    int arg;

    if (!(text = AllocVec(sizeof(ZText) + ZTEXT_ARENA_SIZE, MEMF_ANY)))
        return NULL;
    text->width = text->height = 0;
    text->styles = 0;
    text->nontext = 0;
    text->truncate = 0;
    NewList((struct List *)&text->lines);
    text->arena_next = (UBYTE *)(text + 1);
    text->arena_end = text->arena_next + ZTEXT_ARENA_SIZE;

    if (!content)
        content = "";
    preparse_len = preparse ? strlen(preparse) : 0;
    /* The parser only needs a temporary copy; keep short ones on stack */
    if (preparse_len + strlen(content) < sizeof(local_content))
        dup_content = (STRPTR)local_content;
    else if (!(dup_content = mui_alloc(preparse_len + strlen(content) + 1)))
    {
        mui_free(text);
        return NULL;
    }

    if (preparse_len)
        strcpy(dup_content, preparse);
    strcpy(&dup_content[preparse_len], content);

    buf = dup_content;

    zc.ztext = text;
    zc.pen = -1;
    zc.dripen = -1;
    zc.style = FS_NORMAL;
    zc.align = ZTL_LEFT;
    zc.imspec = NULL;
    zc.obj = NULL;
    zc.alpha_data = NULL;

    if (argtype == ZTEXT_ARG_HICHAR)
    {
        /* store lower and upper case in arg */
        arg = ToLower(argbyte) | (ToUpper(argbyte) << 8);
    }
    else
        arg = argbyte;

    /* the other elements are done by other functions */

    while (1)
    {
        struct ZTextLine *ztl =
            zune_text_parse_line(&buf, &zc, &argtype, arg);
        if (ztl)
            AddTail((struct List *)&text->lines, (struct Node *)ztl);
        else
            break;
        if (*buf == '\n')
        {
            buf++;
            continue;
        }
        if (!(*buf))
            break;
    }

    if (dup_content != (STRPTR)local_content)
        mui_free(dup_content);
    return text;
}

/**************************************************************************
If a ZText holds nothing but the plain string s (one line, one text
chunk), return that chunk, so that zune_text_set_plain() can give it a
new plain string without parsing; otherwise NULL.
**************************************************************************/
ZTextChunk *zune_text_plain_chunk(ZText *text, CONST_STRPTR s)
{
    ZTextLine *line;
    ZTextChunk *chunk;

    if (!text || !s || !*s)
        return NULL;
    line = (ZTextLine *)text->lines.mlh_Head;
    if (!line->node.mln_Succ || line->node.mln_Succ->mln_Succ)
        return NULL;
    chunk = (ZTextChunk *)line->chunklist.mlh_Head;
    if (!chunk->node.mln_Succ || chunk->node.mln_Succ->mln_Succ
        || !chunk->str || chunk->spec || chunk->obj || chunk->alpha_data
        || strcmp(chunk->str, s))
        return NULL;
    return chunk;
}

/**************************************************************************
Point the only chunk of a plain ZText (zune_text_plain_chunk) at s, a
plain string of len bytes the caller keeps alive, and measure it: the
same result as zune_text_get_bounds() for such a text, without the
loops and the RastPort copy. The object must be set up.
**************************************************************************/
void zune_text_set_plain(ZText *text, ZTextChunk *chunk, STRPTR s,
    LONG len, Object *obj)
{
    ZTextLine *line = (ZTextLine *)text->lines.mlh_Head;
    struct TextFont *font = _font(obj);
    struct RastPort *rp = _rp(obj);
    struct RastPort copy;

    chunk->str = s;
    chunk->clen = len;
    /* a bold or italic line has an overhang to measure */
    if (chunk->style & (FSF_BOLD | FSF_ITALIC))
    {
        zune_text_get_bounds(text, obj);
        return;
    }
    /* TextLength only reads the RastPort; a copy is needed only when
     * its font or soft style is not the object's plain one */
    if (rp->Font != font || rp->AlgoStyle)
    {
        copy = *rp;
        if (copy.Font != font)
            SetFont(&copy, font);
        if (copy.AlgoStyle)
            SetSoftStyle(&copy, FS_NORMAL, AskSoftStyle(&copy));
        rp = &copy;
    }
    chunk->cheight = font->tf_YSize;
    chunk->cwidth = TextLength(rp, s, len);
    line->lheight = font->tf_YSize;
    line->lwidth = chunk->cwidth;
    line->centreskip = 0;
    text->width = chunk->cwidth;
    text->height = font->tf_YSize;
    text->styles = chunk->style;
    text->nontext = 0;
    if (text->styles & (FSF_BOLD | FSF_ITALIC))
        text->width++;
}

/**************************************************************************
Completely frees a ZText
**************************************************************************/
void zune_text_destroy(ZText * text)
{
    struct ZTextLine *ztl;
    struct ZTextChunk *ztc;

    while ((ztl = (struct ZTextLine *)RemTail((struct List *)&text->lines)))
    {
        while ((ztc =
                (struct ZTextChunk *)RemTail((struct List *)&ztl->
                    chunklist)))
        {
            /* String chunks hold their text inline (zune_text_chunk_new) */
            if (ztc->spec)
            {
                FreeVec((APTR) ztc->spec);
                if (ztc->image)
                {
                    zune_imspec_cleanup(ztc->image);
                }
            }
            ztext_free(text, ztc);
        }
        ztext_free(text, ztl);
    }
    mui_free(text);
}

/************************************************************/
/* Parsing */
/************************************************************/


/**************************************************************************
Allocate and initialize a new text chunk and add it to the list.
The context contains the values for the chunk to be created.
**************************************************************************/
void zune_text_chunk_new(struct zune_context *zc)
{
    ZTextChunk *ztc;

    /* No char has been processed so we needn't allocate anything */
    if (zc->text == zc->text_start)
        return;

    /* A text chunk carries its string in the same allocation, which
     * halves allocator traffic when lists re-parse entries per draw */
    if (!zc->imspec && !zc->obj && !zc->alpha_data)
    {
        LONG len = zc->text - zc->text_start;

        if (!(ztc = ztext_alloc(zc->ztext, sizeof(ZTextChunk) + len + 1)))
            return;
        ztc->str = (char *)(ztc + 1);
        ztc->clen = len;
        memcpy(ztc->str, zc->text_start, len);
        ztc->str[len] = 0;
    }
    else if (!(ztc = ztext_alloc(zc->ztext, sizeof(ZTextChunk))))
        return;

    ztc->style = zc->style;
    ztc->dripen = zc->dripen;
    ztc->pen = zc->pen;
    if (zc->imspec)
    {
        D(bug("zune_text_chunk_new: imspec %s\n", zc->imspec));
        ztc->spec = zc->imspec;
        zc->imspec = NULL;

        AddTail((struct List *)&zc->line->chunklist, (struct Node *)ztc);
    }
    else if (zc->obj)
    {
        ztc->obj = zc->obj;
        zc->obj = NULL;

        AddTail((struct List *)&zc->line->chunklist, (struct Node *)ztc);
    }
    else if (zc->alpha_data)
    {
        ztc->alpha_data = zc->alpha_data;
        zc->alpha_data = NULL;

        AddTail((struct List *)&zc->line->chunklist, (struct Node *)ztc);
    }
    else
    {
        D(bug("zune_text_chunk_new: string %s\n", ztc->str));

        AddTail((struct List *)&zc->line->chunklist, (struct Node *)ztc);
    }
}


/**************************************************************************
Calculates the length of the string excluding line feed or 0 byte
**************************************************************************/
static int strlenlf(const char *str)
{
    char c;
    int len = 0;
    while ((c = *str))
    {
        if (c == '\n')
            break;
        len++;
        str++;
    }
    return len;
}

/**************************************************************************
Note: Only alignments at the beginning of a line should affect this line
(tested in MUI)
**************************************************************************/
static CONST_STRPTR parse_escape_code(ZTextLine * ztl,
    struct zune_context *zc, CONST_STRPTR s)
{
    unsigned char c;
    c = *s;

    zune_text_chunk_new(zc);
    s++;                        /* s now points after the command */
    zc->text_start = zc->text = s;

    switch (c)
    {
    case 'c':
        zc->align = ztl->align = ZTL_CENTER;
        break;
    case 'r':
        zc->align = ztl->align = ZTL_RIGHT;
        break;
    case 'l':
        zc->align = ztl->align = ZTL_LEFT;
        break;
    case 'n':
        zc->style = FS_NORMAL;
        break;
    case 'U':
        zc->style &= ~FSF_UNDERLINED;
        break;
    case 'u':
        zc->style |= FSF_UNDERLINED;
        break;
    case 'b':
        zc->style |= FSF_BOLD;
        break;
    case 'i':
        zc->style |= FSF_ITALIC;
        break;
    case 'I':                  /* image spec */
        {
            char *t;

            if (*s != '[')
                break;
            s++;
            /* s points on the first char of imagespec.
             *  Extract it to the trailing ']'.
             */
            t = strchr(s, ']');
            if (t == NULL)
                break;
            *t = 0;
            D(bug("imspec = %s\n", s));
            zc->imspec = StrDup(s);
            *t = ']';
            zc->text = t;
            zune_text_chunk_new(zc);
            zc->text_start = t + 1;
            break;
        }
    case 'O':                  /* pointer from List_CreateImage */
        {
            struct ListImage *li;
            IPTR tmp;
            char *t;

            if (*s != '[')
                break;
            s++;
            /* s points on the first char of the hex-encoded pointer created
             * by MUIM_List_CreateImage. Extract it to the trailing ']'.
             */
            t = strchr(s, ']');
            if (t == NULL)
                break;
            *t = 0;
            if (HexToIPTR(s, &tmp) != -1)
            {
                D(bug("listimage = %p\n", tmp));
                if (tmp == 0)
                {
                    /* According to the MUI autodocs, the result of
                     * CreateImage may be NULL, but then \33O[] has to
                     * simply draw nothing, so it shouldn't be considered
                     * an error.
                     * Without this, AROS crashed, if 00000000 was used.
                     */
                    *t = ']';
                    zc->text = t + 1;
                    zc->text_start = t + 1;
                    break;
                }
                li = (struct ListImage *)tmp;
                zc->obj = li->obj;
            }
            *t = ']';
            zc->text = t;
            zune_text_chunk_new(zc);
            zc->text_start = t + 1;
            break;
        }
    case 'A':                  /* pointer to MUI_AlphaData */
        {
            IPTR tmp;
            char *t;

            if (*s != '[')
                break;
            s++;
            /* s points on the first char of the hex-encoded pointer created
             * by MUIM_List_CreateImage. Extract it to the trailing ']'.
             */
            t = strchr(s, ']');
            if (t == NULL)
                break;
            *t = 0;
            if (HexToIPTR(s, &tmp) != -1)
            {
                D(bug("image = %p\n", tmp));
                if (tmp == 0)
                {
                    /* Draw nothing if image pointer is NULL, as MUI 4 does
                     * (although undocumented)
                     */
                    *t = ']';
                    zc->text = t + 1;
                    zc->text_start = t + 1;
                    break;
                }
                zc->alpha_data = (struct MUI_AlphaData *)tmp;
            }
            *t = ']';
            zc->text = t;
            zune_text_chunk_new(zc);
            zc->text_start = t + 1;
            break;
        }
    case 'P':                  /* pen number */
        {
            LONG pen;
            STRPTR t;
            UWORD len;

            if (*s != '[')
                break;
            s++;
            /* s points on the first char of pen from ObtainPen printed as %ld.
             *  Extract it to the trailing ']'.
             */
            t = strchr(s, ']');
            if (t == NULL)
                break;
            len = t - s;
            if (len < 6)
            {
                *t = 0;
                if (StrToLong(s, &pen) == -1)
                    pen = 0;
                D(bug("pen = %ld\n", pen));
                zc->pen = pen;
                *t = ']';
            }
            zc->text = s - 1;
            zune_text_chunk_new(zc);
            zc->text_start = t + 1;
            break;
        }
    case '-':
        zc->text += strlenlf(s);
        break;                  /* disable engine */

    default:                   /* some other ESC code? */
        if (isdigit(c))         /* pen */
        {
            zc->dripen = c - '0';
        }
        break;
    }
    return zc->text;
}

/**************************************************************************
Parse a text line, and create text chunks.
Whenever an escape code is encountered, the current chunk
is terminated, and the escape parameters are stacked until
a normal character is read. Then a new chunk begins with this char.

s_ptr is a pointer to the text string. After this function has been
executed it is changed to point to the end of the string (either the '\n'
or 0 byte)

This is probably a function to rewrite to deal with wide chars.

Note that the contents in s_ptr is overwritten.
**************************************************************************/
static ZTextLine *zune_text_parse_line(CONST_STRPTR * s_ptr,
    struct zune_context *zc, int *argtype, int arg)
{
    CONST_STRPTR s;
    UBYTE c;

    ZTextLine *ztl;

    if (!s_ptr)
        return NULL;
    if (!(s = *s_ptr))
        return NULL;
    if (!(ztl = ztext_alloc(zc->ztext, sizeof(ZTextLine))))
        return NULL;
    NewList((struct List *)&ztl->chunklist);

    ztl->align = zc->align;
    zc->text_start = zc->text = s;
    zc->line = ztl;

    while ((c = *s))
    {
        if (c == '\n')
            break;

        if (c == '\33')
        {
            s++;
            if (*s == 0)
                break;
            s = parse_escape_code(ztl, zc, s);
            continue;
        }

        if (*argtype == ZTEXT_ARG_HICHAR && (((arg & 0xff) == c)
                || (((arg >> 8) & 0xff) == c)))
        {
            ULONG styleback = zc->style;
            zune_text_chunk_new(zc);
            zc->style |= ZTEXT_STYLE_HICHAR;
            zc->text_start = s;
            zc->text = ++s;
            zune_text_chunk_new(zc);
            zc->text_start = s;
            zc->style = styleback;
            *argtype = ZTEXT_ARG_NONE;
            continue;
        }

        if (*argtype == ZTEXT_ARG_HICHARIDX && arg == c)
        {
            ULONG styleback = zc->style;
            /* underline next char; unlike HiChar, MUI 3.9 uses the
             * soft style here (tests/native/ulprobe.c) */
            zune_text_chunk_new(zc);
            zc->style |= FSF_UNDERLINED;
            zc->text_start = ++s;
            zc->text = ++s;
            zune_text_chunk_new(zc);
            zc->text_start = s;
            zc->style = styleback;
            *argtype = ZTEXT_ARG_NONE;
        }

        zc->text = ++s;
    }                           /* while */
    zune_text_chunk_new(zc);
    *s_ptr = s;
    return ztl;
}

/************************************************************/
/* Bounds */
/************************************************************/

void zune_text_get_bounds(ZText * text, Object * obj)
{
    struct RastPort copy, *rp = &copy;
    struct TextFont *font;

    ZTextLine *line_node;
    ZTextChunk *chunk_node;

    if (!text || !obj)
        return;

    text->width = 0;
    text->height = 0;
    text->styles = 0;
    text->nontext = 0;

    font = _font(obj);
    /* Measuring needs a RastPort with the object's font in the plain
     * style. TextLength and TextExtent only read it, so the object's own
     * RastPort serves as it is when its font and style are right; a
     * copy (100 bytes) is made only to change them. Bold and italic
     * chunks below set the style on it, so they take the copy too. */
    if ((_flags(obj) & MADF_SETUP) && muiRenderInfo(obj)
        && muiRenderInfo(obj)->mri_RastPort)
    {
        if (_rp(obj)->Font == font && !_rp(obj)->AlgoStyle)
            rp = _rp(obj);
        else
        {
            copy = *_rp(obj);
            if (copy.Font != font)
                SetFont(&copy, font);
            /* Measure with the plain style, as a fresh RastPort would */
            if (copy.AlgoStyle)
                SetSoftStyle(&copy, FS_NORMAL, AskSoftStyle(&copy));
        }
    }
    else
    {
        InitRastPort(&copy);
        SetFont(&copy, font);
    }

    for (line_node = (ZTextLine *) text->lines.mlh_Head;
        line_node->node.mln_Succ;
        line_node = (ZTextLine *) line_node->node.mln_Succ)
    {
        line_node->lheight = font->tf_YSize;
        line_node->lwidth = 0;
        line_node->centreskip = 0;

        for (chunk_node = (ZTextChunk *) line_node->chunklist.mlh_Head;
            chunk_node->node.mln_Succ;
            chunk_node = (ZTextChunk *) chunk_node->node.mln_Succ)
        {
            if (!chunk_node->str)
                text->nontext = 1;
            if (chunk_node->spec)
            {
                struct MUI_MinMax minmax;

                chunk_node->image =
                    zune_imspec_setup((IPTR) chunk_node->spec,
                    muiRenderInfo(obj));
                if (!chunk_node->image)
                    return;
                zune_imspec_askminmax(chunk_node->image, &minmax);
                chunk_node->cwidth = minmax.DefWidth;
                chunk_node->cheight = minmax.DefHeight;
                line_node->lheight =
                    MAX(line_node->lheight, chunk_node->cheight);
            }
            else if (chunk_node->obj)
            {
                struct MUI_MinMax MinMax;
                struct MUIP_AskMinMax amm = { MUIM_AskMinMax, &MinMax };

                /* Area points the message at the object's own minmax and
                 * __area_finish_minmax updates it in place. */
                DoMethodA(chunk_node->obj, (Msg)&amm);
                __area_finish_minmax(chunk_node->obj, amm.MinMaxInfo);
                chunk_node->cwidth = amm.MinMaxInfo->DefWidth;
                chunk_node->cheight = amm.MinMaxInfo->DefHeight;
                line_node->lheight =
                    MAX(line_node->lheight, chunk_node->cheight);
            }
            else if (chunk_node->alpha_data)
            {
                chunk_node->cwidth = chunk_node->alpha_data->width;
                chunk_node->cheight = chunk_node->alpha_data->height;
                line_node->lheight =
                    MAX(line_node->lheight, chunk_node->cheight);
            }
            else if (chunk_node->str)
            {
                chunk_node->cheight = font->tf_YSize;
                chunk_node->cwidth =
                    TextLength(rp, chunk_node->str, chunk_node->clen);
                text->styles |= chunk_node->style;
                D(bug("zune_text_get_bounds(%s,%x) => cwidth=%d\n",
                        chunk_node->str, obj, chunk_node->cwidth));
            }
            line_node->lwidth += chunk_node->cwidth;
        }

        /* MUI 3.9 adds the overhang of a bold or italic line end, the
         * part of its TextExtent beyond the advance (tests/native/
         * textstyles.c) */
        chunk_node = (ZTextChunk *) line_node->chunklist.mlh_TailPred;
        while (chunk_node->node.mln_Pred && !chunk_node->str)
            chunk_node = (ZTextChunk *) chunk_node->node.mln_Pred;
        if (chunk_node->node.mln_Pred && chunk_node->str
            && (chunk_node->style & (FSF_BOLD | FSF_ITALIC)))
        {
            struct TextExtent te;

            if (rp != &copy)
            {
                /* the style must not touch the object's RastPort */
                copy = *rp;
                rp = &copy;
            }
            SetSoftStyle(rp, chunk_node->style & ~ZTEXT_STYLE_HICHAR,
                AskSoftStyle(rp));
            TextExtent(rp, chunk_node->str, chunk_node->clen, &te);
            SetSoftStyle(rp, FS_NORMAL, AskSoftStyle(rp));
            line_node->centreskip = te.te_Extent.MaxX - te.te_Extent.MinX
                + 1 - te.te_Width;
            line_node->lwidth += line_node->centreskip;
            /* a line in one style is centred on its advance alone, a
             * line ending in another style with the overhang
             * (tests/native/centrestyle.c) */
            if (chunk_node->node.mln_Pred->mln_Pred)
                line_node->centreskip = 0;
        }

        text->height += line_node->lheight;
        text->width = MAX(text->width, line_node->lwidth);
    }
    /* ... and one pixel to a text using bold or italic anywhere */
    if (text->styles & (FSF_BOLD | FSF_ITALIC))
        text->width++;
}

/************************************************************/
/* Drawing                                                  */
/************************************************************/

// problem is cheight being seldom 0

void zune_text_draw(ZText * text, Object * obj, WORD left, WORD right,
    WORD top)
{
    struct RastPort *rp;
    ULONG pensave;
    ULONG style = FS_NORMAL;

    ZTextLine *line_node;
    ZTextChunk *chunk_node;

    if (!text || !obj)
        return;

    D(bug("zune_text_draw(%p) %d %d %d\n", obj, left, right, top));

    rp = _rp(obj);
    if (rp->Font != _font(obj))
        SetFont(rp, _font(obj));
    /* SetSoftStyle and the AskSoftStyle it needs are not free; most
     * RastPorts already draw plain text */
    if (rp->AlgoStyle != style)
        SetSoftStyle(rp, style, AskSoftStyle(rp));

    for (line_node = (ZTextLine *) text->lines.mlh_Head;
        line_node->node.mln_Succ;
        line_node = (ZTextLine *) line_node->node.mln_Succ)
    {
        LONG x;

        if (line_node->align == ZTL_CENTER)
            x = (left + right + 1 - (line_node->lwidth
                - line_node->centreskip)) / 2;
        else if (line_node->align == ZTL_RIGHT)
            /* right-aligned the same way: the overhang may stick out
             * (tests/native/textset.c) */
            x = right - (line_node->lwidth - line_node->centreskip) + 1;
        else
            x = left;

        // like MUI, never truncates the beginning of a line
        if (x < left)
            x = left;

        //D(bug("zune_text_draw(%x,%d,%d,%d, align=%d) : x = %d\n",
        //obj, left, right, line_node->lwidth, line_node->align, x));

        for (chunk_node = (ZTextChunk *) line_node->chunklist.mlh_Head;
            chunk_node->node.mln_Succ;
            chunk_node = (ZTextChunk *) chunk_node->node.mln_Succ)
        {
            /*
             * Show/Hide stuff should be put in new api calls
             */
            if (chunk_node->image)
            {
                WORD top_im = top;
                top_im += (line_node->lheight - chunk_node->cheight) / 2;
                zune_imspec_show(chunk_node->image, obj);
                pensave = GetAPen(rp);
                zune_imspec_draw(chunk_node->image, muiRenderInfo(obj), x,
                    top_im, chunk_node->cwidth,
                    chunk_node->cheight, 0, 0, 0);
                SetAPen(rp, pensave);
                zune_imspec_hide(chunk_node->image);
            }
            else if (chunk_node->obj)
            {
                _left(chunk_node->obj) = x;
                _top(chunk_node->obj) =
                    top + (line_node->lheight - chunk_node->cheight) / 2;
                _width(chunk_node->obj) = chunk_node->cwidth;
                _height(chunk_node->obj) = chunk_node->cheight;
                DoShowMethod(chunk_node->obj);
                pensave = GetAPen(rp);
                MUI_Redraw(chunk_node->obj,
                    MADF_DRAWOBJECT | MADF_DRAWFRAME);
                SetAPen(rp, pensave);
                DoHideMethod(chunk_node->obj);
            }
            else if (chunk_node->alpha_data)
            {
#ifndef ZUNE_NO_CYBERGRAPHICS
                struct MUI_AlphaData *alpha_data = chunk_node->alpha_data;
                WORD top_im = top;
                top_im += (line_node->lheight - chunk_node->cheight) / 2;
                WritePixelArrayAlpha(alpha_data->data, 0, 0,
                    alpha_data->width * 4, rp, x, top_im, alpha_data->width,
                    alpha_data->height, 0xFFFFFFFF);
#endif
            }
            else if (chunk_node->str)
            {
                if ((chunk_node->style & ~ZTEXT_STYLE_HICHAR) != style)
                {
                    style = chunk_node->style & ~ZTEXT_STYLE_HICHAR;
                    SetSoftStyle(rp, style, 0xff);
                }
                if (chunk_node->cheight == 0)
                    Move(rp, x, _font(obj)->tf_Baseline + top);
                else
                    Move(rp, x,
                        _font(obj)->tf_Baseline + (line_node->lheight -
                            chunk_node->cheight) / 2 + top);
#if 0
                D(bug
                    ("zune_text_draw(%p) Moved to %d (baseline=%d, lh=%d, ch=%d, top=%d)\n",
                        obj,
                        _font(obj)->tf_Baseline + (line_node->lheight -
                            chunk_node->cheight) / 2 + top,
                        _font(obj)->tf_Baseline, line_node->lheight,
                        chunk_node->cheight, top));
#endif
                if (chunk_node->dripen != -1)
                {
                    D(bug("chunk_node->dripen == %d\n",
                            chunk_node->dripen));
                    SetABPenDrMd(rp,
                        _dri(obj)->dri_Pens[chunk_node->dripen], 0, JAM1);
                }
                else if (chunk_node->pen != -1)
                {
                    D(bug("chunk_node->pen == %d\n", chunk_node->pen));
                    SetABPenDrMd(rp, chunk_node->pen, 0, JAM1);
                }
                else if (rp->DrawMode != JAM1)
                {
                    SetDrMd(rp, JAM1);
                }
                {
                    LONG len = chunk_node->clen;

                    if (text->truncate && x + chunk_node->cwidth > right + 1)
                    {
                        struct TextExtent te;

                        len = (right + 1 > x) ? TextFit(rp, chunk_node->str,
                            len, &te, NULL, 1, right + 1 - x, 32767) : 0;
                    }
                    if (len > 0)
                        Text(rp, chunk_node->str, len);
                    /* MUI 3.9 underlines a shortcut one pixel short of
                     * its advance, two rows below the baseline but within
                     * the font (tests/native/ulprobe.c) */
                    if (len > 0 && (chunk_node->style & ZTEXT_STYLE_HICHAR))
                    {
                        struct TextFont *font = _font(obj);
                        WORD y = rp->cp_y - font->tf_Baseline;

                        y += MIN(font->tf_Baseline + 2, font->tf_YSize - 1);
                        RectFill(rp, x, y,
                            x + MAX(chunk_node->cwidth - 2, 0), y);
                    }
                }
            }
            x += chunk_node->cwidth;
        }
        top += line_node->lheight;
    }
}

/* Elide parsed chunks rather than cutting escape sequences in the source.
 * Text keeps its original contents, styles, images and per-line alignment. */
static LONG elide_chunk(struct RastPort *rp, const ZTextChunk *source,
    ZTextChunk *dest, LONG available, BOOL tail)
{
    struct TextExtent extent;
    LONG length;
    if (available <= 0) return 0;
    *dest = *source;
    if (source->cwidth <= available) return source->cwidth;
    if (!source->str || !source->clen || source->image || source->obj)
        return 0;
    SetSoftStyle(rp, source->style & ~ZTEXT_STYLE_HICHAR, AskSoftStyle(rp));
    length = TextFit(rp, source->str + (tail ? source->clen - 1 : 0),
        source->clen, &extent, NULL, tail ? -1 : 1, available, 32767);
    if (length <= 0) return 0;
    dest->str += tail ? source->clen - length : 0;
    dest->clen = length;
    dest->cwidth = TextLength(rp, dest->str, length);
    return dest->cwidth;
}

static void text_shortened(ZText *text, Object *obj, WORD left,
    WORD right, WORD top, LONG mode,
    void (*visit)(ZText *, Object *, WORD, WORD, WORD, APTR), APTR state)
{
    ZTextLine *source;
    LONG available = right - left + 1;
    if (!text || available <= 0) return;
    if (mode == MUIV_Text_Shorten_Hide && text->width > available) return;
    for (source = (APTR)text->lines.mlh_Head; source->node.mln_Succ;
        source = (APTR)source->node.mln_Succ)
    {
        ZText view = *text;
        ZTextLine line = *source;
        ZTextChunk *chunk, *chunks = NULL;
        struct RastPort rp = *_rp(obj);
        LONG count = 0, used = 0, suffix, i, budget, width;
        NewList((struct List *)&view.lines);
        AddTail((struct List *)&view.lines, (struct Node *)&line.node);
        view.height = line.lheight;
        if (source->lwidth <= available)
            visit(&view, obj, left, right, top, state);
        else if (mode != MUIV_Text_Shorten_Hide)
        {
            for (chunk = (APTR)source->chunklist.mlh_Head; chunk->node.mln_Succ;
                chunk = (APTR)chunk->node.mln_Succ) count++;
            chunks = AllocVec((2 * count + 1) * sizeof(*chunks), MEMF_CLEAR);
            if (!chunks)
            {
                view.truncate = TRUE;
                visit(&view, obj, left, right, top, state);
            }
            else
            {
                SetFont(&rp, _font(obj)); SetSoftStyle(&rp, 0, AskSoftStyle(&rp));
                LONG dots = TextLength(&rp, "...", 3);
                LONG remaining = MAX(0, available - dots);
                LONG prefix = mode == MUIV_Text_Shorten_ElideLeft ? 0 :
                    mode == MUIV_Text_Shorten_ElideCenter ? remaining / 2 : remaining;
                NewList((struct List *)&line.chunklist);
                line.lwidth = 0; line.centreskip = 0;
                budget = prefix;
                for (chunk = (APTR)source->chunklist.mlh_Head;
                    chunk->node.mln_Succ && budget > 0; chunk = (APTR)chunk->node.mln_Succ)
                {
                    width = elide_chunk(&rp, chunk, &chunks[used], budget, FALSE);
                    if (!width) break;
                    AddTail((struct List *)&line.chunklist, (struct Node *)&chunks[used++].node);
                    budget -= width; line.lwidth += width;
                    if (width < chunk->cwidth) break;
                }
                chunks[used].str = "..."; chunks[used].clen = 3;
                chunks[used].cwidth = dots; chunks[used].cheight = rp.TxHeight;
                chunks[used].dripen = TEXTPEN; chunks[used].pen = -1;
                AddTail((struct List *)&line.chunklist, (struct Node *)&chunks[used++].node);
                line.lwidth += dots;
                budget = mode == MUIV_Text_Shorten_ElideRight ? 0 :
                    remaining - (line.lwidth - dots);
                suffix = used;
                for (chunk = (APTR)source->chunklist.mlh_TailPred;
                    chunk->node.mln_Pred && budget > 0; chunk = (APTR)chunk->node.mln_Pred)
                {
                    width = elide_chunk(&rp, chunk, &chunks[used], budget, TRUE);
                    if (!width) break;
                    used++; budget -= width; line.lwidth += width;
                    if (width < chunk->cwidth) break;
                }
                for (i = used - 1; i >= suffix; i--)
                    AddTail((struct List *)&line.chunklist, (struct Node *)&chunks[i].node);
                view.width = line.lwidth; view.truncate = TRUE;
                visit(&view, obj, left, right, top, state);
                FreeVec(chunks);
            }
        }
        top += source->lheight;
    }
}

static void draw_view(ZText *text, Object *obj, WORD left, WORD right,
    WORD top, APTR state)
{
    zune_text_draw(text, obj, left, right, top);
}

void zune_text_draw_shortened(ZText *text, Object *obj, WORD left,
    WORD right, WORD top, LONG mode)
{
    text_shortened(text, obj, left, right, top, mode, draw_view, NULL);
}

struct MarkWalk
{
    ZText *source;
    LONG first, last, hit, mousex, mousey;
    BOOL draw;
};

/* Views made by elision point into the original parsed chunks. Map those
 * pointers back to character offsets; generated ellipses are not text. */
static LONG text_offset(ZText *text, const char *str)
{
    ZTextLine *line;
    ZTextChunk *chunk;
    LONG offset = 0;
    for (line = (APTR)text->lines.mlh_Head; line->node.mln_Succ;
        line = (APTR)line->node.mln_Succ)
    {
        for (chunk = (APTR)line->chunklist.mlh_Head; chunk->node.mln_Succ;
            chunk = (APTR)chunk->node.mln_Succ)
            if (chunk->str)
            {
                if ((IPTR)str >= (IPTR)chunk->str &&
                    (IPTR)str < (IPTR)chunk->str + chunk->clen)
                    return offset + (str - chunk->str);
                offset += chunk->clen;
            }
        offset++;
    }
    return -1;
}

static void mark_view(ZText *text, Object *obj, WORD left, WORD right,
    WORD top, APTR state)
{
    struct MarkWalk *walk = state;
    struct RastPort rp = *_rp(obj);
    ZTextLine *line;
    ZTextChunk *chunk;
    SetFont(&rp, _font(obj));
    SetDrMd(&rp, COMPLEMENT);
    for (line = (APTR)text->lines.mlh_Head; line->node.mln_Succ;
        line = (APTR)line->node.mln_Succ)
    {
        LONG x = left;
        BOOL first_chunk = TRUE;
        if (line->align == ZTL_CENTER)
            x = MAX(left, (left + right + 1 -
                (line->lwidth - line->centreskip)) / 2);
        else if (line->align == ZTL_RIGHT)
            x = MAX(left, right - (line->lwidth - line->centreskip) + 1);
        for (chunk = (APTR)line->chunklist.mlh_Head; chunk->node.mln_Succ;
            chunk = (APTR)chunk->node.mln_Succ)
        {
            LONG offset = chunk->str ? text_offset(walk->source, chunk->str) : -1;
            if (offset >= 0)
            {
                LONG i, cx = x;
                if (first_chunk && !walk->draw && walk->mousey >= top &&
                    walk->mousey < top + line->lheight) walk->hit = offset;
                first_chunk = FALSE;
                SetSoftStyle(&rp, chunk->style & ~ZTEXT_STYLE_HICHAR,
                    AskSoftStyle(&rp));
                for (i = 0; i < chunk->clen; i++)
                {
                    LONG next = x + TextLength(&rp, chunk->str, i + 1);
                    if (next > right + 1) break;
                    if (walk->draw && offset + i >= walk->first &&
                        offset + i < walk->last && next > cx)
                    {
                        LONG y1 = MAX(top, _mtop(obj));
                        LONG y2 = MIN(top + line->lheight - 1, _mbottom(obj));
                        if (y1 <= y2)
                            RectFill(&rp, cx, y1, next - 1, y2);
                    }
                    if (!walk->draw)
                    {
                        if (walk->mousey >= top + line->lheight)
                            walk->hit = offset + i + 1;
                        else if (walk->mousey >= top)
                        {
                            if (walk->mousex >= (cx + next) / 2)
                                walk->hit = offset + i + 1;
                        }
                    }
                    cx = next;
                }
            }
            x += chunk->cwidth;
        }
        top += line->lheight;
    }
}

LONG zune_text_mark(ZText *text, Object *obj, LONG mode, LONG first,
    LONG last, LONG mousex, LONG mousey, BOOL draw)
{
    struct MarkWalk walk = {text, first, last, 0, mousex, mousey, draw};
    WORD top;
    if (!text) return 0;
    top = _mtop(obj) + (_mheight(obj) - text->height) / 2;
    if (mode >= MUIV_Text_Shorten_Hide)
        text_shortened(text, obj, _mleft(obj), _mright(obj), top,
            mode, mark_view, &walk);
    else
        mark_view(text, obj, _mleft(obj), _mright(obj), top, &walk);
    return walk.hit;
}

/* Return a plain copy of a marked range, with line breaks but without
 * preparse escapes, inline objects or synthetic elision characters. */
STRPTR zune_text_copy(ZText *text, LONG first, LONG last)
{
    ZTextLine *line;
    ZTextChunk *chunk;
    LONG offset = 0, used = 0, i;
    STRPTR result;
    if (!text || first < 0 || last <= first) return NULL;
    result = AllocVec(last - first + 1, MEMF_ANY);
    if (!result) return NULL;
    for (line = (APTR)text->lines.mlh_Head; line->node.mln_Succ;
        line = (APTR)line->node.mln_Succ)
    {
        for (chunk = (APTR)line->chunklist.mlh_Head; chunk->node.mln_Succ;
            chunk = (APTR)chunk->node.mln_Succ)
            if (chunk->str)
                for (i = 0; i < chunk->clen; i++, offset++)
                    if (offset >= first && offset < last)
                        result[used++] = chunk->str[i];
        if (line->node.mln_Succ->mln_Succ)
        {
            if (offset >= first && offset < last) result[used++] = '\n';
            offset++;
        }
    }
    result[used] = 0;
    return result;
}

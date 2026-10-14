/* 
    Copyright (C) 1999, David Le Corfec.
    Copyright (C) 2002-2018, The AROS Development Team.
    All rights reserved.

*/

#ifndef _MUI_TEXTENGINE_H__
#define _MUI_TEXTENGINE_H__

/* This is the clone of the MUI text engine, used by the Text class
 * and all class needing to draw text strings.
 * 
 */

struct MUI_ImageSpec_intern;

/* Chunk style bit for a shortcut character, whose underline is drawn
 * like MUI 3.9's rather than by graphics.library (FSF_UNDERLINED) */
#define ZTEXT_STYLE_HICHAR 0x80
#define ZTEXT_STYLE_OUTLINE 0x08
#define ZTEXT_STYLE_SHADOW 0x10
#define ZTEXT_STYLE_GHOST 0x20
#define ZTEXT_JAM2 0x01
#define ZTEXT_INVERSE 0x02
#define ZTEXT_HIDEIMAGES 0x04
#define ZTEXT_SELECTED 0x08
#define ZTEXT_RGBPEN 0x10
#define ZTEXT_RGBBACK 0x20
#define ZTEXT_ALPHA 0x40

typedef struct ZTextChunk {
    struct MinNode node; /* embedded node */
    char                 *str;
    CONST_STRPTR          spec;
    struct MUI_ImageSpec_intern *image;
    Object               *obj; /* Area subclass, see List_CreateImage */
    union {
        struct MUI_AlphaData *alpha_data; /* non-text chunks */
        LONG backpen;                  /* text: alpha and background color */
    };
    LONG                  dripen;
    LONG                  pen;
    UBYTE                 style; /* FSF_UNDERLINED, FSF_BOLD, ... */
    UBYTE                 effects;
    WORD                  cwidth;
    WORD                  cheight;
    WORD                  clen;   /* strlen(str), kept for measuring and drawing */
} ZTextChunk;

typedef struct ZTextLine {
    struct MinNode node;       /* embedded node */
    UBYTE  align;              /* ZTL_CENTER, ZTL_RIGHT, ZTL_LEFT */
    struct MinList chunklist;
    WORD   lwidth;
    WORD   lheight;
    WORD   centreskip;         /* overhang not used for centring */
} ZTextLine;

struct ZText {
    WORD width; /* store calculated bounds - read only ! */
    WORD height;

/* private */
    struct MinList lines;
    UBYTE *arena_next;  /* inline space for lines and chunks */
    UBYTE *arena_end;
    UBYTE styles;       /* text chunk styles, set by zune_text_get_bounds */
    UBYTE nontext;      /* image or object chunks, set by get_bounds */
    UBYTE truncate;     /* draw only whole characters left of the right
                         * edge, as MUI's Text class does */
    UBYTE plain;       /* one nonempty, unformatted, left-aligned chunk */
};

typedef struct ZText ZText;

/* zune_text_new argtypes */
enum {
    ZTEXT_ARG_NONE,
    ZTEXT_ARG_HICHAR,    /* following arg is the HiChar */
    ZTEXT_ARG_HICHARIDX, /* following arg is the HiCharIdx */
};

ZText *zune_text_new (CONST_STRPTR preparse, CONST_STRPTR content,
    int argtype, TEXT arg);
void zune_text_destroy (ZText *text);
ZTextChunk *zune_text_plain_chunk (ZText *text, CONST_STRPTR s);
void zune_text_set_plain (ZText *text, ZTextChunk *chunk, STRPTR s,
    LONG len, Object *obj);
void zune_text_get_bounds (ZText *text, Object *obj);
void zune_text_draw (ZText *text, Object *obj, WORD left, WORD right,
    WORD top);

void zune_text_draw_shortened(ZText *text, Object *obj, WORD left,
    WORD right, WORD top, LONG mode);

LONG zune_text_mark(ZText *, Object *, LONG, LONG, LONG, LONG, LONG, BOOL);
STRPTR zune_text_copy(ZText *, LONG, LONG);

#endif

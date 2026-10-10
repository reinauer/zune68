/*
    Copyright (C) 2003, The AROS Development Team.
    All rights reserved.
    
*/

#include <intuition/imageclass.h>
#include <proto/graphics.h>
#include <proto/exec.h>

#include "debug.h"
#include "mui.h"
#include "imspec_intern.h"
#include "support.h"

extern struct Library *MUIMasterBase;

//#define SPACING 1
#define HSPACING 0
#define VSPACING 0


/* MUI 3.8/3.9 built-in arrows are chevrons with two pixel wide strokes:
 * 10x5 pointing up or down and 4x8 pointing left or right. Larger areas
 * centre the same shape. */
static void arrow_span(struct RastPort *rp, LONG x, LONG y, LONG minx,
    LONG maxx)
{
    LONG x2 = x + 1;

    if (x < minx)
        x = minx;
    if (x2 > maxx)
        x2 = maxx;
    if (x <= x2)
        RectFill(rp, x, y, x2, y);
}

static void arrow_vertical(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, BOOL up)
{
    struct RastPort *rport = mri->mri_RastPort;
    LONG n = MIN(height, width / 2), cx = left + width / 2, y0, r;

    if (n < 1)
        return;
    y0 = top + (height - n) / 2;
    SetAPen(rport, mri->mri_Pens[MPEN_TEXT]);
    for (r = 0; r < n; r++)
    {
        LONG y = up ? y0 + r : y0 + n - 1 - r;

        arrow_span(rport, cx - 1 - r, y, left, left + width - 1);
        arrow_span(rport, cx - 1 + r, y, left, left + width - 1);
    }
}

static void arrow_horizontal(struct MUI_RenderInfo *mri, LONG left,
    LONG top, LONG width, LONG height, BOOL leftwards)
{
    struct RastPort *rport = mri->mri_RastPort;
    LONG n = MIN(height / 2, width), x0, y0, r;

    if (n < 1)
        return;
    x0 = left + (width - n) / 2;
    y0 = top + (height - 2 * n) / 2;
    SetAPen(rport, mri->mri_Pens[MPEN_TEXT]);
    for (r = 0; r < n; r++)
    {
        LONG x = leftwards ? x0 + n - 1 - r : x0 + r - 1;

        arrow_span(rport, x, y0 + r, x0, x0 + n - 1);
        arrow_span(rport, x, y0 + 2 * n - 1 - r, x0, x0 + n - 1);
    }
}

void arrowup_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    arrow_vertical(mri, left, top, width, height, TRUE);
}

void arrowdown_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    arrow_vertical(mri, left, top, width, height, FALSE);
}

void arrowleft_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    arrow_horizontal(mri, left, top, width, height, TRUE);
}

void arrowright_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    arrow_horizontal(mri, left, top, width, height, FALSE);
}

/* MUI 3.8/3.9 built-in glyphs drawn at their natural size, one bit per
 * pixel with the most significant bit leftmost, centred in the area. */
static void glyph_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, const UWORD *rows, LONG gw, LONG gh)
{
    struct RastPort *rport = mri->mri_RastPort;
    LONG x0 = left + (width - gw) / 2, y0 = top + (height - gh) / 2;
    LONG y, x, start;

    SetAPen(rport, mri->mri_Pens[MPEN_TEXT]);
    for (y = 0; y < gh; y++)
    {
        if (y0 + y < top || y0 + y >= top + height)
            continue;
        for (x = 0; x < gw; x = start)
        {
            while (x < gw && !(rows[y] & (0x8000 >> x)))
                x++;
            for (start = x; start < gw && (rows[y] & (0x8000 >> start));)
                start++;
            if (start > x)
            {
                LONG l = MAX(x0 + x, left);
                LONG r = MIN(x0 + start - 1, left + width - 1);

                if (l <= r)
                    RectFill(rport, l, y0 + y, r, y0 + y);
            }
        }
    }
}

static const UWORD checkmark_glyph[] = {
    0x0038, 0x0060, 0x00c0, 0xe180, 0x7300, 0x3e00, 0x1c00
};
static const UWORD popfile_glyph[] = {
    0xff00, 0xc180, 0xc140, 0xc120, 0xc1f0, 0xc030, 0xc030, 0xc030,
    0xc030, 0xfff0
};
static const UWORD popdrawer_glyph[] = {
    0x03c0, 0x0420, 0xf810, 0xfc10, 0xc7f0, 0xc010, 0xc010, 0xc010,
    0xfff0
};

void checkbox_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    /* Draw checkmark (only if image is in selected state) */
    if (state == IDS_SELECTED)
        glyph_draw(mri, left, top, width, height, checkmark_glyph, 13, 7);
}

/* MUI 3.8/3.9 radio button: top-left edge (T), bottom-right edge (B),
 * the inside (I) and the selected fill (F), swapping the edge pens with
 * the state. The inside is painted with the background pen and the fill
 * only when selected; the corners outside the outline stay transparent,
 * as MUI 3.9 shows on a white IBrowse page. */
static const char *const mx_map[] = {
    "..TTTTTTTTTTTTB..",
    ".TTIIIIIIIIIIIBB.",
    "TTIIIFFFFFFFIIIBB",
    "TTIIFFFFFFFFFIIBB",
    "TTIIFFFFFFFFFIIBB",
    "TTIIFFFFFFFFFIIBB",
    "TTIIIFFFFFFFIIIBB",
    ".TTIIIIIIIIIIIBB.",
    "..TBBBBBBBBBBBB..",
};

static LONG mx_pen(char c, BOOL selected)
{
    switch (c)
    {
    case 'T': return selected ? MPEN_SHADOW : MPEN_SHINE;
    case 'B': return selected ? MPEN_SHINE : MPEN_SHADOW;
    case 'F': return selected ? MPEN_FILL : MPEN_BACKGROUND;
    case 'I': return MPEN_BACKGROUND;
    }
    return -1;
}

void mx_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    BOOL selected = state == IDS_SELECTED;
    LONG x0 = left + (width - 17) / 2, y0 = top + (height - 9) / 2;
    LONG x, y;

    /* Runs of four or more pixels are cheaper as RectFill than as
     * WritePixel (tests/native/gfxprims.c) */
    for (y = 0; y < 9; y++)
    {
        if (y0 + y < top || y0 + y >= top + height)
            continue;
        for (x = 0; x < 17; x++)
        {
            LONG pen = mx_pen(mx_map[y][x], selected), end;

            if (pen < 0)
                continue;
            for (end = x; end + 1 < 17
                && mx_pen(mx_map[y][end + 1], selected) == pen;)
                end++;
            {
                LONG l = MAX(x0 + x, left);
                LONG rt = MIN(x0 + end, left + width - 1);

                SetAPen(rport, mri->mri_Pens[pen]);
                if (rt - l >= 3)
                    RectFill(rport, l, y0 + y, rt, y0 + y);
                else
                    for (; l <= rt; l++)
                        WritePixel(rport, l, y0 + y);
            }
            x = end;
        }
    }
}

void cycle_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int bottom = top + height - 1;
    int right = left + width - 1;
#if BIGGER_ARROW
    int arrow_top;
#endif

    SetAPen(rport, mri->mri_Pens[MPEN_TEXT]);

    Move(rport, left, top + 1);
    Draw(rport, left, bottom - 1);
    Move(rport, left + 1, top);
    Draw(rport, left + 1, bottom);
    Draw(rport, right - 7, bottom);
    Move(rport, right - 7, bottom - 1);
    Draw(rport, right - 6, bottom - 1);
    Move(rport, left + 2, top);
    Draw(rport, right - 7, top);
    Move(rport, right - 7, top + 1);
    Draw(rport, right - 6, top + 1);

#if BIGGER_ARROW
    arrow_top = top + (3 * height / 4) - 3;
    /* prevent arrow touching bottom */
    if (arrow_top + 4 >= bottom - 1)
        arrow_top = bottom - 5;

    RectFill(rport, right - 7, top + 1, right - 6, arrow_top - 1);
#endif
    /* The small arrow */
    Move(rport, right - 6 - 3, top + 2);
    Draw(rport, right - 7 + 3, top + 2);
    Move(rport, right - 6 - 2, top + 3);
    Draw(rport, right - 7 + 2, top + 3);
    Move(rport, right - 6 - 1, top + 4);
    Draw(rport, right - 7 + 1, top + 4);
#if BIGGER_ARROW
    /* makes arrow bigger */
    if (arrow_top - top - 2 >= 4)
    {
        Move(rport, right - 6 - 3, arrow_top - 1);
        Draw(rport, right - 7 + 3, arrow_top - 1);
        Move(rport, right - 6 - 2, arrow_top);
        Draw(rport, right - 7 + 2, arrow_top);
        Move(rport, right - 6 - 2, arrow_top + 1);
        Draw(rport, right - 7 + 2, arrow_top + 1);
        Move(rport, right - 6 - 1, arrow_top + 2);
        Draw(rport, right - 7 7 + 1, arrow_top + 2);
    }
    else
    {
        Move(rport, right - 6 - 3, arrow_top);
        Draw(rport, right - 7 + 3, arrow_top);
        Move(rport, right - 6 - 2, arrow_top + 1);
        Draw(rport, right - 7 + 2, arrow_top + 1);
        Move(rport, right - 6 - 1, arrow_top + 2);
        Draw(rport, right - 7 + 1, arrow_top + 2);
    }
#endif

    /* The right bar */
    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, right - 1, top);
    Draw(rport, right - 1, bottom);
    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    Move(rport, right, top);
    Draw(rport, right, bottom);
}

/* MUI 3.9's popup arrow, measured in a PopButton(MUII_PopUp) */
static const UWORD popup_glyph[] = {
    0x0000, 0xc0c0, 0x6180, 0x3300, 0x1e00, 0x0c00, 0x0000, 0xffc0,
    0xffc0, 0x0000
};

void popup_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG state)
{
    glyph_draw(mri, left, top, width, height, popup_glyph, 10, 10);
}

void popfile_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    glyph_draw(mri, left, top, width, height, popfile_glyph, 12, 10);
}

void popdrawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    glyph_draw(mri, left, top, width, height, popdrawer_glyph, 12, 9);
}

static void stddrawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right = left + width - 1;
    int bottom = top + height - 1;
    int x, y;

#define XX(x)   (x * (width - 1) / 100)
#define YY(y)   (y * (height - 1) / 100)
#define YOFF    45
#define XOFF    24

    for (y = 0; y < YY(YOFF); y++)
    {
        x = (YY(XOFF) > 1) ? XX(XOFF) * y / (YY(YOFF) - 1) : 1;
        SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
        RectFill(rport, left + XX(XOFF) - x, top + y, right - XX(XOFF) + x,
            top + y);
        SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
        WritePixel(rport, left + XX(XOFF) - x, top + y);
        WritePixel(rport, right - XX(XOFF) + x, top + y);
    }


    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    RectFill(rport, left, top + YY(YOFF), right, bottom);

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, left + XX(XOFF), top);
    Draw(rport, right - XX(XOFF), top);

    Move(rport, left, top + YY(YOFF));
    Draw(rport, left, bottom);
    Draw(rport, right, bottom);
    Draw(rport, right, top + YY(YOFF));
    Draw(rport, left, top + YY(YOFF));

#undef XX
#undef YY
#undef YOFF
#undef XOFF
}


void drawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right = left + width - 1;

    stddrawer_draw(mri, left, top, width, height);

#define XX(x) x * (width - 1) / 100
#define YY(y) y * (height - 1) / 100

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, left + XX(40), top + YY(75));
    Draw(rport, right - XX(40), top + YY(75));

#undef XX
#undef YY

}

void harddisk_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;

    stddrawer_draw(mri, left, top, width, height);

#define XX(x) x * (width - 1) / 100
#define YY(y) y * (height - 1) / 100

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, left + XX(20), top + YY(75));
    Draw(rport, left + XX(30), top + YY(75));

#undef XX
#undef YY
}

void disk_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right = left + width - 1;
    int bottom = top + height - 1;
    int x, y;

#define XX(x)   (x * (width - 1) / 100)
#define YY(y)   (y * (height - 1) / 100)
#define YOFF    29
#define XOFF    25

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    for (y = 0; y < YY(YOFF); y++)
    {
        x = (YY(YOFF) > 1) ? XX(XOFF) * y / (YY(YOFF) - 1) : 1;
        RectFill(rport, left, top + y, right - XX(XOFF) + x, top + y);
    }
    RectFill(rport, left, top + YY(YOFF), right, bottom);

    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    RectFill(rport, left + XX(33), top, right - XX(33), top + YY(YOFF));
    RectFill(rport, left + XX(25), top + YY(60), right - XX(25), bottom);

#undef XX
#undef YY
#undef YOFF
#undef XOFF
}

void ram_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right, bottom;

#define XX(x) left + x * (width - 1) / 100
#define YY(y) top  + y * (height - 1) / 100

    right = left + width - 1;
    bottom = top + height - 1;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    RectFill(rport, left, top, right, bottom);

    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    Move(rport, XX(33), YY(20));
    Draw(rport, XX(33), YY(80));

    Move(rport, XX(71), YY(20));
    Draw(rport, XX(61), YY(20));
    Draw(rport, XX(61), YY(80));
    Draw(rport, XX(71), YY(80));

#undef XX
#undef YY
}

void volume_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right, bottom;

#define XX(x) left + x * (width - 1) / 100
#define YY(y) top  + y * (height - 1) / 100

    right = left + width - 1;
    bottom = top + height - 1;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    RectFill(rport, left, top, right, bottom);

    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    Move(rport, XX(8), YY(20));
    Draw(rport, XX(8), YY(60));
    Draw(rport, XX(16), YY(80));
    Draw(rport, XX(24), YY(60));
    Draw(rport, XX(24), YY(20));

    Move(rport, XX(40), YY(20));
    Draw(rport, XX(60), YY(20));
    Draw(rport, XX(60), YY(80));
    Draw(rport, XX(40), YY(80));
    Draw(rport, XX(40), YY(20));

    Move(rport, XX(80), YY(20));
    Draw(rport, XX(80), YY(80));
    Draw(rport, XX(92), YY(80));

#undef XX
#undef YY
}

void network_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right = left + width - 1;

    stddrawer_draw(mri, left, top, width, height);

#define XX(x) x * (width - 1) / 100
#define YY(y) y * (height - 1) / 100

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, left + XX(40), top + YY(75));
    Draw(rport, right - XX(40), top + YY(75));

#undef XX
#undef YY
}

void assign_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right = left + width - 1;

    stddrawer_draw(mri, left, top, width, height);

#define XX(x) x * (width - 1) / 100
#define YY(y) y * (height - 1) / 100

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, left + XX(40), top + YY(75));
    Draw(rport, right - XX(40), top + YY(75));

#undef XX
#undef YY
}

void tape_play_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int x, half = height / 2;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    for (x = 0; x < width; x++)
    {
        int y = (width > 1) ? half * x / (width - 1) : 1;

        RectFill(rport, left + width - 1 - x, top + half - y,
            left + width - 1 - x, top + half + y);
    }
}

void tape_playback_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int x, half = height / 2;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    for (x = 0; x < width; x++)
    {
        int y = (width > 1) ? half * x / (width - 1) : 1;

        RectFill(rport, left + x, top + half - y, left + x, top + half + y);
    }
}

void tape_pause_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    RectFill(rport, left, top, left + width / 4, top + height - 1);
    RectFill(rport, left + width - 1 - width / 4, top, left + width - 1,
        top + height - 1);
}

void tape_stop_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;

    SetAPen(rport, mri->mri_Pens[MPEN_TEXT]);
    RectFill(rport, left, top, left + width - 1, top + height - 1);
}

void tape_record_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int rx = (width - 1) / 2;
    int ry = (height - 1) / 2;
    int x = rx, y = 0;          /* ellipse points */

    /* intermediate terms to speed up loop */
    int t1 = rx * rx, t2 = t1 << 1, t3 = t2 << 1;
    int t4 = ry * ry, t5 = t4 << 1, t6 = t5 << 1;
    int t7 = rx * t5, t8 = t7 << 1, t9 = 0L;
    int d1 = t2 - t7 + (t4 >> 1);       /* error terms */
    int d2 = (t1 >> 1) - t8 + t5;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    while (d2 < 0)              /* til slope = -1 */
    {

        /* draw 4 points using symmetry */
        RectFill(rport, left + rx - x, top + ry + y, left + rx + x,
            top + ry + y);
        RectFill(rport, left + rx - x, top + ry - y, left + rx + x,
            top + ry - y);

        y++;                    /* always move up here */
        t9 = t9 + t3;
        if (d1 < 0)             /* move straight up */
        {
            d1 = d1 + t9 + t2;
            d2 = d2 + t9;
        }
        else                    /* move up and left */
        {
            x--;
            t8 = t8 - t6;
            d1 = d1 + t9 + t2 - t8;
            d2 = d2 + t9 + t5 - t8;
        }
    }

    do                          /* rest of top right quadrant */
    {
        /* draw 4 points using symmetry */
        RectFill(rport, left + rx - x, top + ry + y, left + rx + x,
            top + ry + y);
        RectFill(rport, left + rx - x, top + ry - y, left + rx + x,
            top + ry - y);

        x--;                    /* always move left here */
        t8 = t8 - t6;
        if (d2 < 0)             /* move up and left */
        {
            y++;
            t9 = t9 + t3;
            d2 = d2 + t9 + t5 - t8;
        }
        else                    /* move straight left */
        {
            d2 = d2 + t5 - t8;
        }

    }
    while (x >= 0);

}

void tape_up_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int y, half = width / 2;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    for (y = 0; y < height; y++)
    {
        int x = (height > 1) ? half * y / (height - 1) : 1;

        RectFill(rport, left + half - x, top + y, left + half + x, top + y);
    }
}

void tape_down_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int y, half = width / 2;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    for (y = 0; y < height; y++)
    {
        int x = (height > 1) ? half * y / (height - 1) : 1;

        RectFill(rport, left + half - x, top + height - 1 - y,
            left + half + x, top + height - 1 - y);
    }
}

struct vector_image
{
    int minwidth;
    int minheight;
    VECTOR_DRAW_FUNC draw_func;
};

static const struct vector_image vector_table[] = {
    {10, 5, arrowup_draw},
    {10, 5, arrowdown_draw},
    {4, 8, arrowleft_draw},
    {4, 8, arrowright_draw},
    {13, 7, checkbox_draw},
    {17, 9, mx_draw},
    {15, 8, cycle_draw},
    {10, 10, popup_draw},
    {12, 10, popfile_draw},
    {12, 9, popdrawer_draw},

    {13, 8, drawer_draw},
    {13, 8, harddisk_draw},
    {12, 8, disk_draw},
    {14, 6, ram_draw},
    {14, 6, volume_draw},
    {13, 8, network_draw},
    {13, 8, assign_draw},

    {4, 7, tape_play_draw},
    {4, 7, tape_playback_draw},
    {6, 7, tape_pause_draw},
    {6, 7, tape_stop_draw},
    {7, 7, tape_record_draw},
    {7, 4, tape_up_draw},
    {7, 4, tape_down_draw},
};

#define VECTOR_TABLE_ENTRIES (sizeof(vector_table)/sizeof(vector_table[0]))

struct MUI_ImageSpec_intern *zune_imspec_create_vector(LONG vect)
{
    struct MUI_ImageSpec_intern *spec;

    if (!(vect >= 0 && vect < VECTOR_TABLE_ENTRIES))
        return NULL;

    if ((spec = mui_alloc_struct(struct MUI_ImageSpec_intern)))
    {
        spec->type = IST_VECTOR;
        spec->u.vect.type = vect;
        spec->u.vect.draw = vector_table[vect].draw_func;
    }
    return spec;
}

BOOL zune_imspec_vector_get_minmax(struct MUI_ImageSpec_intern *spec,
    struct MUI_MinMax *minmax)
{
    if (!spec || spec->type != IST_VECTOR)
        return FALSE;
    if ((spec->u.vect.type >= 0)
        && (spec->u.vect.type < VECTOR_TABLE_ENTRIES))
    {
        minmax->MinWidth = vector_table[spec->u.vect.type].minwidth;
        minmax->MinHeight = vector_table[spec->u.vect.type].minheight;
        minmax->DefWidth = minmax->MinWidth;
        minmax->DefHeight = minmax->MinHeight;
        minmax->MaxWidth = MUI_MAXMAX;
        minmax->MaxHeight = MUI_MAXMAX;
    }
    else
    {
        return FALSE;
    }
    return TRUE;
}

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
*/

#ifndef _SMALLFONT_H_
#define _SMALLFONT_H_

/* A 3x5 pixel font for the value and label displays of Knob and
 * Levelmeter, which are too small for any screen font. Each glyph is
 * 5 rows of 3 bits, top row and leftmost pixel in the highest bits;
 * characters outside 32..126 draw as blanks. */

#include <proto/graphics.h>

#define SMALLFONT_WIDTH   3
#define SMALLFONT_HEIGHT  5
#define SMALLFONT_ADVANCE 4

static const UWORD smallfont_glyphs[] =
{
    0x0000, 0x2482, 0x5a00, 0x5f7d, 0x3c9e, 0x42a1, 0x2aab, 0x2400,
    0x1491, 0x4494, 0x0aa8, 0x05d0, 0x0014, 0x01c0, 0x0002, 0x12a4,
    0x7b6f, 0x2c97, 0x62a7, 0x628e, 0x5bc9, 0x798e, 0x39aa, 0x7292,
    0x7bef, 0x7bce, 0x0410, 0x0414, 0x1511, 0x0e38, 0x4454, 0x6282,
    0x2be3, 0x2bed, 0x6bae, 0x3923, 0x6b6e, 0x79a7, 0x79a4, 0x396b,
    0x5bed, 0x7497, 0x126a, 0x5bad, 0x4927, 0x5fed, 0x6b6d, 0x2b6a,
    0x6ba4, 0x2b73, 0x6bad, 0x388e, 0x7492, 0x5b6f, 0x5b52, 0x5bfd,
    0x5aad, 0x5a92, 0x72a7, 0x3493, 0x4889, 0x6496, 0x2a00, 0x0007,
    0x4400, 0x076b, 0x4d6e, 0x0723, 0x176b, 0x05e3, 0x35d2, 0x075e,
    0x4d6d, 0x2092, 0x106a, 0x4bad, 0x6491, 0x0ded, 0x0d6d, 0x056a,
    0x0d74, 0x0759, 0x0724, 0x079e, 0x2e91, 0x0b6b, 0x0b52, 0x0b7d,
    0x0a95, 0x0b5e, 0x0ef7, 0x3513, 0x2492, 0x6456, 0x0780,
};

static LONG SmallFont_TextLength(CONST_STRPTR text)
{
    LONG len = strlen(text);

    return len ? len * SMALLFONT_ADVANCE - 1 : 0;
}

/* Draw text centred in the box left..right at row top with the current
 * APen, clipped to the box. */
static void SmallFont_Centre(struct RastPort *rp, LONG left, LONG top,
    LONG right, CONST_STRPTR text)
{
    LONG x = left + (right - left + 1 - SmallFont_TextLength(text)) / 2;
    UBYTE c;

    for (; (c = *text); text++, x += SMALLFONT_ADVANCE)
    {
        UWORD bits;
        LONG row, col;

        if (c < 32 || c > 126)
            continue;
        bits = smallfont_glyphs[c - 32];
        for (row = 0; row < SMALLFONT_HEIGHT; row++)
            for (col = 0; col < SMALLFONT_WIDTH; col++)
                if ((bits & (0x4000 >> (row * SMALLFONT_WIDTH + col)))
                    && x + col >= left && x + col <= right)
                    WritePixel(rp, x + col, top + row);
    }
}

#endif /* _SMALLFONT_H_ */

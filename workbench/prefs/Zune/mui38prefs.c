/*
 * MUI 3.8 settings file -> Zune settings file (see mui38prefs.h).
 *
 * Both are IFF FORM PREF files with a PRHD chunk and a MUIC chunk of
 * items: a 4-byte id, a 4-byte length, then that many bytes, no padding
 * between items. Zune took its ids and encodings from MUI, so most items
 * are copied as they are, other classes' settings (Listtree, NList, ...)
 * too. What MUI 3.8 stores differently:
 *
 * - title bar gadgets: one item each, 1 = on (Iconify 0x13, Snapshot 0x76,
 *   Prefs 0x77, Popup 0x78); Zune keeps one mask (MUICFG_Window_Buttons);
 * - the System page and the window refresh under other ids: pop to front
 *   0x7c, iconify hotkey 0x29, show icon 0x14, show menu 0x7d, iconified
 *   on startup 0x15, ARexx 0x7e, first and next bubble help delay 0x81 and
 *   0x82, refresh 0x1c;
 * - frame specs: types 5-10 are numbered differently (MUI 3.8's 5, 6, 7,
 *   8, 9, 10 are Zune's 6, 9, 10, 8, 7, 5);
 * - images from MUI:Images are "3:<name>" for brushes as well; Zune's "4:"
 *   is a brush, its "3:" a BOOPSI image class (".image");
 * - window positions in a MUIW chunk (per window its id, length 40, then
 *   its box and the box of its other zoom state as WORDs); Zune keeps them
 *   in one item (MUICFG_WindowPos).
 *
 * Pictures and image classes ("3:", "4:", "5:") the caller cannot find are
 * left out, so that Zune's default is drawn instead of nothing. MUI 3.8's
 * frame thickness (0x0e) is copied as it is; Zune has no such setting.
 *
 * The ids and encodings were measured black-box on MUI 3.8 by changing
 * one setting at a time. Ported from OpenMUI (mui38prefs.c, Tobias
 * Karlsson).
 */
#include "mui38prefs.h"

#define ID(a, b, c, d) (((unsigned long)(a) << 24) | ((unsigned long)(b) << 16) \
    | ((unsigned long)(c) << 8) | (unsigned long)(d))

/* Zune's ids (workbench/libs/muimaster/classes/configdata.h) */
#define Z_WINDOW_REFRESH   0x404
#define Z_WINDOWPOS        0x509
#define Z_WINDOW_BUTTONS   0x50a
#define Z_POPTOFRONT       0x700

/* MUIV_Window_Button_* (classes/window.h) */
#define B_MUI      1
#define B_SNAPSHOT 2
#define B_ICONIFY  4
#define B_POPUP    8

/* The application's window position table holds 300 entries, of which
 * MUIA_Application_GetWinPosSize uses 299 */
#define MAX_WINDOWS 299

static const struct { unsigned short mui, zune; } renamed[] = {
    { 0x1c, Z_WINDOW_REFRESH },
    { 0x7c, Z_POPTOFRONT },       /* PublicScreen_PopToFront */
    { 0x29, Z_POPTOFRONT + 1 },   /* Iconification_Hotkey */
    { 0x14, Z_POPTOFRONT + 2 },   /* Iconification_ShowIcon */
    { 0x7d, Z_POPTOFRONT + 3 },   /* Iconification_ShowMenu */
    { 0x15, Z_POPTOFRONT + 4 },   /* Iconification_OnStartup */
    { 0x7e, Z_POPTOFRONT + 5 },   /* Interfaces_EnableARexx */
    { 0x81, Z_POPTOFRONT + 6 },   /* BubbleHelp_FirstDelay */
    { 0x82, Z_POPTOFRONT + 7 },   /* BubbleHelp_NextDelay */
};

static unsigned long get32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16)
        | ((unsigned long)p[2] << 8) | p[3];
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

/* Frame items: Drag 0x18, Button..Slider 0x2b-0x36, Knob 0x90 */
static int is_frame(unsigned long id)
{
    return id == 0x18 || (id >= 0x2b && id <= 0x36) || id == 0x90;
}

/* Image and background items: 0x37-0x5e, Page 0x95, ReadList 0x96 */
static int is_image(unsigned long id)
{
    return (id >= 0x37 && id <= 0x5e) || id == 0x95 || id == 0x96;
}

/* A frame type digit; MUI writes 10 as 'a', 'A' or ':' ('0' + 10) */
static int frame_type(unsigned char c)
{
    if (c >= '0' && c <= ':')
        return c - '0';
    if (c == 'a' || c == 'A')
        return 10;
    return -1;
}

/* "<name>.image" in v[0..n), any case */
static int ends_in_image(const unsigned char *v, unsigned long n)
{
    static const char suffix[] = ".image";
    unsigned long i;

    if (n < 6)
        return 0;
    for (i = 0; i < 6; i++)
        if ((v[n - 6 + i] | (i ? 0x20 : 0)) != (unsigned char)suffix[i])
            return 0;
    return 1;
}

/* Write one item; NULL when it does not fit */
static unsigned char *put_item(unsigned char *o, unsigned char *end,
    unsigned long id, const unsigned char *v, unsigned long len)
{
    unsigned long i;

    if ((unsigned long)(end - o) < 8 || (unsigned long)(end - o) - 8 < len)
        return 0;
    put32(o, id);
    put32(o + 4, len);
    for (i = 0; i < len; i++)
        o[8 + i] = v[i];
    return o + 8 + len;
}

/* One MUIC item, translated; NULL when the output is full */
static unsigned char *item(unsigned char *o, unsigned char *end,
    unsigned long id, const unsigned char *v, unsigned long len,
    unsigned long *buttons, mui38_have_file have)
{
    static const unsigned char frametypes[11] =
        { 0, 1, 2, 3, 4, 6, 9, 10, 8, 7, 5 };
    unsigned char *p;
    unsigned int i;

    switch (id)
    {
    case 0x13: case 0x76: case 0x77: case 0x78:
        if (len == 4 && get32(v))
            *buttons |= id == 0x13 ? B_ICONIFY : id == 0x76 ? B_SNAPSHOT
                : id == 0x77 ? B_MUI : B_POPUP;
        return o;
    }
    for (i = 0; i < sizeof(renamed) / sizeof(renamed[0]); i++)
        if (renamed[i].mui == id)
            return put_item(o, end, renamed[i].zune, v, len);

    p = put_item(o, end, id, v, len);
    if (!p)
        return 0;
    if (is_frame(id) && len >= 6 && len <= 8)
    {
        int t = frame_type(v[0]);

        if (t >= 0)
            o[8] = "0123456789a"[frametypes[t]];
    }
    else if (is_image(id) && len >= 3 && v[len - 1] == 0
        && v[0] >= '3' && v[0] <= '5' && v[1] == ':')
    {
        /* a file that is not there would draw nothing (MUI:Images is
         * often missing): Zune's default stays instead */
        if (have && !have((const char *)v + 2))
            return o;
        if (v[0] == '3' && !ends_in_image(v + 2, len - 3))
            o[8] = '4';
    }
    return p;
}

/* The MUIW chunk c[0..clen) as Zune's window position item at o */
static unsigned char *windows(unsigned char *o, unsigned char *end,
    const unsigned char *c, unsigned long clen)
{
    const unsigned char *cend = c + clen;
    unsigned char *start = o;
    unsigned long n = 0;

    if ((unsigned long)(end - o) < 12)
        return 0;
    o += 12;
    while ((unsigned long)(cend - c) >= 8 && n < MAX_WINDOWS)
    {
        unsigned long len = get32(c + 4);
        unsigned int k;

        if (len > (unsigned long)(cend - c) - 8)
            break;
        /* id, then x, y, width, height and the other zoom state's box;
         * an entry without a width would end Zune's table */
        if (len >= 16 && (c[12] | c[13]))
        {
            if ((unsigned long)(end - o) < 20)
                return 0;
            for (k = 0; k < 4; k++)
                o[k] = c[k];
            for (k = 0; k < 16; k++)
                o[4 + k] = c[8 + k];
            /* no other box: the same box */
            if (!(c[8 + 12] | c[8 + 13]))
                for (k = 0; k < 8; k++)
                    o[12 + k] = o[4 + k];
            o += 20;
            n++;
        }
        c += 8 + len;
    }
    if (!n)
        return start;
    put32(start, Z_WINDOWPOS);
    put32(start + 4, 4 + n * 20);
    put32(start + 8, 4 + n * 20);
    return o;
}

long mui38_convert(const unsigned char *in, long inlen,
                   unsigned char *out, long outmax, mui38_have_file have)
{
    unsigned char *o, *end = out + outmax, *muic;
    const unsigned char *p, *inend;
    unsigned long buttons = 0, formlen;

    if (inlen < 12 || outmax < MUI38_ITEMS_OFFSET
        || get32(in) != ID('F','O','R','M') || get32(in + 8) != ID('P','R','E','F'))
        return -1;
    formlen = get32(in + 4);
    if (formlen < 4 || formlen > (unsigned long)inlen - 8)
        formlen = inlen - 8;
    inend = in + 8 + formlen;

    /* FORM PREF, a PRHD as Zune writes it, then one MUIC chunk */
    put32(out, ID('F','O','R','M'));
    put32(out + 8, ID('P','R','E','F'));
    put32(out + 12, ID('P','R','H','D'));
    put32(out + 16, 6);
    for (o = out + 20; o < out + 26; o++)
        *o = 0;
    muic = out + 26;
    put32(muic, ID('M','U','I','C'));
    o = out + MUI38_ITEMS_OFFSET;

    for (p = in + 12; (unsigned long)(inend - p) >= 8; )
    {
        unsigned long cid = get32(p), clen = get32(p + 4);
        const unsigned char *c = p + 8, *cend;

        if (clen > (unsigned long)(inend - c))
            clen = inend - c;
        cend = c + clen;
        if (cid == ID('M','U','I','C'))
        {
            while ((unsigned long)(cend - c) >= 8)
            {
                unsigned long id = get32(c), len = get32(c + 4);

                if (len > (unsigned long)(cend - c) - 8)
                    break;
                if (!(o = item(o, end, id, c + 8, len, &buttons, have)))
                    return -1;
                c += 8 + len;
            }
        }
        else if (cid == ID('M','U','I','W'))
        {
            if (!(o = windows(o, end, c, clen)))
                return -1;
        }
        /* chunks are padded to even lengths */
        if (clen & 1 && cend < inend)
            cend++;
        p = cend;
    }
    if (buttons)
    {
        unsigned char v[4];

        put32(v, buttons);
        if (!(o = put_item(o, end, Z_WINDOW_BUTTONS, v, 4)))
            return -1;
    }
    put32(muic + 4, o - (muic + 8));
    if ((o - out) & 1)
    {
        if (o >= end)
            return -1;
        *o++ = 0;
    }
    put32(out + 4, o - out - 8);
    return o - out;
}

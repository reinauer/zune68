/* Encoded text for Zune68. Sources are borrowed, conversions object-owned.
 * codesets.library supplies the system charset and additional encodings.
 * The basic Latin-1/UTF-8/UTF-16 path also works without that library.
 */
#include <string.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include "gcc/codesets_compat.h"
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"

struct Conversion
{
    struct Conversion *next;
    ULONG encoding;
    APTR text;
    BOOL codesets;
};
struct TextdataData
{
    /* Public fields immediately follow Notify's instance data. */
    CONST_APTR text;
    ULONG encoding;
    LONG bytes;
    struct Library *codesets;
    struct Conversion *conversions;
};

static ULONG source_length(const UBYTE *s, ULONG encoding)
{
    ULONG n = 0;
    if (encoding == 1000 || encoding == 1013 || encoding == 1015)
        while (s[n] || s[n + 1]) n += 2;
    else n = strlen((CONST_STRPTR)s);
    return n;
}

/* Strict decoding prevents truncated UTF sequences from reading past a
 * SourceBytes boundary. Unsupported or malformed conversions fail. */
static BOOL decode(const UBYTE **src, const UBYTE *end, ULONG enc, ULONG *cp)
{
    const UBYTE *s = *src;
    ULONG c, min = 0, n = 0;
    if (s == end) return FALSE;
    c = *s++;
    if (enc == 106 && c >= 128)
    {
        if (c >= 0xc2 && c <= 0xdf) { c &= 31; n = 1; min = 128; }
        else if (c >= 0xe0 && c <= 0xef) { c &= 15; n = 2; min = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { c &= 7; n = 3; min = 0x10000; }
        else return FALSE;
        if ((ULONG)(end - s) < n) return FALSE;
        while (n--)
        {
            if ((*s & 0xc0) != 0x80) return FALSE;
            c = (c << 6) | (*s++ & 63);
        }
        if (c < min) return FALSE;
    }
    else if (enc == 1000 || enc == 1013 || enc == 1015)
    {
        if (s == end) return FALSE;
        c = (c << 8) | *s++;
        if (c >= 0xd800 && c <= 0xdbff)
        {
            ULONG low;
            if (end - s < 2) return FALSE;
            low = ((ULONG)s[0] << 8) | s[1];
            if (low < 0xdc00 || low > 0xdfff) return FALSE;
            c = 0x10000 + ((c - 0xd800) << 10) + low - 0xdc00;
            s += 2;
        }
    }
    if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return FALSE;
    *src = s;
    *cp = c;
    return TRUE;
}
static BOOL basic_encoding(ULONG e)
{
    return e == 4 || e == 106 || e == 1000 || e == 1013 || e == 1015;
}
static APTR basic_convert(struct TextdataData *d, ULONG from, ULONG to)
{
    const UBYTE *s = d->text, *end;
    UBYTE *result, *out;
    ULONG len, c, units, factor, terminator;
    if (!basic_encoding(from) || !basic_encoding(to)) return NULL;
    len = d->bytes < 0 ? source_length(s, from) : d->bytes;
    /* Bound destination bytes by the input encoding. UTF-16 contributes
     * at most one scalar per code unit; UTF-8 never expands when narrowed
     * to Latin-1. Avoid reserving four bytes for every source byte. */
    BOOL wide_source = from == 1000 || from == 1013 || from == 1015;
    units = wide_source ? len / 2 : len;
    factor = to == 4 ? 1 : to == 106 ? (from == 4 ? 2 : 3) : 2;
    terminator = to == 4 || to == 106 ? 1 : 2;
    if (units > (0x7fffffffUL - terminator) / factor) return NULL;
    result = AllocVec(units * factor + terminator, MEMF_ANY);
    if (!result) return NULL;
    out = result;
    end = s + len;
    while (s < end)
    {
        if (!decode(&s, end, from, &c)) { FreeVec(result); return NULL; }
        if (to == 4) *out++ = c <= 255 ? c : '?';
        else if (to == 106)
        {
            if (c < 128) *out++ = c;
            else
            {
                if (c >= 0x10000) { *out++ = 0xf0 | (c >> 18); *out++ = 0x80 | ((c >> 12) & 63); }
                else if (c >= 0x800) *out++ = 0xe0 | (c >> 12);
                else *out++ = 0xc0 | (c >> 6);
                if (c >= 0x800) *out++ = 0x80 | ((c >> 6) & 63);
                *out++ = 0x80 | (c & 63);
            }
        }
        else
        {
            if (c >= 0x10000)
            {
                ULONG high = 0xd800 + ((c - 0x10000) >> 10);
                *out++ = high >> 8; *out++ = high;
                c = 0xdc00 + ((c - 0x10000) & 1023);
            }
            *out++ = c >> 8; *out++ = c;
        }
    }
    *out++ = 0;
    if (terminator == 2) *out = 0;
    return result;
}
static ULONG default_encoding(struct TextdataData *d)
{
    return d->codesets ? ZuneCodesetsDefault(d->codesets) : 4;
}
static IPTR convert(struct TextdataData *d, ULONG encoding)
{
    struct Conversion *c;
    ULONG from = d->encoding ? d->encoding : default_encoding(d);
    ULONG to = encoding ? encoding : default_encoding(d);
    struct Library *CodesetsBase = d->codesets;
    if (from == to) return (IPTR)d->text;
    for (c = d->conversions; c; c = c->next)
        if (c->encoding == to) return (IPTR)c->text;
    c = AllocVec(sizeof(*c), MEMF_ANY | MEMF_CLEAR);
    if (!c) return 0;
    if (CodesetsBase)
    {
        c->text = ZuneCodesetsConvert(CodesetsBase, d->text,
            d->bytes < 0 ? source_length(d->text, from) : d->bytes, from, to);
        c->codesets = TRUE;
    }
    else c->text = basic_convert(d, from, to);
    if (!c->text) { FreeVec(c); return 0; }
    c->encoding = to;
    c->next = d->conversions;
    d->conversions = c;
    return (IPTR)c->text;
}

BOOPSI_DISPATCHER(IPTR, Textdata_Dispatcher, cl, obj, msg)
{
    struct TextdataData *d;
    if (msg->MethodID == OM_NEW)
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList;
        obj = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!obj) return 0;
        d = INST_DATA(cl, obj);
        static const UBYTE empty[2] = {0, 0};
        d->text = (APTR)GetTagData(MUIA_Textdata_Contents, (IPTR)empty, tags);
        d->encoding = GetTagData(MUIA_Textdata_Encoding, 0, tags);
        d->bytes = GetTagData(MUIA_Textdata_SourceBytes, -1, tags);
        if (!d->text) d->text = empty;
        d->codesets = OpenLibrary("codesets.library", 6);
        return (IPTR)obj;
    }
    d = INST_DATA(cl, obj);
    switch (msg->MethodID)
    {
    case OM_GET:
    {
        struct opGet *g = (APTR)msg;
        if (g->opg_AttrID == MUIA_Textdata_Contents)
            *g->opg_Storage = (IPTR)d->text;
        else if (g->opg_AttrID == MUIA_Textdata_Encoding)
            *g->opg_Storage = d->encoding;
        else return DoSuperMethodA(cl, obj, msg);
        return TRUE;
    }
    case MUIM_Textdata_Convert:
        return convert(d, ((struct MUIP_Textdata_Convert *)msg)->encoding);
    case OM_DISPOSE:
    {
        struct Conversion *c = d->conversions;
        struct Library *CodesetsBase = d->codesets;
        while (c)
        {
            struct Conversion *next = c->next;
            if (c->codesets) ZuneCodesetsFree(CodesetsBase, c->text);
            else FreeVec(c->text);
            FreeVec(c);
            c = next;
        }
        if (CodesetsBase) CloseLibrary(CodesetsBase);
        break;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END
const struct __MUIBuiltinClass _MUI_Textdata_desc = {
    MUIC_Textdata, MUIC_Notify, sizeof(struct TextdataData), (void *)Textdata_Dispatcher
};

#ifndef ZUNE68_CODESETS_COMPAT_H
#define ZUNE68_CODESETS_COMPAT_H

/* The small public codesets.library v6 ABI used by Textdata. Keeping these
 * bindings here avoids making its optional SDK a build-time requirement.
 * The installed library and its charset tables remain external resources.
 */
#include <exec/libraries.h>
#include <exec/lists.h>
#include <utility/tagitem.h>
#include <inline/macros.h>

struct ZuneCodesetChar
{
    UBYTE code, utf8[8];
    ULONG ucs4;
};
struct ZuneCodeset
{
    struct MinNode node;
    STRPTR name, alternate, description;
    LONG readonly;
    struct ZuneCodesetChar table[256], sorted[256];
    ULONG mib;
};
static inline ULONG ZuneCodesetsDefault(struct Library *base)
{
    struct ZuneCodeset *cs = LP2(0x66, struct ZuneCodeset *, CodesetsFindA,
        CONST_STRPTR, NULL, a0, struct TagItem *, NULL, a1, , base);
    return cs ? cs->mib : 4;
}
static inline APTR ZuneCodesetsConvert(struct Library *base,
    CONST_APTR source, ULONG length, ULONG from, ULONG to)
{
    const ULONG tagbase = 0xfec901f4;
    struct TagItem tags[] = {
        {tagbase + 2, (IPTR)source}, {tagbase + 1, length},
        {tagbase + 31, from}, {tagbase + 32, to},
        {tagbase + 12, FALSE}, {TAG_DONE, 0}
    };
    return LP1(0xa2, APTR, CodesetsConvertStrA,
        struct TagItem *, tags, a0, , base);
}
static inline void ZuneCodesetsFree(struct Library *base, APTR text)
{
    LP2NR(0x5a, CodesetsFreeA, APTR, text, a0,
        struct TagItem *, NULL, a1, , base);
}
#endif

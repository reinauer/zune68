/* Libraries that not every program needs, opened on first use.
 *
 * Forced into every library source by the GNUmakefile. The first
 * ZUNE_ASL() etc. opens the library under a semaphore in the library base
 * (muimaster_init.c); later ones cost one compare. A library that cannot
 * be opened stays NULL and only the feature that needs it fails. Every
 * function that calls into one of these libraries tests its macro first,
 * or works on something (a requester, an IFF handle, a broker, an icon)
 * that exists only because the test succeeded.
 */
#ifndef ZUNE_LAZYLIB_H
#define ZUNE_LAZYLIB_H

#define ZUNE_LIB_ASL        0
#define ZUNE_LIB_CX         1
#define ZUNE_LIB_REXX       2
#define ZUNE_LIB_DATATYPES  3
#define ZUNE_LIB_IFFPARSE   4
#define ZUNE_LIB_DISKFONT   5
#define ZUNE_LIB_ICON       6
#define ZUNE_LIB_WB         7
#define ZUNE_LIB_MATHBAS    8
#define ZUNE_LIB_MATHTRANS  9
#define ZUNE_LIB_COUNT      10

struct Library;
/* Returns the opened base, or NULL if the library cannot be opened. */
struct Library *zune_lazylib_open(unsigned which);

#define ZUNE_LAZY_BASE(base, which) \
    ((base) ? (void *)(base) : (void *)zune_lazylib_open(which))

#define ZUNE_ASL()       ZUNE_LAZY_BASE(AslBase, ZUNE_LIB_ASL)
#define ZUNE_CX()        ZUNE_LAZY_BASE(CxBase, ZUNE_LIB_CX)
#define ZUNE_REXX()      ZUNE_LAZY_BASE(RexxSysBase, ZUNE_LIB_REXX)
#define ZUNE_DATATYPES() ZUNE_LAZY_BASE(DataTypesBase, ZUNE_LIB_DATATYPES)
#define ZUNE_IFFPARSE()  ZUNE_LAZY_BASE(IFFParseBase, ZUNE_LIB_IFFPARSE)
#define ZUNE_DISKFONT()  ZUNE_LAZY_BASE(DiskfontBase, ZUNE_LIB_DISKFONT)
#define ZUNE_ICON()      ZUNE_LAZY_BASE(IconBase, ZUNE_LIB_ICON)
#define ZUNE_WB()        ZUNE_LAZY_BASE(WorkbenchBase, ZUNE_LIB_WB)
/* The soft-float runtime (libnix) calls these bases directly: code that
 * computes with doubles tests ZUNE_MATH(), or ZUNE_MATHTRANS() for sin,
 * cos and atan2, before it starts. */
#define ZUNE_MATH()      zune_lazylib_open(ZUNE_LIB_MATHBAS)
#define ZUNE_MATHTRANS() zune_lazylib_open(ZUNE_LIB_MATHTRANS)

#endif /* ZUNE_LAZYLIB_H */

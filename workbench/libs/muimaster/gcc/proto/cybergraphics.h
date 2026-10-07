#ifndef ZUNE68_GCC_CYBERGRAPHICS_H
#define ZUNE68_GCC_CYBERGRAPHICS_H

#include_next <proto/cybergraphics.h>

/* The native planar build does not open CyberGraphX. These newer APIs
 * are absent from the classic SDK and their paths are disabled at runtime.
 */
#ifdef ZUNE_NO_CYBERGRAPHICS
#define WritePixelArrayAlpha(...) ((void)0)
#define ProcessPixelArray(...) ((void)0)
#define POP_DARKEN 0
#define POP_BRIGHTEN 0
#endif

#endif

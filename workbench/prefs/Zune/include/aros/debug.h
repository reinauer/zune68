#ifndef AROS_DEBUG_H
#define AROS_DEBUG_H

/*
 * Minimal stand-in for <aros/debug.h> when building with SAS/C.
 * DEBUG 0 builds need D() and bug() to exist as no-ops / declarations.
 */

#ifndef bug
#ifdef __amigaos4__
#define bug DebugPrintF
#else
void kprintf(char *fmt, ...);
#define bug kprintf
#endif
#endif

#ifndef D
#define D(x)
#endif

#ifndef ASSERT
#define ASSERT(x)
#endif

#endif /* AROS_DEBUG_H */

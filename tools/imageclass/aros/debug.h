/* Native build: AROS debug output compiles away. */
#ifndef AROS_DEBUG_H
#define AROS_DEBUG_H
#define bug(...) ((void)0)
#ifndef D
#define D(x)
#endif
#endif

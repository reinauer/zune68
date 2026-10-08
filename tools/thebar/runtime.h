/* TheBar uses open-ended BOOPSI object macros closed by MUI's End token.
 * The SDK's function-like NewObject macro cannot parse that syntax. Keep
 * the existing native varargs entry point, backed by NewObjectA instead.
 */
#ifndef ZUNE68_THEBAR_RUNTIME_H
#define ZUNE68_THEBAR_RUNTIME_H
#define __NOLIBBASE__
#include <proto/intuition.h>
#undef __NOLIBBASE__
#undef NewObject
#endif

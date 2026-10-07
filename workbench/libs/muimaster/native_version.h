#ifndef ZUNE68_NATIVE_VERSION_H
#define ZUNE68_NATIVE_VERSION_H

/* Native distribution identity, shared by both native library names.
 * These are Zune68 release numbers, not a promise of a MUI feature level.
 * Keep class implementation versions independent of the library version;
 * external classes retain their own MUIA_Version/MUIA_Revision handlers.
 */
#define ZUNE68_LIBRARY_VERSION 19
#define ZUNE68_LIBRARY_REVISION 81
#define ZUNE68_LIBRARY_DATE "07.10.2026"
#define ZUNE68_STRINGIFY_(x) #x
#define ZUNE68_STRINGIFY(x) ZUNE68_STRINGIFY_(x)
#define ZUNE68_LIBRARY_VERSION_STRING \
    ZUNE68_STRINGIFY(ZUNE68_LIBRARY_VERSION) "." \
    ZUNE68_STRINGIFY(ZUNE68_LIBRARY_REVISION)

/* Retain the established implementation series until a release changes it.
 * Do not raise these to bypass an application's unsupported-feature check. */
#define ZUNE68_BUILTIN_VERSION 1
#define ZUNE68_BUILTIN_REVISION 1

#endif

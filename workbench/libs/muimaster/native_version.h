#ifndef ZUNE68_NATIVE_VERSION_H
#define ZUNE68_NATIVE_VERSION_H

/* Native distribution identity, shared by both native library names.
 * The library reports the version of MUI 5, because applications gate
 * their current code paths on it. That is not a claim of complete MUI 5
 * compatibility, and the number is not bumped for Zune68 releases. A
 * release is identified by ZUNE68_LIBRARY_DATE and ZUNE68_RELEASE_STAGE,
 * which tools/release.py reads from the $VER string to name the archive and
 * fill the readmes; the release tag is the same date as vYYYY.MM.DD.
 * MUI5_TEST=0 builds the AROS source baseline identity (muimaster.conf).
 * External classes retain their own MUIA_Version/MUIA_Revision handlers.
 */
#if ZUNE68_MUI5_TEST
#define ZUNE68_LIBRARY_VERSION 21
#define ZUNE68_LIBRARY_REVISION 227
#else
#define ZUNE68_LIBRARY_VERSION 19
#define ZUNE68_LIBRARY_REVISION 81
#endif
#define ZUNE68_LIBRARY_DATE "01.11.2026"
#define ZUNE68_RELEASE_STAGE "Beta 2"
#define ZUNE68_STRINGIFY_(x) #x
#define ZUNE68_STRINGIFY(x) ZUNE68_STRINGIFY_(x)
#define ZUNE68_LIBRARY_VERSION_STRING \
    ZUNE68_STRINGIFY(ZUNE68_LIBRARY_VERSION) "." \
    ZUNE68_STRINGIFY(ZUNE68_LIBRARY_REVISION)

/* Do not raise these to bypass an application's unsupported-feature check. */
#if ZUNE68_MUI5_TEST
#define ZUNE68_BUILTIN_VERSION 21
#define ZUNE68_BUILTIN_REVISION 227
#else
#define ZUNE68_BUILTIN_VERSION 1
#define ZUNE68_BUILTIN_REVISION 1
#endif

#endif

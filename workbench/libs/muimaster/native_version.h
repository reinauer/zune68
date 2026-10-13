#ifndef ZUNE68_NATIVE_VERSION_H
#define ZUNE68_NATIVE_VERSION_H

/* Native distribution identity, shared by both native library names.
 * The library version follows the AROS source baseline (muimaster.conf);
 * it is not a promise of a MUI feature level and is not bumped for Zune68
 * releases. A Zune68 release is identified by ZUNE68_LIBRARY_DATE and
 * ZUNE68_RELEASE_STAGE, which tools/release.py reads from the $VER string
 * to name the archive and fill the readmes; the release tag is the same
 * date as vYYYY.MM.DD.
 * Keep class implementation versions independent of the library version;
 * The opt-in MUI5_TEST profile deliberately reports 21.227 for validation;
 * it is a development identity, not a complete compatibility claim.
 * External classes retain their own MUIA_Version/MUIA_Revision handlers.
 */
#if ZUNE68_MUI5_TEST
/* Exercise library and class gates together in disposable test guests. */
#define ZUNE68_LIBRARY_VERSION 21
#define ZUNE68_LIBRARY_REVISION 227
#else
#define ZUNE68_LIBRARY_VERSION 19
#define ZUNE68_LIBRARY_REVISION 81
#endif
#define ZUNE68_LIBRARY_DATE "12.10.2026"
#if ZUNE68_MUI5_TEST
#define ZUNE68_RELEASE_STAGE "MUI5 development"
#else
#define ZUNE68_RELEASE_STAGE "Beta 1"
#endif
#define ZUNE68_STRINGIFY_(x) #x
#define ZUNE68_STRINGIFY(x) ZUNE68_STRINGIFY_(x)
#define ZUNE68_LIBRARY_VERSION_STRING \
    ZUNE68_STRINGIFY(ZUNE68_LIBRARY_VERSION) "." \
    ZUNE68_STRINGIFY(ZUNE68_LIBRARY_REVISION)

/* Retain the established implementation series until a release changes it.
 * Do not raise these to bypass an application's unsupported-feature check. */
#if ZUNE68_MUI5_TEST
#define ZUNE68_BUILTIN_VERSION 21
#define ZUNE68_BUILTIN_REVISION 227
#else
#define ZUNE68_BUILTIN_VERSION 1
#define ZUNE68_BUILTIN_REVISION 1
#endif

#endif

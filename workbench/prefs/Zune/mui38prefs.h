#ifndef ZUNE_MUI38PREFS_H
#define ZUNE_MUI38PREFS_H

/*
 * MUI 3.8's settings files (ENVARC:MUI/«Global».prefs, the per-program
 * <BASE>.<n>.prefs files and the presets in MUI:Presets) converted into
 * Zune's form. mui38_convert() is plain C without AmigaOS calls, so that
 * tests/check_mui38_import.py and tests/check_mui38_fuzz.py run it on the
 * build host.
 */

/* Says whether the file of a picture or image class spec ("3:", "4:" or
 * "5:", the name without the prefix) can be found; NULL keeps them all */
typedef int (*mui38_have_file)(const char *name);

/* Convert the file image in[0..inlen) into a Zune settings file image in
 * out[0..outmax). Returns the output length, or -1 when the input is not
 * a FORM PREF or the output does not fit. outmax = inlen + 64 is always
 * enough. */
long mui38_convert(const unsigned char *in, long inlen,
                   unsigned char *out, long outmax, mui38_have_file have);

/* Where the MUIC items of a file mui38_convert() made start (the FORM,
 * PRHD and MUIC headers before them) */
#define MUI38_ITEMS_OFFSET 34

#endif

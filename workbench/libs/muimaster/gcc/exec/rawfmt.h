#ifndef ZUNE68_RAWFMT_H
#define ZUNE68_RAWFMT_H
/* Exec RawDoFmt uses an A3 destination and emits a final NUL. */
static const unsigned short zune_rawfmt_string[] = { 0x16c0, 0x4e75 };
#define RAWFMTFUNC_STRING ((VOID_FUNC)zune_rawfmt_string)
#endif

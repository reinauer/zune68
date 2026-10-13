#ifndef _MUI_CLASSES_TEXTDATA_H
#define _MUI_CLASSES_TEXTDATA_H

#define MUIC_Textdata "Textdata.mui"
#define MUIM_Textdata_Convert 0x80425988
struct MUIP_Textdata_Convert { STACKED ULONG MethodID; STACKED ULONG encoding; };
#define MUIA_Textdata_Contents 0x80420c3f
#define MUIA_Textdata_Encoding 0x8042c587
#define MUIA_Textdata_SourceBytes 0x804242c0
#define MUIA_Textdata_SourceEncoding 0x80426d1a
#ifndef MIBENUM_ISO_8859_1
#define MIBENUM_ISO_8859_1 4
#endif
#ifndef MIBENUM_UTF_8
#define MIBENUM_UTF_8 106
#endif
#ifndef MIBENUM_ISO_10646_UCS_2
#define MIBENUM_ISO_10646_UCS_2 1000
#endif

struct MUI_TextdataPublic
{
    struct MUI_NotifyData notify;
    APTR text;
    ULONG encoding;
};
#define muiTextdata(obj) ((struct MUI_TextdataPublic *)(obj))
#define _text(obj) (muiTextdata(obj)->text)
#define _encoding(obj) (muiTextdata(obj)->encoding)
extern const struct __MUIBuiltinClass _MUI_Textdata_desc; /* PRIV */
#endif

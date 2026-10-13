#ifndef _MUI_CLASSES_OBJECTMAP_H
#define _MUI_CLASSES_OBJECTMAP_H

/* Public MUI map ABI. Implementation is native to Zune68. */
#define MUIC_Objectmap "Objectmap.mui"
#define MUIM_Objectmap_Clear 0x80422ee5
struct MUIP_Objectmap_Clear { STACKED ULONG MethodID; };
#define MUIM_Objectmap_Find 0x80426506
struct MUIP_Objectmap_Find { STACKED ULONG MethodID; STACKED CONST_STRPTR key; };
#define MUIM_Objectmap_Iterate 0x804262bc
struct MUIP_Objectmap_Iterate { STACKED ULONG MethodID; STACKED ULONG *counter; };
#define MUIM_Objectmap_IterationKey 0x8042d7ff
struct MUIP_Objectmap_IterationKey { STACKED ULONG MethodID; STACKED ULONG *counter; };
#define MUIM_Objectmap_Remove 0x8042f649
struct MUIP_Objectmap_Remove { STACKED ULONG MethodID; STACKED CONST_STRPTR key; };
#define MUIM_Objectmap_Set 0x80421ec5
struct MUIP_Objectmap_Set { STACKED ULONG MethodID; STACKED Object *o; STACKED CONST_STRPTR key; };
#define MUIA_Objectmap_AutoLock 0x8042e65f
#define MUIA_Objectmap_CopyKeys 0x8042b964
#define MUIA_Objectmap_Count 0x80426006
#define MUIA_Objectmap_Pool 0x80422ed3

extern const struct __MUIBuiltinClass _MUI_Objectmap_desc; /* PRIV */
#endif

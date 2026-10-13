#ifndef _MUI_CLASSES_DATAMAP_H
#define _MUI_CLASSES_DATAMAP_H

/* Public MUI map ABI. Implementation is native to Zune68. */
#define MUIC_Datamap "Datamap.mui"
#define MUIM_Datamap_Clear 0x8042eebc
struct MUIP_Datamap_Clear { STACKED ULONG MethodID; };
#define MUIM_Datamap_Find 0x8042d650
struct MUIP_Datamap_Find { STACKED ULONG MethodID; STACKED CONST_STRPTR key; };
#define MUIM_Datamap_Get 0x8042c2ba
struct MUIP_Datamap_Get { STACKED ULONG MethodID; STACKED CONST_STRPTR key; STACKED ULONG *size; };
#define MUIM_Datamap_Iterate 0x8042fda1
struct MUIP_Datamap_Iterate { STACKED ULONG MethodID; STACKED ULONG *counter; };
#define MUIM_Datamap_IterationKey 0x8042bc15
struct MUIP_Datamap_IterationKey { STACKED ULONG MethodID; STACKED ULONG *counter; };
#define MUIM_Datamap_Remove 0x804203d8
struct MUIP_Datamap_Remove { STACKED ULONG MethodID; STACKED CONST_STRPTR key; };
#define MUIM_Datamap_Set 0x8042b84f
struct MUIP_Datamap_Set { STACKED ULONG MethodID; STACKED CONST_APTR data; STACKED LONG len; STACKED CONST_STRPTR key; };
#define MUIA_Datamap_AutoLock 0x8042fbe4
#define MUIA_Datamap_CopyKeys 0x8042a179
#define MUIA_Datamap_Count 0x80427580
#define MUIA_Datamap_Pool 0x80424724

extern const struct __MUIBuiltinClass _MUI_Datamap_desc; /* PRIV */
#endif

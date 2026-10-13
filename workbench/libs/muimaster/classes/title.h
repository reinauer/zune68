#ifndef _MUI_CLASSES_TITLE_H
#define _MUI_CLASSES_TITLE_H

/*
    Copyright © 2012, The AROS Development Team. All rights reserved.
    $Id$
*/

/*** Name *******************************************************************/
#define MUIC_Title          "Title.mui"

/*** Identifier base (for Zune extensions) **********************************/


/*** Attributes *************************************************************/
#define MUIA_Title_Clickable            0x80425959
#define MUIA_Title_Closable             0x80420402
#define MUIA_Title_EventHandlerPriority 0x804286bc
#define MUIA_Title_Newable              0x80424145
#define MUIA_Title_OnLastClose          0x804253cf
#define MUIA_Title_Position             0x804273a3
#define MUIA_Title_Sortable             0x804211f1
#define MUIV_Title_EventHandlerPriority_Default 0
#define MUIV_Title_OnLastClose_Remove 0
#define MUIV_Title_OnLastClose_WindowAction 1
#define MUIV_Title_Position_Top 0
#define MUIV_Title_Position_Bottom 1
#define MUIV_Title_Position_Left 2
#define MUIV_Title_Position_Right 3
#define MUIM_Title_Close    0x8042303a
#define MUIM_Title_New      0x804247a6
#define MUIM_Title_FindPage 0x80423d0d
struct MUIP_Title_Close { STACKED ULONG MethodID; STACKED Object *tito; };
struct MUIP_Title_New { STACKED ULONG MethodID; };
struct MUIP_Title_FindPage { STACKED ULONG MethodID; STACKED Object *titlebutton; };



extern const struct __MUIBuiltinClass _MUI_Title_desc;  /* PRIV */

#endif /* _MUI_CLASSES_TITLE_H */

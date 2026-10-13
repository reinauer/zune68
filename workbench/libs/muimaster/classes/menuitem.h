#ifndef _MUI_CLASSES_MENUITEM_H
#define _MUI_CLASSES_MENUITEM_H

/*
    Copyright © 2002-2003, The AROS Development Team. All rights reserved.
    $Id$
*/

/****************************************************************************/
/*** Name *******************************************************************/
#define MUIC_Menustrip         "Menustrip.mui"

/*** Identifier base (for Zune extensions) **********************************/
#define MUIB_Menustrip         (MUIB_ZUNE | 0x00001a00)

/*** Attributes *************************************************************/
#define MUIM_Menustrip_InitChange 0x8042dcd9
#define MUIM_Menustrip_ExitChange 0x8042ce4d
#define MUIM_Menustrip_Popup      0x80420e76
struct MUIP_Menustrip_InitChange { STACKED ULONG MethodID; };
struct MUIP_Menustrip_ExitChange { STACKED ULONG MethodID; };
struct MUIP_Menustrip_Popup
{
    STACKED ULONG MethodID;
    STACKED Object *parent;
    STACKED ULONG flags;
    STACKED LONG x;
    STACKED LONG y;
};
#define MUIA_Menustrip_CaseSensitive 0x8042d718 /* V20 i.g BOOL */
#define MUIA_Menustrip_Enabled (MUIB_MUI | 0x0042815b)    /* MUI: V8  isg BOOL */



/****************************************************************************/
/*** Name *******************************************************************/
#define MUIC_Menu         "Menu.mui"

/*** Identifier base (for Zune extensions) **********************************/
#define MUIB_Menu         (MUIB_ZUNE | 0x00001b00)

/*** Attributes *************************************************************/
#define MUIA_Menu_CopyStrings 0x8042dbe2 /* V20 i.. BOOL */
#define MUIA_Menu_Enabled (MUIB_MUI | 0x0042ed48) /* MUI: V8  isg BOOL   */
#define MUIA_Menu_Title   (MUIB_MUI | 0x0042a0e3) /* MUI: V8  isg STRPTR */



/****************************************************************************/
/*** Name *******************************************************************/
#define MUIC_Menuitem               "Menuitem.mui"

/*** Identifier base (for Zune extensions) **********************************/
#define MUIB_Menuitem               (MUIB_ZUNE | 0x00001c00)

/*** Attributes *************************************************************/
#define MUIA_Menuitem_CopyStrings 0x8042dc1b /* V16 i.. BOOL */
#define MUIA_Menuitem_AISSName    0x804248f2 /* V21 isg STRPTR */
#define MUIA_Menuitem_Image       0x8042080b /* V20 isg struct Image * */
#define MUIA_Menuitem_FreeImage   0x8042f95f /* V20 i.. BOOL */
#define MUIA_Menuitem_Checked \
    (MUIB_MUI | 0x0042562a)       /* MUI: V8  isg BOOL              */
#define MUIA_Menuitem_Checkit \
    (MUIB_MUI | 0x00425ace)       /* MUI: V8  isg BOOL              */
#define MUIA_Menuitem_CommandString \
    (MUIB_MUI | 0x0042b9cc)       /* MUI: V16 isg BOOL              */
#define MUIA_Menuitem_Enabled \
    (MUIB_MUI | 0x0042ae0f)       /* MUI: V8  isg BOOL              */
#define MUIA_Menuitem_Exclude \
    (MUIB_MUI | 0x00420bc6)       /* MUI: V8  isg LONG              */
#define MUIA_Menuitem_Shortcut \
    (MUIB_MUI | 0x00422030)       /* MUI: V8  isg STRPTR            */
#define MUIA_Menuitem_Title \
    (MUIB_MUI | 0x004218be)       /* MUI: V8  isg STRPTR            */
#define MUIA_Menuitem_Toggle \
    (MUIB_MUI | 0x00424d5c)       /* MUI: V8  isg BOOL              */
#define MUIA_Menuitem_Trigger \
    (MUIB_MUI | 0x00426f32)       /* MUI: V8  ..g struct MenuItem * */

#define MUIA_Menuitem_NewMenu \
    (MUIB_Menuitem | 0x00000000)  /* Zune: V1 ..g struct NewMenu *  */


#define MUIV_Menuitem_Shortcut_Check (-1)


extern const struct __MUIBuiltinClass _MUI_Menuitem_desc;       /* PRIV */
extern const struct __MUIBuiltinClass _MUI_Menu_desc;           /* PRIV */
extern const struct __MUIBuiltinClass _MUI_Menustrip_desc;      /* PRIV */

#endif /* _MUI_CLASSES_MENUITEM_H */

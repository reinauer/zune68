#ifndef ZUNE_SELECTORPANEL_H
#define ZUNE_SELECTORPANEL_H
/* MUI requester panels. The AROS collapsible Panel has a different API. */
#define MUIM_Panel_Run 0x8042d789
struct MUIP_Panel_Run { STACKED ULONG MethodID; STACKED Object *app, *win; };
#define MUIC_Filepanel "Filepanel.mui"
#define MUIC_Fontpanel "Fontpanel.mui"
#define MUIA_Filepanel_AcceptPattern 0x80426f3b
#define MUIA_Filepanel_DoMultiSelect 0x8042fd78
#define MUIA_Filepanel_DoPatterns 0x80420b3b
#define MUIA_Filepanel_DoSaveMode 0x80429022
#define MUIA_Filepanel_Drawer 0x8042e802
#define MUIA_Filepanel_DrawersOnly 0x80427726
#define MUIA_Filepanel_File 0x80427acf
#define MUIA_Filepanel_FilterDrawers 0x804298a1
#define MUIA_Filepanel_FilterFunc 0x80429c9d
#define MUIA_Filepanel_Pattern 0x8042c330
#define MUIA_Filepanel_RejectIcons 0x80423450
#define MUIA_Filepanel_RejectPattern 0x804281ab
#define MUIA_Fontpanel_ShowCollection 0x804225ea
#define MUIV_Fontpanel_ShowCollection_All (-1)
#define MUIV_Fontpanel_ShowCollection_FixedWidth 1
#define MUIV_Fontpanel_ShowCollection_Bitmap 2
#define MUIV_Fontpanel_ShowCollection_TrueType 4
#define MUIV_Fontpanel_ShowCollection_User 8
extern const struct __MUIBuiltinClass _MUI_SelectorPanel_desc;
extern const struct __MUIBuiltinClass _MUI_Filepanel_desc;
extern const struct __MUIBuiltinClass _MUI_Fontpanel_desc;

/* Internal messages shared by the selector implementations. */
#define MUIM_Zune_PanelFinish (MUIB_ZUNE | 0x4500)
#define MUIM_Zune_PanelAction (MUIB_ZUNE | 0x4501)
#endif

/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.

*/
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <exec/types.h>
#include <prefs/prefhdr.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/iffparse.h>
#include <proto/dos.h>
#include <proto/commodities.h>
#include <proto/muimaster.h>

/*  #define MYDEBUG 1 */
#include "debug.h"

#include "muimaster_intern.h"
#include "mui.h"
#include "support.h"
#include "prefs.h"
#include "imspec.h"

#include <proto/muiscreen.h>

#define PUBSCREEN_FILE "env:Zune/PublicScreens.iff"

#define DFSN(x)
#define DPSD(x)

extern struct Library *MUIMasterBase;

struct MUI_ConfigdataData
{
    Object                      *app;
    CONST_STRPTR                appbase;
    struct ZunePrefsNew         prefs;
    struct SignalSemaphore      psLock;
    struct MsgPort             *fsNotifyPort;
    struct DateStamp            created;   /* public screens read before this */
    struct MsgPort             *signalPort;
    struct MUI_InputHandlerNode fsNotifyIHN;
    struct NotifyRequest        fsNotifyRequest;
    struct List                 pubscreens;
    struct IClass             *init_dataspace;
};

IPTR Dataspace__MUIM_Find(struct IClass *cl, Object *obj,
    struct MUIP_Dataspace_Find *msg);

/* A Configdata is built for every application and reads about 80
 * settings at creation; ask the dataspace, our superclass, for each
 * without passing through our own dispatcher first, and take the
 * defaults from the tables being walked (perfbench create+dispose) */
static APTR cfg_find(struct IClass *cl, Object *obj, ULONG id)
{
    struct MUIP_Dataspace_Find msg = { MUIM_Dataspace_Find, id };

    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);

    /* The constructor holds the dataspace lock across all initial reads. */
    if (data->init_dataspace)
        return (APTR)Dataspace__MUIM_Find(data->init_dataspace, obj, &msg);
    return (APTR) DoSuperMethodA(cl, obj, (Msg) &msg);
}

static ULONG cfg_ulong(struct IClass *cl, Object *obj, ULONG id);
static CONST_STRPTR cfg_string(struct IClass *cl, Object *obj, ULONG id);




/**************************************************************************
 Default ImageSpec values
**************************************************************************/

struct spec_cfg
{
    ULONG muiv;
    ULONG cfgid;
    CONST_STRPTR defspec;
};

const static struct spec_cfg DefImspecValues[] =
{
    { MUII_WindowBack,    MUICFG_Background_Window,     "0:128" },
    { MUII_RequesterBack, MUICFG_Background_Requester,  "0:137" },
    { MUII_ButtonBack,    MUICFG_Background_Button,     "0:128" },
    { MUII_ListBack,      MUICFG_Background_List,       "0:128" },
    { MUII_TextBack,      MUICFG_Background_Text,       "0:128" },
    { MUII_PropBack,      MUICFG_Background_Prop,       "0:128" },
    { MUII_PopupBack,     MUICFG_Background_PopUp,      "0:128" },
    { MUII_SelectedBack,  MUICFG_Background_Selected,   "0:131" },
    { MUII_ListCursor,    MUICFG_Background_ListCursor, "0:131" },
    { MUII_ListSelect,    MUICFG_Background_ListSelect, "0:135" },
    { MUII_ListSelCur,    MUICFG_Background_ListSelCur, "0:138" },
    { MUII_ArrowUp,       MUICFG_Image_ArrowUp,         "1:0" },
    { MUII_ArrowDown,     MUICFG_Image_ArrowDown,       "1:1" },
    { MUII_ArrowLeft,     MUICFG_Image_ArrowLeft,       "1:2" },
    { MUII_ArrowRight,    MUICFG_Image_ArrowRight,      "1:3" },
    { MUII_CheckMark,     MUICFG_Image_CheckMark,       "1:4" },
    { MUII_RadioButton,   MUICFG_Image_RadioButton,     "1:5" },
    { MUII_Cycle,         MUICFG_Image_Cycle,           "1:6" },
    { MUII_PopUp,         MUICFG_Image_PopUp,           "1:7" },
    { MUII_PopFile,       MUICFG_Image_PopFile,         "1:8" },
    { MUII_PopDrawer,     MUICFG_Image_PopDrawer,       "1:9" },
    { MUII_PropKnob,      MUICFG_Image_PropKnob,        "0:129" },
    { MUII_Drawer,        MUICFG_Image_Drawer,          "1:10" },
    { MUII_HardDisk,      MUICFG_Image_HardDisk,        "1:11" },
    { MUII_Disk,          MUICFG_Image_Disk,            "1:12" },
    { MUII_Chip,          MUICFG_Image_Chip,            "1:13" },
    { MUII_Volume,        MUICFG_Image_Volume,          "1:14" },
    { MUII_RegisterBack,  MUICFG_Background_Register,   "0:128" },
    { MUII_Network,       MUICFG_Image_Network,         "1:15" },
    { MUII_Assign,        MUICFG_Image_Assign,          "1:16" },
    { MUII_TapePlay,      MUICFG_Image_TapePlay,        "1:17" },
    { MUII_TapePlayBack,  MUICFG_Image_TapePlayBack,    "1:18" },
    { MUII_TapePause,     MUICFG_Image_TapePause,       "1:19" },
    { MUII_TapeStop,      MUICFG_Image_TapeStop,        "1:20" },
    { MUII_TapeRecord,    MUICFG_Image_TapeRecord,      "1:21" },
    { MUII_GroupBack,     MUICFG_Background_Framed,     "0:128" },
    { MUII_SliderBack,    MUICFG_Background_Slider,     "0:128" },
    { MUII_SliderKnob,    MUICFG_Background_SliderKnob, "0:128" },
    { MUII_TapeUp,        MUICFG_Image_TapeUp,          "1:22" },
    { MUII_TapeDown,      MUICFG_Image_TapeDown,        "1:23" },
    { MUII_PageBack,      MUICFG_Background_Page,       "0:128" },
    { MUII_ReadListBack,  MUICFG_Background_ReadList,   "0:128" },
    /* MUI 3.9 draws the font popup like the popup arrow and has no
     * setting of its own for it (tests/native/imgspec.c) */
    { MUII_PopFont,       MUICFG_Image_PopUp,           "1:7" },
    { 0,                  0,                            NULL },
};

/* called by Configdata_New */
static void init_imspecs(struct IClass *cl, Object *obj,
    struct MUI_ConfigdataData *data)
{
    int i;

    for (i = 0; DefImspecValues[i].defspec; i++)
    {
        CONST_STRPTR imspec;
        const struct spec_cfg *img = DefImspecValues + i;

        imspec = cfg_find(cl, obj, img->cfgid);
        if (!imspec)
            imspec = img->defspec;
/*          D(bug("init_imspecs: %ld %lx %s ...\n", img->muiv, img->cfgid, imspec)); */
        data->prefs.imagespecs[img->muiv] = imspec;
        if (!data->prefs.imagespecs[img->muiv])
        {
/*              D(bug("*** init_imspecs: null imagespec %ld\n", img->muiv)); */
        }
    }
}

/**************************************************************************
 Default FrameSpec values
**************************************************************************/

/* spec format : type, recessed, left, right, up, down spacing */
const static struct spec_cfg DefFramespecValues[] =
{
    { MUIV_Frame_None,        MUICFG_Invalid,           "000000" }, /* invisible frame          */
    { MUIV_Frame_Button,      MUICFG_Frame_Button,      "202222" }, /* text button              */
    { MUIV_Frame_ImageButton, MUICFG_Frame_ImageButton, "202222" }, /* image button             */
    { MUIV_Frame_Text,        MUICFG_Frame_Text,        "212222" }, /* textfield without input  */
    { MUIV_Frame_String,      MUICFG_Frame_String,      "302222" }, /* string gadget            */
    { MUIV_Frame_ReadList,    MUICFG_Frame_ReadList,    "212222" }, /* list without input       */
    { MUIV_Frame_InputList,   MUICFG_Frame_InputList,   "202222" }, /* list with input          */
    { MUIV_Frame_Prop,        MUICFG_Frame_Prop,        "202222" }, /* scrollbar container      */
    { MUIV_Frame_Gauge,       MUICFG_Frame_Gauge,       "210000" }, /* gauge                    */
    { MUIV_Frame_Group,       MUICFG_Frame_Group,       "314444" }, /* normal group             */
    { MUIV_Frame_PopUp,       MUICFG_Frame_PopUp,       "112222" }, /* cycle menu, popup window */
    { MUIV_Frame_Virtual,     MUICFG_Frame_Virtual,     "212222" }, /* virt group               */
    { MUIV_Frame_Slider,      MUICFG_Frame_Slider,      "400000" }, /* slider container         */
    { MUIV_Frame_Knob,        MUICFG_Frame_Knob,        "202211" }, /* slider knob              */
    { MUIV_Frame_Drag,        MUICFG_Frame_Drag,        "300000" }, /* dnd frame                */
    { -1,                     -1,                       NULL     },
};

/* called by Configdata_New */
static void init_framespecs(struct IClass *cl, Object *obj,
    struct MUI_ConfigdataData *data)
{
    int i;

    for (i = 0; DefFramespecValues[i].defspec; i++)
    {
        CONST_STRPTR framespec;
        const struct spec_cfg *fcfg = DefFramespecValues + i;

        framespec = cfg_find(cl, obj, fcfg->cfgid);
        if (!framespec)
            framespec = fcfg->defspec;
        zune_frame_spec_to_intern(framespec,
            &data->prefs.frames[fcfg->muiv]);
    }
}

/**************************************************************************
 Default ULONG values
**************************************************************************/

struct def_ulval
{
    ULONG id;
    ULONG val;
    UWORD offset;
    UWORD size;
};

#define PREF_DEFAULT(id, value, field) \
    {id, value, offsetof(struct ZunePrefsNew, field), \
     sizeof(((struct ZunePrefsNew *)0)->field)}

const static struct def_ulval DefULValues[] = {
    PREF_DEFAULT(MUICFG_Window_Spacing_Left, 4, window_inner_left),
    PREF_DEFAULT(MUICFG_Window_Spacing_Right, 4, window_inner_right),
    PREF_DEFAULT(MUICFG_Window_Spacing_Top, 4, window_inner_top),
#ifdef ZUNE68_GCC_NATIVE
    PREF_DEFAULT(MUICFG_Window_Buttons, MUIV_Window_Button_MUI | MUIV_Window_Button_Iconify, window_buttons),
#else
    PREF_DEFAULT(MUICFG_Window_Buttons, 0, window_buttons),
#endif
    PREF_DEFAULT(MUICFG_Window_Spacing_Bottom, 4, window_inner_bottom),
    PREF_DEFAULT(MUICFG_Window_Positions, WINDOW_POSITION_FORGET_ON_EXIT, window_position),
    PREF_DEFAULT(MUICFG_Window_Redraw, WINDOW_REDRAW_WITHOUT_CLEAR, window_redraw),
    /* MUI 3.8's default; MUI 3.9 changed to simple refresh, which loses
     * covered contents of applications that draw outside MUIM_Draw
     * (AmiPG_MUI's keyboard) and repaints slower */
    PREF_DEFAULT(MUICFG_Window_Refresh, WINDOW_REFRESH_SMART, window_refresh),
    PREF_DEFAULT(MUICFG_Radio_HSpacing, 4, radiobutton_hspacing),
    PREF_DEFAULT(MUICFG_Radio_VSpacing, 1, radiobutton_vspacing),
    PREF_DEFAULT(MUICFG_Group_HSpacing, 4, group_hspacing),
    PREF_DEFAULT(MUICFG_Group_VSpacing, 4, group_vspacing),
    PREF_DEFAULT(MUICFG_Cycle_MenuCtrl_Position, CYCLE_MENU_POSITION_BELOW, cycle_menu_position),
    PREF_DEFAULT(MUICFG_Cycle_MenuCtrl_Level, 2, cycle_menu_min_entries),
    PREF_DEFAULT(MUICFG_Cycle_MenuCtrl_Speed, 0, cycle_menu_speed),
    PREF_DEFAULT(MUICFG_Cycle_Menu_Recessed, FALSE, cycle_menu_recessed_entries),
    PREF_DEFAULT(MUICFG_Listview_Font_Leading, 1, list_linespacing),
    PREF_DEFAULT(MUICFG_Listview_Smoothed, FALSE, list_smoothed),
    PREF_DEFAULT(MUICFG_Listview_SmoothVal, 0, list_smoothval),
    PREF_DEFAULT(MUICFG_Listview_Refresh, LISTVIEW_REFRESH_MIXED, list_refresh),
    PREF_DEFAULT(MUICFG_Listview_Multi, LISTVIEW_MULTI_SHIFTED, list_multi),
    PREF_DEFAULT(MUICFG_GroupTitle_Position, GROUP_TITLE_POSITION_CENTERED, group_title_position),
    PREF_DEFAULT(MUICFG_GroupTitle_Color, GROUP_TITLE_COLOR_HILITE, group_title_color),
    PREF_DEFAULT(MUICFG_Scrollbar_Type, SCROLLBAR_TYPE_STANDARD, scrollbar_type),
    PREF_DEFAULT(MUICFG_Scrollbar_Arrangement, SCROLLBAR_ARRANGEMENT_TOP, scrollbar_arrangement),
    PREF_DEFAULT(MUICFG_Balance_Look, BALANCING_SHOW_FRAMES, balancing_look),
    PREF_DEFAULT(MUICFG_Dragndrop_Look, DND_LOOK_GHOSTED_ON_BOX, drag_look),
    PREF_DEFAULT(MUICFG_Drag_Autostart, TRUE, drag_autostart),
    PREF_DEFAULT(MUICFG_Drag_Autostart_Length, 3, drag_autostart_length),
    PREF_DEFAULT(MUICFG_Drag_LeftButton, TRUE, drag_left_button),
    PREF_DEFAULT(MUICFG_Drag_MiddleButton, FALSE, drag_middle_button),
    PREF_DEFAULT(MUICFG_Register_TruncateTitles, FALSE, register_truncate_titles),
    PREF_DEFAULT(MUICFG_Screen_Mode, 0, screenmode),
    PREF_DEFAULT(MUICFG_Screen_Mode_ID, -1, screenmodeid),
    PREF_DEFAULT(MUICFG_Screen_Width, 0, screen_width),
    PREF_DEFAULT(MUICFG_Screen_Height, 0, screen_height),
    PREF_DEFAULT(MUICFG_PublicScreen_PopToFront, TRUE, publicscreen_pop_to_front),
    PREF_DEFAULT(MUICFG_Iconification_ShowIcon, TRUE, iconification_show_icon),
    PREF_DEFAULT(MUICFG_Iconification_ShowMenu, FALSE, iconification_show_menu),
    PREF_DEFAULT(MUICFG_Iconification_OnStartup, FALSE, iconification_on_startup),
    PREF_DEFAULT(MUICFG_Interfaces_EnableARexx, TRUE, interfaces_enable_arexx),
    PREF_DEFAULT(MUICFG_BubbleHelp_FirstDelay, 30, bubblehelp_first_delay),
    PREF_DEFAULT(MUICFG_BubbleHelp_NextDelay, 10, bubblehelp_next_delay),
    {0, 0, 0, 0},
};

#undef PREF_DEFAULT

/**************************************************************************
 Default string values
**************************************************************************/

struct def_strval
{
    ULONG id;
    CONST_STRPTR val;
};

/* NULL values not allowed */
const static struct def_strval DefStrValues[] = {
    {MUICFG_Font_Normal, ""},
    {MUICFG_Font_List, ""},
    {MUICFG_Font_Tiny, "helvetica/9"},
    {MUICFG_Font_Fixed, ""},
    {MUICFG_Font_Title, "helvetica/9"},
    {MUICFG_Font_Big, "helvetica/15"},
    {MUICFG_Font_Button, ""},
    {MUICFG_Font_Knob, ""},
    {MUICFG_String_Background, "2:m2"},
    {MUICFG_String_Text, "m5"},
    {MUICFG_String_ActiveBackground, "2:m1"},
    {MUICFG_String_ActiveText, "m5"},
    {MUICFG_String_Cursor, "m7"},
    {MUICFG_String_MarkedBackground, "m6"},
    {MUICFG_String_MarkedText, "m0"},
    {MUICFG_ActiveObject_Color, "m0"},
    {MUICFG_Keyboard_Press, "-upstroke return"},
    {MUICFG_Keyboard_Toggle, "-repeat space"},
    {MUICFG_Keyboard_Up, "-repeat up"},
    {MUICFG_Keyboard_Down, "-repeat down"},
    {MUICFG_Keyboard_PageUp, "-repeat shift up"},
    {MUICFG_Keyboard_PageDown, "-repeat shift down"},
    {MUICFG_Keyboard_Top, "control up"},
    {MUICFG_Keyboard_Bottom, "control down"},
    {MUICFG_Keyboard_Left, "-repeat left"},
    {MUICFG_Keyboard_Right, "-repeat right"},
    {MUICFG_Keyboard_WordLeft, "-repeat control left"},
    {MUICFG_Keyboard_WordRight, "-repeat control right"},
    {MUICFG_Keyboard_LineStart, "shift left"},
    {MUICFG_Keyboard_LineEnd, "shift right"},
    {MUICFG_Keyboard_NextGadget, "-repeat tab"},
    {MUICFG_Keyboard_PrevGadget, "-repeat shift tab"},
    {MUICFG_Keyboard_GadgetOff, "control tab"},
    {MUICFG_Keyboard_CloseWindow, "esc"},
    {MUICFG_Keyboard_NextWindow, "-repeat alt tab"},
    {MUICFG_Keyboard_PrevWindow, "-repeat alt shift tab"},
    {MUICFG_Keyboard_Help, "help"},
    {MUICFG_Keyboard_Popup, "control p"},
    {MUICFG_Drag_LMBModifier, "control"},
    {MUICFG_Drag_MMBModifier, ""},
    {MUICFG_PublicScreen, ""},
    {MUICFG_Iconification_Hotkey, ""},
    {0, 0},
};

/* ParseIX inverts the keymap and compares qualifier names: about 0.3 ms
 * per binding, and every Configdata parses 24 of them. The default
 * strings are constants shared by all applications, so their parsed
 * expressions are kept here; saved bindings are parsed per object. */
#define KEYCACHE 32
static struct { CONST_STRPTR str; IX ix; LONG well; } keycache[KEYCACHE];
static int keycache_count;

static BOOL is_default_string(CONST_STRPTR val)
{
    int i;

    for (i = 0; DefStrValues[i].id; i++)
        if (DefStrValues[i].val == val)
            return TRUE;
    return FALSE;
}

static void Configdata__SetKey(ZuneKeySpec *key, CONST_STRPTR val)
{
    int i, count;

    if ((key->readable_hotkey = val) == NULL)
    {
        key->ix_well = 0;
        return;
    }
    count = keycache_count;
    for (i = 0; i < count; i++)
        if (keycache[i].str == val)
        {
            key->ix = keycache[i].ix;
            key->ix_well = keycache[i].well;
            return;
        }
    key->ix_well = !ParseIX(key->readable_hotkey, &key->ix);
    if (is_default_string(val))
    {
        Forbid();
        if (keycache_count < KEYCACHE)
        {
            keycache[keycache_count].str = val;
            keycache[keycache_count].ix = key->ix;
            keycache[keycache_count].well = key->ix_well;
            keycache_count++;
        }
        Permit();
    }
}

/* Always assign a !persistent! string stored in dataspace (or default) to ZunePrefsNew fields */
static void Configdata_StoreString(struct MUI_ConfigdataData *data,
    ULONG id, CONST_STRPTR val)
{

    switch (id) {
    case MUICFG_Font_Normal:
        data->prefs.fonts[-MUIV_Font_Normal] = val;
        break;

    case MUICFG_Font_List:
        data->prefs.fonts[-MUIV_Font_List] = val;
        break;

    case MUICFG_Font_Tiny:
        data->prefs.fonts[-MUIV_Font_Tiny] = val;
        break;

    case MUICFG_Font_Fixed:
        data->prefs.fonts[-MUIV_Font_Fixed] = val;
        break;

    case MUICFG_Font_Title:
        data->prefs.fonts[-MUIV_Font_Title] = val;
        break;

    case MUICFG_Font_Big:
        data->prefs.fonts[-MUIV_Font_Big] = val;
        break;

    case MUICFG_Font_Button:
        data->prefs.fonts[-MUIV_Font_Button] = val;
        break;

    case MUICFG_Font_Knob:
        data->prefs.fonts[-MUIV_Font_Knob] = val;
        break;

    /*---------- system stuff ----------*/
    case MUICFG_PublicScreen:
        data->prefs.publicscreen_name = val;
        break;

    case MUICFG_Iconification_Hotkey:
        data->prefs.iconification_hotkey = val;
        break;

    /*---------- Strings ----------*/
    case MUICFG_String_Background:
        data->prefs.string_bg_inactive = val;
        break;

    case MUICFG_String_Text:
        data->prefs.string_text_inactive = val;
        break;

    case MUICFG_String_ActiveBackground:
        data->prefs.string_bg_active = val;
        break;

    case MUICFG_String_ActiveText:
        data->prefs.string_text_active = val;
        break;

    case MUICFG_String_Cursor:
        data->prefs.string_cursor = val;
        break;

    case MUICFG_String_MarkedBackground:
        data->prefs.string_bg_marked = val;
        break;

    case MUICFG_String_MarkedText:
        data->prefs.string_text_marked = val;
        break;

    /*---------- Navigation ----------*/

    case MUICFG_Drag_LMBModifier:
        Configdata__SetKey(&data->prefs.drag_left_modifier, val);
        break;

    case MUICFG_Drag_MMBModifier:
        Configdata__SetKey(&data->prefs.drag_middle_modifier, val);
        break;

    case MUICFG_ActiveObject_Color:
        data->prefs.active_object_color = val;
        break;

    /*---------- mui keys ----------*/
    case MUICFG_Keyboard_Press:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_PRESS], val);
        break;

    case MUICFG_Keyboard_Toggle:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_TOGGLE], val);
        break;

    case MUICFG_Keyboard_Up:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_UP], val);
        break;

    case MUICFG_Keyboard_Down:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_DOWN], val);
        break;

    case MUICFG_Keyboard_PageUp:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_PAGEUP], val);
        break;

    case MUICFG_Keyboard_PageDown:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_PAGEDOWN], val);
        break;

    case MUICFG_Keyboard_Top:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_TOP], val);
        break;

    case MUICFG_Keyboard_Bottom:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_BOTTOM], val);
        break;

    case MUICFG_Keyboard_Left:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_LEFT], val);
        break;

    case MUICFG_Keyboard_Right:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_RIGHT], val);
        break;

    case MUICFG_Keyboard_WordLeft:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_WORDLEFT], val);
        break;

    case MUICFG_Keyboard_WordRight:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_WORDRIGHT], val);
        break;

    case MUICFG_Keyboard_LineStart:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_LINESTART], val);
        break;

    case MUICFG_Keyboard_LineEnd:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_LINEEND], val);
        break;

    case MUICFG_Keyboard_NextGadget:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_GADGET_NEXT], val);
        break;

    case MUICFG_Keyboard_PrevGadget:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_GADGET_PREV], val);
        break;

    case MUICFG_Keyboard_GadgetOff:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_GADGET_OFF], val);
        break;

    case MUICFG_Keyboard_CloseWindow:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_WINDOW_CLOSE], val);
        break;

    case MUICFG_Keyboard_NextWindow:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_WINDOW_NEXT], val);
        break;

    case MUICFG_Keyboard_PrevWindow:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_WINDOW_PREV], val);
        break;

    case MUICFG_Keyboard_Help:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_HELP], val);
        break;

    case MUICFG_Keyboard_Popup:
        Configdata__SetKey(&data->prefs.muikeys[MUIKEY_POPUP], val);
        break;

    /*---------- CustomFrames ----------*/
    case MUICFG_CustomFrame_1:
        data->prefs.customframe_config_1 = val;
        break;
    case MUICFG_CustomFrame_2:
        data->prefs.customframe_config_2 = val;
        break;
    case MUICFG_CustomFrame_3:
        data->prefs.customframe_config_3 = val;
        break;
    case MUICFG_CustomFrame_4:
        data->prefs.customframe_config_4 = val;
        break;
    case MUICFG_CustomFrame_5:
        data->prefs.customframe_config_5 = val;
        break;
    case MUICFG_CustomFrame_6:
        data->prefs.customframe_config_6 = val;
        break;
    case MUICFG_CustomFrame_7:
        data->prefs.customframe_config_7 = val;
        break;
    case MUICFG_CustomFrame_8:
        data->prefs.customframe_config_8 = val;
        break;
    case MUICFG_CustomFrame_9:
        data->prefs.customframe_config_9 = val;
        break;
    case MUICFG_CustomFrame_10:
        data->prefs.customframe_config_10 = val;
        break;
    case MUICFG_CustomFrame_11:
        data->prefs.customframe_config_11 = val;
        break;
    case MUICFG_CustomFrame_12:
        data->prefs.customframe_config_12 = val;
        break;
    case MUICFG_CustomFrame_13:
        data->prefs.customframe_config_13 = val;
        break;
    case MUICFG_CustomFrame_14:
        data->prefs.customframe_config_14 = val;
        break;
    case MUICFG_CustomFrame_15:
        data->prefs.customframe_config_15 = val;
        break;
    case MUICFG_CustomFrame_16:
        data->prefs.customframe_config_16 = val;
        break;
    }
}

static void Configdata_SynchronizeString(struct IClass *cl, Object *obj,
    ULONG id)
{
    Configdata_StoreString(INST_DATA(cl, obj), id,
        cfg_string(cl, obj, id));
}

/**************************************************************************
 OM_NEW
 Load global (and maybe application-specific) prefs files into the dataspace,
 then fill the prefs struct with dataspace or default values
**************************************************************************/
/**************************************************************************
 MUIM_Configdata_SetupNotify: watch the public screen file. Called by the
 application when it first waits for input.
**************************************************************************/
static IPTR Configdata_LoadPubScreens(struct IClass *cl, Object *obj,
    Msg msg);

static IPTR Configdata__MUIM_SetupNotify(struct IClass *cl, Object *obj,
    Msg msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);

    if (data->fsNotifyPort || !data->app)
        return 0;
    /* Shares the application's signal when it gave us one */
    data->fsNotifyPort = ZuneCreateSharedPort(data->signalPort);
    if (data->fsNotifyPort) {
        /* Setup filesystem notification handler ---------------------------*/
        data->fsNotifyIHN.ihn_Signals = 1UL << data->fsNotifyPort->mp_SigBit;
        data->fsNotifyIHN.ihn_Object  = obj;
        data->fsNotifyIHN.ihn_Method  = MUIM_Configdata_PubScreensChanged;

        DoMethod(data->app, MUIM_Application_AddInputHandler, (IPTR)&data->fsNotifyIHN);

        data->fsNotifyRequest.nr_Name                 = PUBSCREEN_FILE;
        data->fsNotifyRequest.nr_Flags                = NRF_SEND_MESSAGE;
        data->fsNotifyRequest.nr_stuff.nr_Msg.nr_Port = data->fsNotifyPort;
        if (StartNotify(&data->fsNotifyRequest)) {
            DFSN(bug("[MUI:Cfg] %s: FileSystem-Notification setup for '%s'\n", __func__, data->fsNotifyRequest.nr_Name));
        } else {
            DFSN(bug("[MUI:Cfg] %s: FAILED to setup FileSystem-Notification for '%s'\n", __func__, data->fsNotifyRequest.nr_Name));
            DoMethod(data->app, MUIM_Application_RemInputHandler, (IPTR)&data->fsNotifyIHN);
            if (data->signalPort)
                ZuneDeleteSharedPort(data->fsNotifyPort);
            else
                DeleteMsgPort(data->fsNotifyPort);
            data->fsNotifyPort = NULL;
        }
    }

    /* Catch a public screen file written before the notification */
    if (data->fsNotifyPort
        && zune_file_written_since(PUBSCREEN_FILE, &data->created))
        Configdata_LoadPubScreens(cl, obj, msg);
    return data->fsNotifyPort != NULL;
}

IPTR Configdata__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_ConfigdataData *data;
    struct TagItem *tags;
    struct TagItem *tag;
    //APTR cdata;
    int i, res = 0;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
        return (IPTR) NULL;

/*      D(bug("Configdata_New(%p)\n", obj)); */

    data = INST_DATA(cl, obj);

    NEWLIST(&data->pubscreens);
    InitSemaphore(&data->psLock);
    DateStamp(&data->created);

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Configdata_SignalPort:
            data->signalPort = (struct MsgPort *) tag->ti_Data;
            break;

        case MUIA_Configdata_Application:
            data->app = (Object *) tag->ti_Data;
            break;
        case MUIA_Configdata_ApplicationBase:
            data->appbase = (CONST_STRPTR) tag->ti_Data;
            break;
        }
    }

    if (data->app && !data->appbase)
    {
        get(data->app, MUIA_Application_Base, &data->appbase);
    }

    /* Without a preference drawer there is nothing to load: one Lock
     * replaces up to four failing Open calls (each a handler round
     * trip) per application. ENV: is restored from ENVARC: at boot and
     * the preferences program creates both drawers when it first saves. */
    {
        BPTR drawer = Lock("ENV:zune", SHARED_LOCK);

        if (drawer)
            UnLock(drawer);
        else
            res = -1;
    }

    if (data->appbase && res == 0)
    {
        char filename[255];
        snprintf(filename, 255, "ENV:zune/%s.prefs", data->appbase);
        res = DoMethod(obj, MUIM_Configdata_Load, (IPTR) filename);
        if (!res)
        {
            snprintf(filename, 255, "ENVARC:zune/%s.prefs", data->appbase);
            res = DoMethod(obj, MUIM_Configdata_Load, (IPTR) filename);
        }
    }

    // load only global prefs if no local app pref is found
    if (res == 0)
    {
        if (!DoMethod(obj, MUIM_Configdata_Load,
                (IPTR) "ENV:zune/global.prefs"))
        {
            DoMethod(obj, MUIM_Configdata_Load,
                (IPTR) "ENVARC:zune/global.prefs");
        }
    }

    /* The public screen file notification starts when the application
     * first waits for input (MUIM_Configdata_SetupNotify): a DOS
     * notification costs a handler round trip, which create/dispose
     * cycles and non-interactive uses never need. */

    /* Only bypass dispatch for the builtin object during construction.
     * Subclasses retain their inherited lookup and semaphore methods. */
    if (OCLASS(obj) == cl && cl->cl_Super->cl_ID &&
        !strcmp(cl->cl_Super->cl_ID, MUIC_Dataspace))
    {
        DoMethod(obj, MUIM_Semaphore_ObtainShared);
        data->init_dataspace = cl->cl_Super;
    }

    /*---------- init strings ----------*/
    for (i = 0; DefStrValues[i].id; i++)
    {
        CONST_STRPTR value = cfg_find(cl, obj, DefStrValues[i].id);
        Configdata_StoreString(data, DefStrValues[i].id,
            value ? value : DefStrValues[i].val);
    }

    /*---------- images stuff ----------*/

    init_imspecs(cl, obj, data);

    /*---------- frame stuff ----------*/

    init_framespecs(cl, obj, data);

    /* Walk defaults once, storing each value in its native field width. */
    for (i = 0; DefULValues[i].id; i++)
    {
        const struct def_ulval *def = &DefULValues[i];
        ULONG *stored = cfg_find(cl, obj, def->id);
        ULONG value = stored ? AROS_BE2LONG(*stored) : def->val;
        APTR target = (UBYTE *)&data->prefs + def->offset;
        if (def->size == sizeof(UWORD))
            *(UWORD *)target = value;
        else
            *(ULONG *)target = value;
    }
    data->prefs.register_look = REGISTER_LOOK_TRADITIONAL;

    /*---------- CustomFrames ----------*/
    data->prefs.customframe_config_1 =
        cfg_find(cl, obj, MUICFG_CustomFrame_1);
    data->prefs.customframe_config_2 =
        cfg_find(cl, obj, MUICFG_CustomFrame_2);
    data->prefs.customframe_config_3 =
        cfg_find(cl, obj, MUICFG_CustomFrame_3);
    data->prefs.customframe_config_4 =
        cfg_find(cl, obj, MUICFG_CustomFrame_4);
    data->prefs.customframe_config_5 =
        cfg_find(cl, obj, MUICFG_CustomFrame_5);
    data->prefs.customframe_config_6 =
        cfg_find(cl, obj, MUICFG_CustomFrame_6);
    data->prefs.customframe_config_7 =
        cfg_find(cl, obj, MUICFG_CustomFrame_7);
    data->prefs.customframe_config_8 =
        cfg_find(cl, obj, MUICFG_CustomFrame_8);
    data->prefs.customframe_config_9 =
        cfg_find(cl, obj, MUICFG_CustomFrame_9);
    data->prefs.customframe_config_10 =
        cfg_find(cl, obj, MUICFG_CustomFrame_10);
    data->prefs.customframe_config_11 =
        cfg_find(cl, obj, MUICFG_CustomFrame_11);
    data->prefs.customframe_config_12 =
        cfg_find(cl, obj, MUICFG_CustomFrame_12);
    data->prefs.customframe_config_13 =
        cfg_find(cl, obj, MUICFG_CustomFrame_13);
    data->prefs.customframe_config_14 =
        cfg_find(cl, obj, MUICFG_CustomFrame_14);
    data->prefs.customframe_config_15 =
        cfg_find(cl, obj, MUICFG_CustomFrame_15);
    data->prefs.customframe_config_16 =
        cfg_find(cl, obj, MUICFG_CustomFrame_16);

    /*---------- Special ----------*/
    /* all taken care of in frames and images */

    if (data->init_dataspace)
    {
        data->init_dataspace = NULL;
        DoMethod(obj, MUIM_Semaphore_Release);
    }

    return (IPTR) obj;
}

static void Configdata__DisposeDescriptors(struct MUI_ConfigdataData *data)
{
    struct Node *psNode, *tmp;

    DPSD(bug("[MUI:Cfg] %s()\n", __func__);)

    ObtainSemaphore(&data->psLock);
    ForeachNodeSafe(&data->pubscreens, psNode, tmp) {
        Remove(psNode);
        MUIS_FreePubScreenDesc((struct MUI_PubScreenDesc *)psNode->ln_Name);
        FreeVec(psNode);
    }
    ReleaseSemaphore(&data->psLock);
}

/**************************************************************************
 OM_DISPOSE
**************************************************************************/
IPTR Configdata__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);

    if (data->fsNotifyPort) {
        DoMethod(data->app, MUIM_Application_RemInputHandler, (IPTR) &data->fsNotifyIHN);
        EndNotify(&data->fsNotifyRequest);
        if (data->signalPort)
            ZuneDeleteSharedPort(data->fsNotifyPort);
        else
            DeleteMsgPort(data->fsNotifyPort);
    }

    if (MUIScreenBase)
        Configdata__DisposeDescriptors(data);

    return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
 OM_GET
**************************************************************************/
IPTR Configdata__OM_GET(struct IClass *cl, Object *obj,
    struct opGet *msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    IPTR *store = msg->opg_Storage;
    ULONG tag = msg->opg_AttrID;

    switch (tag)
    {
    case MUIA_Configdata_ZunePrefs:
        *store = (IPTR) & data->prefs;
        DoMethod(obj, MUIM_Configdata_GetWindowPos, 0);
        return 1;
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

#ifndef AROS_BIG_ENDIAN
#define AROS_BIG_ENDIAN 0
#endif

#if (!AROS_BIG_ENDIAN)
static LONG windowpos_swapbytes(IPTR data, LONG size)
{
    LONG items;
    WORD cnt, i, j;
    void *p = (void *)data;

    *((LONG *) p) = AROS_SWAP_BYTES_LONG(*((LONG *) p));

    cnt = sizeof(LONG);
    items = (size - sizeof(LONG)) / sizeof(struct windowpos);
    D(bug("[MUI:Cfg] %s: size = %d items = %d\n", __func__, size, items));
    for (i = 0; i < items; i++) {
        *((LONG *) ((IPTR)p + cnt)) =
            AROS_SWAP_BYTES_LONG(*((LONG *) ((IPTR)p + cnt)));
        cnt += sizeof(LONG);
        for (j = 0; j < 8; j++) {
            *((WORD *) ((IPTR)p + cnt)) =
                AROS_SWAP_BYTES_WORD(*((WORD *) ((IPTR)p + cnt)));
            cnt += sizeof(WORD);
        }
    }
    D(bug("[MUI:Cfg] %s: actual = %d\n", __func__, cnt));
    return cnt;
}
#endif

static IPTR Configdata_GetWindowPos(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetString *msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    IPTR s;
    s = (IPTR) DoMethod(obj, MUIM_Dataspace_Find, MUICFG_WindowPos);
    if (s && data->app) {
#if (!AROS_BIG_ENDIAN)
        LONG size = AROS_BE2LONG(*((LONG *)s));
        D(bug("[MUI:Cfg] %s: %ld bytes\n", __func__, size));
        windowpos_swapbytes(s, size);
#endif
        set(data->app, MUIA_Application_CopyWinPosToApp, s);
#if (!AROS_BIG_ENDIAN)
        windowpos_swapbytes(s, size);
#endif
    }
    return s;
}

static IPTR Configdata_SetWindowPos(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetString *msg)
{
    struct MUI_ConfigdataData *data;
    //kprintf ("setwindowpos\n");
    data = INST_DATA(cl, obj);

    if (data->app)
    {
        IPTR addr = 0;
        IPTR size = 0;

        get(data->app, MUIA_Application_GetWinPosAddr, &addr);
        get(data->app, MUIA_Application_GetWinPosSize, &size);

        D(bug("[MUI:Cfg] %s: 0x%p (%d bytes)\n", __func__, addr, size));

#if (!AROS_BIG_ENDIAN)
        windowpos_swapbytes(addr, size);
#endif
        DoMethod(obj, MUIM_Dataspace_Add, addr, size, MUICFG_WindowPos);
#if (!AROS_BIG_ENDIAN)
        windowpos_swapbytes(addr, size);
#endif
    }
    return 0;
}

/**************************************************************************
 MUIM_Configdata_GetString
 Check if string is found in dataspace, then if not found, search each
 builtin array
**************************************************************************/
static CONST_STRPTR cfg_string(struct IClass *cl, Object *obj, ULONG id)
{
    CONST_STRPTR s = cfg_find(cl, obj, id);
    int i;

    if (s)
        return s;
    for (i = 0; DefStrValues[i].id; i++)
    {
        if (DefStrValues[i].id == id)
            return DefStrValues[i].val;
    }
    for (i = 0; DefImspecValues[i].defspec; i++)
    {
        if (DefImspecValues[i].cfgid == id)
            return DefImspecValues[i].defspec;
    }
    for (i = 0; DefFramespecValues[i].defspec; i++)
    {
        if (DefFramespecValues[i].cfgid == id)
            return DefFramespecValues[i].defspec;
    }
    return NULL;
}

IPTR Configdata__MUIM_GetString(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetString *msg)
{
    return (IPTR) cfg_string(cl, obj, msg->id);
}

/**************************************************************************
 MUIM_Configdata_SetImspec
 search in builtin array first, to not not have in dataspace the default
 value (would be redundant)
**************************************************************************/
IPTR Configdata__MUIM_SetImspec(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_SetImspec *msg)
{
    int i;

    if (!msg->imspec || !*msg->imspec || *msg->imspec == '6')
    {
/*          D(bug("Configdata_SetImspec(%p) : id %08lx, val invalid\n", */
/*                obj, msg->id)); */
        return 0;
    }

    for (i = 0; DefImspecValues[i].defspec; i++)
    {
        if (DefImspecValues[i].cfgid == msg->id)
            if (!strcmp(DefImspecValues[i].defspec, msg->imspec))
            {
/*                  D(bug("Configdata_SetImspec(%p) : set to def, id %08lx, val %s\n", */
/*                        obj, msg->id, msg->imspec)); */
                DoMethod(obj, MUIM_Dataspace_Remove, msg->id);
                return 0;
            }
    }

    for (i = 0; DefStrValues[i].id; i++)
    {
        if (DefStrValues[i].id == msg->id)
            if (!strcmp(DefStrValues[i].val, msg->imspec))
            {
                DoMethod(obj, MUIM_Dataspace_Remove, msg->id);
                return 0;
            }
    }

    DoMethod(obj, MUIM_Dataspace_Add, (IPTR) msg->imspec,
        strlen(msg->imspec) + 1, msg->id);
    return 0;
}

/**************************************************************************
 MUIM_Configdata_SetFramespec
**************************************************************************/
IPTR Configdata__MUIM_SetFramespec(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_SetFramespec *msg)
{
    int i;

    if (!msg->framespec || !*msg->framespec)
    {
/*          D(bug("Configdata_SetFramespec(%p) : id %08lx, val invalid\n", */
/*                obj, msg->id)); */
        return 0;
    }

    for (i = 0; DefFramespecValues[i].defspec; i++)
    {
        if (DefFramespecValues[i].cfgid == msg->id)
            if (!strcmp(DefFramespecValues[i].defspec, msg->framespec))
            {
/*                D(bug("Configdata_SetFramespec(%p): " */
/*                    "set to def, id %08lx, val %s\n", */
/*                    obj, msg->id, msg->framespec)); */
                DoMethod(obj, MUIM_Dataspace_Remove, msg->id);
                return 0;
            }
    }

    DoMethod(obj, MUIM_Dataspace_Add, (IPTR) msg->framespec,
        strlen(msg->framespec) + 1, msg->id);
    return 0;
}

/**************************************************************************
 MUIM_Configdata_SetPenspec
**************************************************************************/
IPTR Configdata__MUIM_SetPenspec(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_SetPenspec *msg)
{
    int i;

    if (!msg->penspec || !*msg->penspec)
        return 0;

    for (i = 0; DefStrValues[i].id; i++)
    {
        if (DefStrValues[i].id == msg->id)
            if (!strcmp(DefStrValues[i].val, msg->penspec))
            {
                DoMethod(obj, MUIM_Dataspace_Remove, msg->id);
                return 0;
            }
    }

    DoMethod(obj, MUIM_Dataspace_Add, (IPTR) msg->penspec,
        strlen(msg->penspec) + 1, msg->id);
    return 0;
}

/**************************************************************************
 MUIM_Configdata_SetFont
**************************************************************************/
IPTR Configdata__MUIM_SetFont(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_SetFont *msg)
{
    if (!msg->font || !*msg->font)
    {
/*          D(bug("Configdata_SetFont(%p) : id %08lx, val invalid\n", */
/*                obj, msg->id)); */
        DoMethod(obj, MUIM_Dataspace_Remove, msg->id);
        return 0;
    }

    DoMethod(obj, MUIM_Dataspace_Add, (IPTR) msg->font,
        strlen(msg->font) + 1, msg->id);
    return 0;
}

/**************************************************************************
 MUIM_Configdata_SetString
**************************************************************************/
IPTR Configdata__MUIM_SetString(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_SetString *msg)
{
    int i;

    /* If the value is the same as default value, remove it from dataspace (and config file) */
    for (i = 0; DefStrValues[i].id; i++)
    {
        if (DefStrValues[i].id == msg->id)
            if (!strcmp(DefStrValues[i].val, msg->string))
            {
                DoMethod(obj, MUIM_Dataspace_Remove, msg->id);
                Configdata_SynchronizeString(cl, obj, msg->id);
                return 0;
            }
    }

    DoMethod(obj, MUIM_Dataspace_Add, (IPTR) msg->string,
        strlen(msg->string) + 1, msg->id);
    Configdata_SynchronizeString(cl, obj, msg->id);

    return 0;
}

/**************************************************************************
 MUIM_Configdata_GetULong
**************************************************************************/
static ULONG cfg_ulong(struct IClass *cl, Object *obj, ULONG id)
{
    ULONG *vp = cfg_find(cl, obj, id);
    int i;

    if (vp)
        return AROS_BE2LONG(*vp);
    for (i = 0; DefULValues[i].id != 0; i++)
    {
        if (DefULValues[i].id == id)
            return DefULValues[i].val;
    }
    return 0;
}

IPTR Configdata__MUIM_GetULong(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetULong *msg)
{
    return cfg_ulong(cl, obj, msg->id);
}

/**************************************************************************
 MUIM_GetConfigItem
 Builtin numeric settings return values; custom settings retain dataspace
 pointers. Resolve builtin defaults even when no preferences file exists.
**************************************************************************/
IPTR Configdata__MUIM_GetConfigItem(struct IClass *cl, Object *obj,
    struct MUIP_GetConfigItem *msg)
{
    int i;
    IPTR value;

    if (!msg->storage)
        return FALSE;

    for (i = 0; DefULValues[i].id; i++)
    {
        if (DefULValues[i].id == msg->id)
        {
            struct MUIP_Configdata_GetULong getmsg =
                { MUIM_Configdata_GetULong, msg->id };

            *msg->storage = Configdata__MUIM_GetULong(cl, obj, &getmsg);
            return TRUE;
        }
    }

    {
        struct MUIP_Configdata_GetString getmsg =
            { MUIM_Configdata_GetString, msg->id };

        value = Configdata__MUIM_GetString(cl, obj, &getmsg);
    }
    if (!value)
        return FALSE;

    /* Zune keeps the string backgrounds as image specs, MUI returns pen
     * specs for them; Textinput.mcc obtains a pen from the answer. */
    if ((msg->id == MUICFG_String_Background
        || msg->id == MUICFG_String_ActiveBackground)
        && ((STRPTR)value)[0] == '2' && ((STRPTR)value)[1] == ':')
        value += 2;

    *msg->storage = value;
    return TRUE;
}

/**************************************************************************
 MUIM_Configdata_SetULong
**************************************************************************/
IPTR Configdata__MUIM_SetULong(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_SetULong *msg)
{
    ULONG v = msg->val;
    ULONG *vp = &v;
    int i;

    for (i = 0; DefULValues[i].id != 0; i++)
    {
        if (DefULValues[i].id == msg->id)
            if (DefULValues[i].val == v)
            {
/*                D(bug("Configdata_SetULong(%p):" */
/*                    " set to def, id %08lx, val %ld\n", */
/*                    obj, msg->id, v)); */
                DoMethod(obj, MUIM_Dataspace_Remove, msg->id);
                return 0;
            }
    }

    v = AROS_LONG2BE(v);
/*      D(bug("Configdata_SetULong(%p): adding %08lx to %08lx chunk\n", */
/*          obj, v, msg->id)); */
    DoMethod(obj, MUIM_Dataspace_Add, (IPTR) vp, 4, msg->id);
    return 0;
}


/**************************************************************************
 SavePrefsHeader: Write a PRHD chunk
**************************************************************************/
static int SavePrefsHeader(struct IFFHandle *iff)
{
    if (!PushChunk(iff, 0, MAKE_ID('P', 'R', 'H', 'D'), IFFSIZE_UNKNOWN))
    {
        struct PrefHeader ph;
        ph.ph_Version = 0;
        ph.ph_Type = 0;
        ph.ph_Flags = 0;

        if (WriteChunkBytes(iff, &ph, sizeof(struct PrefHeader)))
            if (!PopChunk(iff))
                return 1;
        PopChunk(iff);
    }
    return 0;
}

/**************************************************************************
 MUIM_Configdata_Save
**************************************************************************/
IPTR Configdata__MUIM_Save(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_Save *msg)
{
    struct IFFHandle *iff;
    if ((iff = AllocIFF()))
    {
        if (!(iff->iff_Stream = (IPTR) Open(msg->filename, MODE_NEWFILE)))
        {
            /* Try to Create the directory where the file is located */
            char *path = StrDup(msg->filename);
            if (path)
            {
                char *path_end = PathPart(path);
                if (path_end != path)
                {
                    BPTR lock;
                    *path_end = 0;
                    if ((lock = CreateDir(path)))
                    {
                        UnLock(lock);
                        iff->iff_Stream =
                            (IPTR) Open(msg->filename, MODE_NEWFILE);
                    }
                }
                FreeVec(path);
            }
        }

        if (iff->iff_Stream)
        {
            InitIFFasDOS(iff);

            if (!OpenIFF(iff, IFFF_WRITE))
            {
                if (!PushChunk(iff, MAKE_ID('P', 'R', 'E', 'F'), ID_FORM,
                        IFFSIZE_UNKNOWN))
                {
                    Configdata_SetWindowPos(cl, obj, (APTR) msg);
                    if (SavePrefsHeader(iff))
                    {
                        DoMethod(obj, MUIM_Dataspace_WriteIFF, (IPTR) iff,
                            0, MAKE_ID('M', 'U', 'I', 'C'));
                    }
                    PopChunk(iff);
                }

                CloseIFF(iff);
            }
            Close((BPTR) iff->iff_Stream);
        }
        FreeIFF(iff);
    }
    return 0;
}


/**************************************************************************
 MUIM_Configdata_Load
 Get the content of the file into the object.
**************************************************************************/
IPTR Configdata__MUIM_Load(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_Load *msg)
{
    struct IFFHandle *iff;
    IPTR res = TRUE;

    if ((iff = AllocIFF()))
    {
        D(bug("loading prefs from %s\n", msg->filename));
        if ((iff->iff_Stream = (IPTR) Open(msg->filename, MODE_OLDFILE)))
        {
            InitIFFasDOS(iff);

            if (!OpenIFF(iff, IFFF_READ))
            {
                StopChunk(iff, MAKE_ID('P', 'R', 'E', 'F'), MAKE_ID('M',
                        'U', 'I', 'C'));

                DoMethod(obj, MUIM_Dataspace_Clear);

                while (!ParseIFF(iff, IFFPARSE_SCAN))
                {
                    struct ContextNode *cn;
                    if (!(cn = CurrentChunk(iff)))
                        continue;
                    if (cn->cn_ID == MAKE_ID('M', 'U', 'I', 'C'))
                        DoMethod(obj, MUIM_Dataspace_ReadIFF, (IPTR) iff);
                }

                CloseIFF(iff);
            }
            else
            {
                res = FALSE;
            }
            Close((BPTR) iff->iff_Stream);
        }
        else
        {
            res = FALSE;
        }
        FreeIFF(iff);
    }
    else
    {
        res = FALSE;
    }

    if (res) {
        DoMethod(obj, MUIM_Configdata_LoadPubScreens);
    }

    return res;
}

static IPTR Configdata_GetPubScrnDesc(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetPubScrnDesc *msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    struct MUI_PubScreenDesc *desc = NULL;

    DPSD(bug("[MUI:Cfg] %s()\n", __func__);)

    if (MUIScreenBase) {
        struct Node *psNode;
        ObtainSemaphoreShared(&data->psLock);
        ForeachNode(&data->pubscreens, psNode) {
            if (!strcmp(((struct MUI_PubScreenDesc *)(psNode->ln_Name))->Name, msg->name)) {
                desc = (struct MUI_PubScreenDesc *)psNode->ln_Name;
                break;
            }
        }
        ReleaseSemaphore(&data->psLock);
    }
    return (IPTR)desc;
}

static IPTR Configdata_LoadPubScreens(struct IClass *cl, Object *obj,
    Msg msg)
{
    DPSD(bug("[MUI:Cfg] %s()\n", __func__);)

    if (MUIScreenBase) {
        struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
        struct Node *psNode;
        APTR pfh;

        Configdata__DisposeDescriptors(data);

        DPSD(bug("[MUI:Cfg] %s: loading public screen details from '%s'\n", __func__, PUBSCREEN_FILE);)

        ObtainSemaphore(&data->psLock);
        if ((pfh = MUIS_OpenPubFile(PUBSCREEN_FILE, MODE_OLDFILE))) {
            struct MUI_PubScreenDesc *desc;
            while ((desc = MUIS_ReadPubFile(pfh))) {
                DPSD(bug("[MUI:Cfg] %s: descriptor @ 0x%p\n", __func__, desc);)
                psNode = AllocVec(sizeof(struct Node), MEMF_CLEAR);
                psNode->ln_Name = (char *)desc;
                AddTail(&data->pubscreens, psNode);
            }
            MUIS_ClosePubFile(pfh);
        }
        ReleaseSemaphore(&data->psLock);
    }
    return (IPTR)TRUE;
}

/* The notification port may share its signal with other ports: reload
 * only when a notification arrived, and reply to every message. */
static IPTR Configdata_PubScreensChanged(struct IClass *cl, Object *obj,
    Msg msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    struct Message *nmsg;
    BOOL changed = FALSE;

    while ((nmsg = GetMsg(data->fsNotifyPort)))
    {
        ReplyMsg(nmsg);
        changed = TRUE;
    }
    return changed ? Configdata_LoadPubScreens(cl, obj, msg) : TRUE;
}

/*
 * The class dispatcher
 */
BOOPSI_DISPATCHER(IPTR, Configdata_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Configdata__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return Configdata__OM_DISPOSE(cl, obj, (APTR) msg);
    case OM_GET:
        return Configdata__OM_GET(cl, obj, (APTR) msg);
    case MUIM_GetConfigItem:
        return Configdata__MUIM_GetConfigItem(cl, obj, (APTR) msg);
    case MUIM_Configdata_GetString:
        return Configdata__MUIM_GetString(cl, obj, (APTR) msg);
    case MUIM_Configdata_GetULong:
        return Configdata__MUIM_GetULong(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetULong:
        return Configdata__MUIM_SetULong(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetImspec:
        return Configdata__MUIM_SetImspec(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetFramespec:
        return Configdata__MUIM_SetFramespec(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetPenspec:
        return Configdata__MUIM_SetPenspec(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetFont:
        return Configdata__MUIM_SetFont(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetString:
        return Configdata__MUIM_SetString(cl, obj, (APTR) msg);
    case MUIM_Configdata_Save:
        return Configdata__MUIM_Save(cl, obj, (APTR) msg);
    case MUIM_Configdata_Load:
        return Configdata__MUIM_Load(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetWindowPos:
        return Configdata_SetWindowPos(cl, obj, (APTR) msg);
    case MUIM_Configdata_GetWindowPos:
        return Configdata_GetWindowPos(cl, obj, (APTR) msg);
    case MUIM_Configdata_GetPubScrnDesc:
        return Configdata_GetPubScrnDesc(cl, obj, (APTR) msg);
    case MUIM_Configdata_LoadPubScreens:
        return Configdata_LoadPubScreens(cl, obj, (APTR) msg);
    case MUIM_Configdata_PubScreensChanged:
        return Configdata_PubScreensChanged(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetupNotify:
        return Configdata__MUIM_SetupNotify(cl, obj, (APTR) msg);
    }

    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_Configdata_desc =
{
    MUIC_Configdata,            /* Class name */
    MUIC_Dataspace,             /* super class name */
    sizeof(struct MUI_ConfigdataData),  /* size of class own datas */
    (void *) Configdata_Dispatcher       /* class dispatcher */
};

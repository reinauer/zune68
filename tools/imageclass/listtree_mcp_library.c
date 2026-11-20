/* Listtree.mcp: the settings page of Listtree.mcc in Zune prefs, with the
 * items, ids and defaults of MUI 3.8's Listtree.mcp (listtree_config.h).
 * MUI 3.8's page also has a sample tree; Listtree.mcc reads its settings
 * in Setup, so a sample here would not follow the gadgets and is left out. */
#include <libraries/mui.h>
#include <mcc_common.h>
#include <SDI_hook.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <string.h>

#define DEBUG 0
#define D(...) ((void)0)
#define E(...) ((void)0)
#include "shellstart.c"

#include "listtree_config.h"

struct ListtreeP_DATA
{
    Object *gadget[8];
};

#define CLASS "Listtree.mcp"
#define SUPERCLASSP MUIC_Mccprefs
#define INSTDATAP ListtreeP_DATA
#define VERSION 20
#define REVISION 1
#define USERLIBID CLASS " 20.1 (20.11.2026) Zune68"
#define MASTERVERSION 19
#define MIN_STACKSIZE 8192
#define SetupDebug() ((void)0)
#define CleanupDebug() ((void)0)
#include "mccinit.c"

/* private: the style changed, enable the pens it uses */
#define MUIM_ListtreeP_Style (TAG_USER | 0x4c540001)

enum { G_Right, G_Down, G_Lines, G_Shadow, G_Style, G_Space, G_Remember, G_Opened };

/* one row per gadget, in the order of enum above */
static const struct
{
    ULONG id;           /* the item in the settings */
    ULONG tag;          /* the gadget's attribute */
    IPTR def;           /* the default: a number or a spec string */
    BOOL string;
} Items[] =
{
    { LTCFG_Right, MUIA_Imagedisplay_Spec, (IPTR)LTCFG_Right_Default, TRUE },
    { LTCFG_Down, MUIA_Imagedisplay_Spec, (IPTR)LTCFG_Down_Default, TRUE },
    { LTCFG_Lines, MUIA_Pendisplay_Spec, (IPTR)LTCFG_Lines_Default, TRUE },
    { LTCFG_Shadow, MUIA_Pendisplay_Spec, (IPTR)LTCFG_Shadow_Default, TRUE },
    { LTCFG_Style, MUIA_Cycle_Active, LTCFG_Style_Default, FALSE },
    { LTCFG_Space, MUIA_Numeric_Value, LTCFG_Space_Default, FALSE },
    { LTCFG_Remember, MUIA_Selected, LTCFG_Remember_Default, FALSE },
    { LTCFG_Opened, MUIA_Selected, LTCFG_Opened_Default, FALSE },
};

#define ITEMS (sizeof(Items) / sizeof(Items[0]))

static const char *const Styles[] =
{
    "Normal", "Inserted", "Lines", "Win95", "Mac", "Lines3D", NULL
};

static Object *TopLabel(const char *text, char key)
{
    return MUI_MakeObject(MUIO_Label, (IPTR)text, MUIO_Label_Centered | key);
}

static Object *MakePopimage(const char *title, const char *help, char key)
{
    return PopimageObject,
        MUIA_Imageadjust_Type, MUIV_Imageadjust_Type_Image,
        MUIA_Imagedisplay_UseDefSize, TRUE,
        MUIA_Window_Title, (IPTR)title,
        MUIA_ShortHelp, (IPTR)help,
        MUIA_ControlChar, key,
        MUIA_CycleChain, TRUE,
        End;
}

static Object *MakePoppen(const char *title, const char *help, char key)
{
    return PoppenObject,
        MUIA_Window_Title, (IPTR)title,
        MUIA_ShortHelp, (IPTR)help,
        MUIA_ControlChar, key,
        MUIA_CycleChain, TRUE,
        End;
}

static Object *Check(const char *help, char key)
{
    return ImageObject, ImageButtonFrame,
        MUIA_InputMode, MUIV_InputMode_Toggle,
        MUIA_Image_Spec, MUII_CheckMark,
        MUIA_Image_FreeVert, TRUE,
        MUIA_Background, MUII_ButtonBack,
        MUIA_ShowSelState, FALSE,
        MUIA_ShortHelp, (IPTR)help,
        MUIA_ControlChar, key,
        MUIA_CycleChain, TRUE,
        End;
}

static Object *BuildPage(Object **g)
{
    return VGroup,
        Child, ColGroup(2),
            Child, TopLabel("Right:", 'r'),
            Child, TopLabel("Down:", 'd'),
            Child, g[G_Right] = MakePopimage("Adjust Image 'Right'",
                "Image for a closed node.", 'r'),
            Child, g[G_Down] = MakePopimage("Adjust Image 'Down'",
                "Image for an opened node.", 'd'),
            Child, TopLabel("Lines:", 'l'),
            Child, TopLabel("Shadow:", 'w'),
            Child, g[G_Lines] = MakePoppen("Adjust Pen 'Lines'",
                "Custom line color for styles\n'Lines', 'Win95' and 'Lines3D'.", 'l'),
            Child, g[G_Shadow] = MakePoppen("Adjust Pen 'Shadow'",
                "Custom shadow color for\nstyle 'Lines3D'.", 'w'),
        End,
        Child, ColGroup(2),
            Child, KeyLabel1("Style:", 'y'),
            Child, g[G_Style] = CycleObject,
                MUIA_Cycle_Entries, (IPTR)Styles,
                MUIA_Font, MUIV_Font_Button,
                MUIA_ShortHelp, (IPTR)"Choose between different\ndisplay modes.",
                MUIA_ControlChar, 'y',
                MUIA_CycleChain, TRUE,
                End,
            Child, KeyLabel2("Space:", 'a'),
            Child, g[G_Space] = SliderObject,
                MUIA_Numeric_Min, 0,
                MUIA_Numeric_Max, LTCFG_Space_Max,
                MUIA_ShortHelp, (IPTR)"Spacing between\nnode image and text.",
                MUIA_ControlChar, 'a',
                MUIA_CycleChain, TRUE,
                End,
        End,
        Child, ColGroup(2),
            Child, KeyLabel1("Remember status:", 's'),
            Child, g[G_Remember] = Check("Closed nodes remember\ntheir childrens states.", 's'),
            Child, KeyLabel1("Display opened:", 'i'),
            Child, g[G_Opened] = Check("Adjust listview position\nafter opening a node.", 'i'),
        End,
        Child, VSpace(0),
    End;
}

DISPATCHER(_DispatcherP)
{
    struct ListtreeP_DATA *data = INST_DATA(cl, obj);
    unsigned int i;

    switch (msg->MethodID)
    {
    case OM_NEW:
    {
        Object *g[ITEMS], *page;

        if (!(obj = (Object *)DoSuperMethodA(cl, obj, msg)))
            return 0;
        data = INST_DATA(cl, obj);
        if (!(page = BuildPage(g)))
        {
            CoerceMethod(cl, obj, OM_DISPOSE);
            return 0;
        }
        memcpy(data->gadget, g, sizeof(data->gadget));
        DoMethod(obj, OM_ADDMEMBER, (IPTR)page);
        DoMethod(g[G_Style], MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
            (IPTR)obj, 1, MUIM_ListtreeP_Style);
        DoMethod(obj, MUIM_ListtreeP_Style);
        return (IPTR)obj;
    }

    case MUIM_ListtreeP_Style:
    {
        IPTR style = 0;

        get(data->gadget[G_Style], MUIA_Cycle_Active, &style);
        set(data->gadget[G_Lines], MUIA_Disabled, style != LTSTYLE_Lines
            && style != LTSTYLE_Win95 && style != LTSTYLE_Lines3D);
        set(data->gadget[G_Shadow], MUIA_Disabled, style != LTSTYLE_Lines3D);
        return 0;
    }

    case MUIM_Settingsgroup_ConfigToGadgets:
    {
        Object *configdata = ((struct MUIP_Settingsgroup_ConfigToGadgets *)msg)->configdata;

        for (i = 0; i < ITEMS; i++)
        {
            IPTR p = DoMethod(configdata, MUIM_Dataspace_Find, Items[i].id);

            if (Items[i].string)
                nnset(data->gadget[i], Items[i].tag, p ? p : Items[i].def);
            else
                nnset(data->gadget[i], Items[i].tag, p ? *(LONG *)p : (LONG)Items[i].def);
        }
        DoMethod(obj, MUIM_ListtreeP_Style);
        return 0;
    }

    case MUIM_Settingsgroup_GadgetsToConfig:
    {
        Object *configdata = ((struct MUIP_Settingsgroup_GadgetsToConfig *)msg)->configdata;

        /* only what differs from MUI 3.8's defaults is stored */
        for (i = 0; i < ITEMS; i++)
        {
            IPTR v = 0;

            get(data->gadget[i], Items[i].tag, &v);
            if (Items[i].string ? v && strcmp((char *)v, (char *)Items[i].def)
                : (LONG)v != (LONG)Items[i].def)
            {
                LONG n = v;

                if (Items[i].string)
                    DoMethod(configdata, MUIM_Dataspace_Add, v, strlen((char *)v) + 1,
                        Items[i].id);
                else
                    DoMethod(configdata, MUIM_Dataspace_Add, (IPTR)&n, sizeof(n),
                        Items[i].id);
            }
            else
                DoMethod(configdata, MUIM_Dataspace_Remove, Items[i].id);
        }
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

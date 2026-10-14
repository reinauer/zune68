/* Font selection using diskfont's installed font inventory. */
#define MUIMASTER_YES_INLINE_STDARG
#include <string.h>
#include <stdio.h>
#include <libraries/asl.h>
#include <diskfont/diskfont.h>
#include <diskfont/diskfonttag.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "selectorpanel.h"
extern struct Library *MUIMasterBase, *DiskfontBase;
struct FontpanelData
{
    Object *list, *name, *size, *preview, *sample;
    Object *bold, *italic, *underline, *front, *back, *mode;
    struct AvailFontsHeader *fonts;
    struct MUI_CustomClass *sample_class;
    struct Hook display;
    struct Hook *filter;
    struct FontRequester request;
    STRPTR sampletext;
    char fontspec[256], number[16];
    LONG collection, minimum, maximum;
    BOOL fixed;
};
static const UBYTE font_modes[] = {JAM1, JAM2, COMPLEMENT, INVERSVID};
static ULONG font_mode_index(ULONG mode)
{
    for (ULONG i = 0; i < sizeof(font_modes); i++)
        if (font_modes[i] == mode) return i;
    return 0;
}
static ULONG font_style(struct FontpanelData *d)
{
    return (XGET(d->bold, MUIA_Selected) ? FSF_BOLD : 0) |
        (XGET(d->italic, MUIA_Selected) ? FSF_ITALIC : 0) |
        (XGET(d->underline, MUIA_Selected) ? FSF_UNDERLINED : 0);
}
BOOPSI_DISPATCHER(IPTR, FontSample_Dispatcher, cl, obj, msg)
{
    IPTR result = DoSuperMethodA(cl, obj, msg);
    if (msg->MethodID == MUIM_AskMinMax)
    {
        struct MUI_MinMax *mm = ((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        mm->MinWidth += 1;
        mm->DefWidth += 240;
        mm->MaxWidth = MUI_MAXMAX;
        mm->MinHeight += _font(obj)->tf_YSize + 4;
        mm->DefHeight = mm->MaxHeight = mm->MinHeight;
    }
    else if (msg->MethodID == MUIM_Draw &&
        (((struct MUIP_Draw *)msg)->flags & MADF_DRAWOBJECT))
    {
        struct FontpanelData *d = (APTR)XGET(obj, MUIA_UserData);
        struct RastPort rp = *_rp(obj);
        APTR clip = MUI_AddClipping(muiRenderInfo(obj),
            _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj));
        SetFont(&rp, _font(obj));
        SetSoftStyle(&rp, font_style(d), AskSoftStyle(&rp));
        SetAPen(&rp, XGET(d->front, MUIA_Numeric_Value));
        SetBPen(&rp, XGET(d->back, MUIA_Numeric_Value));
        SetDrMd(&rp, font_modes[XGET(d->mode, MUIA_Cycle_Active)]);
        Move(&rp, _mleft(obj), _mtop(obj) + _font(obj)->tf_Baseline + 2);
        Text(&rp, d->sampletext, strlen(d->sampletext));
        if (clip != (APTR)-1) MUI_RemoveClipping(muiRenderInfo(obj), clip);
    }
    return result;
}
BOOPSI_DISPATCHER_END
static Object *font_button(CONST_STRPTR label, BOOL toggle)
{
    return MUI_NewObject(MUIC_Text, MUIA_Frame, MUIV_Frame_Button,
        MUIA_Background, MUII_ButtonBack, MUIA_InputMode,
        toggle ? MUIV_InputMode_Toggle : MUIV_InputMode_RelVerify,
        MUIA_Text_Contents, label, MUIA_Text_PreParse, "\33c",
        MUIA_CycleChain, TRUE, TAG_DONE);
}
static BOOL font_add(Object *parent, Object *child)
{
    if (!child) return FALSE;
    DoMethod(parent, OM_ADDMEMBER, child);
    return TRUE;
}
static void font_action(Object *source, ULONG attr, IPTR value, Object *panel, LONG act)
{
    DoMethod(source, MUIM_Notify, attr, value, panel, 2, MUIM_Zune_PanelAction, act);
}
AROS_UFH3S(IPTR, font_display,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(STRPTR *, strings, A2),
    AROS_UFHA(struct AvailFonts *, font, A1))
{
    AROS_USERFUNC_INIT
    struct FontpanelData *d = hook->h_Data;
    if (font)
    {
        strings[0] = font->af_Attr.ta_Name;
        snprintf(d->number, sizeof(d->number), "%u", font->af_Attr.ta_YSize);
        strings[1] = d->number;
    }
    else { strings[0] = "Font"; strings[1] = "Size"; }
    return 0;
    AROS_USERFUNC_EXIT
}
static void font_name(struct FontpanelData *d, CONST_STRPTR name)
{
    char buffer[256];
    if (!name) name = "topaz";
    strncpy(buffer, name, sizeof(buffer) - 1); buffer[sizeof(buffer) - 1] = 0;
    LONG len = strlen(buffer);
    if (len > 5 && !Stricmp(buffer + len - 5, ".font")) buffer[len - 5] = 0;
    set(d->name, MUIA_String_Contents, buffer);
}
static LONG font_size(struct FontpanelData *d)
{
    LONG size = XGET(d->size, MUIA_String_Integer);
    return MAX(d->minimum, MIN(d->maximum, size));
}
static void font_preview(struct FontpanelData *d)
{
    Object *sample;
    snprintf(d->fontspec, sizeof(d->fontspec), "%s/%ld",
        (STRPTR)XGET(d->name, MUIA_String_Contents), font_size(d));
    struct TagItem tags[] = {
        {MUIA_UserData, (IPTR)d}, {MUIA_Font, (IPTR)d->fontspec},
        {TAG_DONE, 0}
    };
    sample = NewObjectA(d->sample_class->mcc_Class, NULL, tags);
    if (!sample) return;
    DoMethod(d->preview, MUIM_Group_InitChange);
    if (d->sample)
    {
        DoMethod(d->preview, OM_REMMEMBER, d->sample);
        MUI_DisposeObject(d->sample);
    }
    d->sample = sample;
    DoMethod(d->preview, OM_ADDMEMBER, sample);
    DoMethod(d->preview, MUIM_Group_ExitChange);
}
/* AFF_SCALED includes Compugraphic fonts. Inspect the outline engine
 * instead of presenting every scalable font as a TrueType family. */
static BOOL font_truetype(CONST_STRPTR name)
{
    char path[512];
    LONG length;
    BOOL result = FALSE;
    if (!name || strlen(name) > sizeof(path) - 12) return FALSE;
    snprintf(path, sizeof(path), "%s%s", strchr(name, ':') ? "" : "FONTS:", name);
    length = strlen(path);
    if (length < 5 || Stricmp(path + length - 5, ".font")) return FALSE;
    strcpy(path + length - 5, ".otag");
    BPTR file = Open(path, MODE_OLDFILE);
    if (!file) return FALSE;
    if (Seek(file, 0, OFFSET_END) < 0) { Close(file); return FALSE; }
    length = Seek(file, 0, OFFSET_BEGINNING);
    if (length >= 16 && length <= 65536)
    {
        struct TagItem *tags = AllocVec(length, MEMF_ANY);
        if (tags && Read(file, tags, length) == length &&
            tags[0].ti_Tag == OT_FileIdent && tags[0].ti_Data == length)
        {
            for (LONG i = 1; i < length / sizeof(*tags) && tags[i].ti_Tag; i++)
                if (tags[i].ti_Tag == OT_Engine && tags[i].ti_Data < length)
                {
                    STRPTR engine = (STRPTR)tags + tags[i].ti_Data;
                    if (memchr(engine, 0, length - tags[i].ti_Data))
                        result = !Stricmp(engine, "freetype2") ||
                            !Stricmp(engine, "ttengine");
                    break;
                }
        }
        FreeVec(tags);
    }
    Close(file);
    return result;
}
static void font_inventory(struct FontpanelData *d)
{
    LONG size = 4096, extra;
    for (;;)
    {
        d->fonts = AllocVec(size, MEMF_ANY);
        if (!d->fonts) return;
        extra = AvailFonts((STRPTR)d->fonts, size, AFF_MEMORY | AFF_DISK);
        if (!extra) break;
        FreeVec(d->fonts); d->fonts = NULL;
        if (extra < 0 || size > 1024 * 1024 - extra) return;
        size += extra;
    }
    struct AvailFonts *font = (APTR)(d->fonts + 1);
    for (LONG i = 0; i < d->fonts->afh_NumEntries; i++, font++)
    {
        BOOL fixed = !(font->af_Attr.ta_Flags & FPF_PROPORTIONAL);
        BOOL scalable = !!(font->af_Type & AFF_SCALED);
        if (font->af_Attr.ta_YSize < d->minimum || font->af_Attr.ta_YSize > d->maximum) continue;
        if (d->fixed && !fixed) continue;
        if (d->collection != MUIV_Fontpanel_ShowCollection_All &&
            !((fixed && (d->collection & MUIV_Fontpanel_ShowCollection_FixedWidth)) ||
              (!scalable && (d->collection & MUIV_Fontpanel_ShowCollection_Bitmap)) ||
              ((d->collection & MUIV_Fontpanel_ShowCollection_TrueType) &&
               font_truetype(font->af_Attr.ta_Name)))) continue;
        d->request.fo_Attr.ta_Name = (STRPTR)XGET(d->name, MUIA_String_Contents);
        d->request.fo_Attr.ta_YSize = font_size(d);
        d->request.fo_Attr.ta_Style = font_style(d);
        if (d->filter && !CallHookPkt(d->filter, &d->request, &font->af_Attr)) continue;
        DoMethod(d->list, MUIM_List_InsertSingle, font, MUIV_List_Insert_Bottom);
    }
}
BOOPSI_DISPATCHER(IPTR, Fontpanel_Dispatcher, cl, obj, msg)
{
    struct FontpanelData *d;
    if (msg->MethodID == OM_NEW)
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList;
        Object *row, *view, *b;
        obj = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!obj) return 0;
        d = INST_DATA(cl, obj);
        d->collection = GetTagData(MUIA_Fontpanel_ShowCollection, -1, tags);
        d->minimum = MAX(1, GetTagData(ASLFO_MinHeight, 1, tags));
        d->maximum = MAX(d->minimum, MIN(32767, GetTagData(ASLFO_MaxHeight, 256, tags)));
        ULONG options = GetTagData(ASLFO_Flags, 0, tags);
        d->fixed = !!GetTagData(ASLFO_FixedWidthOnly, options & FOF_FIXEDWIDTHONLY, tags);
        d->filter = (APTR)GetTagData(ASLFO_FilterFunc, 0, tags);
        d->request.fo_UserData = (APTR)GetTagData(ASLFO_UserData, 0, tags);
        d->request.fo_Attr.ta_Flags = GetTagData(ASLFO_InitialFlags, 0, tags);
        d->sampletext = StrDup((STRPTR)GetTagData(ASLFO_SampleText, (IPTR)"The quick brown fox jumps over the lazy dog.", tags));
        if (!d->sampletext) goto fail;
        d->name = MUI_NewObject(MUIC_String, MUIA_Frame, MUIV_Frame_String,
            MUIA_CycleChain, TRUE, MUIA_String_MaxLen, 256, TAG_DONE);
        if (!font_add(obj, d->name)) goto fail;
        font_name(d, (STRPTR)GetTagData(ASLFO_InitialName, (IPTR)"topaz", tags));
        d->size = MUI_NewObject(MUIC_String, MUIA_Frame, MUIV_Frame_String,
            MUIA_CycleChain, TRUE, MUIA_String_Accept, "0123456789",
            MUIA_String_Integer, GetTagData(ASLFO_InitialSize, 8, tags), TAG_DONE);
        if (!font_add(obj, d->size)) goto fail;
        d->list = MUI_NewObject(MUIC_List, MUIA_List_Format, ",",
            MUIA_List_Title, TRUE, TAG_DONE);
        if (!d->list) goto fail;
        view = MUI_NewObject(MUIC_Listview, MUIA_Listview_List, d->list,
            MUIA_CycleChain, TRUE, TAG_DONE);
        if (!view) { MUI_DisposeObject(d->list); d->list = NULL; goto fail; }
        font_add(obj, view);
        d->display.h_Entry = (HOOKFUNC)font_display; d->display.h_Data = d;
        set(d->list, MUIA_List_DisplayHook, &d->display);
        row = MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
            MUIA_ShowMe, GetTagData(ASLFO_DoStyle, options & FOF_DOSTYLE, tags), TAG_DONE);
        if (!font_add(obj, row)) goto fail;
        d->bold = font_button("Bold", TRUE);
        if (!font_add(row, d->bold)) goto fail;
        d->italic = font_button("Italic", TRUE);
        if (!font_add(row, d->italic)) goto fail;
        d->underline = font_button("Underline", TRUE);
        if (!font_add(row, d->underline)) goto fail;
        ULONG style = GetTagData(ASLFO_InitialStyle, 0, tags);
        set(d->bold, MUIA_Selected, !!(style & FSF_BOLD));
        set(d->italic, MUIA_Selected, !!(style & FSF_ITALIC));
        set(d->underline, MUIA_Selected, !!(style & FSF_UNDERLINED));
        d->front = MUI_NewObject(MUIC_Slider, MUIA_Numeric_Min, 0,
            MUIA_Numeric_Max, MAX(1, GetTagData(ASLFO_MaxFrontPen, 8, tags)) - 1,
            MUIA_Numeric_Value, GetTagData(ASLFO_InitialFrontPen, 1, tags),
            MUIA_ShowMe, GetTagData(ASLFO_DoFrontPen, options & FOF_DOFRONTPEN, tags), TAG_DONE);
        if (!font_add(obj, d->front)) goto fail;
        d->back = MUI_NewObject(MUIC_Slider, MUIA_Numeric_Min, 0,
            MUIA_Numeric_Max, MAX(1, GetTagData(ASLFO_MaxBackPen, 8, tags)) - 1,
            MUIA_Numeric_Value, GetTagData(ASLFO_InitialBackPen, 0, tags),
            MUIA_ShowMe, GetTagData(ASLFO_DoBackPen, options & FOF_DOBACKPEN, tags), TAG_DONE);
        if (!font_add(obj, d->back)) goto fail;
        static const char *modes[] = {"JAM1", "JAM2", "COMPLEMENT", "INVERSVID", NULL};
        d->mode = MUI_NewObject(MUIC_Cycle, MUIA_Cycle_Entries, modes,
            MUIA_Cycle_Active, font_mode_index(GetTagData(ASLFO_InitialDrawMode, JAM1, tags)),
            MUIA_ShowMe, GetTagData(ASLFO_DoDrawMode, options & FOF_DODRAWMODE, tags), TAG_DONE);
        if (!font_add(obj, d->mode)) goto fail;
        d->preview = MUI_NewObject(MUIC_Group, TAG_DONE);
        if (!font_add(obj, d->preview)) goto fail;
        row = MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
        if (!font_add(obj, row)) goto fail;
        b = font_button((STRPTR)GetTagData(ASLFO_PositiveText, (IPTR)"OK", tags), FALSE);
        if (!font_add(row, b)) goto fail;
        font_action(b, MUIA_Pressed, FALSE, obj, 2);
        b = font_button((STRPTR)GetTagData(ASLFO_NegativeText, (IPTR)"Cancel", tags), FALSE);
        if (!font_add(row, b)) goto fail;
        font_action(b, MUIA_Pressed, FALSE, obj, 3);
        font_action(d->list, MUIA_List_Active, MUIV_EveryTime, obj, 1);
        font_action(d->name, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 0);
        font_action(d->size, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 0);
        font_action(d->bold, MUIA_Selected, MUIV_EveryTime, obj, 0);
        font_action(d->italic, MUIA_Selected, MUIV_EveryTime, obj, 0);
        font_action(d->underline, MUIA_Selected, MUIV_EveryTime, obj, 0);
        font_action(d->front, MUIA_Numeric_Value, MUIV_EveryTime, obj, 0);
        font_action(d->back, MUIA_Numeric_Value, MUIV_EveryTime, obj, 0);
        font_action(d->mode, MUIA_Cycle_Active, MUIV_EveryTime, obj, 0);
        d->sample_class = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 0,
            (APTR)FontSample_Dispatcher);
        if (!d->sample_class) goto fail;
        font_inventory(d); font_preview(d);
        return (IPTR)obj;
fail:
        CoerceMethod(cl, obj, OM_DISPOSE); return 0;
    }
    d = INST_DATA(cl, obj);
    switch (msg->MethodID)
    {
    case OM_DISPOSE:
        /* Lists borrow entries from this buffer and must go first. */
    {
        APTR fonts = d->fonts;
        STRPTR text = d->sampletext;
        struct MUI_CustomClass *sample_class = d->sample_class;
        IPTR result = DoSuperMethodA(cl, obj, msg);
        if (sample_class) MUI_DeleteCustomClass(sample_class);
        FreeVec(fonts); FreeVec(text); return result;
    }
    case OM_SET:
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList, *tag;
        BOOL changed = FALSE;
        while ((tag = NextTagItem(&tags)))
        {
            switch (tag->ti_Tag)
            {
            case ASLFO_InitialName: font_name(d, (STRPTR)tag->ti_Data); break;
            case ASLFO_InitialSize: set(d->size, MUIA_String_Integer, tag->ti_Data); break;
            case ASLFO_InitialStyle:
                nnset(d->bold, MUIA_Selected, !!(tag->ti_Data & FSF_BOLD));
                nnset(d->italic, MUIA_Selected, !!(tag->ti_Data & FSF_ITALIC));
                nnset(d->underline, MUIA_Selected, !!(tag->ti_Data & FSF_UNDERLINED));
                break;
            case ASLFO_InitialFlags: d->request.fo_Attr.ta_Flags = tag->ti_Data; break;
            case ASLFO_InitialFrontPen: nnset(d->front, MUIA_Numeric_Value, tag->ti_Data); break;
            case ASLFO_InitialBackPen: nnset(d->back, MUIA_Numeric_Value, tag->ti_Data); break;
            case ASLFO_InitialDrawMode: nnset(d->mode, MUIA_Cycle_Active, font_mode_index(tag->ti_Data)); break;
            default: continue;
            }
            changed = TRUE;
        }
        if (changed && d->sample_class) font_preview(d);
        break;
    }
    case OM_GET:
    {
        struct opGet *getmsg = (APTR)msg;
        IPTR value;
        switch (getmsg->opg_AttrID)
        {
        case ASLFO_InitialName: value = XGET(d->name, MUIA_String_Contents); break;
        case ASLFO_InitialSize: value = font_size(d); break;
        case ASLFO_InitialStyle:
            value = (XGET(d->bold, MUIA_Selected) ? FSF_BOLD : 0) |
                (XGET(d->italic, MUIA_Selected) ? FSF_ITALIC : 0) |
                (XGET(d->underline, MUIA_Selected) ? FSF_UNDERLINED : 0); break;
        case ASLFO_InitialFlags: value = d->request.fo_Attr.ta_Flags; break;
        case ASLFO_InitialFrontPen: value = XGET(d->front, MUIA_Numeric_Value); break;
        case ASLFO_InitialBackPen: value = XGET(d->back, MUIA_Numeric_Value); break;
        case ASLFO_InitialDrawMode: value = font_modes[XGET(d->mode, MUIA_Cycle_Active)]; break;
        default: return DoSuperMethodA(cl, obj, msg);
        }
        *getmsg->opg_Storage = value; return TRUE;
    }
    case MUIM_Zune_PanelAction:
    {
        LONG act = ((IPTR *)msg)[1];
        if (act == 2 || act == 3) return DoMethod(obj, MUIM_Zune_PanelFinish, act == 2);
        if (act == 1)
        {
            struct AvailFonts *font = NULL;
            DoMethod(d->list, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &font);
            if (font)
            {
                font_name(d, font->af_Attr.ta_Name);
                set(d->size, MUIA_String_Integer, font->af_Attr.ta_YSize);
                d->request.fo_Attr.ta_Flags = font->af_Attr.ta_Flags;
            }
        }
        font_preview(d); return TRUE;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END
const struct __MUIBuiltinClass _MUI_Fontpanel_desc = {
    MUIC_Fontpanel, MUIC_Panel, sizeof(struct FontpanelData), (void *)Fontpanel_Dispatcher
};

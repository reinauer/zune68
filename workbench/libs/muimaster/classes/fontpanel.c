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
struct UserFontFamily
{
    struct UserFontFamily *next;
    STRPTR family, font;
};
struct FontpanelData
{
    Object *list, *name, *size, *preview, *sample;
    Object *bold, *italic, *underline, *front, *back, *mode, *special;
    Object *family, *collection_control;
    struct UserFontFamily *user_fonts;
    CONST_STRPTR collection_names[6];
    LONG collection_values[5], selected_collection;
    struct AvailFontsHeader *fonts;
    struct TextFont *sample_font;
    struct Hook display;
    struct Hook *filter;
    struct FontRequester request;
    struct FontRequester *external_request;
    STRPTR sampletext;
    char number[16];
    LONG collection, minimum, maximum;
    BOOL fixed;
};
static BOOL family_member(struct FontpanelData *d, CONST_STRPTR font)
{
    CONST_STRPTR family = d->family ? (APTR)XGET(d->family, MUIA_String_Contents) : NULL;
    for (struct UserFontFamily *f = d->user_fonts; f; f = f->next)
        if (!Stricmp(f->font, font) && (!family || !*family || !Stricmp(f->family, family)))
            return TRUE;
    return FALSE;
}
static void free_families(struct UserFontFamily *f)
{
    while (f)
    {
        struct UserFontFamily *next = f->next;
        FreeVec(f->family); FreeVec(f->font); FreeVec(f); f = next;
    }
}
static BOOL add_family(struct FontpanelData *d, CONST_STRPTR family, CONST_STRPTR font)
{
    if (!family || !*family || !font || !*font ||
        strpbrk(family, "\t\r\n") || strpbrk(font, "\t\r\n")) return FALSE;
    for (struct UserFontFamily *f = d->user_fonts; f; f = f->next)
        if (!Stricmp(f->family, family) && !Stricmp(f->font, font)) return TRUE;
    struct UserFontFamily *f = AllocVec(sizeof(*f), MEMF_ANY | MEMF_CLEAR);
    if (!f) return FALSE;
    f->family = StrDup(family); f->font = StrDup(font);
    if (!f->family || !f->font) { free_families(f); return FALSE; }
    f->next = d->user_fonts; d->user_fonts = f;
    return TRUE;
}
static void load_families(struct FontpanelData *d)
{
    char line[512];
    BPTR file = Open("ENV:Zune/fontfamilies", MODE_OLDFILE);
    if (!file) file = Open("ENVARC:Zune/fontfamilies", MODE_OLDFILE);
    if (!file) return;
    while (FGets(file, line, sizeof(line)))
    {
        char *font = strchr(line, '\t');
        if (!font) continue;
        *font++ = 0;
        font[strcspn(font, "\r\n")] = 0;
        if (!add_family(d, line, font)) break;
    }
    Close(file);
}
static BOOL save_family_directory(struct FontpanelData *d)
{
    BPTR file = Open("fontfamilies.new", MODE_NEWFILE);
    if (!file) return FALSE;
    BOOL ok = TRUE;
    for (struct UserFontFamily *f = d->user_fonts; f; f = f->next)
        if (FPuts(file, f->family) < 0 || FPuts(file, "\t") < 0 ||
            FPuts(file, f->font) < 0 || FPuts(file, "\n") < 0)
        { ok = FALSE; break; }
    if (!Close(file)) ok = FALSE;
    if (!ok) { DeleteFile("fontfamilies.new"); return FALSE; }
    DeleteFile("fontfamilies.bak");
    BOOL had_old = Rename("fontfamilies", "fontfamilies.bak");
    if (!Rename("fontfamilies.new", "fontfamilies"))
    {
        if (had_old) Rename("fontfamilies.bak", "fontfamilies");
        DeleteFile("fontfamilies.new"); return FALSE;
    }
    DeleteFile("fontfamilies.bak");
    return TRUE;
}
static BOOL save_families(struct FontpanelData *d)
{
    static const char *paths[] = {"ENVARC:Zune", "ENV:Zune"};
    BPTR dirs[2] = {0, 0};
    BOOL ok = TRUE;
    for (ULONG i = 0; i < 2; i++)
    {
        dirs[i] = Lock(paths[i], ACCESS_READ);
        if (!dirs[i])
        {
            BPTR created = CreateDir(paths[i]);
            if (created) UnLock(created);
            dirs[i] = Lock(paths[i], ACCESS_READ);
        }
        if (!dirs[i]) { ok = FALSE; break; }
        /* ENV may be a soft link to ENVARC. Resolve the directory before
         * renaming files, and do not replace the same file twice. */
        if (i && SameLock(dirs[0], dirs[1]) == LOCK_SAME) continue;
        BPTR old = CurrentDir(dirs[i]);
        ok = save_family_directory(d);
        CurrentDir(old);
        if (!ok) break;
    }
    for (ULONG i = 0; i < 2; i++) if (dirs[i]) UnLock(dirs[i]);
    return ok;
}
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
struct FontdisplayData { struct FontpanelData *panel; };
BOOPSI_DISPATCHER(IPTR, Fontdisplay_Dispatcher, cl, obj, msg)
{
    IPTR result = DoSuperMethodA(cl, obj, msg);
    if (msg->MethodID == OM_NEW)
    {
        if (result)
        {
            struct FontdisplayData *display = INST_DATA(cl, (Object *)result);
            display->panel = (APTR)GetTagData(MUIA_Zune_FontPreviewData, 0,
                ((struct opSet *)msg)->ops_AttrList);
        }
        return result;
    }
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
        struct FontdisplayData *display = INST_DATA(cl, obj);
        struct FontpanelData *d = display->panel;
        CONST_STRPTR sample = d ? d->sampletext : (CONST_STRPTR)"The quick brown fox jumps over the lazy dog.";
        struct RastPort rp = *_rp(obj);
        APTR clip = MUI_AddClipping(muiRenderInfo(obj),
            _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj));
        SetFont(&rp, _font(obj));
        SetSoftStyle(&rp, d ? font_style(d) : 0, AskSoftStyle(&rp));
        SetAPen(&rp, d ? XGET(d->front, MUIA_Numeric_Value) : _dri(obj)->dri_Pens[TEXTPEN]);
        SetBPen(&rp, d ? XGET(d->back, MUIA_Numeric_Value) : _dri(obj)->dri_Pens[BACKGROUNDPEN]);
        ULONG mode = d ? font_modes[XGET(d->mode, MUIA_Cycle_Active)] : JAM1;
        LONG special = d ? XGET(d->special, MUIA_Cycle_Active) : 0;
        LONG x = _mleft(obj) + 1, y = _mtop(obj) + _font(obj)->tf_Baseline + 2;
        if (special)
        {
            if (mode & JAM2)
            {
                SetAPen(&rp, GetBPen(&rp));
                RectFill(&rp, _mleft(obj), _mtop(obj), _mright(obj), _mbottom(obj));
            }
            SetDrMd(&rp, JAM1);
            SetAPen(&rp, GetBPen(&rp));
            for (LONG dy = -1; dy <= 1; dy++)
                for (LONG dx = -1; dx <= 1; dx++)
                    if ((dx || dy) && (special == FO_SPECIALMODE_OUTLINE || (dx == 1 && dy == 1)))
                    {
                        Move(&rp, x + dx, y + dy);
                        Text(&rp, sample, strlen(sample));
                    }
            SetAPen(&rp, d ? XGET(d->front, MUIA_Numeric_Value) : _dri(obj)->dri_Pens[TEXTPEN]);
            mode = JAM1;
        }
        SetDrMd(&rp, mode);
        Move(&rp, x, y);
        Text(&rp, sample, strlen(sample));
        if (clip != (APTR)-1) MUI_RemoveClipping(muiRenderInfo(obj), clip);
    }
    return result;
}
BOOPSI_DISPATCHER_END
const struct __MUIBuiltinClass _MUI_Fontdisplay_desc = {
    MUIC_Fontdisplay, MUIC_Area, sizeof(struct FontdisplayData), (APTR)Fontdisplay_Dispatcher
};
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
    char name[256];
    snprintf(name, sizeof(name), "%s.font", (STRPTR)XGET(d->name, MUIA_String_Contents));
    struct TextAttr attr = {(STRPTR)name, font_size(d), 0, 0};
    struct TextFont *font = OpenDiskFont(&attr);
    if (!font) font = OpenFont(&attr);
    if (!font) return;
    struct TagItem tags[] = {
        {MUIA_Zune_FontPreviewData, (IPTR)d}, {MUIA_Font, (IPTR)font}, {TAG_DONE, 0}
    };
    sample = MUI_NewObjectA(MUIC_Fontdisplay, tags);
    if (!sample) { CloseFont(font); return; }
    DoMethod(d->preview, MUIM_Group_InitChange);
    if (d->sample)
    {
        DoMethod(d->preview, OM_REMMEMBER, d->sample);
        MUI_DisposeObject(d->sample);
    }
    if (d->sample_font) CloseFont(d->sample_font);
    d->sample_font = font;
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
 }
static void font_filter(struct FontpanelData *d)
{
    DoMethod(d->list, MUIM_List_Clear);
    if (!d->fonts) return;
    LONG collection = d->selected_collection;
    struct AvailFonts *font = (APTR)(d->fonts + 1);
    for (LONG i = 0; i < d->fonts->afh_NumEntries; i++, font++)
    {
        BOOL fixed = !(font->af_Attr.ta_Flags & FPF_PROPORTIONAL);
        BOOL scalable = !!(font->af_Type & AFF_SCALED);
        if (font->af_Attr.ta_YSize < d->minimum || font->af_Attr.ta_YSize > d->maximum) continue;
        if (d->fixed && !fixed) continue;
        if (collection != MUIV_Fontpanel_ShowCollection_All &&
            !((fixed && (collection & MUIV_Fontpanel_ShowCollection_FixedWidth)) ||
              (!scalable && (collection & MUIV_Fontpanel_ShowCollection_Bitmap)) ||
              ((collection & MUIV_Fontpanel_ShowCollection_TrueType) &&
               font_truetype(font->af_Attr.ta_Name)) ||
              ((collection & MUIV_Fontpanel_ShowCollection_User) &&
               family_member(d, font->af_Attr.ta_Name)))) continue;
        d->request.fo_Attr.ta_Name = (STRPTR)XGET(d->name, MUIA_String_Contents);
        d->request.fo_Attr.ta_YSize = font_size(d);
        d->request.fo_Attr.ta_Style = font_style(d);
        if (d->filter)
        {
            struct FontRequester *request = d->external_request;
            struct FontRequester saved;
            if (request) { saved = *request; request->fo_Attr = d->request.fo_Attr; }
            IPTR accepted = CallHookPkt(d->filter,
                request ? request : &d->request, &font->af_Attr);
            if (request) *request = saved;
            if (!accepted) continue;
        }
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
        d->external_request = (APTR)GetTagData(MUIA_Zune_PanelRequester, 0, tags);
        d->collection = GetTagData(MUIA_Fontpanel_ShowCollection, -1, tags);
        d->selected_collection = d->collection;
        d->minimum = MAX(1, GetTagData(ASLFO_MinHeight, 1, tags));
        d->maximum = MAX(d->minimum, MIN(32767, GetTagData(ASLFO_MaxHeight, 256, tags)));
        ULONG options = GetTagData(ASLFO_Flags, 0, tags);
        d->fixed = !!GetTagData(ASLFO_FixedWidthOnly, options & FOF_FIXEDWIDTHONLY, tags);
        d->filter = (APTR)GetTagData(ASLFO_FilterFunc, 0, tags);
        d->request.fo_UserData = (APTR)GetTagData(ASLFO_UserData, 0, tags);
        d->request.fo_Attr.ta_Flags = GetTagData(ASLFO_InitialFlags, 0, tags);
        d->sampletext = StrDup((STRPTR)GetTagData(ASLFO_SampleText, (IPTR)"The quick brown fox jumps over the lazy dog.", tags));
        if (!d->sampletext) goto fail;
        static const char *collection_names[] = {"Fixed width", "Bitmap", "TrueType", "User families"};
        d->collection_names[0] = "All allowed fonts";
        d->collection_values[0] = d->collection;
        LONG n = 1;
        for (LONG i = 0; i < 4; i++)
            if (d->collection & (1L << i))
            {
                d->collection_names[n] = collection_names[i];
                d->collection_values[n++] = 1L << i;
            }
        d->collection_control = MUI_NewObject(MUIC_Cycle,
            MUIA_Cycle_Entries, d->collection_names, MUIA_CycleChain, TRUE, TAG_DONE);
        if (!font_add(obj, d->collection_control)) goto fail;
        font_action(d->collection_control, MUIA_Cycle_Active, MUIV_EveryTime, obj, 4);
        row = MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
            MUIA_ShowMe, !!(d->collection & MUIV_Fontpanel_ShowCollection_User), TAG_DONE);
        if (!font_add(obj, row)) goto fail;
        d->family = MUI_NewObject(MUIC_String, MUIA_Frame, MUIV_Frame_String,
            MUIA_String_MaxLen, 128, MUIA_String_Placeholder, "User family (empty shows all)",
            MUIA_CycleChain, TRUE, TAG_DONE);
        if (!font_add(row, d->family)) goto fail;
        font_action(d->family, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 4);
        b = font_button("Add to family", FALSE);
        if (!font_add(row, b)) goto fail;
        font_action(b, MUIA_Pressed, FALSE, obj, 5);
        b = font_button("Remove from family", FALSE);
        if (!font_add(row, b)) goto fail;
        font_action(b, MUIA_Pressed, FALSE, obj, 6);
        if (d->collection & MUIV_Fontpanel_ShowCollection_User) load_families(d);
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
        static const char *special_modes[] = {"Normal", "Outline", "Shadow", NULL};
        d->special = MUI_NewObject(MUIC_Cycle, MUIA_Cycle_Entries, special_modes,
            MUIA_Cycle_Active, MIN(2, GetTagData(ASLFO_InitialSpecMode, 0, tags)),
            MUIA_ShowMe, GetTagData(ASLFO_DoSpecialMode, FALSE, tags), TAG_DONE);
        if (!font_add(obj, d->special)) goto fail;
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
        font_action(d->special, MUIA_Cycle_Active, MUIV_EveryTime, obj, 0);
        font_inventory(d); font_filter(d); font_preview(d);
        return (IPTR)obj;
fail:
        CoerceMethod(cl, obj, OM_DISPOSE); return 0;
    }
    d = INST_DATA(cl, obj);
    switch (msg->MethodID)
    {
    case MUIM_Zune_PanelExport:
    {
        struct FontRequester *r =
            ((struct MUIP_Zune_PanelExport *)msg)->requester;
        CONST_STRPTR name = (APTR)XGET(d->name, MUIA_String_Contents);
        STRPTR copy = AllocVec(strlen(name) + 6, MEMF_ANY);
        if (!copy) return FALSE;
        strcpy(copy, name);
        strcat(copy, ".font");
        FreeVec(r->fo_Attr.ta_Name);
        r->fo_Attr.ta_Name = copy;
        r->fo_Attr.ta_YSize = font_size(d);
        r->fo_Attr.ta_Style = font_style(d);
        r->fo_Attr.ta_Flags = d->request.fo_Attr.ta_Flags;
        r->fo_FrontPen = XGET(d->front, MUIA_Numeric_Value);
        r->fo_BackPen = XGET(d->back, MUIA_Numeric_Value);
        r->fo_DrawMode = font_modes[XGET(d->mode, MUIA_Cycle_Active)];
        r->fo_SpecialDrawMode = XGET(d->special, MUIA_Cycle_Active);
        r->fo_UserData = d->request.fo_UserData;
        return TRUE;
    }
    case OM_DISPOSE:
        /* Lists borrow entries from this buffer and must go first. */
    {
        APTR fonts = d->fonts;
        struct TextFont *sample_font = d->sample_font;
        struct UserFontFamily *families = d->user_fonts;
        STRPTR text = d->sampletext;
        IPTR result = DoSuperMethodA(cl, obj, msg);
        if (sample_font) CloseFont(sample_font);
        free_families(families);
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
            case ASLFO_InitialSpecMode: nnset(d->special, MUIA_Cycle_Active, MIN(2, tag->ti_Data)); break;
            default: continue;
            }
            changed = TRUE;
        }
        if (changed && d->special) font_preview(d);
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
        case ASLFO_InitialSpecMode: value = XGET(d->special, MUIA_Cycle_Active); break;
        default: return DoSuperMethodA(cl, obj, msg);
        }
        *getmsg->opg_Storage = value; return TRUE;
    }
    case MUIM_Zune_PanelAction:
    {
        LONG act = ((IPTR *)msg)[1];
        if (act == 2 || act == 3) return DoMethod(obj, MUIM_Zune_PanelFinish, act == 2);
        if (act >= 4 && act <= 6)
        {
            if (act != 4)
            {
                CONST_STRPTR family = (APTR)XGET(d->family, MUIA_String_Contents);
                char font[256];
                snprintf(font, sizeof(font), "%s.font", (STRPTR)XGET(d->name, MUIA_String_Contents));
                BOOL ok = TRUE;
                if (act == 5) ok = add_family(d, family, font);
                else
                {
                    struct UserFontFamily **link = &d->user_fonts;
                    while (*link)
                    {
                        struct UserFontFamily *f = *link;
                        if (!Stricmp(f->family, family) && !Stricmp(f->font, font))
                        { *link = f->next; f->next = NULL; free_families(f); }
                        else link = &f->next;
                    }
                }
                if (!ok || !save_families(d))
                {
                    if (_flags(obj) & MADF_SETUP)
                        MUI_Request(_app(obj), _win(obj), 0, "Font families", "OK",
                            "Could not save the user font family.");
                    else DisplayBeep(NULL);
                    return FALSE;
                }
            }
            d->selected_collection = d->collection_values[XGET(d->collection_control, MUIA_Cycle_Active)];
            font_filter(d);
            return TRUE;
        }
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

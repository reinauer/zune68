/* Shortcut capture and editing using the OS input-expression grammar. */
#define MUIMASTER_YES_INLINE_STDARG
#include <string.h>
#include <stdio.h>
#include <devices/inputevent.h>
#include <libraries/commodities.h>
#include <intuition/sghooks.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/commodities.h>
#include <proto/keymap.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "keyadjust.h"
#define MUIM_Zune_KeyPopup (MUIB_ZUNE | 0x4600)
#define MUIM_Zune_KeyChanged (MUIB_ZUNE | 0x4601)
struct KeyadjustData
{
    Object *string, *button;
    struct Hook edit_hook;
    struct MUI_EventHandlerNode handler;
    BOOL multiple, mouse, doubleclick, tripleclick, append;
    ULONG seconds, micros;
    UWORD clicks;
};
static BOOL valid_keys(CONST_STRPTR value, BOOL multiple)
{
    char key[128];
    if (!value || !*value) return TRUE;
    for (;;)
    {
        const char *comma = strchr(value, ',');
        ULONG length = comma ? comma - (const char *)value : strlen(value);
        if (length >= sizeof(key) || (!multiple && comma)) return FALSE;
        memcpy(key, value, length); key[length] = 0;
        struct InputXpression ix;
        if (ParseIX(key, &ix)) return FALSE;
        if (!comma) return TRUE;
        value = (CONST_STRPTR)comma + 1;
    }
}
static Object *key_menu(struct KeyadjustData *d)
{
    Object *strip = MUI_NewObject(MUIC_Menustrip, TAG_DONE);
    Object *menu = MUI_NewObject(MUIC_Menu, MUIA_Menu_Title, "Shortcuts", TAG_DONE);
    if (!strip || !menu)
    { if (strip) MUI_DisposeObject(strip); if (menu) MUI_DisposeObject(menu); return NULL; }
    DoMethod(strip, OM_ADDMEMBER, menu);
    CONST_STRPTR text = (APTR)XGET(d->string, MUIA_String_Contents);
    LONG index = 1;
    while (*text)
    {
        char title[160] = "Remove: ";
        const char *comma = strchr(text, ',');
        ULONG length = comma ? comma - (const char *)text : strlen(text);
        strncat(title, text, MIN(length, sizeof(title) - 9));
        Object *item = MUI_NewObject(MUIC_Menuitem, MUIA_Menuitem_Title, title,
            MUIA_Menuitem_CopyStrings, TRUE, MUIA_UserData, index++, TAG_DONE);
        if (item) DoMethod(menu, OM_ADDMEMBER, item);
        if (!comma) break;
        text = (CONST_STRPTR)comma + 1;
    }
    if (d->multiple)
    {
        Object *item = MUI_NewObject(MUIC_Menuitem, MUIA_Menuitem_Title,
            "Add another key", MUIA_UserData, 65536, TAG_DONE);
        if (item) DoMethod(menu, OM_ADDMEMBER, item);
    }
    return strip;
}
static void choose_key(Object *obj, struct KeyadjustData *d, LONG choice)
{
    if (choice == 65536)
    {
        d->append = TRUE;
        set(_win(obj), MUIA_Window_ActiveObject, d->string);
        return;
    }
    STRPTR value = StrDup((APTR)XGET(d->string, MUIA_String_Contents));
    if (!value) return;
    char *start = value;
    for (LONG i = 1; i < choice && start; i++)
    { start = strchr(start, ','); if (start) start++; }
    if (start)
    {
        char *end = strchr(start, ',');
        if (end) memmove(start, end + 1, strlen(end + 1) + 1);
        else if (start != (char *)value) start[-1] = 0;
        else *start = 0;
        set(obj, MUIA_Keyadjust_Key, value);
    }
    FreeVec(value);
}
static BOOL capture_key(struct IntuiMessage *imsg, char *out)
{
    static const char *qualifiers[] = {"lshift ", "rshift ", "capslock ",
        "control ", "lalt ", "ralt ", "lcommand ", "rcommand ", "numericpad "};
    *out = 0;
    for (ULONG i = 0; i < 9; i++) if (imsg->Qualifier & (1L << i)) strcat(out, qualifiers[i]);
    CONST_STRPTR name = NULL;
    char key[16] = {0};
    if (imsg->Class == IDCMP_MOUSEBUTTONS)
    {
        if (imsg->Code == SELECTDOWN) name = "rawmouse lbutton";
        else if (imsg->Code == MENUDOWN) name = "rawmouse rbutton";
        else if (imsg->Code == MIDDLEDOWN) name = "rawmouse mbutton";
    }
    else
    {
        ULONG code = imsg->Code;
        if (code >= 0x60 || (code & IECODE_UP_PREFIX)) return FALSE;
        if (code >= 0x50 && code <= 0x59) { snprintf(key, sizeof(key), "f%lu", code - 0x4f); name = key; }
        else if (code >= 0x4c && code <= 0x4f)
        { static const char *names[] = {"up", "down", "right", "left"}; name = names[code - 0x4c]; }
        else switch (code)
        {
        case 0x40: name = "space"; break;
        case 0x41: name = "backspace"; break;
        case 0x42: name = "tab"; break;
        case 0x43: name = "enter"; break;
        case 0x44: name = "return"; break;
        case 0x45: name = "esc"; break;
        case 0x46: name = "del"; break;
        case 0x5f: name = "help"; break;
        default:
        {
            struct InputEvent event = {0};
            event.ie_Class = IECLASS_RAWKEY; event.ie_Code = code;
            if (MapRawKey(&event, key, 1, NULL) == 1 && key[0] > ' ' && key[0] != ',') name = key;
            break;
        }
        }
    }
    if (!name) return FALSE;
    strcat(out, name);
    return valid_keys(out, FALSE);
}
AROS_UFH3S(IPTR, key_edit_hook,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(struct SGWork *, work, A2),
    AROS_UFHA(ULONG *, command, A1))
{
    AROS_USERFUNC_INIT
    struct KeyadjustData *d = hook->h_Data;
    if (*command != SGH_KEY) return FALSE;
    struct IntuiMessage event = {0};
    event.Class = IDCMP_RAWKEY; event.Code = work->IEvent->ie_Code;
    event.Qualifier = work->IEvent->ie_Qualifier;
    char key[160];
    if (!capture_key(&event, key)) { work->Actions = 0; return TRUE; }
    char value[1024] = "";
    if (d->append && d->multiple)
    {
        strlcat(value, work->PrevBuffer, sizeof(value));
        if (*value) strlcat(value, ",", sizeof(value));
    }
    strlcat(value, key, sizeof(value)); d->append = FALSE;
    strcpy(work->WorkBuffer, value);
    work->BufferPos = work->NumChars = strlen(value);
    work->Actions = SGA_USE | SGA_REDISPLAY;
    return TRUE;
    AROS_USERFUNC_EXIT
}
BOOPSI_DISPATCHER(IPTR, Keyadjust_Dispatcher, cl, obj, msg)
{
    struct KeyadjustData *d;
    if (msg->MethodID == OM_NEW)
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList;
        obj = (Object *)DoSuperNewTags(cl, obj, NULL, MUIA_Group_Horiz, TRUE, TAG_MORE, tags);
        if (!obj) return 0;
        d = INST_DATA(cl, obj);
        d->multiple = GetTagData(MUIA_Keyadjust_AllowMultipleKeys, TRUE, tags);
        d->mouse = GetTagData(MUIA_Keyadjust_AllowMouseEvents, FALSE, tags);
        d->doubleclick = GetTagData(MUIA_Keyadjust_AllowDoubleClick, TRUE, tags);
        d->tripleclick = GetTagData(MUIA_Keyadjust_AllowTripleClick, TRUE, tags);
        CONST_STRPTR value = (APTR)GetTagData(MUIA_Keyadjust_Key, (IPTR)"", tags);
        if (!valid_keys(value, d->multiple)) value = "";
        d->string = MUI_NewObject(MUIC_String, MUIA_Frame, MUIV_Frame_String,
            MUIA_CycleChain, TRUE, MUIA_String_MaxLen, 1024, MUIA_String_Contents, value, TAG_DONE);
        if (!d->string) goto fail;
        DoMethod(obj, OM_ADDMEMBER, d->string);
        d->edit_hook.h_Entry = (HOOKFUNC)key_edit_hook;
        d->edit_hook.h_Data = d;
        set(d->string, MUIA_String_EditHook, &d->edit_hook);
        DoMethod(d->string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
            obj, 1, MUIM_Zune_KeyChanged);
        d->button = MUI_NewObject(MUIC_Image, MUIA_Image_Spec, MUII_PopUp,
            MUIA_InputMode, MUIV_InputMode_RelVerify, MUIA_Frame, MUIV_Frame_Button, TAG_DONE);
        if (!d->button) goto fail;
        DoMethod(obj, OM_ADDMEMBER, d->button);
        DoMethod(d->button, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_Zune_KeyPopup);
        d->handler.ehn_Object = obj; d->handler.ehn_Class = cl;
        d->handler.ehn_Events = IDCMP_MOUSEBUTTONS;
        d->handler.ehn_Priority = 1; d->handler.ehn_Flags = MUI_EHF_GUIMODE;
        return (IPTR)obj;
fail:   CoerceMethod(cl, obj, OM_DISPOSE); return 0;
    }
    d = INST_DATA(cl, obj);
    switch (msg->MethodID)
    {
    case OM_GET:
    {
        struct opGet *get = (APTR)msg;
        IPTR value;
        switch (get->opg_AttrID)
        {
        case MUIA_Keyadjust_Key: value = XGET(d->string, MUIA_String_Contents); break;
        case MUIA_Keyadjust_AllowMultipleKeys: value = d->multiple; break;
        case MUIA_Keyadjust_AllowMouseEvents: value = d->mouse; break;
        case MUIA_Keyadjust_AllowDoubleClick: value = d->doubleclick; break;
        case MUIA_Keyadjust_AllowTripleClick: value = d->tripleclick; break;
        default: return DoSuperMethodA(cl, obj, msg);
        }
        *get->opg_Storage = value; return TRUE;
    }
    case OM_SET:
    {
        struct TagItem *state = ((struct opSet *)msg)->ops_AttrList, *tag;
        while ((tag = NextTagItem(&state)))
            switch (tag->ti_Tag)
            {
            case MUIA_Keyadjust_Key:
                if (valid_keys((APTR)tag->ti_Data, d->multiple))
                    nnset(d->string, MUIA_String_Contents, tag->ti_Data ? tag->ti_Data : (IPTR)"");
                else tag->ti_Tag = TAG_IGNORE;
                break;
            case MUIA_Keyadjust_AllowMultipleKeys: d->multiple = !!tag->ti_Data; break;
            case MUIA_Keyadjust_AllowMouseEvents: d->mouse = !!tag->ti_Data; break;
            case MUIA_Keyadjust_AllowDoubleClick: d->doubleclick = !!tag->ti_Data; break;
            case MUIA_Keyadjust_AllowTripleClick: d->tripleclick = !!tag->ti_Data; break;
            }
        break;
    }
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg)) return FALSE;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, &d->handler); return TRUE;
    case MUIM_Cleanup:
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &d->handler); break;
    case MUIM_HandleEvent:
    {
        struct IntuiMessage *imsg = ((struct MUIP_HandleEvent *)msg)->imsg;
        if (!imsg || (Object *)XGET(_win(obj), MUIA_Window_ActiveObject) != d->string) return 0;
        if (imsg->Class == IDCMP_MOUSEBUTTONS)
        {
            if (imsg->MouseX < _left(d->string) || imsg->MouseX > _right(d->string) ||
                imsg->MouseY < _top(d->string) || imsg->MouseY > _bottom(d->string)) return 0;
            if (imsg->Code == SELECTDOWN)
            {
                d->clicks = DoubleClick(d->seconds, d->micros, imsg->Seconds, imsg->Micros) ? d->clicks + 1 : 1;
                d->seconds = imsg->Seconds; d->micros = imsg->Micros;
                if ((!d->doubleclick && d->clicks == 2) || (!d->tripleclick && d->clicks >= 3))
                    return MUI_EventHandlerRC_Eat;
            }
            if (!d->mouse) return 0;
        }
        char key[160];
        if (!capture_key(imsg, key)) return imsg->Class == IDCMP_RAWKEY ? MUI_EventHandlerRC_Eat : 0;
        char value[1024]; value[0] = 0;
        if (d->append && d->multiple)
        {
            strlcat(value, (APTR)XGET(d->string, MUIA_String_Contents), sizeof(value));
            if (*value) strlcat(value, ",", sizeof(value));
        }
        strlcat(value, key, sizeof(value)); d->append = FALSE;
        set(obj, MUIA_Keyadjust_Key, value);
        return MUI_EventHandlerRC_Eat;
    }
    case MUIM_Zune_KeyChanged:
        superset(cl, obj, MUIA_Keyadjust_Key, XGET(d->string, MUIA_String_Contents));
        return TRUE;
    case MUIM_Zune_KeyPopup:
    {
        if (!(_flags(obj) & MADF_SETUP)) return 0;
        struct ZuneWindowGuard guard = {0}; Object *win = _win(obj);
        Object *menu = key_menu(d);
        if (!menu) return 0;
        ZuneWatchWindow(win, &guard);
        LONG index = DoMethod(menu, MUIM_Menustrip_Popup, obj, 0,
            _left(d->button) - _left(obj), _bottom(d->button) - _top(obj));
        MUI_DisposeObject(menu);
        if (guard.closed) return 0;
        ZuneUnwatchWindow(win, &guard);
        if (index > 0) choose_key(obj, d, index);
        return TRUE;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END
const struct __MUIBuiltinClass _MUI_Keyadjust_desc = {
    MUIC_Keyadjust, MUIC_Group, sizeof(struct KeyadjustData), (APTR)Keyadjust_Dispatcher
};

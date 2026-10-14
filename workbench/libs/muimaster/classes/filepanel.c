/* Embedded file selector with ASL tag aliases and DOS pattern filtering. */
#define MUIMASTER_YES_INLINE_STDARG
#include <string.h>
#include <dos/exall.h>
#include <libraries/asl.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "selectorpanel.h"
extern struct Library *MUIMasterBase;

#define FP_NAVIGATE 1
#define FP_PICK 2
#define FP_OPEN 3
#define FP_PARENT 4
#define FP_BACK 5
#define FP_FORWARD 6
#define FP_ACCEPT 7
#define FP_CANCEL 8
#define FP_PATTERN 9

struct FilepanelData
{
    Object *list, *drawer, *file, *pattern;
    struct Hook filter;
    struct Hook *userfilter;
    struct FileRequester request;
    STRPTR accept, reject, parsed;
    BOOL multi, save, patterns, drawers, filterdrawers, icons;
    STRPTR history[32];
    LONG position, last;
};
static STRPTR field(Object *obj) { return (STRPTR)XGET(obj, MUIA_String_Contents); }
static IPTR option(struct TagItem *tags, ULONG mui, ULONG asl, IPTR def)
{
    struct TagItem *tag = FindTagItem(mui, tags);
    return tag ? tag->ti_Data : GetTagData(asl, def, tags);
}
static Object *string_field(CONST_STRPTR contents)
{
    return MUI_NewObject(MUIC_String, MUIA_Frame, MUIV_Frame_String,
        MUIA_CycleChain, TRUE, MUIA_String_MaxLen, 1024,
        MUIA_String_Contents, contents ? contents : (CONST_STRPTR)"", TAG_DONE);
}
static Object *button(CONST_STRPTR label)
{
    return MUI_NewObject(MUIC_Text, MUIA_Frame, MUIV_Frame_Button,
        MUIA_Background, MUII_ButtonBack, MUIA_InputMode, MUIV_InputMode_RelVerify,
        MUIA_Text_Contents, label, MUIA_Text_PreParse, "\33c",
        MUIA_CycleChain, TRUE, TAG_DONE);
}
static BOOL add(Object *parent, Object *child)
{
    if (!child) return FALSE;
    DoMethod(parent, OM_ADDMEMBER, child);
    return TRUE;
}
static void action(Object *source, ULONG attr, IPTR value, Object *panel, LONG act)
{
    DoMethod(source, MUIM_Notify, attr, value, panel, 2, MUIM_Zune_PanelAction, act);
}
static void compile_pattern(struct FilepanelData *d)
{
    STRPTR raw = field(d->pattern);
    LONG size = strlen(raw) * 2 + 3;
    STRPTR parsed = AllocVec(size, MEMF_ANY);
    if (parsed && ParsePatternNoCase(raw, parsed, size) >= 0)
    { FreeVec(d->parsed); d->parsed = parsed; }
    else FreeVec(parsed);
}
AROS_UFH3S(IPTR, file_filter,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(Object *, list, A2),
    AROS_UFHA(struct ExAllData *, entry, A1))
{
    AROS_USERFUNC_INIT
    struct FilepanelData *d = hook->h_Data;
    BOOL drawer = entry->ed_Type > 0;
    if (d->userfilter)
    {
        struct AnchorPath anchor = {0};
        struct FileInfoBlock *fib = &anchor.ap_Info;
        strncpy(fib->fib_FileName, entry->ed_Name, sizeof(fib->fib_FileName) - 1);
        fib->fib_DirEntryType = fib->fib_EntryType = entry->ed_Type;
        fib->fib_Size = entry->ed_Size;
        fib->fib_Protection = entry->ed_Prot;
        fib->fib_Date.ds_Days = entry->ed_Days;
        fib->fib_Date.ds_Minute = entry->ed_Mins;
        fib->fib_Date.ds_Tick = entry->ed_Ticks;
        if (entry->ed_Comment)
            strncpy(fib->fib_Comment, entry->ed_Comment, sizeof(fib->fib_Comment) - 1);
        d->request.fr_Drawer = field(d->drawer);
        d->request.fr_File = field(d->file);
        d->request.fr_Pattern = field(d->pattern);
        return CallHookPkt(d->userfilter, &d->request, &anchor);
    }
    if (!drawer && d->drawers) return FALSE;
    if (d->icons)
    {
        LONG len = strlen(entry->ed_Name);
        if (len >= 5 && !Stricmp(entry->ed_Name + len - 5, ".info")) return FALSE;
    }
    if (!drawer || d->filterdrawers)
    {
        if (d->accept && *d->accept && !MatchPatternNoCase(d->accept, entry->ed_Name)) return FALSE;
        if (d->reject && *d->reject && MatchPatternNoCase(d->reject, entry->ed_Name)) return FALSE;
        if (d->patterns && d->parsed && *field(d->pattern) &&
            !MatchPatternNoCase(d->parsed, entry->ed_Name)) return FALSE;
    }
    return TRUE;
    AROS_USERFUNC_EXIT
}
static void navigate(struct FilepanelData *d, CONST_STRPTR path, BOOL remember)
{
    /* The path may be a child object's transient Dirlist_Path buffer. */
    STRPTR copy = StrDup(path ? path : (CONST_STRPTR)"");
    if (!copy) return;
    set(d->drawer, MUIA_String_Contents, copy);
    if (remember && (d->position < 0 || strcmp(d->history[d->position], copy)))
    {
        while (d->last > d->position) FreeVec(d->history[d->last--]);
        if (d->last == 31)
        {
            FreeVec(d->history[0]);
            memmove(d->history, d->history + 1, 31 * sizeof(STRPTR));
            d->last--; d->position--;
        }
        STRPTR history = StrDup(copy);
        if (history) { d->history[++d->last] = history; d->position = d->last; }
    }
    set(d->list, MUIA_Dirlist_Directory, field(d->drawer));
    FreeVec(copy);
}
static IPTR Filepanel_Action(Object *obj, struct FilepanelData *d, LONG act)
{
    struct FileInfoBlock *fib = NULL;
    switch (act)
    {
    case FP_PATTERN:
        compile_pattern(d);
        /* fall through */
    case FP_NAVIGATE:
        navigate(d, field(d->drawer), TRUE);
        break;
    case FP_PICK:
    case FP_OPEN:
        DoMethod(d->list, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &fib);
        if (fib)
        {
            if (fib->fib_DirEntryType > 0)
            {
                if (act == FP_OPEN)
                    navigate(d, (STRPTR)XGET(d->list, MUIA_Dirlist_Path), TRUE);
            }
            else
            {
                set(d->file, MUIA_String_Contents, fib->fib_FileName);
                if (act == FP_OPEN) DoMethod(obj, MUIM_Zune_PanelFinish, TRUE);
            }
        }
        break;
    case FP_PARENT:
    {
        BPTR lock = Lock(field(d->drawer), SHARED_LOCK);
        if (lock)
        {
            BPTR parent = ParentDir(lock);
            char path[1024];
            if (parent)
            {
                if (NameFromLock(parent, path, sizeof(path))) navigate(d, path, TRUE);
                UnLock(parent);
            }
            UnLock(lock);
        }
        break;
    }
    case FP_BACK:
        if (d->position > 0) navigate(d, d->history[--d->position], FALSE);
        break;
    case FP_FORWARD:
        if (d->position < d->last) navigate(d, d->history[++d->position], FALSE);
        break;
    case FP_ACCEPT: return DoMethod(obj, MUIM_Zune_PanelFinish, TRUE);
    case FP_CANCEL: return DoMethod(obj, MUIM_Zune_PanelFinish, FALSE);
    }
    return TRUE;
}
BOOPSI_DISPATCHER(IPTR, Filepanel_Dispatcher, cl, obj, msg)
{
    struct FilepanelData *d;
    if (msg->MethodID == OM_NEW)
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList;
        Object *row, *view, *b;
        obj = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!obj) return 0;
        d = INST_DATA(cl, obj);
        d->position = d->last = -1;
        ULONG flags1 = GetTagData(ASLFR_Flags1, 0, tags);
        ULONG flags2 = GetTagData(ASLFR_Flags2, 0, tags);
        d->save = !!option(tags, MUIA_Filepanel_DoSaveMode, ASLFR_DoSaveMode, flags1 & FRF_DOSAVEMODE);
        d->multi = !d->save && option(tags, MUIA_Filepanel_DoMultiSelect, ASLFR_DoMultiSelect, flags1 & FRF_DOMULTISELECT);
        d->patterns = !!option(tags, MUIA_Filepanel_DoPatterns, ASLFR_DoPatterns, flags1 & FRF_DOPATTERNS);
        d->drawers = !!option(tags, MUIA_Filepanel_DrawersOnly, ASLFR_DrawersOnly, flags2 & FRF_DRAWERSONLY);
        d->filterdrawers = !!option(tags, MUIA_Filepanel_FilterDrawers, ASLFR_FilterDrawers, flags2 & FRF_FILTERDRAWERS);
        d->icons = !!option(tags, MUIA_Filepanel_RejectIcons, ASLFR_RejectIcons, flags2 & FRF_REJECTICONS);
        d->accept = (STRPTR)option(tags, MUIA_Filepanel_AcceptPattern, ASLFR_AcceptPattern, 0);
        d->reject = (STRPTR)option(tags, MUIA_Filepanel_RejectPattern, ASLFR_RejectPattern, 0);
        d->userfilter = (APTR)option(tags, MUIA_Filepanel_FilterFunc, ASLFR_FilterFunc, 0);
        d->request.fr_UserData = (APTR)GetTagData(ASLFR_UserData, 0, tags);
        d->filter.h_Entry = (HOOKFUNC)file_filter; d->filter.h_Data = d;
        d->drawer = string_field((STRPTR)option(tags, MUIA_Filepanel_Drawer, ASLFR_InitialDrawer, (IPTR)""));
        if (!add(obj, d->drawer)) goto fail;
        row = MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
        if (!add(obj, row)) goto fail;
        const char *labels[] = {"Parent", "Back", "Forward"};
        const LONG acts[] = {FP_PARENT, FP_BACK, FP_FORWARD};
        for (LONG i = 0; i < 3; i++)
        { b = button(labels[i]); if (!add(row, b)) goto fail; action(b, MUIA_Pressed, FALSE, obj, acts[i]); }
        d->file = string_field((STRPTR)option(tags, MUIA_Filepanel_File, ASLFR_InitialFile, (IPTR)""));
        if (!add(obj, d->file)) goto fail;
        set(d->file, MUIA_ShowMe, !d->drawers);
        d->pattern = string_field((STRPTR)option(tags, MUIA_Filepanel_Pattern, ASLFR_InitialPattern, (IPTR)"#?"));
        if (!add(obj, d->pattern)) goto fail;
        set(d->pattern, MUIA_ShowMe, d->patterns);
        compile_pattern(d);
        d->list = MUI_NewObject(MUIC_Dirlist, MUIA_Dirlist_FilterHook, &d->filter,
            MUIA_List_Format, "", MUIA_List_Title, FALSE, TAG_DONE);
        if (!d->list) goto fail;
        view = MUI_NewObject(MUIC_Listview, MUIA_Listview_List, d->list,
            MUIA_Listview_MultiSelect, d->multi ? MUIV_Listview_MultiSelect_Default : MUIV_Listview_MultiSelect_None,
            MUIA_CycleChain, TRUE, TAG_DONE);
        if (!view) { MUI_DisposeObject(d->list); d->list = NULL; goto fail; }
        add(obj, view);
        action(d->list, MUIA_List_Active, MUIV_EveryTime, obj, FP_PICK);
        action(view, MUIA_Listview_DoubleClick, TRUE, obj, FP_OPEN);
        action(d->drawer, MUIA_String_Acknowledge, MUIV_EveryTime, obj, FP_NAVIGATE);
        action(d->file, MUIA_String_Acknowledge, MUIV_EveryTime, obj, FP_ACCEPT);
        action(d->pattern, MUIA_String_Acknowledge, MUIV_EveryTime, obj, FP_PATTERN);
        row = MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
        if (!add(obj, row)) goto fail;
        b = button((STRPTR)GetTagData(ASLFR_PositiveText, (IPTR)(d->save ? "Save" : "OK"), tags));
        if (!add(row, b)) goto fail;
        action(b, MUIA_Pressed, FALSE, obj, FP_ACCEPT);
        b = button((STRPTR)GetTagData(ASLFR_NegativeText, (IPTR)"Cancel", tags));
        if (!add(row, b)) goto fail;
        action(b, MUIA_Pressed, FALSE, obj, FP_CANCEL);
        navigate(d, field(d->drawer), TRUE);
        return (IPTR)obj;
fail:
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }
    d = INST_DATA(cl, obj);
    switch (msg->MethodID)
    {
    case OM_DISPOSE:
    {
        /* Keep filter state alive until child lists have been disposed. */
        for (LONG i = 0; i <= d->last; i++) FreeVec(d->history[i]);
        FreeVec(d->parsed);
        break;
    }
    case OM_GET:
    {
        struct opGet *getmsg = (APTR)msg;
        IPTR value;
        switch (getmsg->opg_AttrID)
        {
        case MUIA_Filepanel_Drawer: case ASLFR_InitialDrawer: value = (IPTR)field(d->drawer); break;
        case MUIA_Filepanel_File: case ASLFR_InitialFile: value = (IPTR)field(d->file); break;
        case MUIA_Filepanel_Pattern: case ASLFR_InitialPattern: value = (IPTR)field(d->pattern); break;
        case MUIA_Filepanel_DoMultiSelect: case ASLFR_DoMultiSelect: value = d->multi; break;
        case MUIA_Filepanel_DoSaveMode: case ASLFR_DoSaveMode: value = d->save; break;
        case MUIA_Filepanel_DoPatterns: case ASLFR_DoPatterns: value = d->patterns; break;
        case MUIA_Filepanel_DrawersOnly: case ASLFR_DrawersOnly: value = d->drawers; break;
        case MUIA_Filepanel_FilterDrawers: case ASLFR_FilterDrawers: value = d->filterdrawers; break;
        case MUIA_Filepanel_RejectIcons: case ASLFR_RejectIcons: value = d->icons; break;
        case MUIA_Filepanel_FilterFunc: case ASLFR_FilterFunc: value = (IPTR)d->userfilter; break;
        case MUIA_Filepanel_AcceptPattern: case ASLFR_AcceptPattern: value = (IPTR)d->accept; break;
        case MUIA_Filepanel_RejectPattern: case ASLFR_RejectPattern: value = (IPTR)d->reject; break;
        default: return DoSuperMethodA(cl, obj, msg);
        }
        *getmsg->opg_Storage = value; return TRUE;
    }
    case OM_SET:
    {
        struct TagItem *state = ((struct opSet *)msg)->ops_AttrList, *tag;
        while ((tag = NextTagItem(&state)))
            switch (tag->ti_Tag)
            {
            case MUIA_Filepanel_Drawer: case ASLFR_InitialDrawer:
                if (d->list) navigate(d, (STRPTR)tag->ti_Data, TRUE);
                break;
            case MUIA_Filepanel_File: case ASLFR_InitialFile:
                if (d->file) set(d->file, MUIA_String_Contents, tag->ti_Data);
                break;
            case MUIA_Filepanel_Pattern: case ASLFR_InitialPattern:
                if (d->pattern)
                {
                    set(d->pattern, MUIA_String_Contents, tag->ti_Data);
                    compile_pattern(d);
                    if (d->list) DoMethod(d->list, MUIM_Dirlist_ReRead);
                }
                break;
            }
        break;
    }
    case MUIM_Zune_PanelAction:
        return Filepanel_Action(obj, d, ((IPTR *)msg)[1]);
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END
const struct __MUIBuiltinClass _MUI_Filepanel_desc = {
    MUIC_Filepanel, MUIC_Panel, sizeof(struct FilepanelData), (void *)Filepanel_Dispatcher
};

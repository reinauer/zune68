/* ASL-compatible result ownership around the MUI requester panels. */
#define MUIMASTER_YES_INLINE_STDARG
#include <stddef.h>
#include <string.h>
#include <libraries/asl.h>
#include <dos/dostags.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include "muimaster_intern.h"
#include "support.h"
#include "classes/selectorpanel.h"
#include "requesters.h"

struct Request
{
    struct Request *next;
    ULONG type;
    APTR native;
    struct TagItem *tags;
    BOOL owned, running;
    union { struct FileRequester file; struct FontRequester font; } result;
};
static struct Request *requests;
static BOOL panel_tags(struct Request *r);
struct RequestJob
{
    struct Message message;
    struct Request *request;
    BOOL accepted;
};

/* Tags carrying application callbacks remain borrowed. Strings are copied,
 * just as AllocAslRequest does, including compiled DOS patterns. */
static BOOL string_tag(ULONG type, ULONG tag)
{
    return tag == ASLFR_TitleText || tag == ASLFR_PositiveText ||
        tag == ASLFR_NegativeText || tag == ASLFR_PubScreenName ||
        (type == ASL_FileRequest && (tag == ASLFR_InitialFile ||
        tag == ASLFR_InitialDrawer || tag == ASLFR_InitialPattern ||
        tag == ASLFR_AcceptPattern || tag == ASLFR_RejectPattern)) ||
        (type == ASL_FontRequest && (tag == ASLFO_InitialName ||
        tag == ASLFO_SampleText));
}
static void free_tags(ULONG type, struct TagItem *tags)
{
    if (!tags) return;
    for (struct TagItem *t = tags; t->ti_Tag; t++)
        if (string_tag(type, t->ti_Tag)) FreeVec((APTR)t->ti_Data);
    FreeVec(tags);
}
static struct TagItem *merge_tags(ULONG type, struct TagItem *old,
    struct TagItem *changes)
{
    struct TagItem *state, *t, *copy;
    ULONG count = 0, used = 0;
    state = old;
    while (NextTagItem(&state)) count++;
    state = changes;
    while (NextTagItem(&state)) count++;
    copy = AllocVec((count + 1) * sizeof(*copy), MEMF_ANY | MEMF_CLEAR);
    if (!copy) return NULL;
    for (ULONG pass = 0; pass < 2; pass++)
    {
        state = pass ? changes : old;
        while ((t = NextTagItem(&state)))
        {
            ULONG i;
            IPTR value = t->ti_Data;
            for (i = 0; i < used && copy[i].ti_Tag != t->ti_Tag; i++);
            if (string_tag(type, t->ti_Tag) && value)
            {
                value = (IPTR)StrDup((CONST_STRPTR)value);
                if (!value) { free_tags(type, copy); return NULL; }
            }
            if (i < used && string_tag(type, t->ti_Tag))
                FreeVec((APTR)copy[i].ti_Data);
            copy[i] = (struct TagItem){t->ti_Tag, value};
            if (i == used) used++;
        }
    }
    return copy;
}
static struct Request *find_request(APTR public)
{
    struct Request *r;
    ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    for (r = requests; r && &r->result != public; r = r->next);
    ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    return r;
}
static void free_result(struct Request *r)
{
    if (r->owned)
    {
        if (r->type == ASL_FileRequest) ZuneFreeFileResult(&r->result.file);
        else FreeVec(r->result.font.fo_Attr.ta_Name);
    }
    memset(&r->result, 0, sizeof(r->result));
    r->owned = FALSE;
}
static void native_result(struct Request *r)
{
    free_result(r);
    /* Do not read newer FontRequester extensions from an OS 3.1 allocation. */
    memcpy(&r->result, r->native, r->type == ASL_FileRequest ?
        sizeof(struct FileRequester) : offsetof(struct FontRequester, fo_TAttr));
}
APTR ZuneAllocAslRequest(ULONG type, struct TagItem *tags)
{
    if (type != ASL_FileRequest && type != ASL_FontRequest)
        return AllocAslRequest(type, tags);
    struct Request *r = AllocVec(sizeof(*r), MEMF_ANY | MEMF_CLEAR);
    if (!r) return NULL;
    r->type = type;
    r->native = AllocAslRequest(type, tags);
    r->tags = merge_tags(type, NULL, tags);
    if (!r->native || !r->tags)
    {
        if (r->native) FreeAslRequest(r->native);
        free_tags(type, r->tags); FreeVec(r); return NULL;
    }
    if (!panel_tags(r))
    {
        APTR native = r->native;
        free_tags(type, r->tags); FreeVec(r);
        return native;
    }
    native_result(r);
    if (type == ASL_FontRequest)
        r->result.font.fo_SpecialDrawMode = GetTagData(ASLFO_InitialSpecMode, 0, tags);
    ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    r->next = requests; requests = r;
    ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    return &r->result;
}
void ZuneFreeAslRequest(APTR public)
{
    struct Request *r = find_request(public), **link;
    if (!r) { FreeAslRequest(public); return; }
    if (r->running) return;
    ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    for (link = &requests; *link != r; link = &(*link)->next);
    *link = r->next;
    ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    free_result(r);
    free_tags(r->type, r->tags);
    FreeAslRequest(r->native);
    FreeVec(r);
}

/* Native ASL owns advanced OS-specific options and caller-task IDCMP hooks.
 * Never silently discard such a request or feed a foreign pointer to a panel. */
static BOOL panel_tags(struct Request *r)
{
    for (struct TagItem *t = r->tags; t->ti_Tag; t++)
    {
        switch (t->ti_Tag)
        {
        case ASLFR_Window: case ASLFR_Screen: case ASLFR_PubScreenName:
        case ASLFR_PrivateIDCMP: case ASLFR_TitleText:
        case ASLFR_PositiveText: case ASLFR_NegativeText:
        case ASLFR_UserData: case ASLFR_InitialLeftEdge:
        case ASLFR_InitialTopEdge: case ASLFR_InitialWidth:
        case ASLFR_InitialHeight: case ASLFR_Activate:
            continue;
        }
        if (r->type == ASL_FileRequest)
        {
            switch (t->ti_Tag)
            {
            case ASLFR_Flags1:
                if (t->ti_Data & (FRF_FILTERFUNC | FRF_INTUIFUNC)) return FALSE;
                continue;
            case ASLFR_Flags2: case ASLFR_InitialDrawer:
            case ASLFR_InitialFile: case ASLFR_InitialPattern:
            case ASLFR_DoSaveMode: case ASLFR_DoMultiSelect:
            case ASLFR_DoPatterns: case ASLFR_DrawersOnly:
            case ASLFR_FilterFunc: case ASLFR_RejectIcons:
            case ASLFR_AcceptPattern: case ASLFR_RejectPattern:
            case ASLFR_FilterDrawers:
                continue;
            }
        }
        else
        {
            switch (t->ti_Tag)
            {
            case ASLFO_Flags:
                if (t->ti_Data & (FOF_FILTERFUNC | FOF_INTUIFUNC)) return FALSE;
                continue;
            case ASLFO_InitialName: case ASLFO_InitialSize:
            case ASLFO_InitialStyle: case ASLFO_InitialFlags:
            case ASLFO_InitialFrontPen: case ASLFO_InitialBackPen:
            case ASLFO_InitialDrawMode: case ASLFO_DoFrontPen:
            case ASLFO_DoBackPen: case ASLFO_DoStyle:
            case ASLFO_DoDrawMode: case ASLFO_FixedWidthOnly:
            case ASLFO_MinHeight: case ASLFO_MaxHeight:
            case ASLFO_MaxFrontPen: case ASLFO_MaxBackPen:
            case ASLFO_FilterFunc: case ASLFO_SampleText:
            case ASLFO_DoSpecialMode: case ASLFO_InitialSpecMode:
                continue;
            }
        }
        return FALSE;
    }
    return TRUE;
}
static void SAVEDS requester_process(void)
{
    struct Process *self = (APTR)FindTask(NULL);
    WaitPort(&self->pr_MsgPort);
    struct RequestJob *job = (APTR)GetMsg(&self->pr_MsgPort);
    struct Request *r = job->request;
    struct Window *parent = (APTR)GetTagData(ASLFR_Window, 0, r->tags);
    struct Screen *screen = (APTR)GetTagData(ASLFR_Screen,
        parent ? (IPTR)parent->WScreen : 0, r->tags);
    Object *panel = MUI_NewObject(r->type == ASL_FileRequest ?
        MUIC_Filepanel : MUIC_Fontpanel,
        MUIA_Zune_PanelRequester, &r->result, TAG_MORE, r->tags);
    Object *win = NULL, *app = NULL;
    if (panel)
        win = MUI_NewObject(MUIC_Window,
            MUIA_Window_Title, GetTagData(ASLFR_TitleText,
                (IPTR)(r->type == ASL_FileRequest ? "Select file" : "Select font"), r->tags),
            MUIA_Window_Screen, screen,
            MUIA_Window_PublicScreen, GetTagData(ASLFR_PubScreenName, 0, r->tags),
            MUIA_Window_Activate, GetTagData(ASLFR_Activate, TRUE, r->tags),
            MUIA_Window_LeftEdge, GetTagData(ASLFR_InitialLeftEdge, MUIV_Window_LeftEdge_Centered, r->tags),
            MUIA_Window_TopEdge, GetTagData(ASLFR_InitialTopEdge, MUIV_Window_TopEdge_Centered, r->tags),
            MUIA_Window_Width, GetTagData(ASLFR_InitialWidth, MUIV_Window_Width_Default, r->tags),
            MUIA_Window_Height, GetTagData(ASLFR_InitialHeight, MUIV_Window_Height_Default, r->tags), TAG_DONE);
    if (win)
    {
        set(win, MUIA_Window_RootObject, panel);
        app = MUI_NewObject(MUIC_Application,
            MUIA_Application_Base, "ZUNE_REQUESTER",
            TAG_DONE);
        if (app) DoMethod(app, OM_ADDMEMBER, win);
    }
    if (app)
    {
        BOOL accepted = DoMethod(panel, MUIM_Panel_Run, app, win);
        if (XGET(win, MUIA_Window_Open))
        {
            /* Export into fresh storage so allocation failure keeps old data. */
            union { struct FileRequester file; struct FontRequester font; } result = {0};
            if (DoMethod(panel, MUIM_Zune_PanelExport, &result))
            {
                free_result(r);
                memcpy(&r->result, &result, sizeof(r->result));
                r->owned = TRUE;
                job->accepted = accepted;
            }
        }
        if (r->type == ASL_FileRequest)
        {
            r->result.file.fr_LeftEdge = XGET(win, MUIA_Window_LeftEdge);
            r->result.file.fr_TopEdge = XGET(win, MUIA_Window_TopEdge);
            r->result.file.fr_Width = XGET(win, MUIA_Window_Width);
            r->result.file.fr_Height = XGET(win, MUIA_Window_Height);
        }
        else
        {
            r->result.font.fo_LeftEdge = XGET(win, MUIA_Window_LeftEdge);
            r->result.font.fo_TopEdge = XGET(win, MUIA_Window_TopEdge);
            r->result.font.fo_Width = XGET(win, MUIA_Window_Width);
            r->result.font.fo_Height = XGET(win, MUIA_Window_Height);
        }
        MUI_DisposeObject(app);
    }
    else if (win) MUI_DisposeObject(win);
    else if (panel) MUI_DisposeObject(panel);
    /* Do not let the caller unload our code until this entry has returned. */
    Forbid();
    ReplyMsg(&job->message);
}
struct NativeFilter
{
    struct Hook hook;
    struct Request *request;
    struct Hook *callback;
};
AROS_UFH3S(IPTR, native_filter,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(APTR, requester, A2),
    AROS_UFHA(APTR, message, A1))
{
    AROS_USERFUNC_INIT
    struct NativeFilter *f = (APTR)hook;
    struct Request *r = f->request;
    union { struct FileRequester file; struct FontRequester font; } saved;
    memcpy(&saved, &r->result, sizeof(saved));
    memcpy(&r->result, requester, r->type == ASL_FileRequest ?
        sizeof(struct FileRequester) : offsetof(struct FontRequester, fo_TAttr));
    IPTR result = CallHookPkt(f->callback, &r->result, message);
    memcpy(&r->result, &saved, sizeof(saved));
    return result;
    AROS_USERFUNC_EXIT
}
BOOL ZuneAslRequest(APTR public, struct TagItem *tags)
{
    struct Request *r = find_request(public);
    if (!r) return AslRequest(public, tags);
    if (r->running) return FALSE;
    struct TagItem state[17];
    ULONG count;
    if (r->type == ASL_FileRequest)
    {
        struct FileRequester *f = &r->result.file;
        state[0] = (struct TagItem){ASLFR_InitialFile, (IPTR)f->fr_File};
        state[1] = (struct TagItem){ASLFR_InitialDrawer, (IPTR)f->fr_Drawer};
        state[2] = (struct TagItem){ASLFR_InitialPattern, (IPTR)f->fr_Pattern};
        count = 3;
        state[count++] = (struct TagItem){ASLFR_InitialLeftEdge, f->fr_LeftEdge};
        state[count++] = (struct TagItem){ASLFR_InitialTopEdge, f->fr_TopEdge};
        state[count++] = (struct TagItem){ASLFR_InitialWidth, f->fr_Width};
        state[count++] = (struct TagItem){ASLFR_InitialHeight, f->fr_Height};
    }
    else
    {
        struct FontRequester *f = &r->result.font;
        state[0] = (struct TagItem){ASLFO_InitialName, (IPTR)f->fo_Attr.ta_Name};
        state[1] = (struct TagItem){ASLFO_InitialSize, f->fo_Attr.ta_YSize};
        state[2] = (struct TagItem){ASLFO_InitialStyle, f->fo_Attr.ta_Style};
        state[3] = (struct TagItem){ASLFO_InitialFlags, f->fo_Attr.ta_Flags};
        state[4] = (struct TagItem){ASLFO_InitialFrontPen, f->fo_FrontPen};
        state[5] = (struct TagItem){ASLFO_InitialBackPen, f->fo_BackPen};
        state[6] = (struct TagItem){ASLFO_InitialDrawMode, f->fo_DrawMode};
        state[7] = (struct TagItem){ASLFO_InitialSpecMode, f->fo_SpecialDrawMode};
        count = 8;
        state[count++] = (struct TagItem){ASLFO_InitialLeftEdge, f->fo_LeftEdge};
        state[count++] = (struct TagItem){ASLFO_InitialTopEdge, f->fo_TopEdge};
        state[count++] = (struct TagItem){ASLFO_InitialWidth, f->fo_Width};
        state[count++] = (struct TagItem){ASLFO_InitialHeight, f->fo_Height};
    }
    state[count] = (struct TagItem){TAG_DONE, 0};
    struct TagItem *current = merge_tags(r->type, r->tags, state);
    if (!current) return FALSE;
    struct TagItem *merged = merge_tags(r->type, current, tags);
    free_tags(r->type, current);
    if (!merged) return FALSE;
    free_tags(r->type, r->tags); r->tags = merged;
    r->running = TRUE;
    BOOL accepted = FALSE;
    if (!panel_tags(r))
    {
        ULONG filter_tag = r->type == ASL_FileRequest ? ASLFR_FilterFunc : ASLFO_FilterFunc;
        struct NativeFilter filter = {0};
        filter.hook.h_Entry = (HOOKFUNC)native_filter;
        filter.request = r;
        filter.callback = (APTR)GetTagData(filter_tag, 0, r->tags);
        struct TagItem fallback[] = {
            {filter.callback ? filter_tag : TAG_IGNORE, (IPTR)&filter.hook},
            {TAG_MORE, (IPTR)r->tags}};
        accepted = AslRequest(r->native, fallback);
        native_result(r);
    }
    else
    {
        struct MsgPort *port = CreateMsgPort();
        if (port)
        {
            struct RequestJob job = {0};
            job.message.mn_ReplyPort = port;
            job.message.mn_Length = sizeof(job);
            job.request = r;
            struct TagItem process_tags[] = {
                {NP_Entry, (IPTR)requester_process},
                {NP_Name, (IPTR)"Zune requester"},
                {NP_StackSize, 65536}, {TAG_DONE, 0}};
            struct Process *process = CreateNewProc(process_tags);
            if (process)
            {
                PutMsg(&process->pr_MsgPort, &job.message);
                WaitPort(port); GetMsg(port);
                accepted = job.accepted;
            }
            DeleteMsgPort(port);
        }
    }
    r->running = FALSE;
    return accepted;
}

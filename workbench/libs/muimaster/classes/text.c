/*
    Copyright  2002-2026, The AROS Development Team.
    Copyright  1999, David Le Corfec.
    All rights reserved.

*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <exec/types.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "textengine.h"
#include "clipboard.h"
#include <devices/inputevent.h>

/****** Text.mui/MUIC_Text ************
*
*   NAME
*       MUIC_Text
*
*   SUPERCLASS
*       Area.mui
*
*   DESCRIPTION
*       Display text using the Zune text engine. Contents and PreParse
*       default to empty strings. SetMin and SetVMax default to TRUE; SetMax
*       defaults to FALSE. No editing, cursor or automatic line wrapping is
*       implemented by this class. Newline bytes separate text-engine lines.
*       Version and Revision return the library's built-in class values.
*       The object owns a copy of PreParse and, unless MUIA_Text_Copy is FALSE,
*       of Contents, and frees them on disposal. Setup creates and measures a
*       parsed text object; Cleanup destroys it and clears its pointer.
*       Complete Cleanup before disposal because Dispose frees the strings but
*       does not destroy the parsed text. Getters return borrowed pointers.
*
*   METHODS
*       Successful string setters copy the new string before releasing the
*       old one, update the parsed text while set up, and request a redraw of
*       the content background and text without the frame. They do not request
*       parent relayout. Copy failure leaves the old string and parsed text
*       unchanged, but the original tag still reaches superclass notification
*       processing. The setter returns the superclass result and does not
*       report copy failure separately.
*       Setup returns FALSE on Area failure, otherwise TRUE even if text-engine
*       allocation fails. Bounds measurement tolerates NULL, but AskMinMax and
*       Draw dereference the parsed object without checking it. Construction
*       fails if copying Contents or a nonempty PreParse fails.
*       Draw calls Area only when the object or its frame is to be drawn. It
*       clips to the content rectangle only when the text, plus room for any
*       bold or italic overhang, does not fit; wider text without overhang or
*       embedded objects is cut to whole characters instead. It centers the
*       parsed block vertically. Horizontal alignment comes from
*       the text engine. Update requests return zero unless the contents are
*       flagged for update, as setters do. Normal drawing returns TRUE and
*       clears that flag. Show converts the superclass result to BOOL; Hide
*       returns it unchanged. Import and Export are local no-ops returning zero
*       without delegating to Area.
*
*   NOTES
*       The public text macros map to escape-byte commands: MUIX_L, MUIX_C and
*       MUIX_R select left, center and right alignment; MUIX_N resets style;
*       MUIX_B, MUIX_I and MUIX_U add bold, italic and underline; MUIX_PT and
*       MUIX_PH select draw-info pen indices 2 and 8. PreParse is concatenated
*       directly before Contents, with no inserted separator. Alignment and
*       style state can continue across lines. Parsing is byte-oriented, not
*       Unicode character indexing.
*       HiChar and HiCharIdx only control underlining. They do not set
*       Area_ControlChar or register keyboard activation. Supply valid
*       text-engine escapes and keep any external objects or image data
*       referenced by pointer escapes alive; copying the string does not copy
*       those objects. Text bounds use WORD fields, so large strings or
*       dimensions are not an unlimited-size layout contract.
*
*/
/****** Text.mui/MUIA_Text_Contents ************
*
*   NAME
*       MUIA_Text_Contents
*
*   FUNCTION
*       Set or retrieve the displayed text string.
*
*   INPUTS
*       Initialize, set and get, STRPTR. Default empty string. Supply readable
*       NUL-terminated text; setting NULL is equivalent to setting an empty
*       string.
*
*   NOTES
*       The object copies the string unless MUIA_Text_Copy is FALSE, in which
*       case it borrows the pointer. Get returns the stored source text,
*       including escapes and marker characters, not a rendered-text buffer. Do
*       not free or modify the returned pointer; it expires on successful
*       replacement or disposal. Because replacement is copied first, the
*       current returned pointer can be supplied back to the setter.
*       Even identical text is copied; no equality suppression occurs. Plain
*       single-line text replacing plain text updates the parsed text in place;
*       other changes reparse it. A successful change while set up remeasures
*       the parsed text and requests redraw without relayout. On copy failure
*       the old value remains, yet the original tag is forwarded and can trigger
*       notifications. Do not treat a notification as proof of successful
*       replacement.
*       At construction, NULL Contents tags are ignored and only the last
*       non-NULL tag is copied; a failed copy fails construction. Newline bytes
*       create separate lines; text-engine escape sequences are interpreted.
*
*/
/****** Text.mui/MUIA_Text_PreParse ************
*
*   NAME
*       MUIA_Text_PreParse
*
*   FUNCTION
*       Set or retrieve text prepended to Contents before parsing.
*
*   INPUTS
*       Initialize, set and get, STRPTR. Default empty string. NULL becomes an
*       empty string on set; construction also falls back to an empty string.
*
*   NOTES
*       The string is always copied and owned. Get returns a borrowed
*       pointer valid until successful replacement or disposal. It normally
*       supplies formatting escapes, but ordinary characters are rendered too.
*       No separator is inserted before Contents, and highlighting also scans
*       ordinary bytes in this prefix.
*       Each successful set destroys the old parsed text, copies the replacement
*       into stored state, reparses while set up and requests redraw without
*       relayout. Allocation failure retains the old value but does not suppress
*       the forwarded notification tag. Construction copies only the last
*       PreParse tag. Supplying the current getter pointer to the setter is
*       safe because duplication precedes release.
*
*/
/****** Text.mui/MUIA_Text_HiChar ************
*
*   NAME
*       MUIA_Text_HiChar
*
*   FUNCTION
*       Underline the first matching character during text parsing.
*
*   INPUTS
*       Initialize only, char stored as TEXT. Default zero disables this mode.
*
*   NOTES
*       The text engine compares ordinary bytes against the supplied character
*       converted with ToLower and ToUpper, underlining the first match and then
*       disabling further matches. It scans the concatenated PreParse and
*       Contents, across lines. No local setter or getter exists, and this
*       attribute does not assign a keyboard shortcut.
*       HiChar and HiCharIdx share one character field but use separate flags.
*       If both flags are enabled, HiChar mode wins while using the last stored
*       character value. Use one mode per object to avoid this interaction.
*
*/
/****** Text.mui/MUIA_Text_HiCharIdx ************
*
*   NAME
*       MUIA_Text_HiCharIdx
*
*   FUNCTION
*       Use a marker byte to underline the following byte.
*
*   INPUTS
*       Initialize only, char stored as TEXT. Default zero disables this mode.
*       The value is a marker character, not a numeric position.
*
*   NOTES
*       At the first matching ordinary marker byte, the parser removes the
*       marker from rendered text and underlines the next byte. Later markers
*       remain ordinary text because highlighting is then disabled. Get of
*       Contents still returns the original marker-containing string. There is
*       no local setter or getter and no keyboard binding.
*       The parser advances once more after consuming the underlined byte,
*       skipping inspection of the following byte. A marker at end of input or
*       immediately before the final character can therefore advance beyond the
*       terminator. Use a marker followed by the character to underline and at
*       least one further ordinary byte. Do not place escape or newline bytes at
*       that skipped position. This limitation is in the current text engine,
*       not a positional-index feature.
*       HiChar takes precedence when its flag is also enabled, but both
*       attributes share the same stored character field. Use only one
*       highlighting mode.
*
*/
/****** Text.mui/MUIA_Text_SetMin ************
*
*   NAME
*       MUIA_Text_SetMin
*
*   FUNCTION
*       Choose whether text width contributes to minimum width.
*
*   INPUTS
*       Initialize only, BOOL. Default TRUE. No local setter or getter.
*
*   NOTES
*       When TRUE and Shorten is not set, AskMinMax adds measured text width to
*       the superclass minimum width, and text width plus the frame and inner
*       spacing once more to the default width. Otherwise the minimum keeps
*       only the superclass contribution and the default adds the width of
*       three W characters. Height constraints are unaffected. This permits a
*       narrower allocation; it does not enable line wrapping.
*
*/
/****** Text.mui/MUIA_Text_SetMax ************
*
*   NAME
*       MUIA_Text_SetMax
*
*   FUNCTION
*       Choose whether maximum width follows measured text width.
*
*   INPUTS
*       Initialize only, BOOL. Default FALSE. No local setter or getter.
*
*   NOTES
*       TRUE retains the superclass maximum width plus measured text width.
*       FALSE adds MUI_MAXMAX to the superclass maximum width, allowing
*       horizontal expansion. Minimum and default widths still follow their
*       separate rules. This controls size negotiation, not string length or
*       allocation size.
*
*/
/****** Text.mui/MUIA_Text_SetVMax ************
*
*   NAME
*       MUIA_Text_SetVMax
*
*   FUNCTION
*       Choose whether maximum height follows measured text height.
*
*   INPUTS
*       Initialize and set, BOOL. Default TRUE. No local getter.
*
*   NOTES
*       A set value applies at the next layout; the setter requests none.
*       AskMinMax uses the greater of parsed text height and font YSize for
*       minimum and default height. TRUE adds that same height to the superclass
*       maximum. FALSE adds MUI_MAXMAX to the superclass maximum rather than
*       replacing it. Width constraints are unaffected. Drawing centers the
*       parsed text block vertically within the actual content rectangle.
*
*/

extern struct Library *MUIMasterBase;

//#define MYDEBUG 1
#include "debug.h"


struct TextMark
{
    struct MUI_EventHandlerNode handler;
    LONG anchor, end;
    BOOL dragging, focused;
};

struct MUI_TextData
{
    ULONG mtd_Flags;
    STRPTR contents;
    ULONG contents_size;    /* allocated bytes of contents */
    CONST_STRPTR preparse;
    TEXT hichar;
    ZText *ztext;
    ZTextChunk *plain_chunk; /* the one chunk of a ztext of plain contents */

    LONG update;        /* type of update: 1 - everything,
                         * 2 - insert char, no scroll */
    STRPTR owned_contents;
    BOOL copy_contents;
    LONG shorten;
    TEXT controlchar;
    Object *textdata;
    struct TextMark *mark;
};

#define MTDF_SETMIN    (1<<0)
#define MTDF_SETMAX    (1<<1)
#define MTDF_SETVMAX   (1<<2)
#define MTDF_HICHAR    (1<<3)
#define MTDF_HICHARIDX (1<<4)


#include "native_version.h"

/*
 * Create owned parsed text from PreParse and Contents, or the Text_Data
 * conversion when set, selecting HiChar before HiCharIdx, then measure
 * bounds. Requires live font/render context and no
 * unreleased previous parsed object. Stores NULL on allocation failure
 * without reporting status; later sizing/drawing assume non-NULL. Caller
 * owns lifecycle and must destroy parsed text.
 */
static void setup_text(struct MUI_TextData *data, Object *obj);

/* The length of contents that parse to themselves alone (no escapes,
 * one line), or -1 */
static LONG plain_length(CONST_STRPTR s)
{
    CONST_STRPTR c = s;

    for (; *c; c++)
        if ((UBYTE)*c < 32 && (*c == '\33' || *c == '\n' || *c == '\t'))
            return -1;
    return c == s ? -1 : c - s;
}

/* Copy new contents, reusing the buffer when the text fits. Status
 * lines are set repeatedly with similar lengths, so round the size up.
 * The source may point into the current buffer. */
static BOOL store_contents_len(struct MUI_TextData *data, CONST_STRPTR src,
    ULONG len)
{
    STRPTR buffer;
    ULONG size;
    if (!data->copy_contents)
    {
        if (!data->owned_contents || (IPTR)src < (IPTR)data->owned_contents ||
            (IPTR)src >= (IPTR)data->owned_contents + data->contents_size)
        {
            FreeVec(data->owned_contents);
            data->owned_contents = NULL;
            data->contents_size = 0;
        }
        data->contents = (STRPTR)src;
        return TRUE;
    }
    len++;
    if (data->owned_contents && len <= data->contents_size)
    {
        STRPTR dest = data->owned_contents;
        while ((*dest++ = *src++)) ;
        data->contents = data->owned_contents;
        return TRUE;
    }
    size = (len + 31) & ~31UL;
    buffer = AllocVec(size, MEMF_ANY);
    if (!buffer) return FALSE;
    memcpy(buffer, src, len);
    FreeVec(data->owned_contents);
    data->contents = data->owned_contents = buffer;
    data->contents_size = size;
    return TRUE;
}

static BOOL store_contents(struct MUI_TextData *data, CONST_STRPTR src)
{
    return store_contents_len(data, src, strlen(src));
}

/**************************************************************************
OM_NEW
**************************************************************************/
/*
 * Construct Area and store flags, then copy the last Contents tag (unless
 * Copy is FALSE) and last nonempty PreParse tag once. Dispose partial object
 * and return zero if a copy or the marking state cannot be allocated.
 * Otherwise return object; no local notifications or parsing yet.
 */
IPTR Text__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_TextData *data;
    struct TagItem *tags, *tag;
    CONST_STRPTR initial_contents = "", initial_preparse = NULL;
    BOOL marking = FALSE;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
        return FALSE;

    data = INST_DATA(cl, obj);
    data->mtd_Flags = MTDF_SETMIN | MTDF_SETVMAX;
    data->copy_contents = TRUE;

    /* parse initial taglist */

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Text_Contents:
            if (tag->ti_Data) initial_contents = (APTR)tag->ti_Data;
            break;

        case MUIA_Text_ControlChar:
            data->controlchar = tag->ti_Data;
            /* The text engine's shortcut chunks preserve surrounding styles. */
            /* fall through */
        case MUIA_Text_HiChar:
            data->hichar = tag->ti_Data;
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_HICHAR);
            break;

        case MUIA_Text_HiCharIdx:
            data->hichar = tag->ti_Data;
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_HICHARIDX);
            break;

        case MUIA_Text_PreParse:
            initial_preparse = (APTR)tag->ti_Data;
            break;

        case MUIA_Text_Copy: data->copy_contents = !!tag->ti_Data; break;
        case MUIA_Text_Shorten: data->shorten = tag->ti_Data; break;
        case MUIA_Text_Data: data->textdata = (APTR)tag->ti_Data; break;
        case MUIA_Text_Marking: marking = !!tag->ti_Data; break;

        case MUIA_Text_SetMin:
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_SETMIN);
            break;

        case MUIA_Text_SetMax:
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_SETMAX);
            break;

        case MUIA_Text_SetVMax:
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_SETVMAX);
            break;

        }
    }

    if (initial_preparse && *initial_preparse)
        data->preparse = StrDup(initial_preparse);
    if (!store_contents(data, initial_contents) ||
        (initial_preparse && *initial_preparse && !data->preparse))
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }
    if (marking)
    {
        data->mark = AllocVec(sizeof(*data->mark), MEMF_CLEAR);
        if (!data->mark) { CoerceMethod(cl, obj, OM_DISPOSE); return 0; }
        data->mark->handler.ehn_Events = IDCMP_MOUSEBUTTONS |
            IDCMP_MOUSEMOVE | IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW;
        data->mark->handler.ehn_Priority = 1;
        data->mark->handler.ehn_Flags = MUI_EHF_GUIMODE;
        data->mark->handler.ehn_Object = obj;
        data->mark->handler.ehn_Class = cl;
    }

    return (IPTR) obj;
}

/**************************************************************************
OM_DISPOSE
**************************************************************************/
/*
 * Free owned Contents and PreParse strings, marking state and any Text_Data
 * object, and return Area disposal result. Requires prior Cleanup for parsed
 * text and no active consumers of getter pointers. Does not destroy ztext
 * itself.
 */
IPTR Text__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);

    FreeVec(data->mark);
    FreeVec(data->owned_contents);
    FreeVec((APTR) data->preparse);
    if (data->textdata) MUI_DisposeObject(data->textdata);

    return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
OM_SET
**************************************************************************/
/*
 * Copy each replacement before releasing prior string and parsed text; NULL
 * means empty. Reparse if set up and request a contents redraw; only
 * Text_Data and Shorten changes also relayout the parent. Failed duplication
 * retains old state. Forward original tags and return Area result, with no
 * separate allocation status.
 */
IPTR Text__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *tag;
    BOOL relayout = FALSE;

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        if (data->mark && (tag->ti_Tag == MUIA_Text_Data ||
            tag->ti_Tag == MUIA_Text_Contents || tag->ti_Tag == MUIA_Text_PreParse ||
            tag->ti_Tag == MUIA_Text_ControlChar))
        {
            data->mark->anchor = data->mark->end = 0;
            data->mark->dragging = FALSE;
        }
        switch (tag->ti_Tag)
        {
        case MUIA_Text_Data:
            /* Replacing returns ownership of the previous object. */
            data->textdata = (Object *)tag->ti_Data;
            if (data->ztext) { zune_text_destroy(data->ztext); data->ztext = NULL; }
            data->plain_chunk = NULL;
            if (_flags(obj) & MADF_SETUP) setup_text(data, obj);
            relayout = TRUE;
            data->update = 1;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;
        case MUIA_Text_Copy:
            data->copy_contents = !!tag->ti_Data;
            break;
        case MUIA_Text_Shorten:
            relayout = TRUE;
            data->shorten = MIN(tag->ti_Data, MUIV_Text_Shorten_ElideRight);
            data->update = 1;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;
        case MUIA_Text_ControlChar:
            data->controlchar = data->hichar = tag->ti_Data;
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_HICHAR);
            if (data->ztext) { zune_text_destroy(data->ztext); data->ztext = NULL; }
            data->plain_chunk = NULL;
            if (_flags(obj) & MADF_SETUP) setup_text(data, obj);
            data->update = 1;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;
        case MUIA_Text_Contents:
            {
                CONST_STRPTR src = ((char *)tag->ti_Data) ?
                    (char *)tag->ti_Data : "";
                LONG len;

                /* Plain status text keeps its parsed form and gets the
                 * new string, which needs no parsing either */
                if (!data->textdata && data->plain_chunk && (len = plain_length(src)) > 0
                    && store_contents_len(data, src, len))
                {
                    zune_text_set_plain(data->ztext, data->plain_chunk,
                        data->contents, len, obj);
                    data->update = 1;
                    MUI_Redraw(obj, MADF_DRAWUPDATE);
                }
                else if (store_contents(data, src))
                {
                    if (data->ztext)
                    {
                        zune_text_destroy(data->ztext);
                        data->ztext = NULL;
                        data->plain_chunk = NULL;
                    }
                    if (_flags(obj) & MADF_SETUP)
                        setup_text(data, obj);
                    /* Repaint only the contents; frame and outer
                     * background are unchanged, as in MUI 3.8/3.9 */
                    data->update = 1;
                    MUI_Redraw(obj, MADF_DRAWUPDATE);
                }
            }
            break;

        case MUIA_Text_SetVMax:
            /* Documented as init-only, but MUI 3.9 takes it from SetAttrs
             * for the next layout: IBrowse sets it FALSE on its Add and
             * Search buttons so they fill the Location row's height. */
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_SETVMAX);
            break;

        case MUIA_Text_PreParse:
            {
                char *new_preparse =
                    StrDup(((char *)tag->ti_Data) ? (char *)tag->
                    ti_Data : "");
                if (new_preparse)
                {
                    if (data->ztext)
                    {
                        zune_text_destroy(data->ztext);
                        data->ztext = NULL;
                        data->plain_chunk = NULL;
                    }
                    FreeVec((APTR) data->preparse);
                    data->preparse = new_preparse;
                    if (_flags(obj) & MADF_SETUP)
                        setup_text(data, obj);
                    /* Repaint only the contents; frame and outer
                     * background are unchanged, as in MUI 3.8/3.9 */
                    data->update = 1;
                    MUI_Redraw(obj, MADF_DRAWUPDATE);
                }
            }
            break;

        case MUIA_Selected:
            D(bug("Text_Set(%p) : MUIA_Selected val=%ld sss=%d\n", obj,
                    tag->ti_Data, ! !(_flags(obj) & MADF_SHOWSELSTATE)));
            break;
        }
    }

    IPTR result = DoSuperMethodA(cl, obj, (Msg)msg);
    if (relayout && (_flags(obj) & MADF_SETUP) && _parent(obj))
    {
        DoMethod(_parent(obj), MUIM_Group_InitChange);
        DoMethod(_parent(obj), MUIM_Group_ExitChange);
    }
    return result;
}


/**************************************************************************
OM_GET
**************************************************************************/
/*
 * Write borrowed Contents/PreParse pointer, other local state or the
 * built-in class version/revision to valid caller-owned IPTR storage and
 * return TRUE. Delegate other
 * attributes. Returned strings expire on replacement/disposal and transfer
 * no ownership.
 */
IPTR Text__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);

#define STORE *(msg->opg_Storage)
    switch (msg->opg_AttrID)
    {
    case MUIA_Text_Marking: STORE = data->mark != NULL; return TRUE;
    case MUIA_Text_Data: STORE = (IPTR)data->textdata; return TRUE;
    case MUIA_Text_Copy: STORE = data->copy_contents; return TRUE;
    case MUIA_Text_ControlChar: STORE = data->controlchar; return TRUE;
    case MUIA_Text_Shorten: STORE = data->shorten; return TRUE;
    case MUIA_Text_Shortened:
        STORE = data->shorten && data->ztext &&
            data->ztext->width > _mwidth(obj);
        return TRUE;
    case MUIA_Text_Contents:
        STORE = (IPTR) data->contents;
        return TRUE;

    case MUIA_Text_PreParse:
        STORE = (IPTR)(data->preparse ? data->preparse : (CONST_STRPTR)"");
        return TRUE;

    case MUIA_Version:
        STORE = ZUNE68_BUILTIN_VERSION;
        return TRUE;

    case MUIA_Revision:
        STORE = ZUNE68_BUILTIN_REVISION;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg) msg);
#undef STORE
}

/**************************************************************************
...
**************************************************************************/
/*
 * Create owned parsed text from PreParse and Contents, or the Text_Data
 * conversion when set, selecting HiChar before HiCharIdx, then measure
 * bounds. Requires live font/render context and no
 * unreleased previous parsed object. Stores NULL on allocation failure
 * without reporting status; later sizing/drawing assume non-NULL. Caller
 * owns lifecycle and must destroy parsed text.
 */
static void setup_text(struct MUI_TextData *data, Object *obj)
{
    CONST_STRPTR contents = data->contents;
    if (data->textdata)
    {
        contents = (CONST_STRPTR)DoMethod(data->textdata,
            MUIM_Textdata_Convert, 0);
        if (!contents) contents = "";
    }
    if (data->mtd_Flags & MTDF_HICHAR)
    {
        data->ztext = zune_text_new(data->preparse, contents,
            ZTEXT_ARG_HICHAR, data->hichar);
    }
    else if (data->mtd_Flags & MTDF_HICHARIDX)
    {
        data->ztext = zune_text_new(data->preparse, contents,
            ZTEXT_ARG_HICHARIDX, data->hichar);
    }
    else
    {
        data->ztext = zune_text_new(data->preparse, contents,
            ZTEXT_ARG_NONE, 0);
    }
    zune_text_get_bounds(data->ztext, obj);
    data->plain_chunk = (data->mtd_Flags & (MTDF_HICHAR | MTDF_HICHARIDX))
        ? NULL : zune_text_plain_chunk(data->ztext, contents);

/*      D(bug("muimaster.library/text.c: ZText of 0x%p at 0x%p\n", */
/*          obj, data->ztext)); */
}

/**************************************************************************
MUIM_Setup
**************************************************************************/
/*
 * Call Area setup and return FALSE on failure; otherwise create/measure
 * parsed text and return TRUE even if parsing allocation failed. Requires
 * live font/context and paired Cleanup; no previous unreleased parsed
 * object.
 */
IPTR Text__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);

    if (!(DoSuperMethodA(cl, obj, (Msg) msg)))
        return FALSE;

    setup_text(data, obj);
    if (data->mark)
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->mark->handler);

    return TRUE;
}

/**************************************************************************
MUIM_Cleanup
**************************************************************************/
/*
 * Destroy non-NULL parsed text, clear pointer, then return Area Cleanup
 * result. Requires completed drawing and matched lifecycle. Stored strings
 * remain owned until replacement/disposal.
 */
IPTR Text__MUIM_Cleanup(struct IClass *cl, Object *obj,
    struct MUIP_Cleanup *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);

    if (data->mark)
    {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->mark->handler);
        data->mark->anchor = data->mark->end = 0;
        data->mark->dragging = data->mark->focused = FALSE;
    }
    if (data->ztext)
    {
        zune_text_destroy(data->ztext);
        data->ztext = NULL;
        data->plain_chunk = NULL;
    }

    return (DoSuperMethodA(cl, obj, (Msg) msg));
}

/**************************************************************************
MUIM_Show
**************************************************************************/
/*
 * Call Area Show and normalize its result to TRUE or FALSE. No local
 * resource acquisition or drawing; requires normal setup/show lifecycle.
 */
IPTR Text__MUIM_Show(struct IClass *cl, Object *obj,
    struct MUIP_Show *msg)
{
    //struct MUI_TextData *data = INST_DATA(cl, obj);

    if (!(DoSuperMethodA(cl, obj, (Msg) msg)))
        return FALSE;

    return TRUE;
}

/**************************************************************************
MUIM_Hide
**************************************************************************/
/*
 * Return Area Hide result unchanged. No local release or parsed-text
 * mutation; Cleanup remains responsible for parsed text.
 */
IPTR Text__MUIM_Hide(struct IClass *cl, Object *obj,
    struct MUIP_Hide *msg)
{
    //struct MUI_TextData *data = INST_DATA(cl, obj);
    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
MUIM_AskMinMax
**************************************************************************/
/*
 * Call Area, read measured text bounds and font height, then apply
 * SetMin/SetMax/SetVMax rules to writable size fields. Return TRUE ignoring
 * superclass result. Requires successful non-NULL parsed text; does not
 * parse or request relayout.
 */
IPTR Text__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    int height;
    struct MUI_TextData *data = INST_DATA(cl, obj);

    DoSuperMethodA(cl, obj, (Msg) msg);

    height = data->ztext->height;
    if (_font(obj)->tf_YSize > height)
        height = _font(obj)->tf_YSize;
/*      D(bug("YSize=%ld\n", _font(obj)->tf_YSize)); */


    /* Area supplied the frame and inner spacing. MUI 3.8/3.9 pad the
     * default width with that spacing once more; without a minimum, the
     * default content width is three 'W' glyphs. */
    if ((data->mtd_Flags & MTDF_SETMIN) && !data->shorten)
    {
        msg->MinMaxInfo->MinWidth += data->ztext->width;
        msg->MinMaxInfo->DefWidth += data->ztext->width + _subwidth(obj);
    }
    else
    {
        struct RastPort rp;

        InitRastPort(&rp);
        SetFont(&rp, _font(obj));
        msg->MinMaxInfo->DefWidth += TextLength(&rp, "WWW", 3);
    }

    if (data->mtd_Flags & MTDF_SETMAX)
        msg->MinMaxInfo->MaxWidth += data->ztext->width;
    else
        msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;

    msg->MinMaxInfo->MinHeight += height;
    msg->MinMaxInfo->DefHeight += height;

    if (!(data->mtd_Flags & MTDF_SETVMAX))
        msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;
    else
        msg->MinMaxInfo->MaxHeight += height;

    D(bug
        ("Text_AskMinMax 0x%p (%s): Min=%ldx%ld Max=%ldx%ld Def=%ldx%ld\n",
            obj, data->contents, msg->MinMaxInfo->MinWidth,
            msg->MinMaxInfo->MinHeight, msg->MinMaxInfo->MaxWidth,
            msg->MinMaxInfo->MaxHeight, msg->MinMaxInfo->DefWidth,
            msg->MinMaxInfo->DefHeight));

    return TRUE;
}

/**************************************************************************
MUIM_Draw
**************************************************************************/
/*
 * Call Area only for object or frame drawing; skip update requests when
 * the private update field is zero. Otherwise clear the content background
 * for setter updates, clip only text that does not fit, draw the parsed
 * block centered vertically, remove any clip and clear update. Return zero
 * on skip, TRUE after draw. Requires valid parsed text/window/render
 * context; clipping status is not checked.
 */
IPTR Text__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);
    APTR clip;
    WORD overhang;

/*    D(bug("muimaster.library/text.c: Draw Text Object at " */
/*        "0x%p %ldx%ldx%ldx%ld\n", obj, _left(obj), _top(obj), */
/*        _right(obj), _bottom(obj))); */

    /* Area's Draw only moves the draw flags into the message when
     * neither the object nor its frame is to be drawn; do that here and
     * save the super call for the contents updates below */
    {
        ULONG flags = _flags(obj) & MADF_DRAWFLAGS;

        if (flags & MADF_DRAWALL)
            flags |= MADF_DRAWOBJECT;
        if (flags & (MADF_DRAWOBJECT | MADF_DRAWFRAME))
            DoSuperMethodA(cl, obj, (Msg) msg);
        else
        {
            msg->flags = flags;
            _flags(obj) &= ~MADF_DRAWFLAGS;
        }
    }

    if ((msg->flags & MADF_DRAWUPDATE) && !data->update)
        return 0;

    if (!(msg->flags & MADF_DRAWUPDATE))
        data->update = 0;

    if (msg->flags & MADF_DRAWUPDATE && data->update == 1)
    {
        DoMethod(obj, MUIM_DrawBackground, _mleft(obj), _mtop(obj),
            _mwidth(obj), _mheight(obj), _mleft(obj), _mtop(obj),
            _flags(obj));
    }

    /* Installing a clip region rebuilds the layer's clip rectangles.
     * Text that fits needs no clipping; TextLength does not include the
     * overhang of algorithmic bold or italic, so leave room for that. */
    overhang = ((data->ztext->styles & (FSF_BOLD | FSF_ITALIC))
        || (_font(obj)->tf_Style & FSF_ITALIC)) ? 8 : 0;
    /* Like MUI, wider plain text is cut to whole characters, not
     * clipped */
    data->ztext->truncate = 1;
    if (data->ztext->height <= _mheight(obj)
        && (data->ztext->width + overhang <= _mwidth(obj)
            || (!overhang && !data->ztext->nontext)))
        clip = (APTR)-1;
    else
        clip = MUI_AddClipping(muiRenderInfo(obj), _mleft(obj),
            _mtop(obj), _mwidth(obj), _mheight(obj));

    SetAPen(_rp(obj), _pens(obj)[MPEN_TEXT]);

    {
        int y = (_mheight(obj) - data->ztext->height) / 2;
        if (data->shorten >= MUIV_Text_Shorten_Hide)
            zune_text_draw_shortened(data->ztext, obj, _mleft(obj),
                _mright(obj), _mtop(obj) + y, data->shorten);
        else
            zune_text_draw(data->ztext, obj,
                _mleft(obj), _mright(obj), _mtop(obj) + y);
    }

    if (data->mark && data->mark->anchor != data->mark->end)
        zune_text_mark(data->ztext, obj, data->shorten,
            MIN(data->mark->anchor, data->mark->end),
            MAX(data->mark->anchor, data->mark->end), 0, 0, TRUE);

    if (clip != (APTR)-1)
        MUI_RemoveClipping(muiRenderInfo(obj), clip);
    data->update = 0;
    return TRUE;
}

/**************************************************************************
MUIM_Export : to export an object's "contents" to a dataspace object.
**************************************************************************/
/*
 * Return zero without exporting data or delegating to Area. The historical
 * serialization body is excluded by #if 0. No allocation, ownership change
 * or dataspace access.
 */
IPTR Text__MUIM_Export(struct IClass *cl, Object *obj,
    struct MUIP_Export *msg)
{
    //struct MUI_TextData *data = INST_DATA(cl, obj);
    //STRPTR id;

#if 0
    if ((id = muiNotifyData(obj)->mnd_ObjectID))
    {
        DoMethod(msg->dataspace, MUIM_Dataspace_AddString,
            _U(id), _U("contents"), _U(data->contents));
    }
#endif
    return 0;
}


/**************************************************************************
MUIM_Import : to import an object's "contents" from a dataspace object.
**************************************************************************/
/*
 * Return zero without importing data or delegating to Area. The historical
 * deserialization body is excluded by #if 0. No allocation, ownership
 * change or dataspace access.
 */
IPTR Text__MUIM_Import(struct IClass *cl, Object *obj,
    struct MUIP_Import *msg)
{
    //STRPTR id;
    //STRPTR s;

#if 0
    if ((id = muiNotifyData(obj)->mnd_ObjectID))
    {
        if ((s = (STRPTR) DoMethod(msg->dataspace,
                    MUIM_Dataspace_FindString, _U(id), _U("contents"))))
        {
            set(obj, MUIA_Text_Contents, _U(s));
        }
    }
#endif
    return 0;
}


static IPTR Text_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);
    struct TextMark *mark = data->mark;
    struct IntuiMessage *imsg = msg->imsg;
    BOOL inside;
    if (!mark || !imsg || !(_flags(obj) & MADF_CANDRAW) ||
        muiAreaData(obj)->mad_DisableCount) return 0;
    inside = imsg->MouseX >= _mleft(obj) && imsg->MouseX <= _mright(obj) &&
        imsg->MouseY >= _mtop(obj) && imsg->MouseY <= _mbottom(obj);
    if (imsg->Class == IDCMP_INACTIVEWINDOW)
        mark->dragging = FALSE;
    else if (imsg->Class == IDCMP_MOUSEBUTTONS && imsg->Code == SELECTDOWN)
    {
        mark->focused = inside;
        mark->dragging = inside;
        if (inside) set(_win(obj), MUIA_Window_ActiveObject, obj);
        mark->anchor = mark->end = inside ? zune_text_mark(data->ztext,
            obj, data->shorten, 0, 0, imsg->MouseX, imsg->MouseY, FALSE) : 0;
        data->update = 1;
        MUI_Redraw(obj, MADF_DRAWUPDATE);
        return inside ? MUI_EventHandlerRC_Eat : 0;
    }
    else if (mark->dragging && (imsg->Class == IDCMP_MOUSEMOVE ||
        (imsg->Class == IDCMP_MOUSEBUTTONS && imsg->Code == SELECTUP)))
    {
        mark->end = zune_text_mark(data->ztext, obj, data->shorten, 0, 0,
            imsg->MouseX, imsg->MouseY, FALSE);
        if (imsg->Class == IDCMP_MOUSEBUTTONS) mark->dragging = FALSE;
        data->update = 1;
        MUI_Redraw(obj, MADF_DRAWUPDATE);
        return MUI_EventHandlerRC_Eat;
    }
    else if (XGET(_win(obj), MUIA_Window_ActiveObject) == (IPTR)obj &&
        mark->focused && imsg->Class == IDCMP_RAWKEY &&
        (imsg->Qualifier & IEQUALIFIER_RCOMMAND) && imsg->Code == 0x33)
    {
        STRPTR text = zune_text_copy(data->ztext,
            MIN(mark->anchor, mark->end), MAX(mark->anchor, mark->end));
        if (text) { clipboard_write_text(text, strlen(text)); FreeVec(text); }
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

/*
 * Dispatch Text overrides and return selected handler results; delegate all
 * other messages to Area. Requires ABI-correct message and live object
 * except construction. Ownership and lifecycle follow class contract.
 */
BOOPSI_DISPATCHER(IPTR, Text_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Text__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return Text__OM_DISPOSE(cl, obj, msg);
    case OM_SET:
        return Text__OM_SET(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return Text__OM_GET(cl, obj, (struct opGet *)msg);

    case MUIM_HandleEvent:
        return Text_HandleEvent(cl, obj, (APTR)msg);
    case MUIM_AskMinMax:
        return Text__MUIM_AskMinMax(cl, obj, (APTR) msg);
    case MUIM_Draw:
        return Text__MUIM_Draw(cl, obj, (APTR) msg);
    case MUIM_Setup:
        return Text__MUIM_Setup(cl, obj, (APTR) msg);
    case MUIM_Cleanup:
        return Text__MUIM_Cleanup(cl, obj, (APTR) msg);
    case MUIM_Show:
        return Text__MUIM_Show(cl, obj, (APTR) msg);
    case MUIM_Hide:
        return Text__MUIM_Hide(cl, obj, (APTR) msg);
    case MUIM_Export:
        return Text__MUIM_Export(cl, obj, (APTR) msg);
    case MUIM_Import:
        return Text__MUIM_Import(cl, obj, (APTR) msg);
    }

    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
* Class descriptor.
*/
const struct __MUIBuiltinClass _MUI_Text_desc =
{
    MUIC_Text,
    MUIC_Area,
    sizeof(struct MUI_TextData),
    (void *) Text_Dispatcher
};

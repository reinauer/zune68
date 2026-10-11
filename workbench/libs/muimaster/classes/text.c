/*
    Copyright  2002-2020, The AROS Development Team.
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

extern struct Library *MUIMasterBase;

//#define MYDEBUG 1
#include "debug.h"


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
};

#define MTDF_SETMIN    (1<<0)
#define MTDF_SETMAX    (1<<1)
#define MTDF_SETVMAX   (1<<2)
#define MTDF_HICHAR    (1<<3)
#define MTDF_HICHARIDX (1<<4)


#include "native_version.h"

static void setup_text(struct MUI_TextData *data, Object *obj);

/* The length of contents that parse to themselves alone (no escapes,
 * one line), or -1 */
static LONG plain_length(CONST_STRPTR s)
{
    CONST_STRPTR c = s;

    for (; *c; c++)
        if (*c == '\33' || *c == '\n')
            return -1;
    return c == s ? -1 : c - s;
}

/* Copy new contents, reusing the buffer when the text fits. Status
 * lines are set repeatedly with similar lengths, so round the size up.
 * The source may point into the current buffer. */
static BOOL store_contents_len(struct MUI_TextData *data, CONST_STRPTR src,
    ULONG len)
{
    ULONG size;
    STRPTR buf;

    len++;

    if (data->contents && len <= data->contents_size)
    {
        /* a byte loop beats memmove's CopyMem for status lengths; a
         * source inside the buffer itself lies at or after it */
        STRPTR d = data->contents;

        while ((*d++ = *src++))
            ;
        return TRUE;
    }
    size = (len + 31) & ~31UL;
    if (!(buf = AllocVec(size, MEMF_ANY)))
        return FALSE;
    memcpy(buf, src, len);
    FreeVec(data->contents);
    data->contents = buf;
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
IPTR Text__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_TextData *data;
    struct TagItem *tags, *tag;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
        return FALSE;

    data = INST_DATA(cl, obj);
    data->mtd_Flags = MTDF_SETMIN | MTDF_SETVMAX;

    /* parse initial taglist */

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Text_Contents:
            if (tag->ti_Data)
                store_contents(data, (STRPTR) tag->ti_Data);
            break;

        case MUIA_Text_HiChar:
            data->hichar = tag->ti_Data;
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_HICHAR);
            break;

        case MUIA_Text_HiCharIdx:
            data->hichar = tag->ti_Data;
            _handle_bool_tag(data->mtd_Flags, tag->ti_Data, MTDF_HICHARIDX);
            break;

        case MUIA_Text_PreParse:
            data->preparse = StrDup((STRPTR) tag->ti_Data);
            break;

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

    if (!data->preparse)
        data->preparse = StrDup("");
    if (!data->contents)
        store_contents(data, "");

    if (!data->contents || !data->preparse)
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return (IPTR) NULL;
    }

    return (IPTR) obj;
}

/**************************************************************************
OM_DISPOSE
**************************************************************************/
IPTR Text__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);

    FreeVec(data->contents);
    FreeVec((APTR) data->preparse);

    return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
OM_SET
**************************************************************************/
IPTR Text__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *tag;

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Text_Contents:
            {
                CONST_STRPTR src = ((char *)tag->ti_Data) ?
                    (char *)tag->ti_Data : "";
                LONG len;

                /* Plain status text keeps its parsed form and gets the
                 * new string, which needs no parsing either */
                if (data->plain_chunk && (len = plain_length(src)) > 0
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

    return DoSuperMethodA(cl, obj, (Msg) msg);
}


/**************************************************************************
OM_GET
**************************************************************************/
IPTR Text__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);

#define STORE *(msg->opg_Storage)
    switch (msg->opg_AttrID)
    {
    case MUIA_Text_Contents:
        STORE = (IPTR) data->contents;
        return TRUE;

    case MUIA_Text_PreParse:
        STORE = (IPTR) data->preparse;
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
static void setup_text(struct MUI_TextData *data, Object *obj)
{
    if (data->mtd_Flags & MTDF_HICHAR)
    {
        data->ztext = zune_text_new(data->preparse, data->contents,
            ZTEXT_ARG_HICHAR, data->hichar);
    }
    else if (data->mtd_Flags & MTDF_HICHARIDX)
    {
        data->ztext = zune_text_new(data->preparse, data->contents,
            ZTEXT_ARG_HICHARIDX, data->hichar);
    }
    else
    {
        data->ztext = zune_text_new(data->preparse, data->contents,
            ZTEXT_ARG_NONE, 0);
    }
    zune_text_get_bounds(data->ztext, obj);
    data->plain_chunk = (data->mtd_Flags & (MTDF_HICHAR | MTDF_HICHARIDX))
        ? NULL : zune_text_plain_chunk(data->ztext, data->contents);

/*      D(bug("muimaster.library/text.c: ZText of 0x%p at 0x%p\n", */
/*          obj, data->ztext)); */
}

/**************************************************************************
MUIM_Setup
**************************************************************************/
IPTR Text__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);

    if (!(DoSuperMethodA(cl, obj, (Msg) msg)))
        return FALSE;

    setup_text(data, obj);

    return TRUE;
}

/**************************************************************************
MUIM_Cleanup
**************************************************************************/
IPTR Text__MUIM_Cleanup(struct IClass *cl, Object *obj,
    struct MUIP_Cleanup *msg)
{
    struct MUI_TextData *data = INST_DATA(cl, obj);

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
IPTR Text__MUIM_Hide(struct IClass *cl, Object *obj,
    struct MUIP_Hide *msg)
{
    //struct MUI_TextData *data = INST_DATA(cl, obj);
    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
MUIM_AskMinMax
**************************************************************************/
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
    if (data->mtd_Flags & MTDF_SETMIN)
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
        zune_text_draw(data->ztext, obj,
            _mleft(obj), _mright(obj), _mtop(obj) + y);
    }

    if (clip != (APTR)-1)
        MUI_RemoveClipping(muiRenderInfo(obj), clip);
    data->update = 0;
    return TRUE;
}

/**************************************************************************
MUIM_Export : to export an object's "contents" to a dataspace object.
**************************************************************************/
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

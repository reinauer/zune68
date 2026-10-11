/*
    Copyright (C) 2002-2020, The AROS Development Team. All rights reserved.
*/

#define MUIMASTER_YES_INLINE_STDARG

#include <stdio.h>

#include <graphics/gfx.h>
#include <graphics/view.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include <string.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "textengine.h"
#include "font.h"
#include "support.h"
#include "support_classes.h"
#include "scale_private.h"

/*  #define MYDEBUG 1 */
#include "debug.h"

extern struct Library *MUIMasterBase;


IPTR Scale__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Scale_DATA *data;
    struct TagItem *tag, *tags;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
        return FALSE;

    data = INST_DATA(cl, obj);

    data->horiz = TRUE;

    /* parse initial taglist */

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Scale_Horiz:
            data->horiz = tag->ti_Data;
            break;
        }
    }

    /* MUI 3.9 labels a scale in the tiny font unless told otherwise */
    if (!FindTagItem(MUIA_Font, msg->ops_AttrList))
        set(obj, MUIA_Font, MUIV_Font_Tiny);

    D(bug("muimaster.library/scale.c: Scale Object created at 0x%p\n",
            obj));

    return (IPTR) obj;
}

IPTR Scale__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Scale_DATA *data;
    struct TagItem *tag, *tags;
    int need_redraw = 0;

    data = INST_DATA(cl, obj);

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Scale_Horiz:
            if (data->horiz != tag->ti_Data)
                need_redraw = 1;
            data->horiz = tag->ti_Data;
            break;
        }
    }

    if (need_redraw)
    {
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
    return DoSuperMethodA(cl, obj, (Msg) msg);
}

IPTR Scale__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct Scale_DATA *data = INST_DATA(cl, obj);
    IPTR *store = msg->opg_Storage;
    IPTR tag = msg->opg_AttrID;

    switch (tag)
    {
    case MUIA_Scale_Horiz:
        *store = (IPTR) data->horiz;
        return TRUE;
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

IPTR Scale__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct Scale_DATA *data = INST_DATA(cl, obj);

    if (!(DoSuperMethodA(cl, obj, (Msg) msg)))
        return 0;

    {
        char *minlabel = "0% 100%";
        struct RastPort rp;

        InitRastPort(&rp);
        SetFont(&rp, _font(obj));

        data->label_minwidth = TextLength(&rp, minlabel, strlen(minlabel));
        data->label_height = _font(obj)->tf_YSize;
    }

    return 1;
}

IPTR Scale__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct Scale_DATA *data = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg) msg);

    if (data->horiz)
    {
        msg->MinMaxInfo->MinWidth += data->label_minwidth;
        msg->MinMaxInfo->MinHeight += data->label_height + 4;
        msg->MinMaxInfo->DefWidth += data->label_minwidth;
        msg->MinMaxInfo->DefHeight += data->label_height + 4;
        msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
        msg->MinMaxInfo->MaxHeight += data->label_height + 4;
    }
    else
    {
        /* MUI 3.9 reserves this much for a vertical scale and draws
         * nothing into it (tests/native/widgetsizes.c, scaleview.c) */
        msg->MinMaxInfo->MinWidth += data->label_minwidth + 4;
        msg->MinMaxInfo->MinHeight += data->label_height * 2 + 2;
        msg->MinMaxInfo->DefWidth += data->label_minwidth + 4;
        msg->MinMaxInfo->DefHeight += data->label_height * 2 + 2;
        msg->MinMaxInfo->MaxWidth += data->label_minwidth + 4;
        msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    }
    return 0;
}

/* Sum of the label widths for 2^k intervals, in the tiny font MUI 3.9
 * uses for the decision whatever the scale's own font is */
static LONG scale_labels_width(struct RastPort *rp, int intervals)
{
    char buf[16];
    LONG total = 0;
    int k;

    for (k = 0; k <= intervals; k++)
    {
        snprintf(buf, sizeof(buf), "%d%%", k * 100 / intervals);
        total += TextLength(rp, buf, strlen(buf));
    }
    return total;
}

static void scale_label(Object *obj, CONST_STRPTR preparse, STRPTR text,
    LONG left)
{
    ZText *ztext = zune_text_new(preparse, text, ZTEXT_ARG_NONE, 0);

    if (ztext)
    {
        zune_text_get_bounds(ztext, obj);
        zune_text_draw(ztext, obj, left, _mright(obj), _mtop(obj) + 4);
        zune_text_destroy(ztext);
    }
}

/* MUI 3.9's horizontal scale (tests/native/scaleticks.c): a bar on the
 * fourth row, labelled major ticks three pixels high at n equal
 * intervals and two pixel minor ticks halfway between. n doubles while
 * the labels for the next n fit into 9/16 of the width. A vertical scale
 * only reserves its space. */
IPTR Scale__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct Scale_DATA *data = INST_DATA(cl, obj);
    struct RastPort *rp = _rp(obj), measure;
    struct TextFont *tiny;
    LONG width = _mwidth(obj), n = 1, k;
    char buf[16];

    DoSuperMethodA(cl, obj, (Msg) msg);
    if (!data->horiz || !(msg->flags & MADF_DRAWOBJECT) || width <= 0)
        return 0;

    InitRastPort(&measure);
    tiny = zune_font_get(obj, MUIV_Font_Tiny);
    SetFont(&measure, tiny ? tiny : _font(obj));
    while (2 * n < width
        && scale_labels_width(&measure, 2 * n) * 16 <= width * 9)
        n *= 2;

    SetAPen(rp, _pens(obj)[MPEN_TEXT]);
    Move(rp, _mleft(obj), _mtop(obj) + 3);
    Draw(rp, _mright(obj), _mtop(obj) + 3);
    for (k = 0; k <= 2 * n; k++)
    {
        LONG x = k == 2 * n ? _mright(obj) : _mleft(obj) + width * k / (2 * n);

        Move(rp, x, _mtop(obj) + (k & 1 ? 1 : 0));
        Draw(rp, x, _mtop(obj) + 2);
    }
    for (k = 0; k <= n; k++)
    {
        snprintf(buf, sizeof(buf), "%ld%%", (long)(k * 100 / n));
        if (k == 0)
            scale_label(obj, NULL, buf, _mleft(obj));
        else if (k == n)
            scale_label(obj, "\33r", buf, _mleft(obj));
        else
            scale_label(obj, NULL, buf, _mleft(obj) + width * k / n
                - TextLength(rp, buf, strlen(buf)) / 2);
        SetAPen(rp, _pens(obj)[MPEN_TEXT]);
    }
    return 0;
}

#if ZUNE_BUILTIN_SCALE
BOOPSI_DISPATCHER(IPTR, Scale_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Scale__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_SET:
        return Scale__OM_SET(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return Scale__OM_GET(cl, obj, (struct opGet *)msg);
    case MUIM_Setup:
        return Scale__MUIM_Setup(cl, obj, (struct MUIP_Setup *)msg);
    case MUIM_AskMinMax:
        return Scale__MUIM_AskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
    case MUIM_Draw:
        return Scale__MUIM_Draw(cl, obj, (struct MUIP_Draw *)msg);
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Scale_desc =
{
    MUIC_Scale,
    MUIC_Area,
    sizeof(struct Scale_DATA),
    (void *) Scale_Dispatcher
};
#endif /* ZUNE_BUILTIN_SCALE */

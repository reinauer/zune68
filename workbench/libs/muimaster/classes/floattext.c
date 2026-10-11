/*
    Copyright (C) 2002-2015, The AROS Development Team. All rights reserved.
*/

#define MUIMASTER_YES_INLINE_STDARG

#include <exec/memory.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <proto/graphics.h>

#include <string.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "area_macros.h"
#include "support_classes.h"
#include "floattext_private.h"

extern struct Library *MUIMasterBase;

// like strlen(), but \n ends string, too.
static long MyStrLen(const char *ptr)
{
    const char *start = ptr;

    while (*ptr && (*ptr != '\n'))
        ptr++;

    return (((long)ptr) - ((long)start));
}

#define FLOATTEXT_MARGIN 4

/*
 * Fits the next line of a paragraph into a pixel width like MUI 3.9
 * (tests/native/ftwidth.c): the line ends before the last space among the
 * characters that fit and the next line starts right after that space, so
 * a space following a word that fills a line exactly makes an empty line;
 * a word without such a space is split. Returns the number of characters
 * consumed and stores the length of the line itself in *line_count.
 */
static UWORD FitParagraphLine(STRPTR text, ULONG length, WORD width,
   ULONG offset, struct RastPort *rp, ULONG *line_count)
{
    struct TextExtent temp_extent;
    ULONG char_count, i;

    *line_count = 0;
    if (offset >= length || width <= 0)
        return 0;

    char_count = TextFit(rp, text + offset,
        MIN(length - offset, 65535), &temp_extent, NULL, 1, width, 32767);

    /* A glyph wider than the available space must still make progress. */
    if (char_count == 0)
        char_count = 1;

    if (offset + char_count < length)
    {
        for (i = char_count; i-- > 0;)
        {
            if (text[offset + i] == ' ')
            {
                *line_count = i;
                return i + 1;
            }
        }
    }

    *line_count = char_count;
    return char_count;
}

static void SetText(struct IClass *cl, Object *obj,
    struct Floattext_DATA *data)
{
    WORD width;
    struct Window *window;
    struct RastPort rp;
    ULONG i, count, pos, line_size = 0, space_count = 0, extra_space_count,
        space_width, full_passes = 0, partial_left = 0, line_len,
        stripped_pos,
        stripped_count, stripped_size = 0, control_count, old_control_count,
        tab_count, consumed;
    UBYTE *text, *p, *q, *r, c, *line = NULL, *old_line,
        *stripped_text = NULL;
    LONG len, stripped_len;
    BOOL justify, found, is_empty;

    /* Avoid redrawing list while inserting lines */

    data->typesetting = TRUE;

    DoMethod(obj, MUIM_List_Clear);

    window = (struct Window *)XGET(obj, MUIA_Window);
#if ZUNE_BUILTIN_FLOATTEXT
    /* Wrap to the entry area, which excludes the List's scroller, less
     * a margin: MUI 3.9 fills 26 pixels less than the Listview's width
     * (tests/native/ftwidth.c) */
    width = List_EntryAreaWidth(cl->cl_Super, obj);
    if (width > 0)
        width = MAX(width - FLOATTEXT_MARGIN, 1);
#else
    width = XGET(obj, MUIA_Width) - XGET(obj, MUIA_InnerLeft)
        - XGET(obj, MUIA_InnerRight) - 8;
#endif

    /* Only do layout if we have some text and a window in which to put it */

    if (data->text && window != NULL && width > 0)
    {
        /* Measure in the list's own font: the window's RastPort holds
         * whichever font was drawn last */
        InitRastPort(&rp);
        SetFont(&rp, _font(obj) ? _font(obj) : window->RPort->Font);

        /* Get width of a space character */

        space_width = TextLength(&rp, " ", 1);

        /* Lay out each paragraph */

        for (text = data->text; text[0] != '\0'; text += pos)
        {
            stripped_pos = pos = 0;
            len = MyStrLen(text);

            if (len > 0)
            {
                /* Allocate paragraph buffer (the buffer size assumes the
                 * worst-case scenario: every character is a tab) */

                if (stripped_text == NULL
                    || len * data->tabsize >= stripped_size)
                {
                    FreeVec(stripped_text);
                    stripped_size = len * data->tabsize + 1;
                    stripped_text = AllocVec(stripped_size, MEMF_ANY);
                }
                if (stripped_text == NULL)
                {
                    FreeVec(line);
                    DeinitRastPort(&rp);
                    data->typesetting = FALSE;
                    return;
                }

                /* Make a copy of paragraph text without control sequences
                 * or skip-chars, and with tabs expanded to spaces */

                for (p = text, q = stripped_text; *p != '\0' && *p != '\n'; )
                {
                    if (*p == '\33')
                    {
                        p++;
                        if (*p && *p != '\n') p++;
                    }
                    else if (*p == '\t')
                    {
                        for (i = 0; i < data->tabsize; i++)
                            *q++ = ' ';
                        p++;
                    }
                    else if (data->skipchars != NULL)
                    {
                        for (r = data->skipchars; *r != '\0' && *r != *p; r++);
                        if (*r == '\0')
                            *q++ = *p;
                        p++;
                    }
                    else
                        *q++ = *p++;
                }
                *q = '\0';
                stripped_len = MyStrLen(stripped_text);

                /* Reuse old line buffer, but don't carry over control codes
                 * into new paragraph */

                if (line != NULL)
                    line[0] = '\0';
                old_control_count = control_count = 0;

                /* Divide this paragraph into lines */

                while (pos < len && (consumed = FitParagraphLine(
                    stripped_text, stripped_len, width, stripped_pos, &rp,
                    &stripped_count)) != 0)
                {
                    /* Count number of characters for this line in original
                     * text */

                    old_control_count += control_count;
                    control_count = tab_count = 0;
                    for (i = 0, p = text + pos; p < text + len &&
                        (i < stripped_count || *p == '\33');
                        p++)
                    {
                        if (*p == '\33')
                        {
                            control_count++;
                            if (p + 1 < text + len)
                            {
                                control_count++;
                                p++;
                            }
                        }
                        else if (*p == '\t')
                        {
                            control_count++;
                            tab_count++;
                            i += data->tabsize;
                        }
                        else if (data->skipchars != NULL)
                        {
                            for (r = data->skipchars;
                                *r != '\0' && *r != *p; r++);
                            if (*r != '\0' || (*p == ' ' && i == 0))
                                control_count++;
                            else
                                i++;
                        }
                        else
                            i++;
                    }
                    /* Consume whole tabs even when TextFit splits their
                     * expanded spaces; never underflow the source offset. */
                    stripped_count = i;
                    count = p - (text + pos);

                    /* A line breaking at a tab keeps the tab, like
                     * MUI 3.9 */
                    if (consumed > stripped_count && p < text + len
                        && *p == '\t')
                    {
                        control_count++;
                        tab_count++;
                        stripped_count += data->tabsize;
                        count++;
                    }

                    /* Like MUI 3.9, justify every line of a paragraph but
                     * its last */

                    justify = data->justify && pos + count < len;
                    if (justify)
                    {
                        /* Count number of spaces in stripped line */

                        p = stripped_text + stripped_pos;
                        for (i = 0, space_count = 0; i < stripped_count; i++)
                        {
                            if (p[i] == ' ')
                                space_count++;
                        }
                        if (space_count >= tab_count * data->tabsize)
                            space_count -= tab_count * data->tabsize;
                        else
                            space_count = 0;

                        if (space_count == 0 || space_width == 0)
                            justify = FALSE;
                    }

                    extra_space_count = full_passes = partial_left = 0;
                    if (justify)
                    {
                        ULONG spaces = space_count;
                        LONG spare;

                        /* Find out how many extra spaces to insert for fully
                         * justified text */

                        spare = width - TextLength(&rp,
                            stripped_text + stripped_pos, stripped_count);
                        if (spare > 0)
                            extra_space_count = spare / space_width;

                        /* MUI 3.9 adds a space after every space of the
                         * line, left to right, in passes until the extra
                         * spaces run out: full passes double each gap and
                         * the last pass widens the leftmost ones
                         * (tests/native/ftwidth.c) */
                        partial_left = extra_space_count;
                        while (partial_left >= spaces)
                        {
                            partial_left -= spaces;
                            spaces <<= 1;
                            full_passes++;
                        }
                    }

                    /* Count number of characters in line that will be
                     * inserted into List object */

                    line_len = old_control_count + control_count
                        + stripped_count
                        + tab_count * (data->tabsize - 1) + extra_space_count;

                    /* Allocate line buffer (we allocate more space than
                     * necessary to reduce the need to reallocate if later
                     * lines are longer) */

                    old_line = line;
                    if (line == NULL || line_len >= line_size)
                    {
                        line_size = line_len * 2 + 1;
                        line = AllocVec(line_size, MEMF_ANY);
                    }
                    if (line == NULL)
                    {
                        FreeVec(old_line);
                        FreeVec(stripped_text);
                        DeinitRastPort(&rp);
                        data->typesetting = FALSE;
                        return;
                    }

                    /* Prefix new line with all control characters contained
                     * in previous line */

                    q = line;
                    if (old_line != NULL)
                    {
                        for (p = old_line; *p != '\0'; p++)
                        {
                            if (*p == '\33')
                            {
                                *q++ = *p++;
                                *q++ = *p;
                            }
                        }
                    }
                    if (old_line != line)
                        FreeVec(old_line);

                    /* Generate line to insert in List object */

                    is_empty = TRUE;
                    p = text + pos;
                    c = p[count];
                    p[count] = '\0';
                    while (*p != '\0')
                    {
                        if (*p == ' ' && justify)
                        {
                            /* Widen this run of spaces to justify the text */

                            ULONG run, size, add;

                            for (run = 0; p[run] == ' '; run++);
                            size = run << full_passes;
                            add = MIN(size, partial_left);
                            partial_left -= add;
                            for (i = 0; i < size + add; i++)
                                *q++ = ' ';
                            p += run;
                        }
                        else if (*p == '\t')
                        {
                            /* Expand tab to spaces */

                            for (i = 0; i < data->tabsize; i++)
                                *q++ = ' ';
                            p++;
                        }
                        else if (data->skipchars != NULL)
                        {
                            /* Filter out skip-chars */

                            for (r = data->skipchars;
                                *r != '\0' && *r != *p; r++);
                            if (*r == '\0' && !(*p == ' ' && is_empty))
                            {
                                *q++ = *p;
                                is_empty = FALSE;
                            }
                            p++;
                        }
                        else
                        {
                            /* No skip-chars, so direct copy */

                            *q++ = *p++;
                            is_empty = FALSE;
                        }
                    }
                    *q = '\0';

                    /* Add line to list */

                    DoMethod(obj, MUIM_List_InsertSingle, line,
                        MUIV_List_Insert_Bottom);
                    *p = c;

                    /* Move on past the space the line broke at (unless
                     * the line took a whole tab instead) */

                    stripped_pos += stripped_count;
                    pos += count;
                    if (consumed > stripped_count)
                    {
                        stripped_pos++;
                        for (found = FALSE; !found && pos < len; pos++)
                        {
                            p = text + pos;
                            if (*p == '\33')
                            {
                                if (pos + 1 < len)
                                    pos++;
                                continue;
                            }
                            for (r = data->skipchars;
                                r != NULL && *r != '\0' && *r != *p; r++);
                            if (r == NULL || *r == '\0' || *p == ' ')
                                found = TRUE;
                        }
                    }
                }
                /* Empty/filtered paragraphs still advance to the newline. */
                pos = len;
            }
            else
                DoMethod(obj, MUIM_List_InsertSingle, "",
                    MUIV_List_Insert_Bottom);

            if (text[pos] == '\n')
                pos++;
        }
        FreeVec(line);
        FreeVec(stripped_text);
        DeinitRastPort(&rp);
    }

    data->typesetting = FALSE;

    DoMethod(obj, MUIM_List_Redraw, MUIV_List_Redraw_All);
}

IPTR Floattext__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Floattext_DATA *data;
    struct TagItem *tag;
    struct TagItem *tags;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);

    if (!obj)
    {
        return 0;
    }

    data = INST_DATA(cl, obj);

    SetAttrs(obj, MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
        MUIA_List_DestructHook, MUIV_List_DestructHook_String, TAG_DONE);

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Floattext_Justify:
            data->justify = tag->ti_Data;
            break;

        case MUIA_Floattext_SkipChars:
            data->skipchars = (STRPTR) tag->ti_Data;
            break;

        case MUIA_Floattext_TabSize:
            data->tabsize = tag->ti_Data;
            break;

        case MUIA_Floattext_Text:
            data->text = StrDup((STRPTR) tag->ti_Data);
            break;
        }
    }

    if (data->tabsize == 0)
        data->tabsize = 8;
    else if (data->tabsize > 20)
        data->tabsize = 20;

    SetText(cl, obj, data);

    return (IPTR) obj;
}

IPTR Floattext__OM_DISPOSE(struct IClass *cl, Object *obj,
    struct opSet *msg)
{
    struct Floattext_DATA *data = INST_DATA(cl, obj);

    FreeVec(data->text);

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

IPTR Floattext__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct Floattext_DATA *data = INST_DATA(cl, obj);
#define STORE *(msg->opg_Storage)

    switch (msg->opg_AttrID)
    {
    case MUIA_Floattext_Justify:
        STORE = data->justify;
        return 1;

    case MUIA_Floattext_Text:
        STORE = (IPTR) data->text;
        return 1;

    }

#undef STORE

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

IPTR Floattext__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Floattext_DATA *data = INST_DATA(cl, obj);
    struct TagItem *tag;
    struct TagItem *tags;
    BOOL changed = FALSE;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Floattext_Justify:
            data->justify = tag->ti_Data != 0;
            changed = TRUE;
            break;

        case MUIA_Floattext_SkipChars:
            data->skipchars = (STRPTR) tag->ti_Data;
            changed = TRUE;
            break;

        case MUIA_Floattext_TabSize:
            data->tabsize = tag->ti_Data;
            changed = TRUE;
            break;

        case MUIA_Floattext_Text:
            {
                STRPTR replacement = StrDup((STRPTR) tag->ti_Data);
                /* Callers may pass the current text (or a substring). */
                if (replacement || !tag->ti_Data)
                {
                    FreeVec(data->text);
                    data->text = replacement;
                    changed = TRUE;
                }
            }
            break;

        }
    }

    if (changed)                // To avoid recursion
    {
        if (data->tabsize == 0)
            data->tabsize = 8;
        else if (data->tabsize > 20)
            data->tabsize = 20;

        SetText(cl, obj, data);
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_Draw
**************************************************************************/
IPTR Floattext__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct Floattext_DATA *data = INST_DATA(cl, obj);

    if (!data->typesetting)
        DoSuperMethodA(cl, obj, (Msg) msg);

    if ((msg->flags & MADF_DRAWOBJECT) != 0 && data->oldwidth != _width(obj))
    {
        data->oldwidth = _width(obj);
        SetText(cl, obj, data);
    }

    return 0;
}

/**************************************************************************
 MUIM_Floattext_Append
**************************************************************************/
IPTR Floattext__MUIM_Floattext_Append(struct IClass *cl, Object *obj,
    struct MUIP_Floattext_Append *msg)
{
    struct Floattext_DATA *data = INST_DATA(cl, obj);

    if (msg->Text)
    {
        ULONG newlen = strlen(msg->Text) + 1;
        TEXT *newtext;
        if (data->text)
        {
            newlen += strlen(data->text);
        }
        newtext = AllocVec(newlen, MEMF_ANY);
        if (newtext)
        {
            newtext[0] = '\0';
            if (data->text)
            {
                strcpy(newtext, data->text);
            }
            strcat(newtext, msg->Text);
            FreeVec(data->text);
            data->text = newtext;
            SetText(cl, obj, data);
        }
    }

    return 0;
}

#if ZUNE_BUILTIN_FLOATTEXT
BOOPSI_DISPATCHER(IPTR, Floattext_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Floattext__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return Floattext__OM_DISPOSE(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return Floattext__OM_GET(cl, obj, (struct opGet *)msg);
    case OM_SET:
        return Floattext__OM_SET(cl, obj, (struct opSet *)msg);
    case MUIM_Draw:
        return Floattext__MUIM_Draw(cl, obj, (struct MUIP_Draw *)msg);
    case MUIM_Floattext_Append:
        return Floattext__MUIM_Floattext_Append(cl, obj,
            (struct MUIP_Floattext_Append *)msg);

    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Floattext_desc =
{
    MUIC_Floattext,
    MUIC_List,
    sizeof(struct Floattext_DATA),
    (void *) Floattext_Dispatcher
};
#endif /* ZUNE_BUILTIN_FLOATTEXT */

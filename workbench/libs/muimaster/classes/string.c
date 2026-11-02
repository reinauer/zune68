/*
    Copyright (C) 2003-2023, The AROS Development Team. All rights reserved.
*/

/* This is based on muimaster/class/text.c (first string version)
 * and on rom/intuition/str*.c
 */

#define MUIMASTER_YES_INLINE_STDARG

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <exec/types.h>
#include <libraries/gadtools.h>
#include <intuition/sghooks.h>
#include <intuition/cghooks.h>
#include <clib/alib_protos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <proto/locale.h>

#ifdef __AROS__
#include <devices/rawkeycodes.h>
#endif

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "prefs.h"
#include "penspec.h"
#include "imspec.h"
#include "clipboard.h"
#include "string.h"

//#define MYDEBUG 1
#include "debug.h"

extern struct Library *MUIMasterBase;

struct MUI_StringData
{
    ULONG msd_Flags;
    CONST_STRPTR msd_Accept;    /* MUIA_String_Accept */
    CONST_STRPTR msd_Reject;    /* MUIA_String_Reject */
    LONG msd_Align;
    struct Hook *msd_EditHook;
    Object *msd_AttachedList;
    LONG msd_RedrawReason;

    ULONG msd_useSecret;
    /* Fields mostly ripped from rom/intuition/strgadgets.c */
    STRPTR Buffer;              /* char container                   */
    STRPTR SecBuffer;           /* Buffer for secret string         */
    STRPTR VisualBuffer;        /* Pointer to either Buffer or SecBuffer */
    ULONG BufferSize;           /* memory allocated                 */
    ULONG NumChars;             /* string length                    */
    ULONG BufferPos;            /* cursor (insert/delete) position  */
    ULONG MarkPos;              /* cursor text marking start pos    */
    LONG DispPos;               /* leftmost visible char            */
    ULONG DispCount;            /* number of visible chars          */

    ULONG OldClick_Sec;
    ULONG OldClick_Micro;
    ULONG NewClick_Sec;
    ULONG NewClick_Micro;
    WORD MultiClick;

    WORD Columns;

    struct MUI_EventHandlerNode ehn;
    struct MUI_PenSpec_intern inactive_text;
    struct MUI_PenSpec_intern active_text;
    struct MUI_PenSpec_intern marked_text;
    struct MUI_PenSpec_intern marked_bg;
    struct MUI_PenSpec_intern cursor;

    BOOL is_active;
    STRPTR AckBuffer;           /* pointer to buffer of MUIA_String_Acknowledge */
    Object *KeyUpFocus;
    Object *KeyDownFocus;
    CONST_STRPTR placeholder;
    long long integer64;
    Object *edit_menu;
    /* Textinput marks are inclusive and independent of the cursor */
    LONG TiMarkStart;
    LONG TiMarkEnd;
    ULONG TiPending;            /* TIP_* collected during input events */
    LONG TiMinVal;
    LONG TiMaxVal;
    UWORD TiWordWrap;
    UBYTE TiStyles;
    UBYTE TiFont;
    LONG MlTopLine;             /* Multiline: first visible line */
    LONG MlLeftCol;             /* Multiline: first visible column */
};

#define MSDF_ADVANCEONCR    (1<<0)
#define MSDF_LONELYEDITHOOK (1<<1)
#define MSDF_MARKING            (1<<2)
#define MSDF_KEYMARKING            (1<<3)
#define MSDF_NOINPUT            (1<<4)
#define MSDF_STAYACTIVE     (1<<5)
#define MSDF_TIMARK         (1<<6)  /* with MSDF_MARKING: Textinput mark */
#define MSDF_CHANGED        (1<<7)
#define MSDF_TINOINPUT      (1<<8)
#define MSDF_MULTILINE      (1<<9)
#define MSDF_RESETMARK      (1<<10)
#define MSDF_ISNUMERIC      (1<<11)
#define MSDF_INEVENT        (1<<12) /* key input is calling methods */
#define MSDF_SETMIN         (1<<13)
#define MSDF_SETMAX         (1<<14)
#define MSDF_SETVMIN        (1<<15)
#define MSDF_SETVMAX        (1<<16)

#define TIP_UPDATE 1
#define TIP_EDITED 2

#define TI_MARKED(data) (((data)->msd_Flags & (MSDF_MARKING | MSDF_TIMARK)) \
    == (MSDF_MARKING | MSDF_TIMARK))

enum
{
    NO_REASON = 0,
    WENT_ACTIVE = 1,
    WENT_INACTIVE,
    DO_CURSOR_LEFT = 3,
    DO_CURSOR_RIGHT,
    DO_DELETE = 5,
    DO_BACKSPACE,
    DO_ADDCHAR = 7,
    NEW_CONTENTS,
    MOVE_CURSOR,
    DO_UNKNOWN,
};

#define SECRET_CHAR '*'


/****** String.mui/MUIA_String_Accept ****************************************
*
*   NAME
*       MUIA_String_Accept -- (V4) [ISG], STRPTR
*
*   FUNCTION
*       A string containing the characters to be accepted by the string
*       gadget. If NULL is specified, all characters are accepted. The
*       character list is case sensitive.
*
*   NOTES
*       MUIA_String_Reject takes precedence over MUIA_String_Accept.
*
*       The specified string is not cached within the object.
*
*   SEE ALSO
*       MUIA_String_Reject
*
******************************************************************************
*
*/

/****** String.mui/MUIA_String_Reject ****************************************
*
*   NAME
*       MUIA_String_Reject -- (V4) [ISG], STRPTR
*
*   FUNCTION
*       A string containing the characters to be rejected by the string
*       gadget. If NULL is specified, no characters are rejected. The
*       character list is case sensitive.
*
*   NOTES
*       MUIA_String_Reject takes precedence over MUIA_String_Accept.
*
*       The specified string is not cached within the object.
*
*   SEE ALSO
*       MUIA_String_Accept
*
******************************************************************************
*
*/

/****** String.mui/MUIA_String_BufferPos *************************************
*
*   NAME
*       MUIA_String_BufferPos -- (V4) [ISG], LONG
*
*   FUNCTION
*       The cursor's position relative to the start of the string.
*
*   NOTES
*
******************************************************************************
*
*/

/**************************************************************************
 Buffer_Alloc
 Allocate memory for buffer
**************************************************************************/
static BOOL Buffer_Alloc(struct MUI_StringData *data)
{
    UWORD i;

    data->Buffer =
        (STRPTR) AllocVec(data->BufferSize * sizeof(char), MEMF_ANY);
    if (NULL == data->Buffer)
    {
        bug("[MUI:String] Buffer_Alloc: Failed to allocate %ld bytes for buffer1\n",
            data->BufferSize * sizeof(char));
        return FALSE;
    }
    if (data->msd_useSecret)
    {
        data->SecBuffer =
            (STRPTR) AllocVec(data->BufferSize * sizeof(char), MEMF_ANY);
        if (NULL == data->SecBuffer)
        {
            bug("[MUI:String] Buffer_Alloc: Failed to allocate %ld bytes for buffer2\n",
                data->BufferSize * sizeof(char));
            FreeVec(data->Buffer);
            data->Buffer = NULL;
            return FALSE;
        }

        /* Initialise maximum number of asterisks we'll need */
        for (i = 0; i < data->BufferSize - 1; i++)
            data->SecBuffer[i] = SECRET_CHAR;

        data->VisualBuffer = data->SecBuffer;
    }
    else
        data->VisualBuffer = data->Buffer;

    return TRUE;
}

static inline VOID PrepareVisualBuffer(struct MUI_StringData *data)
{
    if (data->msd_useSecret)
        data->VisualBuffer[data->NumChars] = '\0';
}

static inline VOID CleanVisualBuffer(struct MUI_StringData *data)
{
    if (data->msd_useSecret)
        data->VisualBuffer[data->NumChars] = SECRET_CHAR;
}

/**************************************************************************
 Buffer_SetNewContents
 Initialize buffer with a string, replace former content if any. Returns
 TRUE if value actually changed.
**************************************************************************/
static BOOL Buffer_SetNewContents(struct MUI_StringData *data,
    CONST_STRPTR str)
{
    if (NULL == data->Buffer)
        return FALSE;

    if (NULL == str)
    {
        data->Buffer[0] = 0;
        data->NumChars = 0;
    }
    else
    {
        if (!strcmp(data->Buffer, str))
            return FALSE;

        data->NumChars = strlen(str);
        if (data->NumChars >= data->BufferSize)
            data->NumChars = data->BufferSize - 1;

        strncpy(data->Buffer, str, data->BufferSize);
        data->Buffer[data->BufferSize - 1] = 0;
    }

    // avoid to BufferPos jumps to end of string if characters are inserted
    // in string
    if (data->BufferPos > data->NumChars)
        data->BufferPos = data->NumChars;
    data->DispPos = 0;
    data->MlTopLine = data->MlLeftCol = 0;
    data->AckBuffer = data->Buffer;
    return TRUE;
}

/**************************************************************************
 Multiline: lines end at '\n'. Columns are character counts.
**************************************************************************/
#define MULTILINE(data) ((data)->msd_Flags & MSDF_MULTILINE)

static LONG Ml_LineStart(struct MUI_StringData *data, LONG pos)
{
    while (pos > 0 && data->Buffer[pos - 1] != '\n')
        pos--;
    return pos;
}

static LONG Ml_LineEnd(struct MUI_StringData *data, LONG pos)
{
    while (pos < (LONG)data->NumChars && data->Buffer[pos] != '\n')
        pos++;
    return pos;
}

static LONG Ml_LineOf(struct MUI_StringData *data, LONG pos)
{
    LONG line = 0, i;

    for (i = 0; i < pos; i++)
        if (data->Buffer[i] == '\n')
            line++;
    return line;
}

/* Start of a line; lines past the end give the last line */
static LONG Ml_LinePos(struct MUI_StringData *data, LONG line)
{
    LONG pos = 0, end;

    while (line-- > 0 && (end = Ml_LineEnd(data, pos)) < (LONG)data->NumChars)
        pos = end + 1;
    return pos;
}

/* The same column delta lines away, or the nearest line's end */
static LONG Ml_Vertical(struct MUI_StringData *data, LONG pos, LONG delta)
{
    LONG start = Ml_LineStart(data, pos), column = pos - start, end;

    for (; delta < 0 && start > 0; delta++)
        start = Ml_LineStart(data, start - 1);
    for (; delta > 0; delta--)
    {
        end = Ml_LineEnd(data, start);
        if (end >= (LONG)data->NumChars)
            break;
        start = end + 1;
    }
    return MIN(start + column, Ml_LineEnd(data, start));
}

/**************************************************************************
 Buffer_AddChar
 Add a char on cursor position
**************************************************************************/
static BOOL Buffer_AddChar(struct MUI_StringData *data, unsigned char code)
{
    STRPTR dst;

    if (data->Buffer == NULL)
        return FALSE;

    if (data->NumChars + 1 >= data->BufferSize)
        return FALSE;

    dst = &data->Buffer[data->BufferPos + 1];

    memmove(dst, &data->Buffer[data->BufferPos],
        data->NumChars - data->BufferPos);

    dst[data->NumChars - data->BufferPos] = 0;
    dst[-1] = code;

    data->BufferPos++;
    data->NumChars++;
    return TRUE;
}

static LONG Buffer_GetWordStartIndex(struct MUI_StringData *data,
    LONG startindex)
{
    LONG index = startindex;

    while (index > 0)
    {
        if (data->Buffer[index - 1] == ' ')
        {
            break;
        }
        index--;
    }

    return index;
}

static LONG Buffer_GetWordEndIndex(struct MUI_StringData *data,
    LONG startindex)
{
    LONG index = startindex;

    while (index < data->NumChars)
    {
        if (data->Buffer[index] == ' ')
        {
            break;
        }
        index++;
    }

    return index;
}

static LONG Buffer_GetPrevWordIndex(struct MUI_StringData *data,
    LONG startindex)
{
    LONG index = startindex;

    while (index > 0)
    {
        index--;

        if ((index == 0) ||
            ((data->Buffer[index - 1] == ' ') &&
                (data->Buffer[index] != ' ')))
        {
            break;
        }

    }

    return index;
}

static LONG Buffer_GetSuccWordIndex(struct MUI_StringData *data,
    LONG startindex)
{
    LONG index = startindex;

    while (index < data->NumChars)
    {
        index++;

        if ((index == data->NumChars) ||
            ((data->Buffer[index - 1] == ' ') &&
                (data->Buffer[index] != ' ')))
        {
            break;
        }
    }

    return index;
}

static BOOL Buffer_GetMarkedRange(struct MUI_StringData *data, LONG *start,
    LONG *stop)
{
    LONG markstart = MIN(data->MarkPos, data->NumChars);
    LONG markstop = MIN(data->BufferPos, data->NumChars);

    if (TI_MARKED(data))
    {
        markstart = MIN(data->TiMarkStart, data->TiMarkEnd);
        markstop = MAX(data->TiMarkStart, data->TiMarkEnd);
        markstart = MIN(markstart, (LONG)data->NumChars);
        markstop = MIN(markstop + 1, (LONG)data->NumChars);
        if (markstart >= markstop)
            return FALSE;
        if (start)
            *start = markstart;
        if (stop)
            *stop = markstop;
        return TRUE;
    }

    markstart = MIN(markstart, data->NumChars);
    markstart = MAX(markstart, 0);

    markstop = MIN(markstop, data->NumChars);
    markstop = MAX(markstop, 0);

    if (markstart > markstop)
    {
        markstart ^= markstop;
        markstop ^= markstart;
        markstart ^= markstop;
    }

    switch (data->MultiClick)
    {
    case 0:
        /* char select */
        break;

    case 1:
        /* word select */
        markstart = Buffer_GetWordStartIndex(data, markstart);
        markstop = Buffer_GetWordEndIndex(data, markstop);
        break;

    default:
        /* select all */
        markstart = 0;
        markstop = data->NumChars;
        break;

    }

    if (markstart == markstop)
        return FALSE;

    if (start)
        *start = markstart;
    if (stop)
        *stop = markstop;

    //kprintf("Buffer_GetMarkedRange: returning %d .. %d\n",
    //    markstart, markstop);

    return TRUE;
}

/* BetterString's signed length is relative to the cursor, which stays
 * in place. There is no special select-all value: -1 selects one character
 * to the left. Clamp before adding so LONG_MIN/LONG_MAX cannot wrap.
 */
static void Buffer_Select(struct MUI_StringData *data, LONG length)
{
    ULONG cursor = MIN(data->BufferPos, data->NumChars);
    ULONG extent;

    data->BufferPos = cursor;
    if (length < 0)
    {
        extent = 0UL - (ULONG)length;
        data->MarkPos = cursor - MIN(extent, cursor);
    }
    else
    {
        extent = MIN((ULONG)length, data->NumChars - cursor);
        data->MarkPos = cursor + extent;
    }
    data->MultiClick = 0;
    data->msd_Flags &= ~(MSDF_MARKING | MSDF_KEYMARKING | MSDF_TIMARK);
    if (data->MarkPos != cursor) data->msd_Flags |= MSDF_MARKING;
    data->msd_RedrawReason = MOVE_CURSOR;
}

static BOOL Buffer_AnythingMarked(struct MUI_StringData *data)
{
    if (!(data->msd_Flags & MSDF_MARKING))
        return FALSE;

    return Buffer_GetMarkedRange(data, NULL, NULL);
}

static BOOL Buffer_KillMarked(struct MUI_StringData *data)
{
    LONG markstart = MIN(data->MarkPos, data->NumChars);
    LONG markstop = MIN(data->BufferPos, data->NumChars);
    LONG marklen;

    //kprintf("\nBuffer_KillMarked 1  markpos %d  bufferpos %d  numchars %d\n",
    //    markstart, markstop, data->NumChars);

    BOOL marked;

    if (!(data->msd_Flags & MSDF_MARKING))
        return FALSE;

    marked = Buffer_GetMarkedRange(data, &markstart, &markstop);
    data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);

    if (!marked)
        return FALSE;

    //kprintf("Buffer_KillMarked 2  markstart %d  markstop %d\n",
    //    markstart, markstop);

    if (markstart > markstop)
    {
        markstart ^= markstop;
        markstop ^= markstart;
        markstart ^= markstop;
    }

    marklen = markstop - markstart;

    //kprintf("Buffer_KillMarked: markstart %d  markstop %d\n",
    //    markstart, markstop);

    memmove(&data->Buffer[markstart],
        &data->Buffer[markstart + marklen],
        data->NumChars - markstart - marklen + 1);
    data->NumChars -= marklen;
    data->BufferPos = markstart;

    return TRUE;
}

/* Convert a mouse or keyboard selection into a Textinput mark, which
 * stays put when the cursor moves. */
static void Ti_FreezeMark(struct MUI_StringData *data)
{
    LONG start, stop;

    if (TI_MARKED(data))
        return;
    data->msd_Flags &= ~(MSDF_TIMARK | MSDF_KEYMARKING);
    if (!(data->msd_Flags & MSDF_MARKING))
        return;
    if (Buffer_GetMarkedRange(data, &start, &stop))
    {
        data->TiMarkStart = start;
        data->TiMarkEnd = stop - 1;
        data->msd_Flags |= MSDF_TIMARK;
    }
    else
        data->msd_Flags &= ~MSDF_MARKING;
}

/* Inclusive bounds as MUIA_Textinput_MarkStart/End report them */
static void Ti_GetMark(struct MUI_StringData *data, LONG *start, LONG *end)
{
    LONG markstart, markstop;

    if (TI_MARKED(data))
    {
        markstart = MIN(data->TiMarkStart, data->TiMarkEnd);
        markstop = MAX(data->TiMarkStart, data->TiMarkEnd);
        *start = MIN(markstart, (LONG)data->NumChars);
        *end = MIN(markstop, (LONG)data->NumChars);
    }
    else if ((data->msd_Flags & MSDF_MARKING)
        && Buffer_GetMarkedRange(data, &markstart, &markstop))
    {
        *start = markstart;
        *end = markstop - 1;
    }
    else
        *start = *end = -1;
}

/* A negative bound removes the mark. Setting one bound without a mark
 * marks that single character. Bounds may be given in either order. */
static void Ti_SetMark(struct MUI_StringData *data, BOOL end, LONG value)
{
    if (value < 0)
    {
        data->msd_Flags &= ~(MSDF_MARKING | MSDF_KEYMARKING | MSDF_TIMARK);
        return;
    }
    Ti_FreezeMark(data);
    if (!TI_MARKED(data))
    {
        data->TiMarkStart = data->TiMarkEnd = value;
        data->msd_Flags |= MSDF_MARKING | MSDF_TIMARK;
    }
    else if (end)
        data->TiMarkEnd = value;
    else
        data->TiMarkStart = value;
}

static void Buffer_SetSecret(struct MUI_StringData *data, BOOL secret)
{
    if (secret && !data->SecBuffer)
    {
        data->SecBuffer = AllocVec(data->BufferSize, MEMF_ANY);
        if (!data->SecBuffer)
            return;
        memset(data->SecBuffer, SECRET_CHAR, data->BufferSize - 1);
    }
    data->msd_useSecret = secret;
    data->VisualBuffer = secret ? data->SecBuffer : data->Buffer;
}

/* Insert up to len characters at pos; returns the number inserted */
static LONG Buffer_InsertText(struct MUI_StringData *data, ULONG pos,
    CONST_STRPTR text, LONG len)
{
    LONG count = 0;

    if (!text)
        return 0;
    while ((len < 0 || count < len) && text[count])
        count++;
    count = MIN(count, (LONG)(data->BufferSize - 1 - data->NumChars));
    if (count <= 0)
        return 0;
    pos = MIN(pos, data->NumChars);
    memmove(&data->Buffer[pos + count], &data->Buffer[pos],
        data->NumChars - pos + 1);
    memcpy(&data->Buffer[pos], text, count);
    data->NumChars += count;
    return count;
}

/* Remove the characters from start up to, not including, stop */
static BOOL Buffer_Delete(struct MUI_StringData *data, ULONG start,
    ULONG stop)
{
    stop = MIN(stop, data->NumChars);
    if (start >= stop)
        return FALSE;
    memmove(&data->Buffer[start], &data->Buffer[stop],
        data->NumChars - stop + 1);
    data->NumChars -= stop - start;
    if (data->BufferPos > stop)
        data->BufferPos -= stop - start;
    else if (data->BufferPos > start)
        data->BufferPos = start;
    return TRUE;
}

/* Keep conversion independent of the startup-free printf implementation. */
static void StringFormat64(char *buffer, long long value)
{
    char digits[20];
    unsigned long long magnitude = value;
    unsigned int count = 0;
    if (value < 0)
    {
        *buffer++ = '-';
        magnitude = 0ULL - magnitude;
    }
    do
    {
        digits[count++] = '0' + magnitude % 10;
        magnitude /= 10;
    } while (magnitude);
    while (count) *buffer++ = digits[--count];
    *buffer = 0;
}

static long long StringParse64(CONST_STRPTR text)
{
    unsigned long long value = 0;
    BOOL negative = FALSE;
    while (*text == ' ' || *text == '\t') text++;
    if (*text == '-' || *text == '+') negative = *text++ == '-';
    while (*text >= '0' && *text <= '9')
    {
        unsigned digit = *text++ - '0';
        unsigned long long limit = negative ? 0x8000000000000000ULL :
            0x7fffffffffffffffULL;
        if (value > 922337203685477580ULL ||
            (value == 922337203685477580ULL && digit > (negative ? 8 : 7)))
        {
            value = limit;
            break;
        }
        value = value * 10 + digit;
    }
    return negative ? 0ULL - value : value;
}

/* Textinput attributes that are stored the same way for OM_NEW and
 * OM_SET. Returns TRUE if the display changes. */
static BOOL String_SetTextinput(struct MUI_StringData *data,
    struct TagItem *tag)
{
    switch (tag->ti_Tag)
    {
    case MUIA_Textinput_MarkStart:
    case MUIA_Textinput_MarkEnd:
        Ti_SetMark(data, tag->ti_Tag == MUIA_Textinput_MarkEnd,
            (LONG)tag->ti_Data);
        return TRUE;
    case MUIA_Textinput_Changed:
        _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_CHANGED);
        break;
    case MUIA_Textinput_ResetMarkOnCursor:
        _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_RESETMARK);
        break;
    case MUIA_Textinput_IsNumeric:
        _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_ISNUMERIC);
        break;
    case MUIA_Textinput_MinVal:
        data->TiMinVal = (LONG)tag->ti_Data;
        break;
    case MUIA_Textinput_MaxVal:
        data->TiMaxVal = (LONG)tag->ti_Data;
        break;
    case MUIA_Textinput_WordWrap:
        data->TiWordWrap = (UWORD)tag->ti_Data;
        break;
    case MUIA_Textinput_Styles:
        data->TiStyles = (UBYTE)tag->ti_Data;
        break;
    case MUIA_Textinput_SetMin:
        _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_SETMIN);
        break;
    case MUIA_Textinput_SetMax:
        _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_SETMAX);
        break;
    case MUIA_Textinput_SetVMin:
        _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_SETVMIN);
        break;
    case MUIA_Textinput_SetVMax:
        _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_SETVMAX);
        break;
    }
    return FALSE;
}

/**************************************************************************
 OM_NEW
**************************************************************************/
IPTR String__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_StringData *data;
    struct TagItem *tags, *tag;
    CONST_STRPTR str = NULL;
    char integerbuf[22];
    BOOL fixed = GetTagData(MUIA_Textinput_Font, MUIV_Textinput_Font_Normal,
        msg->ops_AttrList) == MUIV_Textinput_Font_Fixed;

    obj = (Object *) DoSuperNewTags(cl, obj, NULL,
        MUIA_PointerType, MUIV_PointerType_Text,
        MUIA_ContextMenu, 1,
        fixed ? MUIA_Font : TAG_IGNORE, MUIV_Font_Fixed,
        TAG_MORE, (IPTR) msg->ops_AttrList);
    if (!obj)
        return FALSE;

    data = INST_DATA(cl, obj);
    data->msd_useSecret = FALSE;
    data->msd_Align = MUIV_String_Format_Left;
    data->BufferSize = 80;
    data->Columns = -1;
    data->TiWordWrap = 70;
    data->TiFont = fixed;

    Buffer_SetNewContents(data, "");    /* <-- isn't this pointless? */

    /* parse initial taglist */
    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_String_Placeholder:
        case MUIA_String_InactiveContents:
            data->placeholder = (CONST_STRPTR)tag->ti_Data;
            break;

        case MUIA_Textinput_AcceptChars:
        case MUIA_String_Accept:
            data->msd_Accept = (CONST_STRPTR) tag->ti_Data;
            break;

        case MUIA_Textinput_RejectChars:
        case MUIA_String_Reject:
            data->msd_Reject = (CONST_STRPTR) tag->ti_Data;
            break;

        case MUIA_Textinput_AdvanceOnCR:
        case MUIA_String_AdvanceOnCR:
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_ADVANCEONCR);
            break;

        case MUIA_Textinput_CursorPos:
        case MUIA_String_BufferPos:
            data->BufferPos = (ULONG) tag->ti_Data;
            break;

        case MUIA_Textinput_AttachedList:
        case MUIA_String_AttachedList:
            data->msd_AttachedList = (Object *) tag->ti_Data;
            break;

        case MUIA_Textinput_Secret:
        case MUIA_String_Secret:
            data->msd_useSecret = (BOOL) tag->ti_Data;
            break;

        case MUIA_Textinput_Contents:
        case MUIA_String_Contents:
            str = (CONST_STRPTR) tag->ti_Data;
            break;

        case MUIA_Textinput_Editable:
            _handle_bool_tag(data->msd_Flags, !tag->ti_Data, MSDF_NOINPUT);
            break;

        case MUIA_Textinput_NoInput:
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_NOINPUT | MSDF_TINOINPUT);
            break;

        case MUIA_Textinput_RemainActive:
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_STAYACTIVE);
            break;

        case MUIA_Textinput_Multiline:
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_MULTILINE);
            break;

        case MUIA_Textinput_Format:
            data->msd_Align = (LONG) tag->ti_Data;
            break;

        case MUIA_Textinput_MarkStart:
        case MUIA_Textinput_MarkEnd:
        case MUIA_Textinput_Changed:
        case MUIA_Textinput_ResetMarkOnCursor:
        case MUIA_Textinput_IsNumeric:
        case MUIA_Textinput_MinVal:
        case MUIA_Textinput_MaxVal:
        case MUIA_Textinput_WordWrap:
        case MUIA_Textinput_Styles:
        case MUIA_Textinput_SetMin:
        case MUIA_Textinput_SetMax:
        case MUIA_Textinput_SetVMin:
        case MUIA_Textinput_SetVMax:
            String_SetTextinput(data, tag);
            break;

        case MUIA_String_EditHook:
            data->msd_EditHook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_String_Format:
            data->msd_Align = (LONG) tag->ti_Data;
            break;

        case MUIA_String_Integer64:
            if (tag->ti_Data)
            {
                StringFormat64(integerbuf, *(const long long *)tag->ti_Data);
                str = integerbuf;
            }
            break;

        case MUIA_Textinput_Integer:
        case MUIA_String_Integer:
            snprintf(integerbuf, 19, "%ld", tag->ti_Data);
            str = integerbuf;
            break;

        case MUIA_String_LonelyEditHook:
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_LONELYEDITHOOK);
            break;

        case MUIA_Textinput_MaxLen:
        case MUIA_String_MaxLen:
            data->BufferSize = tag->ti_Data;
            if (data->BufferSize < 1)
                data->BufferSize = 1;
            break;

        case MUIA_String_Columns:      /* BetterString */
            data->Columns = (WORD) tag->ti_Data;
            break;

        case MUIA_String_NoInput:      /* BetterString */
            _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_NOINPUT);
            break;

        case MUIA_String_KeyUpFocus:   /* BetterString */
            data->KeyUpFocus = (Object *)tag->ti_Data;
            break;

        case MUIA_String_KeyDownFocus: /* BetterString */
            data->KeyDownFocus = (Object *)tag->ti_Data;
            break;

        case MUIA_String_StayActive:   /* BetterString */
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_STAYACTIVE);
            break;

        }
    }

    if (Buffer_Alloc(data))
    {
        Buffer_SetNewContents(data, str);
        if (data->NumChars)
            data->msd_Flags |= MSDF_CHANGED;
        /* Marks given at creation refer to the initial contents */
        if (data->msd_Flags & MSDF_TIMARK)
            data->msd_Flags |= MSDF_MARKING;
    }

    if (NULL == data->Buffer)
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    D(bug("[MUI:String] New(%p)\n", obj));

    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;
    data->ehn.ehn_Priority = 0;
    data->ehn.ehn_Flags = MUI_EHF_GUIMODE;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;

    CurrentTime(&data->OldClick_Sec, &data->OldClick_Micro);

    return (IPTR) obj;
}

/**************************************************************************
 OM_DISPOSE
**************************************************************************/
IPTR String__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

    if (data->edit_menu) MUI_DisposeObject(data->edit_menu);
    FreeVec(data->Buffer);
    FreeVec(data->SecBuffer);

    D(bug("[MUI:String] Dispose %p\n", obj));

    return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
 OM_SET
**************************************************************************/
IPTR String__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *tag;
    ULONG alias = TAG_IGNORE;
    IPTR result;
    BOOL update = FALSE, setpos = FALSE;

    while ((tag = NextTagItem(&tags)) != NULL)
        if (tag->ti_Tag == MUIA_String_BufferPos
            || tag->ti_Tag == MUIA_Textinput_CursorPos)
            setpos = TRUE;
    tags = msg->ops_AttrList;

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Text_Contents: /* MUI5 */
        case MUIA_String_Contents:
        case MUIA_Textinput_Contents:
        {
            /* As in MUI 21.227, the cursor keeps its distance from the
             * end unless the same call sets its position. */
            LONG keep = (LONG)data->NumChars - (LONG)data->BufferPos;

            if (Buffer_SetNewContents(data, (STRPTR) tag->ti_Data))
            {
                if (!setpos)
                    data->BufferPos = MAX(0, (LONG)data->NumChars - keep);
                data->msd_RedrawReason = NEW_CONTENTS;
                data->msd_Flags &= ~(MSDF_MARKING | MSDF_KEYMARKING |
                    MSDF_TIMARK);
                data->msd_Flags |= MSDF_CHANGED;
                MUI_Redraw(obj, MADF_DRAWOBJECT);
                /* Notify both names of the contents */
                alias = tag->ti_Tag == MUIA_Textinput_Contents ?
                    MUIA_String_Contents : MUIA_Textinput_Contents;
            }
            else
            {
                /* Attribute value not changed, don't fire notification */
                tag->ti_Tag = TAG_IGNORE;
            }
            break;
        }

        case MUIA_Textinput_AcceptChars:
        case MUIA_String_Accept:
            data->msd_Accept = (CONST_STRPTR) tag->ti_Data;
            break;

        case MUIA_Textinput_RejectChars:
        case MUIA_String_Reject:
            data->msd_Reject = (CONST_STRPTR) tag->ti_Data;
            break;

        case MUIA_Textinput_AttachedList:
        case MUIA_String_AttachedList:
            data->msd_AttachedList = (Object *) tag->ti_Data;
            break;

        case MUIA_Textinput_Secret:
        case MUIA_String_Secret:
            if ((BOOL)!!tag->ti_Data != (BOOL)!!data->msd_useSecret)
            {
                Buffer_SetSecret(data, !!tag->ti_Data);
                data->msd_RedrawReason = NEW_CONTENTS;
                MUI_Redraw(obj, MADF_DRAWUPDATE);
            }
            break;

        case MUIA_Textinput_CursorPos:
            /* Moving the cursor keeps the Textinput mark */
            Ti_FreezeMark(data);
            data->BufferPos = MIN((ULONG)tag->ti_Data, data->NumChars);
            data->msd_RedrawReason = MOVE_CURSOR;
            update = TRUE;
            break;

        case MUIA_Textinput_Editable:
            if (tag->ti_Data && !(data->msd_Flags & MSDF_TINOINPUT))
                data->msd_Flags &= ~MSDF_NOINPUT;
            else if (!tag->ti_Data)
                data->msd_Flags |= MSDF_NOINPUT;
            break;

        case MUIA_Textinput_RemainActive:
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_STAYACTIVE);
            break;

        default:
            if (String_SetTextinput(data, tag))
            {
                data->msd_RedrawReason = MOVE_CURSOR;
                update = TRUE;
            }
            break;

        case MUIA_String_EditHook:
            data->msd_EditHook = (struct Hook *)tag->ti_Data;
            break;
        case MUIA_String_LonelyEditHook:
            _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_LONELYEDITHOOK);
            break;

        case MUIA_String_Placeholder:
        case MUIA_String_InactiveContents:
            if (data->placeholder == (CONST_STRPTR)tag->ti_Data)
                tag->ti_Tag = TAG_IGNORE;
            else
            {
                data->placeholder = (CONST_STRPTR)tag->ti_Data;
                MUI_Redraw(obj, MADF_DRAWOBJECT);
            }
            break;

        case MUIA_String_Integer64:
            if (tag->ti_Data)
            {
                char buffer[22];
                long long old = StringParse64(data->Buffer);
                StringFormat64(buffer, *(const long long *)tag->ti_Data);
                set(obj, MUIA_String_Contents, buffer);
                if (old == StringParse64(data->Buffer))
                    tag->ti_Tag = TAG_IGNORE;
            }
            break;

        case MUIA_Textinput_Integer:
        case MUIA_String_Integer:
            {
                char buf[20];
                IPTR oldval, newval;
                ULONG have_oldval;

                have_oldval = get(obj, MUIA_String_Integer, &oldval);
                snprintf(buf, 19, "%ld", tag->ti_Data);
                set(obj, MUIA_String_Contents, buf);
                if (get(obj, MUIA_String_Integer, &newval)
                    && have_oldval && oldval == newval)
                    tag->ti_Tag = TAG_IGNORE; /* Attribute value not changed */
            }
            break;

        case MUIA_Textinput_AdvanceOnCR:
        case MUIA_String_AdvanceOnCR:
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_ADVANCEONCR);
            break;

        case MUIA_String_BufferPos:
            data->BufferPos = (ULONG) tag->ti_Data;
            data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
            data->msd_RedrawReason = MOVE_CURSOR;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;

        case MUIA_String_DisplayPos:
            /* Scrolling the view must not move the editing cursor. Drawing
             * may bring it back into view when the gadget is active. */
            data->DispPos = MIN((ULONG)tag->ti_Data, data->NumChars);
            data->msd_RedrawReason = MOVE_CURSOR;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;

        case MUIA_String_NoInput:      /* BetterString */
            _handle_bool_tag(data->msd_Flags, tag->ti_Data, MSDF_NOINPUT);
            break;

        case MUIA_String_KeyUpFocus:   /* BetterString */
            data->KeyUpFocus = (Object *)tag->ti_Data;
            break;

        case MUIA_String_KeyDownFocus: /* BetterString */
            data->KeyDownFocus = (Object *)tag->ti_Data;
            break;

        case MUIA_String_StayActive:   /* BetterString */
            _handle_bool_tag(data->msd_Flags, tag->ti_Data,
                MSDF_STAYACTIVE);
            break;

        /* MUI's Newstring wrapper forwards its selection anchor to String. */
        case 0x81cd0025: /* MUIA_Newstring_StartPos */
            Buffer_Select(data, (LONG)MIN(tag->ti_Data, data->NumChars) -
                (LONG)MIN(data->BufferPos, data->NumChars));
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;

        case MUIA_String_SelectSize:   /* BetterString */
            Buffer_Select(data, (LONG)tag->ti_Data);
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;

        }
    }

    if (update)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    result = DoSuperMethodA(cl, obj, (Msg) msg);
    if (alias != TAG_IGNORE)
        SetSuperAttrs(cl, obj, MUIA_NoNotify,
            GetTagData(MUIA_NoNotify, FALSE, msg->ops_AttrList),
            alias, (IPTR)data->Buffer, TAG_DONE);
    return result;
}


/**************************************************************************
 OM_GET
**************************************************************************/
IPTR String__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

#define STORE *(msg->opg_Storage)
    switch (msg->opg_AttrID)
    {
    case MUIA_Text_Contents: /* MUI5 */
    case MUIA_String_Contents:
    case MUIA_Textinput_Contents:
        STORE = (IPTR) data->Buffer;
        return TRUE;

    case MUIA_Textinput_CursorPos:
        STORE = MIN(data->BufferPos, data->NumChars);
        return TRUE;

    case MUIA_Textinput_MarkStart:
    case MUIA_Textinput_MarkEnd:
        {
            LONG start, end;
            Ti_GetMark(data, &start, &end);
            STORE = msg->opg_AttrID == MUIA_Textinput_MarkEnd ? end : start;
            return TRUE;
        }

    case MUIA_Textinput_Changed:
        STORE = !!(data->msd_Flags & MSDF_CHANGED);
        return TRUE;
    case MUIA_Textinput_Editable:
        STORE = !(data->msd_Flags & MSDF_NOINPUT);
        return TRUE;
    case MUIA_Textinput_NoInput:
        STORE = !!(data->msd_Flags & MSDF_TINOINPUT);
        return TRUE;
    case MUIA_Textinput_Multiline:
        STORE = !!(data->msd_Flags & MSDF_MULTILINE);
        return TRUE;
    case MUIA_Textinput_RemainActive:
        STORE = !!(data->msd_Flags & MSDF_STAYACTIVE);
        return TRUE;
    case MUIA_Textinput_ResetMarkOnCursor:
        STORE = !!(data->msd_Flags & MSDF_RESETMARK);
        return TRUE;
    case MUIA_Textinput_IsNumeric:
        STORE = !!(data->msd_Flags & MSDF_ISNUMERIC);
        return TRUE;
    case MUIA_Textinput_SetMin:
        STORE = !!(data->msd_Flags & MSDF_SETMIN);
        return TRUE;
    case MUIA_Textinput_SetMax:
        STORE = !!(data->msd_Flags & MSDF_SETMAX);
        return TRUE;
    case MUIA_Textinput_SetVMin:
        STORE = !!(data->msd_Flags & MSDF_SETVMIN);
        return TRUE;
    case MUIA_Textinput_SetVMax:
        STORE = !!(data->msd_Flags & MSDF_SETVMAX);
        return TRUE;
    case MUIA_Textinput_MinVal:
        STORE = data->TiMinVal;
        return TRUE;
    case MUIA_Textinput_MaxVal:
        STORE = data->TiMaxVal;
        return TRUE;
    case MUIA_Textinput_WordWrap:
        STORE = data->TiWordWrap;
        return TRUE;
    case MUIA_Textinput_Styles:
        STORE = data->TiStyles;
        return TRUE;
    case MUIA_Textinput_Font:
        STORE = data->TiFont;
        return TRUE;

    case MUIA_String_EditHook:
        STORE = (IPTR)data->msd_EditHook;
        return TRUE;
    case MUIA_String_LonelyEditHook:
        STORE = !!(data->msd_Flags & MSDF_LONELYEDITHOOK);
        return TRUE;

    case MUIA_String_Secret:
    case MUIA_Textinput_Secret:
        STORE = (IPTR) data->msd_useSecret;
        return TRUE;

    case MUIA_String_Acknowledge:
        STORE = (IPTR) data->AckBuffer;
        return TRUE;

    case MUIA_String_Accept:
    case MUIA_Textinput_AcceptChars:
        STORE = (IPTR) data->msd_Accept;
        return TRUE;

    case MUIA_String_Reject:
    case MUIA_Textinput_RejectChars:
        STORE = (IPTR) data->msd_Reject;
        return TRUE;

    case MUIA_String_AttachedList:
    case MUIA_Textinput_AttachedList:
        STORE = (IPTR) data->msd_AttachedList;
        return TRUE;

    case MUIA_String_Format:
    case MUIA_Textinput_Format:
        STORE = (IPTR) data->msd_Align;
        return TRUE;

    case MUIA_String_Placeholder:
    case MUIA_String_InactiveContents:
        STORE = (IPTR)data->placeholder;
        return TRUE;

    case MUIA_String_Integer64:
        data->integer64 = StringParse64(data->Buffer);
        STORE = (IPTR)&data->integer64;
        return TRUE;

    case MUIA_String_Integer:
    case MUIA_Textinput_Integer:
        {
            STRPTR buf = NULL;
            STORE = 0;
            get(obj, MUIA_String_Contents, &buf);
            if (NULL != buf)
            {
                LONG val = 0;
                StrToLong(buf, &val);
                STORE = val;
            }
            return TRUE;
        }

    case MUIA_String_MaxLen:
    case MUIA_Textinput_MaxLen:
        STORE = (IPTR) data->BufferSize;
        return TRUE;

    case MUIA_String_AdvanceOnCR:
    case MUIA_Textinput_AdvanceOnCR:
        STORE = (data->msd_Flags & MSDF_ADVANCEONCR) ? TRUE : FALSE;
        return TRUE;

    case MUIA_String_BufferPos:
        STORE = data->BufferPos;
        return TRUE;

    case MUIA_String_DisplayPos:
        STORE = data->DispPos;
        return TRUE;

    case MUIA_String_NoInput:  /* BetterString */
        STORE = (data->msd_Flags & MSDF_NOINPUT) ? TRUE : FALSE;
        return TRUE;

    case MUIA_String_StayActive:       /* BetterString */
        STORE = (data->msd_Flags & MSDF_STAYACTIVE) ? TRUE : FALSE;
        return TRUE;

    case MUIA_String_KeyUpFocus:       /* BetterString */
        STORE = (IPTR)data->KeyUpFocus;
        return TRUE;

    case MUIA_String_KeyDownFocus:     /* BetterString */
        STORE = (IPTR)data->KeyDownFocus;
        return TRUE;

    case 0x81cd0025: /* MUIA_Newstring_StartPos */
        STORE = data->msd_Flags & MSDF_MARKING ? data->MarkPos : data->BufferPos;
        return TRUE;

    case MUIA_String_SelectSize:       /* BetterString */
        if (data->msd_Flags & MSDF_MARKING)
        {
            LONG markstart, markstop;

            if (Buffer_GetMarkedRange(data, &markstart, &markstop))
            {
                LONG size = markstop - markstart;

                if (data->MarkPos < data->BufferPos)
                    size = -size;

                STORE = (IPTR) size;

                return 1;
            }

        }

        STORE = 0;
        return TRUE;

    }

    /* Other Textinput attributes have no function here */
    if (msg->opg_AttrID >= MUIA_Textinput_Multiline
        && msg->opg_AttrID <= MUIA_Textinputscroll_HorizBar)
    {
        STORE = 0;
        return TRUE;
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
#undef STORE
}

/**************************************************************************
 MUIM_Setup
**************************************************************************/
IPTR String__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

    if (0 == DoSuperMethodA(cl, obj, (Msg) msg))
        return FALSE;

    data->is_active = FALSE;
    set(obj, MUIA_Background,
        (IPTR) muiGlobalInfo(obj)->mgi_Prefs->string_bg_inactive);

    zune_pen_spec_to_intern(
        (const struct MUI_PenSpec *)muiGlobalInfo(obj)->mgi_Prefs->
        string_text_inactive, &data->inactive_text);
    zune_penspec_setup(&data->inactive_text, muiRenderInfo(obj));

    zune_pen_spec_to_intern(
        (const struct MUI_PenSpec *)muiGlobalInfo(obj)->mgi_Prefs->
        string_text_active, &data->active_text);
    zune_penspec_setup(&data->active_text, muiRenderInfo(obj));

    zune_pen_spec_to_intern(
        (const struct MUI_PenSpec *)muiGlobalInfo(obj)->mgi_Prefs->
        string_text_marked, &data->marked_text);
    zune_penspec_setup(&data->marked_text, muiRenderInfo(obj));

    zune_pen_spec_to_intern(
        (const struct MUI_PenSpec *)muiGlobalInfo(obj)->mgi_Prefs->
        string_bg_marked, &data->marked_bg);
    zune_penspec_setup(&data->marked_bg, muiRenderInfo(obj));

    zune_pen_spec_to_intern(
        (const struct MUI_PenSpec *)muiGlobalInfo(obj)->mgi_Prefs->
        string_cursor, &data->cursor);
    zune_penspec_setup(&data->cursor, muiRenderInfo(obj));

    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) & data->ehn);
    return TRUE;
}

/**************************************************************************
 MUIM_Cleanup
**************************************************************************/
IPTR String__MUIM_Cleanup(struct IClass *cl, Object *obj,
    struct MUIP_Cleanup *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

    D(bug("[MUI:String] Cleanup %p\n", obj));
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) & data->ehn);

    zune_penspec_cleanup(&data->inactive_text);
    zune_penspec_cleanup(&data->active_text);
    zune_penspec_cleanup(&data->marked_text);
    zune_penspec_cleanup(&data->marked_bg);
    zune_penspec_cleanup(&data->cursor);

    return (DoSuperMethodA(cl, obj, (Msg) msg));
}

/**************************************************************************
 MUIM_AskMinMax
**************************************************************************/
IPTR String__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

    DoSuperMethodA(cl, obj, (Msg) msg);

    if (data->Columns >= 0)
    {
        msg->MinMaxInfo->MinWidth += _font(obj)->tf_XSize * data->Columns;
        msg->MinMaxInfo->DefWidth += _font(obj)->tf_XSize * data->Columns;
        msg->MinMaxInfo->MaxWidth += _font(obj)->tf_XSize * data->Columns;
    }
    else
    {
        /* MUI 3.8/3.9 use fixed content widths, independent of font */
        msg->MinMaxInfo->MinWidth += 20;
        msg->MinMaxInfo->DefWidth += 100;
        msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    }

    msg->MinMaxInfo->MinHeight += _font(obj)->tf_YSize;
    msg->MinMaxInfo->DefHeight += _font(obj)->tf_YSize;
    msg->MinMaxInfo->MaxHeight += _font(obj)->tf_YSize;
    /* Textinputscroll subclasses add rows on top of this */
    if (data->msd_Flags & MSDF_MULTILINE)
        msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;

/*    D(bug("String_AskMinMax(%p): Min=%ldx%ld Max=%ldx%ld Def=%ldx%ld\n", */
/*            obj, msg->MinMaxInfo->MinWidth, msg->MinMaxInfo->MinHeight, */
/*            msg->MinMaxInfo->MaxWidth, msg->MinMaxInfo->MaxHeight, */
/*            msg->MinMaxInfo->DefWidth, msg->MinMaxInfo->DefHeight)); */

    return TRUE;
}


static WORD MaxDispPos(struct IClass *cl, Object *obj)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    WORD numfit, max_disppos, numchars;
    struct TextExtent te;
    BOOL cursor_at_end;

    cursor_at_end = (data->BufferPos == data->NumChars);

/*      D(bug("MaxDispPos(current length: %d, bufferpos=%d)\n", */
/*            data->NumChars, data->BufferPos)); */

/*      D(bug("cursor_at_end: %d\n", cursor_at_end)); */

    if (cursor_at_end)          /* Cursor at end of string ? */
    {
/*              D(bug("Making cursor last char\n")); */
        numchars = data->NumChars + 1;  /* Take cursor into account */

/*            This has already been done by UpdateDisp() which called us
        strinfo->Buffer[strinfo->NumChars] = 0x20;

*/
    }
    else
    {
        numchars = data->NumChars;
    }

    /* Find the amount of characters that fit into the bbox, counting
     ** from the last character in the buffer and forward,
     */
    numfit = TextFit(_rp(obj),
        &(data->Buffer[numchars - 1]),
        numchars, &te, NULL, -1, _mwidth(obj), _mheight(obj));

    max_disppos = numchars - numfit;

/*    if ((max_disppos > 0) && (!cursor_at_end))
            max_disppos --;
  */

/*      D(bug("Numchars w/cursor: %d, Numfit: %d, maxdisppos=%d  " */
/*            "bbox->Width = %d  te->te_Width = %d\n", */
/*            numchars, numfit, max_disppos, _mwidth(obj), te.te_Width)); */

    return max_disppos;
}


static void UpdateDisp(struct IClass *cl, Object *obj)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    struct TextExtent te;
    STRPTR dispstr;

    /* If the cursor is at the trailing \0, insert a SPACE instead */
    if (data->BufferPos == data->NumChars)
        data->VisualBuffer[data->NumChars] = ' ';

    /* In this function we check if the cursor has gone outside
     ** of the visible area (because of application setting
     ** strinfo->BufferPos or strinfo->DispPos to a different value, or
     ** because of user input).
     ** This is made a bit difficult by the rule (R), that there
     ** should NOT be available space on the right, and characters
     ** scrolled out at the left, at the same time.
     ** We have 3 possible scenarios:
     ** 1) Cursor to the left of DispPos:
     **    Set DispPos to the lowest of BufferPos and the
     **          maximum allowed disppos (according to (R) ).
     ** 2) Cursor to the right of visible area:
     **    Set dispose sou that the cursor is the last visible character.
     **    This afheres to (R).
     ** 3) Cursor inside visible area. Do a check on rule (R),
     **    and if DispPos > max allowed, then adjust it down,
     **    so that the last character in the buffer becomes last character
     **          displayed. (The cursor will still be visible after adjustion)
     */

    /* 1) Cursor to the left of visible area */
    if (data->BufferPos < data->DispPos)
    {
        WORD max_disppos;

        max_disppos = MaxDispPos(cl, obj);
        data->DispPos = MIN(data->BufferPos, max_disppos);
    }
    else        /* Cursor equal to the right of disppos [ 2) or 3) ] */
    {
        UWORD strsize;

        /* How many pixels are there from current 1st displayed to cursor? */
        strsize = TextLength(_rp(obj),
            data->VisualBuffer + data->DispPos,
            data->BufferPos - data->DispPos + 1);

        /* 2) More than fits into the gadget ? */
        if (strsize > _mwidth(obj))
        {
            /* Compute new DispPos such that the cursor is at the right */
            data->DispPos = data->BufferPos
                - TextFit(_rp(obj),
                &(data->VisualBuffer[data->BufferPos]),
                data->NumChars, &te, NULL, -1,
                _mwidth(obj), _mheight(obj)) + 1;

/*              D(bug("cursor right of visible area, new disppos: %d\n", */
/*                  data->DispPos)); */
        }
        else                    /* 3). Cursor inside gadget */
        {
            WORD max_disppos;

            max_disppos = MaxDispPos(cl, obj);
            if (data->DispPos > max_disppos)
                data->DispPos = max_disppos;

        }        /* if (cursor inside or to the right of visible area ) */

    }

    /* Update the DispCount */
    /* It might be necessary with special handling for centre aligned gads */
    dispstr = &(data->VisualBuffer[data->DispPos]);
/*      D(bug("DispCount before = %d\n", data->DispCount)); */
    data->DispCount = TextFit(_rp(obj), dispstr,
        data->NumChars - data->DispPos,
        &te, NULL, 1, _mwidth(obj), _mheight(obj));
/*      D(bug("DispCount after = %d\n", data->DispCount)); */

    /* 0-terminate string (in case we put a SPACE at the end) */
    data->VisualBuffer[data->NumChars] = '\0';
}


/* Gets left position of text in the string gadget */
static UWORD GetTextLeft(struct IClass *cl, Object *obj)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    UWORD text_left = 0;
    STRPTR dispstr = &(data->VisualBuffer[data->DispPos]);
    UWORD dispstrlen;
    BOOL cursor_at_end;

    cursor_at_end = (data->BufferPos == data->NumChars);
    dispstrlen = MIN(data->DispCount, data->NumChars - data->DispPos);

    switch (data->msd_Align)
    {
    case MUIV_String_Format_Left:
        text_left = _mleft(obj);
        break;

    case MUIV_String_Format_Center:
        {
            WORD textwidth = TextLength(_rp(obj), dispstr, dispstrlen);
            if (cursor_at_end)
                textwidth += TextLength(_rp(obj), " ", 1);
            text_left = _mleft(obj) + ((_mwidth(obj) - textwidth) / 2);
/*              D(bug("GetTextLeft: dispstr=%s, dispstrlen=%d, textw=%d, " */
/*                  "textl=%d\n", */
/*                  dispstr, dispstrlen, textwidth, text_left)); */
        }
        break;

    case MUIV_String_Format_Right:
        {
            WORD textwidth = TextLength(_rp(obj), dispstr, dispstrlen);

            if (cursor_at_end)
                textwidth += TextLength(_rp(obj), " ", 1);
            text_left = _mleft(obj) + (_mwidth(obj) - 1 - textwidth);
        }
        break;
    }
    return (text_left);
}

/* Gets right offset of text in the string gadget */
static UWORD GetTextRight(struct IClass *cl, Object *obj)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    UWORD text_right = 0;
    STRPTR dispstr = &(data->VisualBuffer[data->DispPos]);
    UWORD dispstrlen;
    BOOL cursor_at_end;

    cursor_at_end = (data->BufferPos == data->NumChars);
    dispstrlen = MIN(data->DispCount, data->NumChars - data->DispPos);

    switch (data->msd_Align)
    {
    case MUIV_String_Format_Left:
        text_right =
            _mleft(obj) + TextLength(_rp(obj), dispstr, dispstrlen);
        break;

    case MUIV_String_Format_Center:
        {
            WORD textwidth = TextLength(_rp(obj), dispstr, dispstrlen);

            if (cursor_at_end)
                textwidth += TextLength(_rp(obj), " ", 1);
            text_right = _mright(obj) - ((_mwidth(obj) - textwidth) / 2);
        }
        break;

    case MUIV_String_Format_Right:
        text_right = _mright(obj);
        break;
    }
    return (text_right);
}


/* Updates the stringdata in case user has set some fields */
static VOID UpdateStringData(struct IClass *cl, Object *obj)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

    data->NumChars = strlen(data->Buffer);

    if (data->BufferPos > data->NumChars)
    {
        data->BufferPos = data->NumChars;
    }
}

static VOID TextM(Object *obj, struct MUI_StringData *data,
    STRPTR text, WORD textlen, LONG markstart, LONG markend)
{
    struct RastPort *rp = _rp(obj);
    ULONG textpen;
    LONG len;

    if (data->is_active)
        textpen = data->active_text.p_pen;
    else
        textpen = data->inactive_text.p_pen;

    //kprintf("TextM: textlen %d  markstart %d  markend %d ... \n",
    //    textlen, markstart, markend);

    /* <unmarked><marked><unmarked> */

    /* <unmarked> */

    len = MIN(markstart, textlen);
    len = MAX(len, 0);

    if (len)
    {
        //kprintf("A: %d   ", len);

        SetABPenDrMd(rp, textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
        Text(rp, text, len);

        text += len;
        textlen -= len;

    }

    len = MIN(markend - len, textlen);
    len = MAX(len, 0);

    if (len)
    {
        //kprintf("B: %d   ", len);
        SetABPenDrMd(_rp(obj), data->marked_text.p_pen,
            data->marked_bg.p_pen, JAM2);
        Text(rp, text, len);

        text += len;
        textlen -= len;
    }

    if (textlen)
    {
        //kprintf("C: %d   ", textlen);

        SetABPenDrMd(rp, textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
        Text(rp, text, textlen);
    }
    //kprintf("\n");
}

static LONG Ml_Rows(Object *obj)
{
    return MAX(1, _mheight(obj) / _font(obj)->tf_YSize);
}

/* Scroll so that the cursor is visible */
static void Ml_Scroll(Object *obj, struct MUI_StringData *data)
{
    struct RastPort *rp = _rp(obj);
    LONG rows = Ml_Rows(obj), line = Ml_LineOf(data, data->BufferPos);
    LONG start = Ml_LineStart(data, data->BufferPos);
    LONG column = data->BufferPos - start, cell;
    STRPTR text = data->VisualBuffer + start;

    if (line < data->MlTopLine)
        data->MlTopLine = line;
    else if (line >= data->MlTopLine + rows)
        data->MlTopLine = line - rows + 1;

    SetFont(rp, _font(obj));
    cell = TextLength(rp, column < Ml_LineEnd(data, start) - start ?
        text + column : (STRPTR)" ", 1);
    if (column < data->MlLeftCol)
        data->MlLeftCol = column;
    while (data->MlLeftCol < column && TextLength(rp,
            text + data->MlLeftCol, column - data->MlLeftCol) + cell
        > _mwidth(obj))
        data->MlLeftCol++;
}

/* Buffer position at a point; outside the rows it is in the line beyond */
static LONG Ml_PosAt(Object *obj, struct MUI_StringData *data, WORD x,
    WORD y)
{
    struct RastPort *rp = _rp(obj);
    struct TextExtent te;
    LONG ysize = _font(obj)->tf_YSize, row, start, end, from;

    row = y < _mtop(obj) ? -1 : (y - _mtop(obj)) / ysize;
    start = Ml_LinePos(data, MAX(0, data->MlTopLine + row));
    end = Ml_LineEnd(data, start);
    from = MIN(start + data->MlLeftCol, end);
    if (x <= _mleft(obj))
        return from;
    SetFont(rp, _font(obj));
    return from + TextFit(rp, data->VisualBuffer + from, end - from, &te,
        NULL, 1, x - _mleft(obj), ysize);
}

static void Ml_Draw(Object *obj, struct MUI_StringData *data,
    ULONG textpen)
{
    struct RastPort *rp = _rp(obj);
    struct TextExtent te;
    LONG ysize = _font(obj)->tf_YSize, rows = Ml_Rows(obj), row;
    LONG markstart = 0, markstop = 0;
    LONG pos = Ml_LinePos(data, data->MlTopLine);
    BOOL marked = (data->msd_Flags & MSDF_MARKING)
        && Buffer_GetMarkedRange(data, &markstart, &markstop);
    STRPTR text = data->VisualBuffer;

    for (row = 0; row < rows; row++)
    {
        LONG end = Ml_LineEnd(data, pos);
        LONG from = MIN(pos + data->MlLeftCol, end), count;
        WORD top = _mtop(obj) + row * ysize + rp->Font->tf_Baseline;

        count = TextFit(rp, text + from, end - from, &te, NULL, 1,
            _mwidth(obj), ysize);
        Move(rp, _mleft(obj), top);
        if (marked)
            TextM(obj, data, text + from, count, markstart - from,
                markstop - from);
        else
        {
            SetABPenDrMd(rp, textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
            Text(rp, text + from, count);
            if (data->is_active && (LONG)data->BufferPos >= from
                && (LONG)data->BufferPos <= end)
            {
                SetABPenDrMd(rp, data->active_text.p_pen,
                    data->cursor.p_pen, JAM2);
                Move(rp, _mleft(obj) + TextLength(rp, text + from,
                    data->BufferPos - from), top);
                Text(rp, (LONG)data->BufferPos < end ?
                    text + data->BufferPos : (STRPTR)" ", 1);
            }
        }
        if (end >= (LONG)data->NumChars)
            break;
        pos = end + 1;
    }
}

/**************************************************************************
 MUIM_Draw
**************************************************************************/
IPTR String__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    UWORD text_left;
    UWORD text_top;
    STRPTR dispstr;
    UWORD dispstrlen;
    ULONG textpen;
    UWORD textleft_save;
    LONG markstart = 0, markstop = 0;

    /*  D(bug("\nString_Draw(%p) %ldx%ldx%ldx%ld reason=%ld msgflgs=%ld " */
/*          "curs=%d " */
/*          "displ=%ld len=%ld buf='%s'\n",obj,_mleft(obj),_mtop(obj), */
/*          _mwidth(obj),_mheight(obj), data->msd_RedrawReason, msg->flags, */
/*          data->BufferPos, data->DispPos, data->NumChars, data->Buffer)); */

    DoSuperMethodA(cl, obj, (Msg) msg);

    if (!(msg->flags & MADF_DRAWUPDATE) && !(msg->flags & MADF_DRAWOBJECT))
        return 0;

    if (!data->is_active && !data->Buffer[0] && data->placeholder)
    {
        struct RastPort *rp = _rp(obj);
        struct TextExtent extent;
        ULONG style = rp->AlgoStyle;
        LONG count, x = _mleft(obj), width;
        DoMethod(obj, MUIM_DrawBackground, _mleft(obj), _mtop(obj),
            _mwidth(obj), _mheight(obj), _mleft(obj), _mtop(obj), 0);
        SetFont(rp, _font(obj));
        SetSoftStyle(rp, FSF_ITALIC, AskSoftStyle(rp));
        count = TextFit(rp, data->placeholder, strlen(data->placeholder),
            &extent, NULL, 1, _mwidth(obj), _mheight(obj));
        width = TextLength(rp, data->placeholder, count);
        if (data->msd_Align == MUIV_String_Format_Center)
            x += (_mwidth(obj) - width) / 2;
        else if (data->msd_Align == MUIV_String_Format_Right)
            x += _mwidth(obj) - width;
        SetABPenDrMd(rp, _pens(obj)[MPEN_SHADOW],
            _pens(obj)[MPEN_BACKGROUND], JAM1);
        Move(rp, x, _mtop(obj) + ((_mheight(obj) - rp->Font->tf_YSize) / 2)
            + rp->Font->tf_Baseline);
        Text(rp, data->placeholder, count);
        SetSoftStyle(rp, style, AskSoftStyle(rp));
        data->msd_RedrawReason = NO_REASON;
        return 0;
    }

    PrepareVisualBuffer(data);
    SetFont(_rp(obj), _font(obj));
    if (data->is_active)
        textpen = data->active_text.p_pen;
    else
        textpen = data->inactive_text.p_pen;

    /* Update the stringdata in case of user change */
    UpdateStringData(cl, obj);

    if (MULTILINE(data))
    {
        Ml_Scroll(obj, data);
        if (msg->flags & MADF_DRAWUPDATE)
            DoMethod(obj, MUIM_DrawBackground, _mleft(obj), _mtop(obj),
                _mwidth(obj), _mheight(obj), _mleft(obj), _mtop(obj), 0);
        Ml_Draw(obj, data, textpen);
        data->msd_RedrawReason = NO_REASON;
        CleanVisualBuffer(data);
        return TRUE;
    }

    /* Update the DispPos and DispCount fields so that the gadget renders
     * properly */
    UpdateDisp(cl, obj);

    text_top = _mtop(obj)
        + ((_mheight(obj) - _rp(obj)->Font->tf_YSize) >> 1)
        + _rp(obj)->Font->tf_Baseline;

    dispstr = data->VisualBuffer + data->DispPos;
    dispstrlen = MIN(data->DispCount, data->NumChars - data->DispPos);
    textleft_save = text_left = GetTextLeft(cl, obj);

    // little flicker improvement, don't redraw first part of string
    // when adding a char
    if (msg->flags & MADF_DRAWUPDATE &&
        data->msd_RedrawReason == DO_ADDCHAR &&
        data->msd_Align == MUIV_String_Format_Left && data->DispPos == 0)
    {
        text_left += TextLength(_rp(obj), dispstr, data->BufferPos - 1);
        dispstr += data->BufferPos - 1;
        dispstrlen -= data->BufferPos - 1;
        DoMethod(obj, MUIM_DrawBackground, text_left, _mtop(obj),
            _mwidth(obj) - text_left + _mleft(obj), _mheight(obj),
            text_left, _mtop(obj), 0);
    }
    else if (msg->flags & MADF_DRAWUPDATE)
    {
        DoMethod(obj, MUIM_DrawBackground, _mleft(obj), _mtop(obj),
            _mwidth(obj), _mheight(obj), _mleft(obj), _mtop(obj), 0);
    }

    SetABPenDrMd(_rp(obj), textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
    Move(_rp(obj), text_left, text_top);

    if ((data->msd_Flags & MSDF_MARKING)
        && Buffer_GetMarkedRange(data, &markstart, &markstop))
    {
        TextM(obj, data, dispstr, dispstrlen, markstart - data->DispPos,
            markstop - data->DispPos);

    }
    else
    {
        Text(_rp(obj), dispstr, dispstrlen);

        if (data->is_active)    // active, draw cursor
        {
            UWORD cursoroffset = data->BufferPos - data->DispPos;

            dispstr = data->VisualBuffer + data->DispPos;
            text_left = textleft_save;

            SetABPenDrMd(_rp(obj), data->active_text.p_pen,
                data->cursor.p_pen, JAM2);
            text_left += TextLength(_rp(obj), dispstr, cursoroffset);


            Move(_rp(obj), text_left, text_top);
            Text(_rp(obj),
                ((data->BufferPos < data->NumChars)
                    ? dispstr + cursoroffset : (STRPTR) " "), 1);
        }
    }

    data->msd_RedrawReason = NO_REASON;
    CleanVisualBuffer(data);

    return TRUE;
}

/**************************************************************************
 Returns whether object needs redrawing
**************************************************************************/
static int String_HandleVanillakey(struct IClass *cl, Object *obj,
    unsigned char code, UWORD qual, IPTR *retval)
{
    struct MUI_StringData *data =
        (struct MUI_StringData *)INST_DATA(cl, obj);
    BOOL doinput;

    D(bug("[MUI:String] HandleVanillakey: code=%d qual=%d\n", code, qual));

    if (0 == code)
        return 0;

    doinput = (data->msd_Flags & MSDF_NOINPUT) ? FALSE : TRUE;

    if (doinput && (code == '\b'))      /* backspace */
    {
        if (Buffer_KillMarked(data))
        {
            return 1;
        }

        if (data->BufferPos > 0)
        {
            LONG shift;

            if ((qual & IEQUALIFIER_LSHIFT) || (qual & IEQUALIFIER_RSHIFT))
            {
                shift = data->BufferPos - (MULTILINE(data) ?
                    Ml_LineStart(data, data->BufferPos) : 0);
                data->msd_RedrawReason = NEW_CONTENTS;
            }
            else
            {
                shift = 1;
                data->msd_RedrawReason = DO_BACKSPACE;
            }

            D(bug("[MUI:String] HandleVanillakey: 'Backspace' count=%d pos=%d"
                " shift=%d\n", data->NumChars, data->BufferPos, shift));
            memmove(&data->Buffer[data->BufferPos - shift],
                &data->Buffer[data->BufferPos],
                data->NumChars - data->BufferPos + 1);
            data->BufferPos -= shift;
            data->NumChars -= shift;
            return 1;
        }
        return 0;
    }

    if (doinput && (code == 21))        // ctrl-u == NAK (like shift-bs)
    {
        if (Buffer_KillMarked(data))
        {
            return 1;
        }

        if (Buffer_Delete(data, MULTILINE(data) ?
                Ml_LineStart(data, data->BufferPos) : 0, data->BufferPos))
        {
            data->msd_RedrawReason = NEW_CONTENTS;
            return 1;
        }
        return 0;
    }

    if (doinput && (code == 127))       /* del */
    {
        if (!Buffer_KillMarked(data))
        {
            if ((qual & IEQUALIFIER_LSHIFT) || (qual & IEQUALIFIER_RSHIFT))
            {
                Buffer_Delete(data, data->BufferPos, MULTILINE(data) ?
                    Ml_LineEnd(data, data->BufferPos) : data->NumChars);
                data->msd_RedrawReason = NEW_CONTENTS;
            }
            else
            {
                if (data->BufferPos < data->NumChars)
                {
                    D(bug("[MUI:String] HandleVanillakey: 'Delete' count=%d"
                        " pos=%d\n", data->NumChars, data->BufferPos));
                    memmove(&data->Buffer[data->BufferPos],
                        &data->Buffer[data->BufferPos + 1],
                        data->NumChars - data->BufferPos);
                    data->NumChars--;
                }

                data->msd_RedrawReason = DO_DELETE;
            }
        }
        return 1;
    }

    if (doinput && (code == 11))        // ctrl-k == VT == \v (like shift-del)
    {
        if (!Buffer_KillMarked(data))
        {
            Buffer_Delete(data, data->BufferPos, MULTILINE(data) ?
                Ml_LineEnd(data, data->BufferPos) : data->NumChars);
            data->msd_RedrawReason = NEW_CONTENTS;
        }
        return 1;
    }

    if (doinput && (code == 24))        /* ctrl x == ascii cancel */
    {
        if (!Buffer_KillMarked(data))
        {
            data->Buffer[0] = 0;
            data->BufferPos = 0;
            data->NumChars = 0;
            data->msd_RedrawReason = NEW_CONTENTS;
        }
        return 1;
    }

    if (code == 1)              // ctrl-a, linestart
    {
        data->BufferPos = 0;
        data->msd_Flags &= ~(MSDF_MARKING | MSDF_KEYMARKING);
        return 1;
    }

    if (code == 26)             // ctrl-z, lineend
    {
        data->BufferPos = data->NumChars;
        data->msd_Flags &= ~(MSDF_MARKING | MSDF_KEYMARKING);
        return 1;
    }

    if (code == '\t')             // tab
    {
        return 0;
    }

    if (((ToLower(code) == 'c') || (ToLower(code) == 'x')) &&
        (qual & IEQUALIFIER_RCOMMAND))
    {
        LONG markstart, markstop;

        if ((data->msd_Flags & MSDF_MARKING)
            && Buffer_GetMarkedRange(data, &markstart, &markstop))
        {
            clipboard_write_text(&data->Buffer[markstart],
                markstop - markstart);

            if (doinput && (ToLower(code) == 'x'))
            {
                Buffer_KillMarked(data);
            }
            else
            {
                data->BufferPos = markstop;
                data->msd_Flags &= ~MSDF_MARKING;
            }
            return 1;
        }
        return 0;
    }

    if (doinput && (ToLower(code) == 'v') && (qual & IEQUALIFIER_RCOMMAND))
    {
        STRPTR text;
        int retval;
        struct Locale *locale = OpenLocale(NULL);

        retval = Buffer_KillMarked(data);
        if ((text = clipboard_read_text()))
        {
            STRPTR text2 = text;
            UBYTE c;

            while ((c = *text2++))
            {
                if (!IsPrint(locale, c))
                    break;
                if (!(Buffer_AddChar(data, c)))
                    break;
                if (!retval)
                    retval = 1;
            }

            clipboard_free_text(text);
        }

        CloseLocale(locale);

        return retval;
    }

    if (data->msd_Accept != NULL)
    {
        /* Check if character is accepted */
        if (NULL == strchr(data->msd_Accept, code))
        {
            DisplayBeep(NULL);
            *retval = MUI_EventHandlerRC_Eat;
            return 0;
        }
    }

    if (data->msd_Reject != NULL)
    {
        /* Check if character is rejected */
        if (NULL != strchr(data->msd_Reject, code))
        {
            DisplayBeep(NULL);
            *retval = MUI_EventHandlerRC_Eat;
            return 0;
        }
    }

    if (doinput && MULTILINE(data) && (code == '\r' || code == '\n'))
    {
        Buffer_KillMarked(data);
        if (Buffer_AddChar(data, '\n'))
        {
            data->msd_RedrawReason = NEW_CONTENTS;
            return 2;
        }
        return 0;
    }

    if (doinput)
    {
        struct Locale *locale = OpenLocale(NULL);

        if (!(code >= 0x09 && code <= 0x0D) && IsPrint(locale, code))
        {
            Buffer_KillMarked(data);
            if (Buffer_AddChar(data, code))
            {
                data->msd_RedrawReason = DO_ADDCHAR;
                return 2;
            }
        }

        CloseLocale(locale);
    }

    data->msd_RedrawReason = DO_UNKNOWN;
    return 0;
}


/**************************************************************************
 Cursor movement of the Textinput Do* methods. Returns whether the
 object needs redrawing.
**************************************************************************/
static int String_CursorKey(struct MUI_StringData *data, LONG muikey,
    LONG target)
{
    BOOL kills = (data->msd_Flags & MSDF_MARKING)
        && !(data->msd_Flags & MSDF_KEYMARKING);
    int update = 0;

    /* A Textinput mark goes away and the cursor moves from where it is */
    if (TI_MARKED(data))
    {
        data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
        kills = FALSE;
        update = 1;
    }

    switch (muikey)
    {
    case MUIKEY_LEFT:
        if (kills)
        {
            update = 1;
            data->BufferPos = MIN(data->BufferPos, data->MarkPos);
            if (data->BufferPos > 0)
                data->BufferPos--;

            data->msd_Flags &= ~MSDF_MARKING;
        }
        else if (data->BufferPos > 0)
        {
            update = 1;

            data->BufferPos--;
            data->msd_RedrawReason = DO_CURSOR_LEFT;
        }
        break;
    case MUIKEY_RIGHT:
        if (kills)
        {
            update = 1;
            data->BufferPos = MAX(data->BufferPos, data->MarkPos);
            data->msd_Flags &= ~MSDF_MARKING;
        }
        else if (data->BufferPos < data->NumChars)
        {
            update = 1;
            data->BufferPos++;
            data->msd_RedrawReason = DO_CURSOR_RIGHT;
        }
        break;
    case MUIKEY_WORDLEFT:
        if (data->BufferPos > 0)
        {
            data->BufferPos =
                Buffer_GetPrevWordIndex(data, data->BufferPos);
            update = 1;
            data->msd_RedrawReason = DO_CURSOR_LEFT;
        }
        if (kills)
        {
            data->msd_Flags &= ~MSDF_MARKING;
            update = 1;
        }
        break;
    case MUIKEY_WORDRIGHT:
        if (data->BufferPos < data->NumChars)
        {
            data->BufferPos =
                Buffer_GetSuccWordIndex(data, data->BufferPos);
            update = 1;
            data->msd_RedrawReason = DO_CURSOR_RIGHT;
        }
        if (kills)
        {
            data->msd_Flags &= ~MSDF_MARKING;
            update = 1;
        }
        break;
    case MUIKEY_LINESTART:
        data->BufferPos = MULTILINE(data) ?
            Ml_LineStart(data, data->BufferPos) : 0;
        update = 1;
        if (kills)
        {
            data->msd_Flags &= ~MSDF_MARKING;
        }
        break;

    case MUIKEY_LINEEND:
        data->BufferPos = MULTILINE(data) ?
            Ml_LineEnd(data, data->BufferPos) : data->NumChars;
        update = 1;
        if (kills)
        {
            data->msd_Flags &= ~MSDF_MARKING;
        }
        break;

    case MUIKEY_NONE:   /* to target, for multiline movement */
        data->BufferPos = target;
        update = 1;
        if (kills)
        {
            data->msd_Flags &= ~MSDF_MARKING;
        }
        break;

    }
    return update;
}

/**************************************************************************
 Textinput methods. During key input, MSDF_INEVENT is set and the event
 handler redraws and notifies once; otherwise each method does that.
**************************************************************************/
static void Ti_Done(struct IClass *cl, Object *obj,
    struct MUI_StringData *data, BOOL edited, BOOL moved)
{
    if (edited)
        data->msd_Flags |= MSDF_CHANGED;
    if (data->msd_Flags & MSDF_INEVENT)
    {
        data->TiPending |= (edited ? TIP_EDITED | TIP_UPDATE : 0)
            | (moved ? TIP_UPDATE : 0);
        return;
    }
    if (edited)
    {
        data->AckBuffer = NULL;
        SetSuperAttrs(cl, obj, MUIA_String_Contents, (IPTR)data->Buffer,
            MUIA_Textinput_Contents, (IPTR)data->Buffer, TAG_DONE);
    }
    if (edited || moved)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
}

/* Let subclasses see key input as Textinput methods */
static ULONG Ti_Call(Object *obj, struct MUI_StringData *data,
    ULONG method, ULONG arg)
{
    ULONG pending;

    data->TiPending = 0;
    data->msd_Flags |= MSDF_INEVENT;
    DoMethod(obj, method, arg, 0);
    data->msd_Flags &= ~MSDF_INEVENT;
    pending = data->TiPending;
    data->TiPending = 0;
    return pending;
}

static void Ti_Copy(struct MUI_StringData *data)
{
    LONG start = 0, stop = data->NumChars;

    if (data->msd_useSecret)
        return;
    if (data->msd_Flags & MSDF_MARKING)
        Buffer_GetMarkedRange(data, &start, &stop);
    if (stop > start)
        clipboard_write_text(&data->Buffer[start], stop - start);
}

/* Read a file into a new buffer of at most max bytes, NUL-terminated */
static STRPTR Ti_ReadFile(CONST_STRPTR name, ULONG max)
{
    BPTR file = Open(name, MODE_OLDFILE);
    STRPTR text;
    LONG length;

    if (!file)
        return NULL;
    text = AllocVec(max, MEMF_ANY);
    length = text ? Read(file, text, max - 1) : -1;
    Close(file);
    if (length < 0)
    {
        FreeVec(text);
        return NULL;
    }
    text[length] = 0;
    return text;
}

/* Find the decimal number at or just before the cursor and add delta */
static BOOL Ti_AddToNumber(struct MUI_StringData *data, LONG delta)
{
    STRPTR buf = data->Buffer;
    LONG pos = MIN(data->BufferPos, data->NumChars), start, stop, value = 0;
    char number[16];
    LONG length;

    if (!isdigit(buf[pos]) && pos > 0 && isdigit(buf[pos - 1]))
        pos--;
    if (!isdigit(buf[pos]))
        return FALSE;
    for (start = pos; start > 0 && isdigit(buf[start - 1]); start--);
    for (stop = pos; stop < data->NumChars && isdigit(buf[stop]); stop++);
    if (start > 0 && buf[start - 1] == '-')
        start--;
    StrToLong(&buf[start], &value);
    length = snprintf(number, sizeof(number), "%ld", (long)(value + delta));
    if (data->NumChars - (stop - start) + length >= data->BufferSize)
        return FALSE;
    pos = data->BufferPos;
    Buffer_Delete(data, start, stop);
    Buffer_InsertText(data, start, number, length);
    data->BufferPos = MIN(pos, (LONG)data->NumChars);
    return TRUE;
}

static IPTR String_Textinput(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    BOOL writable = !(data->msd_Flags & MSDF_NOINPUT);
    ULONG oldchars = data->NumChars, oldpos = data->BufferPos;
    BOOL edited = FALSE, moved = FALSE;
    IPTR retval = TRUE, dummy = 0;
    LONG start, stop;

    switch (msg->MethodID)
    {
    case MUIM_Textinput_HandleChar:
        {
            struct MUIP_Textinput_HandleChar *m = (APTR)msg;
            UBYTE ch = m->ch;
            int r;

            if (!writable || ((ch == '\r' || ch == '\n' || ch < 32)
                && !(MULTILINE(data) && (ch == '\r' || ch == '\n')))
                || ch == 127)
                return 0;
            if ((data->msd_Flags & MSDF_ISNUMERIC) && !isdigit(ch)
                && ch != '-')
            {
                DisplayBeep(NULL);
                return 0;
            }
            r = String_HandleVanillakey(cl, obj, ch, 0, &dummy);
            edited = r == 2 || data->NumChars != oldchars;
            retval = edited;
        }
        break;

    case MUIM_Textinput_DoBS:
    case MUIM_Textinput_DoBSSOL:
    case MUIM_Textinput_DoDel:
    case MUIM_Textinput_DoDelEOL:
        if (!writable)
            return 0;
        String_HandleVanillakey(cl, obj,
            msg->MethodID == MUIM_Textinput_DoBS ||
            msg->MethodID == MUIM_Textinput_DoBSSOL ? '\b' : 127,
            msg->MethodID == MUIM_Textinput_DoBSSOL ||
            msg->MethodID == MUIM_Textinput_DoDelEOL ?
            IEQUALIFIER_LSHIFT : 0, &dummy);
        edited = data->NumChars != oldchars;
        moved = data->BufferPos != oldpos;
        break;

    case MUIM_Textinput_DoBSWord:
    case MUIM_Textinput_DoDelWord:
        if (!writable)
            return 0;
        if (!(edited = Buffer_KillMarked(data)))
        {
            stop = data->BufferPos;
            if (msg->MethodID == MUIM_Textinput_DoBSWord)
                edited = Buffer_Delete(data,
                    Buffer_GetPrevWordIndex(data, stop), stop);
            else
                edited = Buffer_Delete(data, stop,
                    Buffer_GetSuccWordIndex(data, stop));
        }
        break;

    case MUIM_Textinput_DoDelLine:
    case MUIM_Textinput_DoCutLine:
        if (!writable)
            return 0;
        start = 0;
        stop = data->NumChars;
        if (MULTILINE(data))
        {
            /* The current line with its newline */
            start = Ml_LineStart(data, data->BufferPos);
            stop = Ml_LineEnd(data, data->BufferPos);
            if (stop < (LONG)data->NumChars)
                stop++;
            else if (start > 0)
                start--;
        }
        if (msg->MethodID == MUIM_Textinput_DoCutLine && stop > start
            && !data->msd_useSecret)
            clipboard_write_text(data->Buffer + start, stop - start);
        data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
        edited = Buffer_Delete(data, start, stop);
        break;

    case MUIM_Textinput_DoCopy:
        Ti_Copy(data);
        moved = !!(data->msd_Flags & MSDF_MARKING);
        data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
        break;

    case MUIM_Textinput_DoCut:
    case MUIM_Textinput_DoCopyCut:
        /* With nothing marked, Cut clears the contents, as in MUI 5 */
        if (msg->MethodID == MUIM_Textinput_DoCopyCut || Buffer_AnythingMarked(data))
            Ti_Copy(data);
        if (!writable)
        {
            data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
            moved = TRUE;
        }
        else if (!(edited = Buffer_KillMarked(data)))
            edited = Buffer_Delete(data, 0, data->NumChars);
        break;

    case MUIM_Textinput_DoPaste:
        if (!writable)
            return 0;
        String_HandleVanillakey(cl, obj, 'v', IEQUALIFIER_RCOMMAND, &dummy);
        edited = TRUE;
        break;

    case MUIM_Textinput_DoMarkStart:
        Ti_FreezeMark(data);
        data->msd_Flags &= ~MSDF_TIMARK;
        data->MarkPos = data->BufferPos;
        data->MultiClick = 0;
        data->msd_Flags |= MSDF_MARKING | MSDF_KEYMARKING;
        moved = TRUE;
        break;

    case MUIM_Textinput_DoMarkAll:
        data->TiMarkStart = 0;
        data->TiMarkEnd = data->NumChars;
        data->BufferPos = data->NumChars;
        data->msd_Flags &= ~MSDF_KEYMARKING;
        data->msd_Flags |= MSDF_MARKING | MSDF_TIMARK;
        moved = TRUE;
        break;

    case MUIM_Textinput_DoLeft:
        moved = String_CursorKey(data, MUIKEY_LEFT, 0);
        break;
    case MUIM_Textinput_DoRight:
        moved = String_CursorKey(data, MUIKEY_RIGHT, 0);
        break;
    case MUIM_Textinput_DoPrevWord:
        moved = String_CursorKey(data, MUIKEY_WORDLEFT, 0);
        break;
    case MUIM_Textinput_DoNextWord:
        moved = String_CursorKey(data, MUIKEY_WORDRIGHT, 0);
        break;
    case MUIM_Textinput_DoLineStart:
        moved = String_CursorKey(data, MUIKEY_LINESTART, 0);
        break;
    case MUIM_Textinput_DoLineEnd:
        moved = String_CursorKey(data, MUIKEY_LINEEND, 0);
        break;
    case MUIM_Textinput_DoTop:
        moved = String_CursorKey(data, MUIKEY_NONE, 0);
        break;
    case MUIM_Textinput_DoBottom:
        moved = String_CursorKey(data, MUIKEY_NONE, data->NumChars);
        break;

    case MUIM_Textinput_DoUp:
    case MUIM_Textinput_DoDown:
    case MUIM_Textinput_DoPageUp:
    case MUIM_Textinput_DoPageDown:
        if (!MULTILINE(data))
            return 0;
        start = msg->MethodID == MUIM_Textinput_DoUp ||
            msg->MethodID == MUIM_Textinput_DoDown ? 1 :
            MAX(1, Ml_Rows(obj) - 1);
        if (msg->MethodID == MUIM_Textinput_DoUp ||
            msg->MethodID == MUIM_Textinput_DoPageUp)
            start = -start;
        moved = String_CursorKey(data, MUIKEY_NONE,
            Ml_Vertical(data, data->BufferPos, start));
        break;

    case MUIM_Textinput_DoToggleCase:
    case MUIM_Textinput_DoToggleCaseEOW:
        if (!writable)
            return 0;
        stop = msg->MethodID == MUIM_Textinput_DoToggleCase ?
            MIN(data->BufferPos + 1, data->NumChars) :
            Buffer_GetWordEndIndex(data, data->BufferPos);
        for (start = data->BufferPos; start < stop; start++)
        {
            UBYTE c = data->Buffer[start];
            data->Buffer[start] = ToUpper(c) != c ? ToUpper(c) : ToLower(c);
            edited = TRUE;
        }
        data->BufferPos = MAX((LONG)data->BufferPos, stop);
        break;

    case MUIM_Textinput_DoIncrementDec:
    case MUIM_Textinput_DoDecrementDec:
        if (!writable)
            return 0;
        edited = Ti_AddToNumber(data,
            msg->MethodID == MUIM_Textinput_DoIncrementDec ? 1 : -1);
        break;

    case MUIM_Textinput_InsertText:
    case MUIM_Textinput_AppendText:
        {
            struct MUIP_Textinput_InsertText *m = (APTR)msg;
            BOOL append = msg->MethodID == MUIM_Textinput_AppendText;
            LONG count = Buffer_InsertText(data,
                append ? data->NumChars : data->BufferPos, m->text, m->len);

            if (!append)
                data->BufferPos += count;
            edited = count > 0;
        }
        break;

    case MUIM_Textinput_SaveToFile:
        {
            struct MUIP_Textinput_SaveToFile *m = (APTR)msg;
            BPTR file = Open(m->filename, MODE_NEWFILE);

            if (!file)
                return FALSE;
            retval = Write(file, data->Buffer, data->NumChars)
                == data->NumChars;
            Close(file);
            return retval;
        }

    case MUIM_Textinput_LoadFromFile:
    case MUIM_Textinput_InsertFromFile:
        {
            struct MUIP_Textinput_LoadFromFile *m = (APTR)msg;
            STRPTR text = Ti_ReadFile(m->filename, data->BufferSize);

            if (!text)
                return FALSE;
            if (msg->MethodID == MUIM_Textinput_LoadFromFile)
            {
                data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
                Buffer_Delete(data, 0, data->NumChars);
                data->BufferPos = 0;
            }
            data->BufferPos += Buffer_InsertText(data, data->BufferPos,
                text, -1);
            FreeVec(text);
            edited = TRUE;
        }
        break;

    case MUIM_Textinput_Acknowledge:
        {
            LONG val = 0;

            data->AckBuffer = data->Buffer;
            set(obj, MUIA_String_Acknowledge, data->Buffer);
            SetSuperAttrs(cl, obj, MUIA_Textinput_Acknowledge,
                (IPTR)data->Buffer, TAG_DONE);
            if (StrToLong(data->Buffer, &val) >= 0)
                superset(cl, obj, MUIA_String_Integer, val);
        }
        return TRUE;

    case MUIM_Textinput_DoNextGadget:
        if (muiRenderInfo(obj) && _win(obj))
            set(_win(obj), MUIA_Window_ActiveObject,
                MUIV_Window_ActiveObject_Next);
        return TRUE;

    default:
        /* Undo, files requesters, popups, URLs and the multiline
         * movements are not available in a single-line String */
        return 0;
    }

    if (edited)
        data->msd_RedrawReason = data->msd_RedrawReason == DO_ADDCHAR ?
            DO_ADDCHAR : NEW_CONTENTS;
    else if (moved && data->msd_RedrawReason == NO_REASON)
        data->msd_RedrawReason = MOVE_CURSOR;
    Ti_Done(cl, obj, data, edited, moved || edited);
    return retval;
}

/* Key input as MUI 5 passes it to Textinput methods. Returns TIP_* */
static ULONG String_KeyToTextinput(Object *obj, struct MUI_StringData *data,
    UBYTE code, UWORD qual, BOOL *handled)
{
    BOOL shift = (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
    ULONG method;

    *handled = TRUE;
    if (qual & IEQUALIFIER_RCOMMAND)
    {
        switch (ToLower(code))
        {
        case 'c': method = MUIM_Textinput_DoCopy; break;
        case 'x': method = MUIM_Textinput_DoCut; break;
        case 'v': method = MUIM_Textinput_DoPaste; break;
        case 'z': method = MUIM_Textinput_DoUndo; break;
        default: *handled = FALSE; return 0;
        }
        return Ti_Call(obj, data, method, 0);
    }
    switch (code)
    {
    case '\b':
        method = shift ? MUIM_Textinput_DoBSSOL : MUIM_Textinput_DoBS;
        break;
    case 21: /* Ctrl-U */
        method = MUIM_Textinput_DoBSSOL;
        break;
    case 127:
        method = shift ? MUIM_Textinput_DoDelEOL : MUIM_Textinput_DoDel;
        break;
    case 11: /* Ctrl-K */
        method = MUIM_Textinput_DoDelEOL;
        break;
    case 24: /* Ctrl-X */
        method = MUIM_Textinput_DoCut;
        break;
    case 1: /* Ctrl-A */
        method = MUIM_Textinput_DoLineStart;
        break;
    case 26: /* Ctrl-Z */
        method = MUIM_Textinput_DoLineEnd;
        break;
    case '\r':
        if (MULTILINE(data))
            return Ti_Call(obj, data, MUIM_Textinput_HandleChar, '\n');
        /* Acknowledge follows from MUIKEY_PRESS */
        Ti_Call(obj, data, MUIM_Textinput_HandleChar, code);
        *handled = FALSE;
        return 0;
    default:
        if (code < 32 || (code >= 128 && code < 160))
        {
            *handled = FALSE;
            return 0;
        }
        return Ti_Call(obj, data, MUIM_Textinput_HandleChar, code);
    }
    return Ti_Call(obj, data, method, 0);
}

/**************************************************************************
 MUIM_HandleEvent
**************************************************************************/
IPTR String__MUIM_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct MUI_StringData *data =
        (struct MUI_StringData *)INST_DATA(cl, obj);
    IPTR retval = 0;
    ULONG hook_actions = 0;
    int update = 0;
    BOOL edited = FALSE;
    LONG muikey = msg->muikey;
    BOOL acknowledge = FALSE;
    STRPTR previous = NULL;
    LONG oldpos = data->BufferPos;
    ULONG oldflags = data->msd_Flags, oldmark = data->MarkPos;
    BOOL hook_key = msg->imsg && msg->imsg->Class == IDCMP_RAWKEY &&
        !(msg->imsg->Code & IECODE_UP_PREFIX) && data->is_active;
    BOOL hook_click = msg->imsg && msg->imsg->Class == IDCMP_MOUSEBUTTONS &&
        msg->imsg->Code == SELECTDOWN &&
        _isinobject(obj, msg->imsg->MouseX, msg->imsg->MouseY);
    if (data->msd_EditHook && (hook_key || hook_click))
    {
        previous = AllocVec(data->BufferSize, MEMF_ANY);
        if (!previous) return MUI_EventHandlerRC_Eat;
        memcpy(previous, data->Buffer, data->BufferSize);
    }

    /* LonelyEditHook still receives the ordinary proposed edit, as with
     * Intuition SGWork. It does not suppress character editing itself. */

    /* Convert raw keys to MUI keys for marking */
    if (muikey == MUIKEY_NONE && msg->imsg != NULL)
    {
        if (msg->imsg->Class == IDCMP_RAWKEY)
        {
            static LONG muikeytable[3][2] = {
                {MUIKEY_LEFT, MUIKEY_RIGHT},
                {MUIKEY_WORDLEFT, MUIKEY_WORDRIGHT},
                {MUIKEY_LINESTART, MUIKEY_LINEEND}
            };
            WORD dirindex = -1, amountindex = 0;

            switch (msg->imsg->Code)
            {
            case 0x4F:
                dirindex = 0;
                break;

            case 0x4E:
                dirindex = 1;
                break;

#ifdef __AROS__
            case RAWKEY_HOME:
                muikey = MUIKEY_LINESTART;
                break;

            case RAWKEY_END:
                muikey = MUIKEY_LINEEND;
                break;
#endif

            }

            if (dirindex != -1)
            {
                if (msg->imsg->
                    Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
                {
                    amountindex = 2;
                }
                else if (msg->imsg->Qualifier & IEQUALIFIER_CONTROL)
                {
                    amountindex = 1;
                }

                muikey = muikeytable[amountindex][dirindex];
            }

        }
    }

    D(bug("[MUI:String] HandleEvent: muikey %d, imsg %p is_active=%d\n", muikey,
            msg->imsg, data->is_active));
    if (muikey != MUIKEY_NONE && data->is_active)
    {
        retval = MUI_EventHandlerRC_Eat;

        /* Vertical movement within a Multiline String */
        if (MULTILINE(data) && muikey >= MUIKEY_UP && muikey <= MUIKEY_BOTTOM)
        {
            static const ULONG methods[] = {
                MUIM_Textinput_DoUp, MUIM_Textinput_DoDown,
                MUIM_Textinput_DoPageUp, MUIM_Textinput_DoPageDown,
                MUIM_Textinput_DoTop, MUIM_Textinput_DoBottom
            };
            ULONG pending = Ti_Call(obj, data,
                methods[muikey - MUIKEY_UP], 0);
            if (pending & TIP_EDITED)
                edited = TRUE;
            if (pending)
                update = 1;
        }
        else switch (muikey)
        {
        case MUIKEY_LEFT:
        case MUIKEY_RIGHT:
        case MUIKEY_WORDLEFT:
        case MUIKEY_WORDRIGHT:
        case MUIKEY_LINESTART:
        case MUIKEY_LINEEND:
            {
                static const ULONG methods[] = {
                    MUIM_Textinput_DoLeft, MUIM_Textinput_DoRight,
                    MUIM_Textinput_DoPrevWord, MUIM_Textinput_DoNextWord,
                    MUIM_Textinput_DoLineStart, MUIM_Textinput_DoLineEnd
                };
                ULONG pending = Ti_Call(obj, data,
                    methods[muikey == MUIKEY_LEFT ? 0 :
                        muikey == MUIKEY_RIGHT ? 1 :
                        muikey == MUIKEY_WORDLEFT ? 2 :
                        muikey == MUIKEY_WORDRIGHT ? 3 :
                        muikey == MUIKEY_LINESTART ? 4 : 5], 0);
                if (pending & TIP_EDITED)
                    edited = TRUE;
                if (pending)
                    update = 1;
            }
            break;

        case MUIKEY_UP:
            if (data->KeyUpFocus && muiRenderInfo(obj) && _win(obj))
                set(_win(obj), MUIA_Window_ActiveObject, data->KeyUpFocus);
            else if (data->msd_AttachedList)
                set(data->msd_AttachedList,
                    MUIA_List_Active, MUIV_List_Active_Up);
            break;
        case MUIKEY_DOWN:
            if (data->KeyDownFocus && muiRenderInfo(obj) && _win(obj))
                set(_win(obj), MUIA_Window_ActiveObject, data->KeyDownFocus);
            else if (data->msd_AttachedList)
                set(data->msd_AttachedList,
                    MUIA_List_Active, MUIV_List_Active_Down);
            break;
        case MUIKEY_PAGEUP:
            if (data->msd_AttachedList)
                set(data->msd_AttachedList,
                    MUIA_List_Active, MUIV_List_Active_PageUp);
            break;
        case MUIKEY_PAGEDOWN:
            if (data->msd_AttachedList)
                set(data->msd_AttachedList,
                    MUIA_List_Active, MUIV_List_Active_PageDown);
            break;
        case MUIKEY_TOP:
            if (data->msd_AttachedList)
                set(data->msd_AttachedList,
                    MUIA_List_Active, MUIV_List_Active_Top);
            break;
        case MUIKEY_BOTTOM:
            if (data->msd_AttachedList)
                set(data->msd_AttachedList,
                    MUIA_List_Active, MUIV_List_Active_Bottom);
            break;
        case MUIKEY_PRESS:
            /* Return is a newline in a Multiline String */
            acknowledge = !MULTILINE(data);
            break;

        default:
            retval = 0;
        }                       // switch(muikey)
    }                           // if (muikey != MUIKEY_NONE)

    if (msg->imsg)
    {
        UWORD code = msg->imsg->Code;
        //UWORD qual = msg->imsg->Qualifier;
        WORD x = msg->imsg->MouseX;
        WORD y = msg->imsg->MouseY;

        //bug("String_HandleEvent: parsing imsg %p, class=%ld\n",
        //    msg->imsg, msg->imsg->Class);

        switch (msg->imsg->Class)
        {
        case IDCMP_MOUSEBUTTONS:       /* set cursor and activate it */
            if (code == SELECTDOWN)
            {
                //bug("String_HandleEvent: code == SELECTDOWN, x=%d y=%d\n",
                //    x, y);

                if (_isinobject(obj, x, y))
                {
                    UWORD text_left, text_right;

                    retval = MUI_EventHandlerRC_Eat;

                    CurrentTime(&data->NewClick_Sec, &data->NewClick_Micro);
                    if (DoubleClick(data->OldClick_Sec,
                            data->OldClick_Micro, data->NewClick_Sec,
                            data->NewClick_Micro))
                    {
                        data->MultiClick++;
                    }
                    else
                    {
                        data->MultiClick = 0;
                    }
                    data->OldClick_Sec = data->NewClick_Sec;
                    data->OldClick_Micro = data->NewClick_Micro;
                    if (TI_MARKED(data))
                        update = 1;
                    data->msd_Flags &= ~MSDF_TIMARK;

                    //kprintf("multiclick %d\n", data->MultiClick);

                    if (!data->is_active)
                    {
                        //bug("String got button, lets activate\n");
                        data->is_active = TRUE;
                        data->msd_RedrawReason = WENT_ACTIVE;
                        // redraw
                        set(obj, MUIA_Background,
                            (IPTR) muiGlobalInfo(obj)->mgi_Prefs->
                            string_bg_active);

                        /* An edit ended by a hook leaves the window's
                         * active object unchanged, so setting it again
                         * would not restore key input. */
                        if (XGET(_win(obj), MUIA_Window_ActiveObject)
                            == (IPTR)obj)
                            DoMethod(obj, MUIM_GoActive);
                        else
                            set(_win(obj), MUIA_Window_ActiveObject, obj);
                        // let other objects a chance to get desactivated
                        //retval = 0;
                    }

                    if (!(data->ehn.ehn_Events & IDCMP_MOUSEMOVE))
                    {
                        DoMethod(_win(obj), MUIM_Window_RemEventHandler,
                            (IPTR) & data->ehn);
                        data->ehn.ehn_Events |= IDCMP_MOUSEMOVE;
                        DoMethod(_win(obj), MUIM_Window_AddEventHandler,
                            (IPTR) & data->ehn);
                    }

                    PrepareVisualBuffer(data);
                    text_left = GetTextLeft(cl, obj);
                    text_right = GetTextRight(cl, obj);
                    CleanVisualBuffer(data);

                    if (MULTILINE(data))
                    {
                        LONG newpos = Ml_PosAt(obj, data, x, y);

                        if ((LONG)data->BufferPos != newpos)
                        {
                            data->BufferPos = newpos;
                            update = 1;
                        }
                    }
                    /* Check if mouseclick is inside displayed text */
                    else if ((x >= text_left) && (x <= text_right))
                    {
                        /* Find new cursor pos. */
                        struct TextExtent te;
                        ULONG newpos;
                        STRPTR dispstr = data->Buffer + data->DispPos;

                        newpos = data->DispPos
                            + TextFit(_rp(obj), dispstr,
                            data->NumChars - data->DispPos, &te, NULL, 1,
                            x - text_left, _rp(obj)->Font->tf_YSize);

                        if (data->BufferPos != newpos)
                        {
                            data->BufferPos = newpos;
                            update = 1;
                        }
                    }
                    else if (x < text_left)
                    {
                        /* Click on empty space at left. Set cursor to first
                         * visible */
                        if (data->BufferPos != data->DispPos)
                        {
                            data->BufferPos = data->DispPos;
                            update = 1;
                        }
                    }
                    else
                    {
                        /* Click on empty space at right. Set cursor to last
                         * visible */
                        if (data->BufferPos !=
                            data->DispPos + data->DispCount)
                        {
                            data->BufferPos =
                                data->DispPos + data->DispCount;
                            update = 1;
                        }
                    }           /* if (click is on text or not) */

                    data->MarkPos = data->BufferPos;

                    if (data->MultiClick == 0)
                    {
                        if (data->msd_Flags & MSDF_MARKING)
                        {
                            data->msd_Flags &= ~MSDF_MARKING;
                            update = 1;
                        }
                    }
                    else if (data->MultiClick
                        && Buffer_GetMarkedRange(data, NULL, NULL))
                    {
                        data->msd_Flags |= MSDF_MARKING;
                        update = 1;
                    }


                }               /* is in object */
                else if (data->is_active
                    && !(data->msd_Flags & MSDF_STAYACTIVE))
                        /* and click not on object */
                {
                    data->is_active = FALSE;
                    set(obj, MUIA_Background,
                        (IPTR) muiGlobalInfo(obj)->mgi_Prefs->
                        string_bg_inactive);
                    //DoMethod(obj, MUIM_GoInactive);
                    // let other objects a chance to get activated
                    //retval = 0;
                }
            }                   /* if (code == SELECTDOWN) */
            else if (code == SELECTUP)
            {
                if (data->ehn.ehn_Events & IDCMP_MOUSEMOVE)
                {
                    DoMethod(_win(obj), MUIM_Window_RemEventHandler,
                        (IPTR) & data->ehn);
                    data->ehn.ehn_Events &= ~IDCMP_MOUSEMOVE;
                    DoMethod(_win(obj), MUIM_Window_AddEventHandler,
                        (IPTR) & data->ehn);
                }
            }
            break;

        case IDCMP_MOUSEMOVE:
            if (data->is_active)
            {
                UWORD text_left, text_right;

                if (!(data->msd_Flags & MSDF_MARKING))
                {
                    data->msd_Flags |= MSDF_MARKING;
                }

                PrepareVisualBuffer(data);
                text_left = GetTextLeft(cl, obj);
                text_right = GetTextRight(cl, obj);
                CleanVisualBuffer(data);

                /* Check if mouseclick is inside displayed text */
                if (MULTILINE(data) || ((x >= text_left) && (x <= text_right)))
                {
                    /* Find new cursor pos. */
                    struct TextExtent te;
                    ULONG newpos;
                    STRPTR dispstr = data->Buffer + data->DispPos;

                    newpos = MULTILINE(data) ? Ml_PosAt(obj, data, x, y) :
                        data->DispPos + TextFit(_rp(obj), dispstr,
                        data->NumChars - data->DispPos, &te, NULL, 1,
                        x - text_left, _rp(obj)->Font->tf_YSize);

                    if (data->BufferPos != newpos)
                    {
                        LONG old_markstart = 0, old_markstop = 0;
                        LONG markstart = 0, markstop = 0;
                        BOOL was_marked, is_marked;

                        was_marked = Buffer_AnythingMarked(data) &&
                            Buffer_GetMarkedRange(data, &old_markstart,
                            &old_markstop);

                        data->BufferPos = newpos;

                        is_marked = Buffer_AnythingMarked(data) &&
                            Buffer_GetMarkedRange(data, &markstart,
                            &markstop);

                        if ((was_marked != is_marked) ||
                            (old_markstart != markstart) ||
                            (old_markstop != markstop))
                        {
                            update = 1;
                        }
                    }
                }
                else if ((x < text_left) && (data->BufferPos > 0))
                {
                    data->BufferPos--;
                    update = 1;
                    data->msd_RedrawReason = DO_CURSOR_LEFT;
                }
                else if ((x > text_right)
                    && (data->BufferPos < data->NumChars))
                {
                    data->BufferPos++;
                    update = 1;
                    data->msd_RedrawReason = DO_CURSOR_RIGHT;
                }
                //kprintf("  ---- bp: %d\n", data->BufferPos);
            }
            break;

        case IDCMP_RAWKEY:
            {
                unsigned char code;

                //bug("String_HandleEvent: idcmp_rawkey\n");

                if (!data->is_active)
                    break;

                code = ConvertKey(msg->imsg);
                if (!code)
                {
                    switch (msg->imsg->Code)
                    {
                    case 0x64: /* LALT */
                    case 0x65: /* RALT */
                    case 0x64 | IECODE_UP_PREFIX:
                    case 0x65 | IECODE_UP_PREFIX:
                        if (msg->imsg->
                            Qualifier & (IEQUALIFIER_LALT |
                                IEQUALIFIER_RALT))
                        {
                            data->MarkPos = data->BufferPos;
                            data->msd_Flags &= ~MSDF_TIMARK;
                            data->msd_Flags |=
                                (MSDF_MARKING | MSDF_KEYMARKING);
                        }
                        else
                        {
                            data->msd_Flags &= ~MSDF_KEYMARKING;
                        }
                        break;

                    }
                }

                if (code)
                {
                    BOOL handled;
                    ULONG pending = String_KeyToTextinput(obj, data, code,
                        msg->imsg->Qualifier, &handled);

                    if (handled)
                        retval = MUI_EventHandlerRC_Eat;
                    if (pending)
                        update = 1;
                    if (pending & TIP_EDITED)
                        edited = TRUE;
                }
            }
            break;
        }
    }

    if (previous)
    {
        struct InputEvent event = {0};
        struct StringExtend extension = {0};
        struct StringInfo info = {0};
        struct Gadget gadget = {0};
        struct GadgetInfo gi = {0};
        struct SGWork work = {0};
        ULONG command = hook_key ? SGH_KEY : SGH_CLICK;
        BOOL handled;
        event.ie_Class = hook_key ? IECLASS_RAWKEY : IECLASS_RAWMOUSE;
        event.ie_Code = msg->imsg->Code;
        event.ie_Qualifier = msg->imsg->Qualifier;
        info.Buffer = previous;
        info.BufferPos = oldpos;
        info.NumChars = strlen(previous);
        info.MaxChars = data->BufferSize;
        info.Extension = &extension;
        extension.WorkBuffer = data->Buffer;
        extension.EditHook = data->msd_EditHook;
        gadget.SpecialInfo = &info;
        gadget.LeftEdge = _mleft(obj); gadget.TopEdge = _mtop(obj);
        gadget.Width = _mwidth(obj); gadget.Height = _mheight(obj);
        gi.gi_Window = _window(obj); gi.gi_Screen = _screen(obj);
        gi.gi_RastPort = _rp(obj);
        work.Gadget = &gadget; work.StringInfo = &info;
        work.GadgetInfo = &gi; work.IEvent = &event;
        work.WorkBuffer = data->Buffer; work.PrevBuffer = previous;
        work.BufferPos = data->BufferPos; work.NumChars = data->NumChars;
        work.Code = hook_key ? ConvertKey(msg->imsg) : 0;
        work.Actions = SGA_USE | SGA_REDISPLAY;
        work.EditOp = EO_NOOP;
        if (acknowledge) work.EditOp = EO_ENTER;
        else if (data->NumChars > info.NumChars) work.EditOp = EO_INSERTCHAR;
        else if (data->NumChars < info.NumChars)
        {
            work.EditOp = EO_DELFORWARD;
            if (work.Code == 8) work.Code = 127;
        }
        else if (data->BufferPos != oldpos) work.EditOp = EO_MOVECURSOR;
        else if (strcmp((char *)previous, (char *)data->Buffer)) work.EditOp = EO_REPLACECHAR;
        handled = CallHookPkt(data->msd_EditHook, &work, &command);
        if (handled && (work.Actions & SGA_USE))
        {
            LONG length = MAX(0, MIN((LONG)data->BufferSize - 1, work.NumChars));
            data->Buffer[length] = 0;
            data->NumChars = strlen((char *)data->Buffer);
            data->BufferPos = MAX(0, MIN((LONG)data->NumChars, work.BufferPos));
        }
        else if (handled)
        {
            memcpy(data->Buffer, previous, data->BufferSize);
            data->NumChars = info.NumChars;
            data->BufferPos = oldpos;
            data->msd_Flags = oldflags;
            data->MarkPos = oldmark;
        }
        if (handled && hook_key) hook_actions = work.Actions;
        if (handled && (work.Actions & SGA_BEEP)) DisplayBeep(_screen(obj));
        edited = strcmp((char *)previous, (char *)data->Buffer) != 0;
        update = TRUE;
        retval = MUI_EventHandlerRC_Eat;
        FreeVec(previous);
    }

    /* Trigger notification */
    if (edited)
    {
        data->AckBuffer = NULL;
        data->msd_Flags |= MSDF_CHANGED;
        SetSuperAttrs(cl, obj, MUIA_String_Contents, (IPTR)data->Buffer,
            MUIA_Textinput_Contents, (IPTR)data->Buffer, TAG_DONE);
    }

    if (update)
    {
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    }
    if ((hook_actions & SGA_END) && !acknowledge)
    {
        /* Ending an edit need not clear Window_ActiveObject. The hook can
         * explicitly request a cycle-chain move, as in MUI 21.227. */
        if (hook_actions & (SGA_NEXTACTIVE | SGA_PREVACTIVE))
            set(_win(obj), MUIA_Window_ActiveObject,
                (hook_actions & SGA_PREVACTIVE) ? MUIV_Window_ActiveObject_Prev : MUIV_Window_ActiveObject_Next);
        else DoMethod(obj, MUIM_GoInactive);
        return (hook_actions & SGA_REUSE) ? 0 : MUI_EventHandlerRC_Eat;
    }
    if (acknowledge)
            {
                UBYTE *buf = NULL;

                get(obj, MUIA_String_Contents, &buf);

                if (data->msd_Flags & MSDF_STAYACTIVE)
                {
                    /* Do not change active object */
                }
                else if (data->msd_Flags & MSDF_ADVANCEONCR)
                {
                    set(_win(obj), MUIA_Window_ActiveObject,
                        MUIV_Window_ActiveObject_Next);
                }
                else if (!(data->msd_Flags & MSDF_STAYACTIVE))
                {
                    set(_win(obj), MUIA_Window_ActiveObject,
                        MUIV_Window_ActiveObject_None);
                }

                /* Notify listeners of the new string and integer value */
                data->msd_Flags &= ~MSDF_CHANGED;
                DoMethod(obj, MUIM_Textinput_Acknowledge, (IPTR)buf);
            }
    //D(bug("String eh return %ld\n", retval));

    return retval;
}


/**************************************************************************
 MUIM_Export : to export an object's "contents" to a dataspace object.
**************************************************************************/
IPTR String__MUIM_Export(struct IClass *cl, Object *obj,
    struct MUIP_Export *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    ULONG id;

    if ((id = muiNotifyData(obj)->mnd_ObjectID))
    {
        if (data->Buffer != NULL)
            DoMethod(msg->dataspace, MUIM_Dataspace_Add,
                (IPTR) data->Buffer, data->NumChars + 1, (IPTR) id);
        else
            DoMethod(msg->dataspace, MUIM_Dataspace_Remove, (IPTR) id);
    }

    return 0;
}


/**************************************************************************
 MUIM_Import : to import an object's "contents" from a dataspace object.
**************************************************************************/
IPTR String__MUIM_Import(struct IClass *cl, Object *obj,
    struct MUIP_Import *msg)
{
    ULONG id;
    STRPTR s;

    if ((id = muiNotifyData(obj)->mnd_ObjectID))
    {
        if ((s = (STRPTR) DoMethod(msg->dataspace, MUIM_Dataspace_Find,
                    (IPTR) id)))
        {
            set(obj, MUIA_String_Contents, s);
        }
    }

    return 0;
}

/**************************************************************************
 MUIM_GoActive
**************************************************************************/
IPTR String__MUIM_GoActive(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_StringData *data =
        (struct MUI_StringData *)INST_DATA(cl, obj);

    //D(bug("String_GoActive %p\n", obj));
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) & data->ehn);
    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) & data->ehn);
    data->is_active = TRUE;
    data->msd_Flags &= ~MSDF_KEYMARKING;
    data->msd_RedrawReason = WENT_ACTIVE;
    // redraw
    set(obj, MUIA_Background,
        (IPTR) muiGlobalInfo(obj)->mgi_Prefs->string_bg_active);

    return 0;
}

/**************************************************************************
 MUIM_GoInactive
**************************************************************************/
IPTR String__MUIM_GoInactive(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_StringData *data =
        (struct MUI_StringData *)INST_DATA(cl, obj);

    //D(bug("String_GoInactive %p\n", obj));

    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) & data->ehn);
    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) & data->ehn);
    data->is_active = FALSE;
    data->msd_RedrawReason = WENT_INACTIVE;
    data->MultiClick = 0;

    // redraw
    set(obj, MUIA_Background,
        (IPTR) muiGlobalInfo(obj)->mgi_Prefs->string_bg_inactive);

    return 0;
}

/**************************************************************************
 MUIM_String_ClearSelected (BetterString)
**************************************************************************/
IPTR String__MUIM_ClearSelected(struct IClass *cl, Object *obj,
    struct MUIP_String_ClearSelected *msg)
{
    struct MUI_StringData *data =
        (struct MUI_StringData *)INST_DATA(cl, obj);

    //D(bug("String_ClearSelected %p\n", obj));

    if (Buffer_KillMarked(data))
    {
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    }

    return 0;
}

/**************************************************************************
 MUIM_String_Insert (BetterString)
**************************************************************************/
IPTR String__MUIM_Insert(struct IClass *cl, Object *obj,
    struct MUIP_String_Insert *msg)
{
    struct MUI_StringData *data =
        (struct MUI_StringData *)INST_DATA(cl, obj);
    LONG pos;
    ULONG old_bufferpos;
    ULONG num_inserted = 0;

    //D(bug("String_Insert %p\n", obj));

    switch ((ULONG) msg->pos)
    {
    case MUIV_String_Insert_StartOfString:
        pos = 0;
        break;

    case MUIV_String_Insert_EndOfString:
        pos = data->NumChars;
        break;

    case MUIV_String_Insert_BufferPos:
        pos = data->BufferPos;
        break;

    default:
        pos = msg->pos;
        break;
    }

    if ((pos < 0) || (pos > data->NumChars))
        return 0;

    old_bufferpos = data->BufferPos;
    data->BufferPos = pos;

    while (msg->text[num_inserted]
        && Buffer_AddChar(data, msg->text[num_inserted]))
    {
        num_inserted++;
    }

    if (num_inserted)
    {
        if (old_bufferpos >= pos)
        {
            data->BufferPos = old_bufferpos + num_inserted;
        }
        else
        {
            data->BufferPos = old_bufferpos;
        }

        MUI_Redraw(obj, MADF_DRAWUPDATE);
    }

    return 0;
}

/**************************************************************************
 MUIM_String_FileNameStart (BetterString)
**************************************************************************/
IPTR String__MUIM_FileNameStart(struct IClass *cl, Object *obj,
    struct MUIP_String_FileNameStart *msg)
{
    struct MUI_StringData *data =
        (struct MUI_StringData *)INST_DATA(cl, obj);
    STRPTR buf;

    //D(bug("String_FileNameStart %p\n", obj));

    buf = data->Buffer;

    // TODO: Return the partial filename start position, not the buffer pointer.

    return (IPTR) buf;
}

/* Keep the default editing menu private; application-supplied menus retain
 * Area's normal ownership and notification rules. */
static IPTR String_ContextBuild(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    IPTR menu = 0;
    get(obj, MUIA_ContextMenu, &menu);
    if (menu != 1) return DoSuperMethodA(cl, obj, msg);
    if (!(_flags(obj) & MADF_SETUP)) return 1;
    if (!data->edit_menu)
    {
        struct NewMenu items[] = {
            {NM_TITLE, "Edit", NULL, 0, 0, NULL},
            {NM_ITEM, "Cut", "X", 0, 0, (APTR)1},
            {NM_ITEM, "Copy", "C", 0, 0, (APTR)2},
            {NM_ITEM, "Paste", "V", 0, 0, (APTR)3},
            {NM_ITEM, "Select all", NULL, 0, 0, (APTR)4},
            {NM_END, NULL, NULL, 0, 0, NULL}
        };
        IPTR params[] = {(IPTR)items, 0};
        data->edit_menu = MUI_MakeObjectA(MUIO_MenustripNM, params);
    }
    if (data->edit_menu)
    {
        BOOL selected = Buffer_AnythingMarked(data);
        BOOL writable = !(data->msd_Flags & MSDF_NOINPUT);
        DoMethod(data->edit_menu, MUIM_SetUData, 1, MUIA_Menuitem_Enabled,
            selected && writable && !data->msd_useSecret);
        DoMethod(data->edit_menu, MUIM_SetUData, 2, MUIA_Menuitem_Enabled,
            selected && !data->msd_useSecret);
        DoMethod(data->edit_menu, MUIM_SetUData, 3, MUIA_Menuitem_Enabled, writable);
    }
    return (IPTR)data->edit_menu;
}

static IPTR String_ContextChoice(struct IClass *cl, Object *obj,
    struct MUIP_ContextMenuChoice *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    IPTR menu = 0, action = 0, retval = 0;
    get(obj, MUIA_ContextMenu, &menu);
    if (menu != 1) return DoSuperMethodA(cl, obj, (Msg)msg);
    get(msg->item, MUIA_UserData, &action);
    if (action == 4)
    {
        data->BufferPos = 0;
        Buffer_Select(data, data->NumChars);
    }
    else if (action >= 1 && action <= 3)
    {
        if (action != 3 && data->msd_useSecret) return 0;
        if (String_HandleVanillakey(cl, obj, "xcv"[action - 1],
            IEQUALIFIER_RCOMMAND, &retval) && action != 2)
        {
            data->AckBuffer = NULL;
            superset(cl, obj, MUIA_String_Contents, data->Buffer);
        }
    }
    MUI_Redraw(obj, MADF_DRAWUPDATE);
    return 0;
}

BOOPSI_DISPATCHER(IPTR, String_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return String__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return String__OM_DISPOSE(cl, obj, msg);
    case OM_SET:
        return String__OM_SET(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return String__OM_GET(cl, obj, (struct opGet *)msg);

    case MUIM_Setup:
        return String__MUIM_Setup(cl, obj, (APTR) msg);
    case MUIM_Cleanup:
        return String__MUIM_Cleanup(cl, obj, (APTR) msg);
    case MUIM_AskMinMax:
        return String__MUIM_AskMinMax(cl, obj, (APTR) msg);
    case MUIM_Draw:
        return String__MUIM_Draw(cl, obj, (APTR) msg);
    case MUIM_Export:
        return String__MUIM_Export(cl, obj, (APTR) msg);
    case MUIM_Import:
        return String__MUIM_Import(cl, obj, (APTR) msg);
    case MUIM_GoActive:
        return String__MUIM_GoActive(cl, obj, (APTR) msg);
    case MUIM_GoInactive:
        return String__MUIM_GoInactive(cl, obj, (APTR) msg);
    case MUIM_HandleEvent:
        return String__MUIM_HandleEvent(cl, obj, (APTR) msg);

    /* BetterString */
    case MUIM_ContextMenuBuild:
        return String_ContextBuild(cl, obj, msg);
    case MUIM_ContextMenuChoice:
        return String_ContextChoice(cl, obj, (APTR)msg);
    case MUIM_String_ClearSelected:
        return String__MUIM_ClearSelected(cl, obj, (APTR) msg);
    case MUIM_String_Insert:
        return String__MUIM_Insert(cl, obj, (APTR) msg);
    case MUIM_String_FileNameStart:
        return String__MUIM_FileNameStart(cl, obj, (APTR) msg);
    }

    if (msg->MethodID >= MUIM_Textinput_ExternalEdit
        && msg->MethodID <= MUIM_Textinput_DoCopyCut)
        return String_Textinput(cl, obj, msg);

    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_String_desc =
{
    MUIC_String,
    MUIC_Area,
    sizeof(struct MUI_StringData),
    (void *)String_Dispatcher
};

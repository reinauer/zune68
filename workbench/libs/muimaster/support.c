/*
    Copyright (C) 2002, The AROS Development Team.
    All rights reserved.
    
*/

#include <string.h>

#include <intuition/classes.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/keymap.h>
#include <proto/utility.h>

#include "mui.h"
#include "support.h"
#include "muimaster_intern.h"

extern struct Library *MUIMasterBase;

/**************************************************************************
 check if region is entirely within given bounds
**************************************************************************/
int isRegionWithinBounds(struct Region *r, int left, int top, int width,
    int height)
{
    if ((left <= r->bounds.MinX) && (left + width - 1 >= r->bounds.MaxX)
        && (top <= r->bounds.MinY) && (top + height - 1 >= r->bounds.MaxY))
        return 1;

    return 0;
}


/**************************************************************************
 Converts a Rawkey to a vanillakey
**************************************************************************/
ULONG ConvertKey(struct IntuiMessage *message)
{
    struct InputEvent event = { 0 };
    UBYTE character = 0;
    event.ie_Class = IECLASS_RAWKEY;
    event.ie_Code = message->Code;
    event.ie_Qualifier = message->Qualifier;
    event.ie_EventAddress = message->IAddress ? *(APTR *)message->IAddress : NULL;
    if (MapRawKey(&event, (STRPTR)&character, 1, NULL) <= 0) return 0;
    return character;
}

/**************************************************************************
 Convenient way to get an attribute of an object easily. If the object
 doesn't support the attribute this call returns an undefined value. So use
 this call only if the attribute is known to be known by the object.
 Implemented as a macro when compiling with GCC.
**************************************************************************/
#ifndef __GNUC__
IPTR XGET(Object * obj, Tag attr)
{
    IPTR storage = 0;
    GetAttr(attr, obj, &storage);
    return storage;
}
#endif /* __GNUC__ */

/**************************************************************************
 Call the Setup Method of an given object, but before set the renderinfo
**************************************************************************/
IPTR DoSetupMethod(Object * obj, struct MUI_RenderInfo * info)
{
    /* MUI set the correct render info *before* it calls MUIM_Setup so please
     * only use this function instead of DoMethodA() */
    muiRenderInfo(obj) = info;
    return DoMethod(obj, MUIM_Setup, (IPTR) info);
}

IPTR DoShowMethod(Object * obj)
{
    IPTR ret;

    ret = DoMethod(obj, MUIM_Show);
    if (ret)
        _flags(obj) |= MADF_CANDRAW;
    return ret;
}

IPTR DoHideMethod(Object * obj)
{
    _flags(obj) &= ~MADF_CANDRAW;
    return DoMethod(obj, MUIM_Hide);
}


void *Node_Next(APTR node)
{
    if (node == NULL)
        return NULL;
    if (((struct MinNode *)node)->mln_Succ == NULL)
        return NULL;
    if (((struct MinNode *)node)->mln_Succ->mln_Succ == NULL)
        return NULL;
    return ((struct MinNode *)node)->mln_Succ;
}

void *List_First(APTR list)
{
    if (!((struct MinList *)list)->mlh_Head)
        return NULL;
    if (((struct MinList *)list)->mlh_Head->mln_Succ == NULL)
        return NULL;
    return ((struct MinList *)list)->mlh_Head;
}

/* subtract rectangle b from rectangle b. resulting rectangles will be put into
   destrectarray which must have place for at least 4 rectangles. Returns number
   of resulting rectangles */

WORD SubtractRectFromRect(struct Rectangle *a, struct Rectangle *b,
    struct Rectangle *destrectarray)
{
    struct Rectangle intersect;
    BOOL intersecting = FALSE;
    WORD numrects = 0;

    /* calc. intersection between a and b */

    if (a->MinX <= b->MaxX)
    {
        if (a->MinY <= b->MaxY)
        {
            if (a->MaxX >= b->MinX)
            {
                if (a->MaxY >= b->MinY)
                {
                    intersect.MinX = MAX(a->MinX, b->MinX);
                    intersect.MinY = MAX(a->MinY, b->MinY);
                    intersect.MaxX = MIN(a->MaxX, b->MaxX);
                    intersect.MaxY = MIN(a->MaxY, b->MaxY);

                    intersecting = TRUE;
                }
            }
        }
    }

    if (!intersecting)
    {
        destrectarray[numrects++] = *a;

    }                           /* not intersecting */
    else
    {
        if (intersect.MinY > a->MinY)   /* upper */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = a->MinY;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = intersect.MinY - 1;

            numrects++;
            destrectarray++;
        }

        if (intersect.MaxY < a->MaxY)   /* lower */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = intersect.MaxY + 1;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = a->MaxY;

            numrects++;
            destrectarray++;
        }

        if (intersect.MinX > a->MinX)   /* left */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = intersect.MinY;
            destrectarray->MaxX = intersect.MinX - 1;
            destrectarray->MaxY = intersect.MaxY;

            numrects++;
            destrectarray++;
        }

        if (intersect.MaxX < a->MaxX)   /* right */
        {
            destrectarray->MinX = intersect.MaxX + 1;
            destrectarray->MinY = intersect.MinY;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = intersect.MaxY;

            numrects++;
            destrectarray++;
        }

    }                           /* intersecting */

    return numrects;

}

ULONG IsObjectVisible(Object * child, struct Library * MUIMasterBase)
{
    Object *wnd;
    Object *obj;

    wnd = _win(child);
    obj = child;

    while (get(obj, MUIA_Parent, &obj))
    {
        if (!obj)
            break;
        if (obj == wnd)
            break;

        if (_right(child) < _mleft(obj) || _left(child) > _mright(obj)
            || _bottom(child) < _mtop(obj) || _top(child) > _mbottom(obj))
            return FALSE;
    }
    return TRUE;
}

#if ZUNE68_TRACE
#define TRACE_FILE_LIMIT 65536L
struct TraceBuffer
{
    STRPTR next;
    ULONG left;
};

static ASM void TracePutChar(REG(d0, UBYTE chr),
    REG(a3, struct TraceBuffer *buffer))
{
    if (buffer->left)
    {
        *buffer->next++ = chr;
        buffer->left--;
    }
}

void ZuneTraceOutput(CONST_STRPTR fmt, ...)
{
    BPTR fh = Output();
    struct SignalSemaphore *sem;
    char text[512];
    struct TraceBuffer buffer;
    LONG end;

    if (fh)
    {
        VFPrintf(fh, fmt, (APTR)(&fmt + 1));
        Flush(fh);
        return; /* the caller owns its Shell output */
    }

    /* Diagnostic-only stack storage; no retained DOS handles or heap.
     * Drop a contended trace instead of blocking another application's GUI.
     * The library semaphore also serializes writers to the bounded file. */
    sem = &((struct MUIMasterBase_intern *)MUIMasterBase)->ZuneSemaphore;
    if (!AttemptSemaphore(sem)) return;
    buffer.next = text;
    buffer.left = sizeof(text) - 1;
    RawDoFmt(fmt, (APTR)(&fmt + 1), (VOID_FUNC)TracePutChar, &buffer);
    *buffer.next = 0;

    fh = Open("T:zune.log", MODE_READWRITE);
    if (fh)
    {
        if (Seek(fh, 0, OFFSET_END) < 0)
            end = -1;
        else
            end = Seek(fh, 0, OFFSET_CURRENT);
        if (end < 0 || end > TRACE_FILE_LIMIT - (LONG)strlen(text))
        {
            Close(fh);
            fh = end < 0 ? 0 : Open("T:zune.log", MODE_NEWFILE);
        }
    }
    if (fh)
    {
        Write(fh, text, strlen(text));
        Close(fh);
    }
    ReleaseSemaphore(sem);
}
#endif

Object *ZuneNextObject(APTR iterator)
{
    struct _Object **cursor = iterator;
    struct _Object *header;
    if (!cursor || !(header = *cursor) || !header->o_Node.mln_Succ)
        return NULL;
    *cursor = (struct _Object *)header->o_Node.mln_Succ;
    return (Object *)BASEOBJECT(header);
}

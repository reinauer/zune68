/* Copyright (C) 2003, The AROS Development Team. All rights reserved. */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <dos/dos.h>
#include <clib/alib_protos.h>
#include "support_amigaos.h"
APTR AllocVecPooled(APTR pool, ULONG size)
{
    ULONG *p;
    if (!pool || size > ~0UL - sizeof(*p)) return NULL;
    size += sizeof(*p);
    p = AllocPooled(pool, size);
    if (!p) return NULL;
    *p++ = size;
    return p;
}
void FreeVecPooled(APTR pool, APTR memory)
{
    ULONG *p = memory;
    if (p) { --p; FreePooled(pool, p, *p); }
}
LONG HexToLong(CONST_STRPTR str, ULONG *value)
{
    char *end;
    *value = strtoul(str, &end, 16);
    return end == (const char *)str ? -1 : end - (const char *)str;
}
LONG HexToIPTR(CONST_STRPTR str, IPTR *value)
{
    return HexToLong(str, value);
}
char *StrDup(const char *str)
{
    char *copy;
    size_t size;
    if (!str) return NULL;
    size = strlen(str) + 1;
    copy = AllocVec(size, MEMF_PUBLIC);
    if (copy) CopyMem((APTR)str, copy, size);
    return copy;
}
Object *DoSuperNewTagList(Class *cl, Object *obj, void *unused,
    struct TagItem *tags)
{
    struct opSet message = { OM_NEW, tags, NULL };
    return (Object *)DoSuperMethodA(cl, obj, (Msg)&message);
}
Object *DoSuperNewTags(Class *cl, Object *obj, void *unused, ...)
{
    va_list args;
    Object *result;
    va_start(args, unused);
    result = DoSuperNewTagList(cl, obj, unused, (struct TagItem *)args);
    va_end(args);
    return result;
}
size_t strlcat(char *dst, const char *src, size_t size)
{
    size_t used = 0, length = strlen(src), count;
    while (used < size && dst[used]) ++used;
    if (used == size) return size + length;
    count = length < size - used - 1 ? length : size - used - 1;
    memcpy(dst + used, src, count);
    dst[used + count] = 0;
    return used + length;
}
struct RastPort *ZuneCloneRastPort(const struct RastPort *rp)
{
    struct RastPort *copy = AllocVec(sizeof(*copy), MEMF_PUBLIC);
    if (copy) *copy = *rp;
    return copy;
}

/* File notifications are started when an application first waits for
 * input, not at construction. A file written in between would send no
 * notification, so the caller compares its date with the construction
 * time once, right after starting the notification. */
BOOL zune_file_written_since(CONST_STRPTR name, const struct DateStamp *since)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    BOOL newer = FALSE;

    if (!(lock = Lock(name, SHARED_LOCK)))
        return FALSE;
    if ((fib = AllocDosObject(DOS_FIB, NULL)))
    {
        if (Examine(lock, fib))
            newer = CompareDates(&fib->fib_Date, (struct DateStamp *)since) <= 0;
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return newer;
}

/* String-keyed data and object containers for Zune68.
 * A sorted pointer index gives binary-search lookups without btree.library.
 * A linked traversal keeps the next entry valid when removing the current
 * entry during iteration. Payloads and borrowed keys never move in memory.
 */
#include <string.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "native_version.h"

extern struct Library *MUIMasterBase;

struct MapEntry
{
    struct MapEntry *next, *prev;
    CONST_STRPTR key;
    APTR value;
    ULONG size, allocation;
};
struct MapData
{
    struct MapEntry end;
    struct MapEntry **index;
    ULONG count, capacity;
    APTR pool;
    BOOL own_pool, copy_keys, auto_lock, objects, clearing;
};

static ULONG position(struct MapData *d, CONST_STRPTR key)
{
    ULONG lo = 0, hi = d->count;
    while (lo < hi)
    {
        ULONG mid = lo + (hi - lo) / 2;
        if (strcmp(d->index[mid]->key, key) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}
static struct MapEntry *find(struct MapData *d, CONST_STRPTR key)
{
    ULONG n;
    if (!key) return NULL;
    n = position(d, key);
    return n < d->count && !strcmp(d->index[n]->key, key) ?
        d->index[n] : NULL;
}
static void free_entry(struct MapData *d, struct MapEntry *e, BOOL dispose)
{
    if (dispose && d->objects) MUI_DisposeObject(e->value);
    FreePooled(d->pool, e, e->allocation);
}
static void clear_map(struct MapData *d)
{
    /* Empty the visible map before callbacks can reenter it. */
    struct MapEntry *e = d->end.next;
    if (d->clearing) return;
    d->clearing = TRUE;
    d->count = 0;
    d->end.next = d->end.prev = &d->end;
    while (e != &d->end)
    {
        struct MapEntry *next = e->next;
        free_entry(d, e, TRUE);
        e = next;
    }
    d->clearing = FALSE;
}
static IPTR set_entry(struct MapData *d, CONST_STRPTR key, APTR value,
    LONG size)
{
    ULONG n, keylen, bytes;
    struct MapEntry *old, *e;
    if (d->clearing || !key || !value || (!d->objects && size <= 0)) return FALSE;
    n = position(d, key);
    old = n < d->count && !strcmp(d->index[n]->key, key) ? d->index[n] : NULL;
    if (old && d->objects && old->value == value) return TRUE;
    keylen = d->copy_keys ? strlen(key) + 1 : 0;
    if ((ULONG)size > 0x7fffffffUL - sizeof(*e) - keylen) return FALSE;
    bytes = sizeof(*e) + keylen + (d->objects ? 0 : size);
    e = AllocPooled(d->pool, bytes);
    if (!e) return FALSE;
    e->allocation = bytes;
    e->size = d->objects ? sizeof(Object *) : size;
    e->key = key;
    if (d->copy_keys)
    {
        e->key = (STRPTR)(e + 1) + (d->objects ? 0 : size);
        memcpy((APTR)e->key, key, keylen);
    }
    e->value = value;
    if (!d->objects)
    {
        e->value = e + 1;
        memcpy(e->value, value, size);
    }
    if (!old && d->count == d->capacity)
    {
        ULONG capacity = d->capacity ? d->capacity * 2 : 16;
        struct MapEntry **index;
        if (capacity < d->capacity || capacity > 0x7fffffffUL / sizeof(*index))
        {
            free_entry(d, e, FALSE);
            return FALSE;
        }
        index = AllocPooled(d->pool, capacity * sizeof(*index));
        if (!index) { free_entry(d, e, FALSE); return FALSE; }
        if (d->index)
        {
            memcpy(index, d->index, d->count * sizeof(*index));
            FreePooled(d->pool, d->index, d->capacity * sizeof(*index));
        }
        d->index = index;
        d->capacity = capacity;
    }
    if (old) { e->prev = old->prev; e->next = old->next; }
    else
    {
        e->next = n < d->count ? d->index[n] : &d->end;
        e->prev = e->next->prev;
        memmove(d->index + n + 1, d->index + n,
            (d->count - n) * sizeof(*d->index));
        d->count++;
    }
    e->prev->next = e;
    e->next->prev = e;
    d->index[n] = e;
    if (old) free_entry(d, old, TRUE);
    return TRUE;
}

static IPTR map_method(struct IClass *cl, Object *obj, Msg msg)
{
    struct MapData *d = INST_DATA(cl, obj);
    struct MapEntry *e;
    switch (msg->MethodID)
    {
    case OM_GET:
    {
        struct opGet *g = (APTR)msg;
        if (g->opg_AttrID == (d->objects ? MUIA_Objectmap_Count : MUIA_Datamap_Count))
            *g->opg_Storage = d->count;
        else if (g->opg_AttrID == MUIA_Version)
            *g->opg_Storage = ZUNE68_BUILTIN_VERSION;
        else if (g->opg_AttrID == MUIA_Revision)
            *g->opg_Storage = ZUNE68_BUILTIN_REVISION;
        else return DoSuperMethodA(cl, obj, msg);
        return TRUE;
    }
    case MUIM_Datamap_Set:
    {
        struct MUIP_Datamap_Set *m = (APTR)msg;
        return set_entry(d, m->key, (APTR)m->data, m->len);
    }
    case MUIM_Objectmap_Set:
    {
        struct MUIP_Objectmap_Set *m = (APTR)msg;
        return set_entry(d, m->key, m->o, 0);
    }
    case MUIM_Datamap_Find:
    case MUIM_Objectmap_Find:
    case MUIM_Datamap_Get:
    {
        struct MUIP_Datamap_Get *m = (APTR)msg;
        e = find(d, m->key);
        if (msg->MethodID == MUIM_Datamap_Get && m->size && e)
            *m->size = e->size;
        return e ? (IPTR)e->value : 0;
    }
    case MUIM_Datamap_Remove:
    case MUIM_Objectmap_Remove:
    {
        CONST_STRPTR key = ((struct MUIP_Datamap_Remove *)msg)->key;
        ULONG n;
        e = find(d, key);
        if (!e) return FALSE;
        n = position(d, key);
        e->prev->next = e->next;
        e->next->prev = e->prev;
        d->count--;
        memmove(d->index + n, d->index + n + 1,
            (d->count - n) * sizeof(*d->index));
        free_entry(d, e, FALSE);
        return TRUE;
    }
    case MUIM_Datamap_Clear:
    case MUIM_Objectmap_Clear:
        clear_map(d);
        return 0;
    case MUIM_Datamap_Iterate:
    case MUIM_Objectmap_Iterate:
    {
        ULONG *counter = ((struct MUIP_Datamap_Iterate *)msg)->counter;
        if (!counter) return 0;
        e = *counter ? (APTR)*counter : d->end.next;
        if (e == &d->end) { *counter = 0; return 0; }
        *counter = (IPTR)e->next;
        return (IPTR)e->value;
    }
    case MUIM_Datamap_IterationKey:
    case MUIM_Objectmap_IterationKey:
    {
        ULONG *counter = ((struct MUIP_Datamap_IterationKey *)msg)->counter;
        if (!counter || !*counter || *counter == ~0UL) return 0;
        e = ((struct MapEntry *)*counter)->prev;
        return e == &d->end ? 0 : (IPTR)e->key;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

BOOPSI_DISPATCHER(IPTR, Map_Dispatcher, cl, obj, msg)
{
    struct MapData *d;
    IPTR result;
    if (msg->MethodID == OM_NEW)
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList;
        obj = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!obj) return 0;
        d = INST_DATA(cl, obj);
        d->objects = !strcmp(cl->cl_ID, MUIC_Objectmap);
        d->end.next = d->end.prev = &d->end;
        d->copy_keys = GetTagData(d->objects ? MUIA_Objectmap_CopyKeys :
            MUIA_Datamap_CopyKeys, TRUE, tags);
        d->auto_lock = GetTagData(d->objects ? MUIA_Objectmap_AutoLock :
            MUIA_Datamap_AutoLock, FALSE, tags);
        d->pool = (APTR)GetTagData(d->objects ? MUIA_Objectmap_Pool :
            MUIA_Datamap_Pool, 0, tags);
        if (!d->pool)
        {
            d->pool = CreatePool(MEMF_ANY, 4096, 4096);
            d->own_pool = TRUE;
        }
        if (!d->pool) { CoerceMethod(cl, obj, OM_DISPOSE); return 0; }
        return (IPTR)obj;
    }
    d = INST_DATA(cl, obj);
    if (msg->MethodID == OM_DISPOSE)
    {
        clear_map(d);
        if (d->index) FreePooled(d->pool, d->index,
            d->capacity * sizeof(*d->index));
        if (d->own_pool && d->pool) DeletePool(d->pool);
        return DoSuperMethodA(cl, obj, msg);
    }
    /* Semaphore methods must bypass this wrapper, including release. */
    switch (msg->MethodID)
    {
    case MUIM_Semaphore_Obtain: case MUIM_Semaphore_ObtainShared:
    case MUIM_Semaphore_Attempt: case MUIM_Semaphore_AttemptShared:
    case MUIM_Semaphore_Release:
        return DoSuperMethodA(cl, obj, msg);
    }
    if (d->auto_lock) DoSuperMethod(cl, obj, MUIM_Semaphore_Obtain);
    result = map_method(cl, obj, msg);
    if (d->auto_lock) DoSuperMethod(cl, obj, MUIM_Semaphore_Release);
    return result;
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Datamap_desc = {
    MUIC_Datamap, MUIC_Semaphore, sizeof(struct MapData), (void *)Map_Dispatcher
};
const struct __MUIBuiltinClass _MUI_Objectmap_desc = {
    MUIC_Objectmap, MUIC_Semaphore, sizeof(struct MapData), (void *)Map_Dispatcher
};

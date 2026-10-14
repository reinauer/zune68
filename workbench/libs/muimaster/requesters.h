#ifndef ZUNE_REQUESTERS_H
#define ZUNE_REQUESTERS_H
APTR ZuneAllocAslRequest(ULONG type, struct TagItem *tags);
BOOL ZuneAslRequest(APTR request, struct TagItem *tags);
void ZuneFreeAslRequest(APTR request);
#endif

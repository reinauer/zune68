#ifndef ZUNE_POINTER_H
#define ZUNE_POINTER_H

#include <exec/types.h>
struct Window;
struct ZunePointer;

/* The storage owns only images created here, never application pointers. */
IPTR ZuneSetPointer(struct Window *window, IPTR type,
    struct ZunePointer **storage);
void ZuneFreePointer(struct Window *window, struct ZunePointer **storage);

#endif

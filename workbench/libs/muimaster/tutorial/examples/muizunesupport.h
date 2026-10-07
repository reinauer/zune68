/*
    Copyright (C) 2003-2011, The AROS Development Team.
    All rights reserved.

*/

#ifndef _ZUNE_MUISUPPORT_H
#define _ZUNE_MUISUPPORT_H

#include <string.h>

#include <exec/memory.h>

#include <libraries/asl.h>
#include <libraries/mui.h>
#include <prefs/prefhdr.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/iffparse.h>

#include <proto/muimaster.h>

Object *MakeLabel(STRPTR str);
LONG xget(Object * obj, ULONG attr);

#define getstring(obj) (char *) xget(obj, MUIA_String_Contents)

#define SimpleText(text) TextObject, MUIA_Text_Contents, (IPTR) text, End


#ifndef __AROS__

struct Library *MUIMasterBase;

#if defined(__GNUC__) && defined(__mc68000__)
ULONG __stack = 65536;
#endif

int open_muimaster(void)
{
    MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
    return MUIMasterBase != NULL;
}

#else

int open_muimaster(void)
{
    return 1;
}

#endif

void close_muimaster(void)
{
#ifndef __AROS__
    if (MUIMasterBase) CloseLibrary(MUIMasterBase);
    MUIMasterBase = NULL;
#endif
}

/****************************************************************
 Open needed libraries
*****************************************************************/
int open_libs(void)
{
    if (open_muimaster())
    {
        return 1;
    }

    return 0;
}


/****************************************************************
 Close opened libraries
*****************************************************************/
void close_libs(void)
{
    close_muimaster();
}


/****************************************************************
 Create a simple label
*****************************************************************/
Object *MakeLabel(STRPTR str)
{
    return (MUI_MakeObject(MUIO_Label, (IPTR)str, 0));
}


/****************************************************************
 Easy getting an attributes value
*****************************************************************/
LONG xget(Object * obj, ULONG attr)
{
    LONG x = 0;
    get(obj, attr, &x);
    return x;
}


#endif /* _ZUNE_MUISUPPORT_H */

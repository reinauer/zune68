/*
    Reading MUI 3.8's settings files into Zune's (mui38prefs.c converts
    them). MUI's files are only read, never changed.
    Ported from OpenMUI (mui38import.c, Tobias Karlsson).
*/

#include <string.h>
#include <stdio.h>

#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include "mui38import.h"
#include "mui38prefs.h"

/* MUI 3.8 calls its global file "«Global».prefs" (ISO-8859-1 guillemets) */
static CONST_STRPTR const global_names[] = {
    "\253Global\273.prefs", "Global.prefs"
};

/* Where the library looks for pictures and image classes named without a
 * volume (IMSPEC_EXTERNAL_PREFIX) */
#define IMAGES_DRAWER "MUI:Images/"

/* Settings files are small: MUI 3.8's presets are about 2 KB */
#define MAX_FILE (256 * 1024)

static BOOL exists(CONST_STRPTR name)
{
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);

    if (lock)
        UnLock(lock);
    return lock != 0;
}

static void make_dir(CONST_STRPTR name)
{
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);

    if (!lock)
        lock = CreateDir((STRPTR)name);
    if (lock)
        UnLock(lock);
}

static int have_file(const char *name)
{
    char path[300];

    if (strchr(name, ':'))
        return exists(name);
    snprintf(path, sizeof(path), IMAGES_DRAWER "%s", name);
    return exists(path);
}

BOOL mui38_is_global_name(CONST_STRPTR name)
{
    return !Stricmp((STRPTR)name, (STRPTR)global_names[0])
        || !Stricmp((STRPTR)name, (STRPTR)global_names[1]);
}

BOOL mui38_global_file(char *buf, LONG size)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        snprintf(buf, size, MUI38_DRAWER "/%s", global_names[i]);
        if (exists(buf))
            return TRUE;
    }
    return FALSE;
}

/* The converted file image of a MUI 3.8 settings file: *out within the
 * returned block (FreeVec() it), NULL when it is no settings file */
static UBYTE *convert(CONST_STRPTR from, UBYTE **out, LONG *outlen)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR oldwin;
    BPTR fh;
    LONG len;
    UBYTE *in = NULL;

    *outlen = -1;
    if (!(fh = Open((STRPTR)from, MODE_OLDFILE)))
        return NULL;
    if (Seek(fh, 0, OFFSET_END) >= 0
        && (len = Seek(fh, 0, OFFSET_BEGINNING)) > 0 && len < MAX_FILE
        && (in = AllocVec(2 * len + 64, MEMF_ANY)))
    {
        *out = in + len;
        /* no "please insert volume MUI" when MUI: is not assigned */
        oldwin = me->pr_WindowPtr;
        me->pr_WindowPtr = (APTR)-1;
        if (Read(fh, in, len) == len)
            *outlen = mui38_convert(in, len, *out, len + 64, have_file);
        me->pr_WindowPtr = oldwin;
    }
    Close(fh);
    if (*outlen < 0)
    {
        FreeVec(in);
        return NULL;
    }
    return in;
}

BOOL mui38_load(CONST_STRPTR from, Object *configdata)
{
    UBYTE *block, *out, *p, *end;
    LONG outlen;

    if (!(block = convert(from, &out, &outlen)))
        return FALSE;
    DoMethod(configdata, MUIM_Dataspace_Clear);
    end = out + outlen;
    for (p = out + MUI38_ITEMS_OFFSET; end - p >= 8; )
    {
        ULONG id = ((ULONG)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
        ULONG len = ((ULONG)p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];

        if (len > (ULONG)(end - p) - 8)
            break;
        DoMethod(configdata, MUIM_Dataspace_Add, (IPTR)(p + 8), len, id);
        p += 8 + len;
    }
    FreeVec(block);
    return TRUE;
}

static BOOL convert_file(CONST_STRPTR from, CONST_STRPTR to)
{
    UBYTE *block, *out;
    LONG outlen;
    BPTR fh;
    BOOL ok = FALSE;

    if (!(block = convert(from, &out, &outlen)))
        return FALSE;
    if ((fh = Open((STRPTR)to, MODE_NEWFILE)))
    {
        ok = Write(fh, out, outlen) == outlen;
        Close(fh);
        if (!ok)
            DeleteFile((STRPTR)to);
    }
    FreeVec(block);
    return ok;
}

LONG mui38_import_programs(CONST_STRPTR drawer)
{
    struct FileInfoBlock *fib;
    BPTR dir;
    LONG count = 0;
    char from[300], to[300];

    if (!(dir = Lock((STRPTR)drawer, SHARED_LOCK)))
        return 0;
    make_dir("ENVARC:zune");
    make_dir("ENV:zune");
    if ((fib = AllocDosObject(DOS_FIB, NULL)))
    {
        if (Examine(dir, fib))
            while (ExNext(dir, fib))
            {
                CONST_STRPTR name = fib->fib_FileName;
                LONG n = strlen(name);

                if (fib->fib_DirEntryType >= 0 || n < 7
                    || Stricmp((STRPTR)name + n - 6, ".prefs")
                    || mui38_is_global_name(name))
                    continue;
                /* Zune's own file for the program wins */
                snprintf(to, sizeof(to), "ENVARC:zune/%s", name);
                if (exists(to))
                    continue;
                snprintf(from, sizeof(from), "%s", drawer);
                if (!AddPart(from, (STRPTR)name, sizeof(from))
                    || !convert_file(from, to))
                    continue;
                count++;
                snprintf(to, sizeof(to), "ENV:zune/%s", name);
                if (!exists(to))
                    convert_file(from, to);
            }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(dir);
    return count;
}

BOOL mui38_first_run(void)
{
    char buf[300];

    return !exists("ENV:zune/global.prefs")
        && !exists("ENVARC:zune/global.prefs")
        && mui38_global_file(buf, sizeof(buf));
}

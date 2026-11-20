/* The MUI version each program sees.
 *
 * Programs and classes choose code paths by the version the library base
 * reports: TheBar lays out its bar itself only below MUI 4, NList and
 * TextEditor branch on MUI 4 revisions. The library reports MUI 5's
 * version (21.227), and a program can be set to see MUI 3.8's (19.35).
 *
 * Exec's OpenLibrary() checks the version asked for against the base in
 * its library list before it calls Open, so that base keeps reporting
 * MUI 5. Exec returns whatever Open returns, so Open hands a program set
 * to MUI 3.8 a copy of the base: the jump table and the Library
 * structure with MUI 3.8's version, made the first time it is needed and
 * shared by every such program (a SetFunction() on the library after that
 * does not reach it). The library's functions do not use the
 * base they are called through, and Close works on the real base for
 * either. A program that asks for version 20 or higher gets the real
 * base whatever the setting, since it needs MUI 4 or later.
 *
 * ENV:zune/MUIVersion, written by Zune prefs, is a text file:
 *   first line:        MUI5 | MUI3.8    the version for all programs
 *   each further line: MUI5 | MUI3.8 <program>
 * The program is the name of its command (the Shell's command name, or
 * the task name of a program started from Workbench) without its path,
 * compared without case. A class library that ramlib loads for a program
 * opens this library from ramlib's process: that open gets the version of
 * the program loading it (ZUNE_GetExternalClass sets IdLoader). The class
 * keeps that base while it stays in memory, for every program using it.
 *
 * The file is read when the library is loaded and again at the first
 * Open after it changed (a DOS notification queues a message on IdPort;
 * while ENV:zune does not exist there is no notification, and each Open
 * tries again). Open runs under Forbid, so it reads only for a process
 * other than ramlib and after it has counted the open. */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <dos/notify.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>
#include <string.h>

#include "muimaster_intern.h"
#include "native_version.h"

#define IDFILE "ENV:zune/MUIVersion"
#define MUI38_VERSION 19
#define MUI38_REVISION 35
static const char mui38_idstring[] = "muimaster.library 19.35 ("
    ZUNE68_LIBRARY_DATE ") Zune68 " ZUNE68_RELEASE_STAGE ", as MUI 3.8\r\n";

static UBYTE lower(UBYTE c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7)
        ? c + 0x20 : c;
}

/* "MUI5", "MUI3.8" (also "MUI3", "3.8", "5"): ZUNE_ID_*, or -1 */
static LONG parse_word(const char **sp, const char *end)
{
    const char *s = *sp;
    LONG id = -1;

    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    if (end - s >= 3 && lower(s[0]) == 'm' && lower(s[1]) == 'u'
        && lower(s[2]) == 'i')
        s += 3;
    if (s < end && *s == '5')
        id = ZUNE_ID_MUI5, s++;
    else if (s < end && *s == '3') {
        id = ZUNE_ID_MUI38, s++;
        if (end - s >= 2 && s[0] == '.' && s[1] == '8')
            s += 2;
    }
    if (s < end && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
        return -1;
    *sp = s;
    return id;
}

/* The settings file into IdDefault and IdList: each entry is the
 * version + 1, the name's length and the name; a 0 ends the list. */
static void identity_parse(struct MUIMasterBase_intern *base,
    const char *text, LONG len)
{
    const char *s = text, *end = text + len, *e, *name;
    UBYTE *list = NULL, *p = NULL;
    ULONG size = 1;
    LONG id;
    int pass, first;

    base->IdDefault = ZUNE_ID_MUI5;
    for (pass = 0; pass < 2; pass++) {
        for (s = text, first = 1; s < end; s = e + 1, first = 0) {
            for (e = s; e < end && *e != '\n'; e++)
                ;
            id = parse_word(&s, e);
            if (id < 0)
                continue;
            while (s < e && (*s == ' ' || *s == '\t'))
                s++;
            name = s;
            while (e > name && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
                e--;
            if (first && e == name) {
                base->IdDefault = id;
            } else if (e > name && e - name < 255) {
                if (pass == 0)
                    size += 2 + (e - name);
                else {
                    *p++ = id + 1;
                    *p++ = e - name;
                    memcpy(p, name, e - name);
                    p += e - name;
                }
            }
            while (e < end && *e != '\n')
                e++;
        }
        if (pass == 0) {
            if (!(list = AllocVec(size, MEMF_PUBLIC)))
                break;
            p = list;
        } else
            *p = 0;
    }
    Forbid();
    p = base->IdList;
    base->IdList = list;
    Permit();
    if (p)
        FreeVec(p);
}

/* No "insert volume ENV:" requester when ENV: is not assigned */
static APTR no_requesters(void)
{
    struct Process *pr = (struct Process *)SysBase->ThisTask;
    APTR old = pr->pr_WindowPtr;

    pr->pr_WindowPtr = (APTR)-1;
    return old;
}

static void identity_read(struct MUIMasterBase_intern *base)
{
    struct FileInfoBlock *fib;
    BPTR fh;
    char *text = NULL;
    LONG len = 0;
    APTR win = no_requesters();

    if (!base->IdNotifying)
        base->IdNotifying = StartNotify(&base->IdNotify);
    if ((fh = Open((STRPTR)IDFILE, MODE_OLDFILE))) {
        if ((fib = AllocDosObject(DOS_FIB, NULL))) {
            if (ExamineFH(fh, fib) && fib->fib_Size > 0
                && fib->fib_Size < 65536
                && (text = AllocVec(fib->fib_Size, MEMF_ANY)))
                len = Read(fh, text, fib->fib_Size);
            FreeDosObject(DOS_FIB, fib);
        }
        Close(fh);
    }
    ((struct Process *)SysBase->ThisTask)->pr_WindowPtr = win;
    identity_parse(base, text ? text : "", len > 0 ? len : 0);
    if (text)
        FreeVec(text);
}

void zune_identity_init(struct MUIMasterBase_intern *base)
{
    base->IdPort.mp_Node.ln_Type = NT_MSGPORT;
    base->IdPort.mp_Flags = PA_IGNORE;
    NewList(&base->IdPort.mp_MsgList);
    base->IdNotify.nr_Name = (STRPTR)IDFILE;
    base->IdNotify.nr_Flags = NRF_SEND_MESSAGE;
    base->IdNotify.nr_stuff.nr_Msg.nr_Port = &base->IdPort;
    identity_read(base);
}

void zune_identity_exit(struct MUIMasterBase_intern *base)
{
    struct Message *msg;

    if (base->IdNotifying) {
        EndNotify(&base->IdNotify);
        base->IdNotifying = FALSE;
    }
    if (base->IdPort.mp_Node.ln_Type == NT_MSGPORT)
        while ((msg = GetMsg(&base->IdPort)))
            ReplyMsg(msg);
    if (base->IdList)
        FreeVec(base->IdList);
    base->IdList = NULL;
    if (base->IdCopy)
        FreeMem((UBYTE *)base->IdCopy - base->library.lib_NegSize,
            base->library.lib_NegSize + sizeof(struct Library));
    base->IdCopy = NULL;
}

/* The version for TASK: its program's line, else the one for all. */
LONG zune_identity_of(struct MUIMasterBase_intern *base, struct Task *task)
{
    char cmd[256];
    const char *name = NULL;
    const UBYTE *p;
    ULONG len, i;

    if (task->tc_Node.ln_Type == NT_PROCESS
        && ((struct Process *)task)->pr_CLI) {
        struct CommandLineInterface *cli =
            BADDR(((struct Process *)task)->pr_CLI);
        UBYTE *b = BADDR(cli->cli_CommandName);
        if (b && b[0]) {
            memcpy(cmd, b + 1, b[0]);
            cmd[b[0]] = 0;
            name = cmd;
        }
    }
    if (!name)
        name = task->tc_Node.ln_Name ? task->tc_Node.ln_Name : "";
    for (i = strlen(name); i > 0 && name[i - 1] != ':' && name[i - 1] != '/'; i--)
        ;
    name += i;
    len = strlen(name);
    for (p = base->IdList; p && p[0]; p += 2 + p[1]) {
        if (p[1] != len)
            continue;
        for (i = 0; i < len && lower(p[2 + i]) == lower(name[i]); i++)
            ;
        if (i == len)
            return p[0] - 1;
    }
    return base->IdDefault;
}

/* The base Open hands out: called by LibOpen after it counted the open. */
struct Library *zune_identity_open(struct MUIMasterBase_intern *base,
    ULONG version)
{
    struct Task *me = SysBase->ThisTask;
    struct Message *msg;
    struct Library *copy;
    BOOL ramlib = me->tc_Node.ln_Name
        && !strcmp(me->tc_Node.ln_Name, "ramlib");
    LONG id;
    ULONG neg = base->library.lib_NegSize;

    if ((msg = GetMsg(&base->IdPort))) {
        do
            ReplyMsg(msg);
        while ((msg = GetMsg(&base->IdPort)));
        base->IdChanged = TRUE;
    }
    if ((base->IdChanged || !base->IdNotifying) && !ramlib
        && me->tc_Node.ln_Type == NT_PROCESS) {
        base->IdChanged = FALSE;
        identity_read(base);
    }
    if (version >= 20)
        return &base->library;
    id = ramlib && base->IdLoader ? base->IdLoader - 1
        : zune_identity_of(base, me);
    if (id != ZUNE_ID_MUI38)
        return &base->library;
    if (!(copy = base->IdCopy)) {
        UBYTE *mem = AllocMem(neg + sizeof(struct Library), MEMF_PUBLIC);
        if (!mem)
            return &base->library;
        CopyMem((UBYTE *)base - neg, mem, neg + sizeof(struct Library));
        copy = (struct Library *)(mem + neg);
        copy->lib_Version = MUI38_VERSION;
        copy->lib_Revision = MUI38_REVISION;
        copy->lib_IdString = (APTR)mui38_idstring;
        copy->lib_PosSize = sizeof(struct Library);
        copy->lib_OpenCnt = 0;
        CacheClearU();              /* the copied jump table is code */
        base->IdCopy = copy;
    }
    return copy;
}

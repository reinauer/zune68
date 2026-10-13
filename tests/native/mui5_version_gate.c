/* TEST ONLY: expose newer application branches in an isolated guest.
 * Keep the library resident until the synchronous test script finishes,
 * then restore its identity. Never install this as a production helper.
 */
#include <stdio.h>
#include <clib/alib_protos.h>
#include <exec/libraries.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include "mui5_fixture.h"
struct Library *MUIMasterBase;
APTR mui5_fixture_original_new;
static struct MUI5Fixture fixture;
void mui5_fixture_record(Object *obj)
{
    struct IClass *cl;
    if (!obj) return;
    for (cl = OCLASS(obj); cl; cl = cl->cl_Super)
        if (cl->cl_ID && !strcmp((char *)cl->cl_ID, MUIC_Application))
        {
            STRPTR base = NULL;
            GetAttr(MUIA_Application_Base, obj, (ULONG *)&base);
            if (base && !strcmp((char *)base, "IBROWSE"))
                fixture.application = obj;
            break;
        }
}
void mui5_fixture_new(void);
__asm__(".text\n.globl _mui5_fixture_new\n_mui5_fixture_new:\n"
    "pea 1f\nmove.l _mui5_fixture_original_new,-(sp)\nrts\n1:\n"
    "movem.l d0-d1/a0-a1,-(sp)\nmove.l d0,-(sp)\n"
    "jsr _mui5_fixture_record\naddq.l #4,sp\n"
    "movem.l (sp)+,d0-d1/a0-a1\nrts\n");
ULONG __stack = 65536;
int main(int argc, char **argv)
{
    struct Library *lib;
    UWORD version, revision;
    LONG result;
    if (argc < 2 || argc > 3) return 20;
    lib = OpenLibrary("muimaster.library", 0);
    if (!lib) return 20;
    MUIMasterBase = lib;
    fixture.magic = MUI5_FIXTURE_MAGIC;
    fixture.port.mp_Node.ln_Type = NT_MSGPORT;
    fixture.port.mp_Node.ln_Name = (STRPTR)MUI5_FIXTURE_PORT;
    fixture.port.mp_Flags = PA_IGNORE;
    NewList(&fixture.port.mp_MsgList);
    AddPort(&fixture.port);
    mui5_fixture_original_new = SetFunction((struct Library *)IntuitionBase,
        -636, (APTR)mui5_fixture_new);
    version = lib->lib_Version; revision = lib->lib_Revision;
    printf("TEST ONLY identity=%u.%u", version, revision);
    if (version < 20 && argc == 2)
    {
        lib->lib_Version = 21; lib->lib_Revision = 227;
        printf(" temporary=21.227");
    }
    puts(""); fflush(stdout);
    result = SystemTags(argv[1], TAG_DONE);
    SetFunction((struct Library *)IntuitionBase, -636, mui5_fixture_original_new);
    RemPort(&fixture.port);
    lib->lib_Version = version; lib->lib_Revision = revision;
    printf("restored=%u.%u script=%ld\n", version, revision, result);
    CloseLibrary(lib);
    return result;
}

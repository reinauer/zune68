/* What an application's base becomes and whether it gets an ARexx port:
 * two applications with the base "lowBase", one with a command table, one
 * with neither commands nor a hook, one with MUIA_Application_UseRexx
 * FALSE; MUIA_Application_Base given back and the public ports found.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit \
 *        appbase.c -o appbase -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static struct MUI_Command commands[] = {
    { "PING", NULL, 0, NULL },
    { NULL, NULL, 0, NULL }
};

static const char *port(const char *name)
{
    struct MsgPort *p;

    Forbid();
    p = FindPort((STRPTR)name);
    Permit();
    return p ? "port" : "no port";
}

static void show(const char *what, Object *app)
{
    STRPTR base = NULL;

    if (!app)
    {
        printf("%-12s not made\n", what);
        return;
    }
    get(app, MUIA_Application_Base, &base);
    printf("%-12s base %s, %s\n", what, base ? (char *)base : "(null)",
        base ? port((char *)base) : "-");
}

int main(void)
{
    Object *a1, *a2, *a3;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    a1 = ApplicationObject, MUIA_Application_Base, "lowBase",
        MUIA_Application_Commands, commands, End;
    show("commands", a1);
    a2 = ApplicationObject, MUIA_Application_Base, "lowBase", End;
    show("plain", a2);
    a3 = ApplicationObject, MUIA_Application_Base, "NoRexx",
        MUIA_Application_UseRexx, FALSE, End;
    show("UseRexx off", a3);
    if (a3)
        MUI_DisposeObject(a3);
    if (a2)
        MUI_DisposeObject(a2);
    if (a1)
        MUI_DisposeObject(a1);
    printf("after dispose: LOWBASE.1 %s, lowBase.1 %s\n", port("LOWBASE.1"),
        port("lowBase.1"));
    CloseLibrary(MUIMasterBase);
    return 0;
}

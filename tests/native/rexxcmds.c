/* ARexx command probe: an application with its own commands (one of them
 * named like a built-in) driven by rexxcmds.rexx, printing the hooks that
 * run, the return ids MUIM_Application_NewInput delivers and the
 * ForceQuit state, for comparing native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit rexxcmds.c -o rexxcmds -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <rexx/storage.h>

#ifndef MC_TEMPLATE_ID
#define MC_TEMPLATE_ID ((STRPTR)~0)
#endif

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static Object *app;

static ULONG echo_func(struct Hook *h __asm("a0"),
    Object *obj __asm("a2"), ULONG *args __asm("a1"))
{
    printf("echo hook: %s\n", args[0] ? (char *)args[0] : "(null)");
    set(obj, MUIA_Application_RexxString, args[0]);
    return 0;
}

static ULONG hide_func(struct Hook *h __asm("a0"),
    Object *obj __asm("a2"), ULONG *args __asm("a1"))
{
    printf("app hide hook\n");
    return 5;
}

static ULONG unknown_func(struct Hook *h __asm("a0"),
    Object *obj __asm("a2"), struct RexxMsg *msg __asm("a1"))
{
    printf("rexx hook: %s\n", (char *)msg->rm_Args[0]);
    return 7;
}

static struct Hook echo_hook = {{0}, (HOOKFUNC)echo_func};
static struct Hook hide_hook = {{0}, (HOOKFUNC)hide_func};
static struct Hook unknown_hook = {{0}, (HOOKFUNC)unknown_func};

static struct MUI_Command commands[] = {
    {"rescan", MC_TEMPLATE_ID, 1234, NULL},
    {"echo", "TEXT/F", 1, &echo_hook},
    {"hide", NULL, 0, &hide_hook},
    {"need", "NAME/A", 1, &echo_hook},
    {NULL, NULL, 0, NULL}
};

int main(void)
{
    Object *win;
    ULONG sigs = 0, ticks = 0, quits = 0, iconified, force;
    LONG id;
    struct MsgPort *port;

    if (!freopen("Bench:rexxcmds.log", "w", stdout)) return 20;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "rexxcmds",
        MUIA_Application_Base, "REXXCMDS",
        MUIA_Application_Author, "probe author",
        MUIA_Application_Version, "$VER: rexxcmds 1.0 (10.10.26)",
        MUIA_Application_Commands, commands,
        MUIA_Application_RexxHook, &unknown_hook,
        SubWindow, win = WindowObject, MUIA_Window_Title, "rexxcmds",
            WindowContents, RectangleObject, End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    Forbid();
    port = FindPort("REXXCMDS");
    Permit();
    printf("port %s\n", port ? "REXXCMDS" : "missing");
    fflush(stdout);
    for (;;)
    {
        id = DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (id)
        {
            get(app, MUIA_Application_ForceQuit, &force);
            get(app, MUIA_Application_Iconified, &iconified);
            printf("id %ld force %lu iconified %lu\n", id, force, iconified);
            fflush(stdout);
            if (id == MUIV_Application_ReturnID_Quit && ++quits == 2) break;
        }
        if (sigs)
        {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) break;
        }
        if (++ticks > 100000) break;
    }
    get(app, MUIA_Application_Iconified, &iconified);
    printf("end iconified %lu\n", iconified);
    set(app, MUIA_Application_ForceQuit, TRUE);
    get(app, MUIA_Application_ForceQuit, &force);
    printf("set force -> %lu\n", force);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

/* Log what a program opens: library, device, file, lock, screen and window
 * opens, MUI object creation and requesters, with their results, for tasks
 * whose name starts with the given prefix (or a shell running such a
 * command), plus custom classes, BOOPSI objects and bitmap blits. Patches
 * with SetFunction for a number of seconds, then restores and prints the
 * log. Test-only, for a disposable guest.
 *   snoopopen <taskprefix> <seconds> [logfile]
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit snoopopen.c -o snoopopen -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

extern struct ExecBase *SysBase;
ULONG __stack = 32768;

/* argument registers saved by the stub: d0, d1, a0, a1 */
enum { R_D0, R_D1, R_A0, R_A1 };
struct Fn { const char *lib; const char *name; WORD lvo; int reg; int ver; };
static struct Fn fns[] = {
    { "exec.library", "OpenLibrary", -552, R_A1, R_D0 },
    { "exec.library", "OldOpenLibrary", -408, R_A1, -1 },
    { "exec.library", "OpenDevice", -444, R_A0, R_D0 },
    { "exec.library", "OpenResource", -498, R_A1, -1 },
    { "dos.library", "Open", -30, R_D1, -1 },
    { "dos.library", "Lock", -84, R_D1, -1 },
    { "dos.library", "LoadSeg", -150, R_D1, -1 },
    { "dos.library", "GetVar", -906, R_D1, -1 },
    { "intuition.library", "OpenScreenTagList", -612, -1, -1 },
    { "intuition.library", "OpenWindowTagList", -606, -1, -1 },
    { "intuition.library", "LockPubScreen", -510, R_A0, -1 },
    { "intuition.library", "EasyRequestArgs", -588, -1, -1 },
    { "muimaster.library", "MUI_NewObjectA", -30, R_A0, -1 },
    { "muimaster.library", "MUI_RequestA", -42, -1, -1 },
    { "muimaster.library", "MUI_CreateCustomClass", -108, R_A1, R_D0 },
    { "muimaster.library", "MUI_MakeObjectA", -120, -1, R_D0 },
    { "intuition.library", "NewObjectA", -636, R_A1, -1 },
    { "graphics.library", "BltBitMapRastPort", -606, -1, R_D0 },
    { "graphics.library", "BltMaskBitMapRastPort", -636, -1, R_D0 },
    { "graphics.library", "BltBitMap", -30, -1, R_D0 },
    { "intuition.library", "DrawImage", -114, -1, R_D0 },
};
#define NFN (sizeof(fns) / sizeof(fns[0]))
static struct Library *bases[NFN];
static APTR orig[NFN];
static UWORD *stubs;

#define NLOG 4000
static struct { UBYTE fn; char arg[40]; LONG ver; ULONG ret; } logs[NLOG];
/* tag lists passed to MUI_NewObjectA for Image.mui */
#define NTAGLOG 1400
static struct { int slot; ULONG tags[24]; char spec[40]; } taglogs[NTAGLOG];
static int ntaglog;
static volatile int nlog, active;
static char prefix[32];
static int prefixlen;
#define NSHADOW 64
static struct { ULONG ret; int slot; } shadow[NSHADOW];
static int depth;
static struct Task *target;

extern void snoop_common(void);
extern void snoop_exit(void);
__asm__("\n"
"    .text\n"
"    .globl _snoop_common\n"
"_snoop_common:\n"
"    movem.l d0-d1/a0-a1,-(sp)\n"
"    lea 16(sp),a0\n"
"    jsr _snoop_enter\n"
"    movem.l (sp)+,d0-d1/a0-a1\n"
"    rts\n"
"    .globl _snoop_exit\n"
"_snoop_exit:\n"
"    subq.l #4,sp\n"
"    movem.l d0-d1/a0-a1,-(sp)\n"
"    move.l sp,a0\n"
"    jsr _snoop_leave\n"
"    move.l d0,16(sp)\n"
"    movem.l (sp)+,d0-d1/a0-a1\n"
"    rts\n");

/* The task name, or for a shell process the command it is running */
static BOOL is_target(void)
{
    struct Task *t = SysBase->ThisTask;
    const char *name = t->tc_Node.ln_Name;
    char cmd[64];

    if (t == target)
        return TRUE;
    if (t->tc_Node.ln_Type == NT_PROCESS && ((struct Process *) t)->pr_CLI)
    {
        struct CommandLineInterface *cli =
            BADDR(((struct Process *) t)->pr_CLI);
        const UBYTE *bs = BADDR(cli->cli_CommandName);

        if (bs && bs[0])
        {
            const char *base;
            int n = bs[0] < sizeof(cmd) - 1 ? bs[0] : sizeof(cmd) - 1;

            memcpy(cmd, bs + 1, n);
            cmd[n] = 0;
            base = strrchr(cmd, '/');
            base = base ? base + 1 : strrchr(cmd, ':') ? strrchr(cmd, ':') + 1 : cmd;
            if (!strncmp(base, prefix, prefixlen))
            {
                target = t;
                return TRUE;
            }
        }
    }
    if (!name || strncmp(name, prefix, prefixlen))
        return FALSE;
    target = t;
    return TRUE;
}

void snoop_enter(ULONG *slots __asm("a0"))
{
    ULONG idx = slots[0], *regs = slots - 4;
    int n;

    slots[0] = (ULONG) orig[idx];
    if (!active || !is_target() || nlog >= NLOG || depth >= NSHADOW)
        return;
    Disable();
    n = nlog++;
    Enable();
    logs[n].fn = idx;
    logs[n].arg[0] = 0;
    logs[n].ver = fns[idx].ver >= 0 ? (LONG) regs[fns[idx].ver] : -1;
    if (fns[idx].reg >= 0 && regs[fns[idx].reg])
    {
        const char *s = (const char *) regs[fns[idx].reg];
        int i;

        for (i = 0; i < 39 && s[i]; i++)
            logs[n].arg[i] = s[i];
        logs[n].arg[i] = 0;
        if (fns[idx].lvo == -30 && fns[idx].reg == R_A0 && ntaglog < NTAGLOG && regs[R_A1])
        {
            const ULONG *t = (const ULONG *) regs[R_A1];
            int k = ntaglog++, j;

            taglogs[k].slot = n;
            taglogs[k].spec[0] = 0;
            for (j = 0; j < 24; j += 2)
            {
                taglogs[k].tags[j] = t[j];
                taglogs[k].tags[j + 1] = t[j + 1];
                if ((t[j] == 0x804233d5 || t[j] == 0x8042545b || t[j] == 0x8042f8dc) && t[j + 1] > 0xffff && !taglogs[k].spec[0])
                {
                    const char *sp = (const char *) t[j + 1];
                    int m;

                    for (m = 0; m < 39 && sp[m]; m++)
                        taglogs[k].spec[m] = sp[m];
                    taglogs[k].spec[m] = 0;
                }
                if (!t[j])
                    break;
            }
        }
    }
    logs[n].ret = 0xdeadbeef;
    shadow[depth].ret = slots[1];
    shadow[depth].slot = n;
    depth++;
    slots[1] = (ULONG) snoop_exit;
}

ULONG snoop_leave(ULONG *regs __asm("a0"))
{
    depth--;
    logs[shadow[depth].slot].ret = regs[0];
    return shadow[depth].ret;
}

int main(int argc, char **argv)
{
    unsigned int i;
    int secs;

    if (argc < 3)
        return 20;
    if (argc > 3 && !freopen(argv[3], "w", stdout))
        return 20;
    strncpy(prefix, argv[1], sizeof(prefix) - 1);
    prefixlen = strlen(prefix);
    secs = atoi(argv[2]);
    stubs = AllocMem(NFN * 12, MEMF_PUBLIC | MEMF_CLEAR);
    if (!stubs)
        return 20;
    for (i = 0; i < NFN; i++)
    {
        UWORD *p = stubs + i * 6;

        p[0] = 0x2f3c;
        p[1] = i >> 16;
        p[2] = i;
        p[3] = 0x4ef9;
        p[4] = (ULONG) snoop_common >> 16;
        p[5] = (ULONG) snoop_common;
    }
    CacheClearU();
    for (i = 0; i < NFN; i++)
    {
        bases[i] = !strcmp(fns[i].lib, "exec.library") ?
            (struct Library *) SysBase : OpenLibrary(fns[i].lib, 0);
        if (bases[i])
            orig[i] = SetFunction(bases[i], fns[i].lvo, (APTR)(stubs + i * 6));
    }
    printf("snooping tasks named %s* for %d s\n", prefix, secs);
    fflush(stdout);
    active = 1;
    Delay(secs * 50);
    active = 0;
    Delay(50);
    for (i = NFN; i-- > 0;)
        if (bases[i])
        {
            SetFunction(bases[i], fns[i].lvo, orig[i]);
            if (bases[i] != (struct Library *) SysBase)
                CloseLibrary(bases[i]);
        }
    CacheClearU();
    for (i = 0; i < (unsigned int) nlog; i++)
    {
        printf("%-18s %-40s", fns[logs[i].fn].name, logs[i].arg);
        if (logs[i].ver >= 0)
            printf(" v%ld", (long) logs[i].ver);
        printf(" -> %08lx%s\n", (unsigned long) logs[i].ret,
            logs[i].ret ? "" : "  FAILED");
    }
    for (i = 0; i < (unsigned int) ntaglog; i++)
    {
        int j;

        printf("tags of entry %d:", taglogs[i].slot + 1);
        for (j = 0; j < 24; j += 2)
        {
            printf(" %08lx=%08lx", (unsigned long) taglogs[i].tags[j],
                (unsigned long) taglogs[i].tags[j + 1]);
            if (!taglogs[i].tags[j])
                break;
        }
        printf(" spec \"%s\"\n", taglogs[i].spec);
    }
    printf("entries %d target %s\n", nlog, target ? "seen" : "never seen");
    printf("PASS\n");
    return 0;
}

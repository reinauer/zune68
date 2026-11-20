/*
 * mccspy: trace what muimaster.library sends to a program's own classes.
 *
 *   mccspy [all]           patch muimaster.library and trace until Ctrl-C
 *                          (or "mccspy stop"), then print the trace
 *   mccspy stop            stop a running mccspy and wait until it is gone
 *   mccspy read [ADDR] [plain]  print the trace at ADDR (default: the
 *                          running mccspy's); plain leaves out sequence
 *                          numbers, addresses and return addresses, so
 *                          that two runs can be compared with diff
 *
 * mccspy patches MUI_CreateCustomClass, MUI_DeleteCustomClass,
 * MUI_DisposeObject and MUI_Redraw. A class created by a call from the
 * calling process's own code (any caller with "all") gets its dispatcher
 * wrapped. Each method a wrapped dispatcher receives is recorded in order
 * with the class, the task, the nesting depth of wrapped calls on that
 * task, the object, the first two message words after the method ID, the
 * result and two return addresses (the direct caller and the first RAM
 * address on the stack after a call instruction). OM_GET, OM_SET,
 * MUIM_HandleEvent and MUIM_HandleInput are not recorded. Class creation
 * and deletion are recorded, and MUI_DisposeObject and MUI_Redraw calls
 * on objects of wrapped classes or from the process's own code.
 *
 * At start mccspy prints "rec=<address>": a block holding struct ms_hdr
 * below, a ring of NREC records, readable with a debugger after the
 * machine died. Classes are named C<n>(<superclass>) in creation order,
 * objects o<n> in order of appearance, so the names match between runs on
 * MUI 3.8 and Zune68. Stopping restores the dispatchers of classes still
 * alive and the library vectors, and waits until no task is in a wrapped
 * call.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s -Ibuild/muimaster/include \
 *        tests/native/mccspy.c tests/native/mccspy_stubs.s -o mccspy
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NSTUB 64
extern ULONG ms_stubs[NSTUB], ms_old[NSTUB], ms_libstubs[3], ms_orig_lib[3];
extern ULONG ms_orig_ccc;
extern volatile ULONG ms_busy;
void ms_ccc(void);

static const WORD liblvo[3] = { -36, -102, -114 };
enum { L_DISPOSE, L_REDRAW, L_DELETE };

#define MAGIC 0x4D535059UL          /* 'MSPY' */
#define NREC 4096
#define NCLS 96
#define NTASK 16
#define NOCLS 0xFF
enum { OP_METHOD = 1, OP_CREATE, OP_DELETE, OP_DISPOSE, OP_REDRAW };
#define OP_DONE 0x80                /* result is valid */

struct ms_ent {
    ULONG seq;
    UBYTE op, cls, task, depth;
    ULONG method;                   /* METHOD: ID; CREATE: superclass
                                       index or NOCLS; REDRAW: flags */
    ULONG obj;                      /* CREATE, DELETE: the MUI_CustomClass */
    ULONG arg[2];                   /* METHOD: msg[1], msg[2];
                                       CREATE: datasize, dispatcher */
    ULONG result;
    ULONG ret[2];
};

struct ms_cls {
    ULONG cl;
    ULONG ours;                     /* 1: wrapped by mccspy */
    char name[24];
};

struct ms_task {
    ULONG task, depth;
    char name[16];
};

struct ms_hdr {
    ULONG magic, size;
    UWORD libver, librev;
    ULONG n;                        /* records written; rec[n % NREC] next */
    ULONG ncls, ntask, nours;
    struct ms_cls cls[NCLS];
    struct ms_task task[NTASK];
    struct ms_ent rec[NREC];
};

static struct ms_hdr *H;
static struct Library *mb;
static volatile int armed;
static int all;
static struct { ULONG cl, cls; } slot[NSTUB];   /* cl 0: free */
static ULONG rlo[16], rhi[16];
static int nr;

static struct msport {
    struct MsgPort mp;
    struct ms_hdr *hdr;
} port;

static int in_ram(ULONG a)
{
    int i;

    if (a & 1)
        return 0;
    for (i = 0; i < nr; i++)
        if (a >= rlo[i] + 8 && a < rhi[i])
            return 1;
    return 0;
}

static int after_call(ULONG a)
{
    UWORD w2 = *(UWORD *)(a - 2), w4 = *(UWORD *)(a - 4),
          w6 = *(UWORD *)(a - 6);

    if ((w2 & 0xFFF8) == 0x4E90)
        return 1;
    if ((w2 & 0xFF00) == 0x6100 && (w2 & 0xFF) && (w2 & 0xFF) != 0xFF)
        return 1;
    if ((w4 & 0xFFF0) == 0x4EA0 && (w4 & 0xF) >= 8)
        return 1;
    if (w4 == 0x4EB8 || w4 == 0x4EBA || w4 == 0x4EBB || w4 == 0x6100)
        return 1;
    if (w6 == 0x4EB9 || w6 == 0x61FF)
        return 1;
    return 0;
}

/* Is a in the code of the current process (its CLI module or the
 * Workbench-started seglist)? */
static int in_process(ULONG a)
{
    struct Process *p = (struct Process *)SysBase->ThisTask;
    struct CommandLineInterface *cli;
    BPTR seg = 0;
    int n = 0;

    if (p->pr_Task.tc_Node.ln_Type != NT_PROCESS)
        return 0;
    cli = BADDR(p->pr_CLI);
    if (cli && cli->cli_Module)
        seg = cli->cli_Module;
    else if (p->pr_SegList && ((BPTR *)BADDR(p->pr_SegList))[0] >= 3)
        seg = ((BPTR *)BADDR(p->pr_SegList))[3];
    for (; seg && n < 64; seg = *(BPTR *)BADDR(seg), n++) {
        ULONG *s = BADDR(seg);
        if (a > (ULONG)s && a < (ULONG)s + s[-1] - 4)
            return 1;
    }
    return 0;
}

static void copy_name(char *d, const char *s, int max)
{
    int n = 0;

    if (s)
        for (; s[n] && n < max - 1; n++)
            d[n] = s[n];
    d[n] = 0;
}

static ULONG task_index(void)
{
    struct Task *t = SysBase->ThisTask;
    ULONG i;

    for (i = 0; i < H->ntask; i++)
        if (H->task[i].task == (ULONG)t)
            return i;
    if (H->ntask == NTASK)
        return NTASK - 1;
    i = H->ntask++;
    H->task[i].task = (ULONG)t;
    H->task[i].depth = 0;
    copy_name(H->task[i].name, t->tc_Node.ln_Name, sizeof(H->task[i].name));
    if (t->tc_Node.ln_Type == NT_PROCESS) {
        struct CommandLineInterface *cli =
            BADDR(((struct Process *)t)->pr_CLI);
        if (cli && cli->cli_Module && cli->cli_CommandName) {
            UBYTE *b = BADDR(cli->cli_CommandName);
            int n = b[0], k;
            for (k = n; k > 0 && b[k] != '/' && b[k] != ':'; k--)
                ;
            if (n - k > 15)
                n = k + 15;
            memcpy(H->task[i].name, b + k + 1, n - k);
            H->task[i].name[n - k] = 0;
        }
    }
    return i;
}

/* Class index of cl: wrapped classes by slot, others by name. */
static ULONG class_index(struct IClass *cl)
{
    ULONG i;

    if (!cl)
        return NOCLS;
    for (i = 0; i < H->ncls; i++)
        if (H->cls[i].cl == (ULONG)cl)
            return i;
    if (H->ncls == NCLS)
        return NOCLS;
    i = H->ncls++;
    H->cls[i].cl = (ULONG)cl;
    copy_name(H->cls[i].name, cl->cl_ID ? (const char *)cl->cl_ID
        : "private", sizeof(H->cls[i].name));
    return i;
}

static struct ms_ent *new_ent(ULONG op, ULONG *sp)
{
    struct ms_ent *e = &H->rec[H->n % NREC];
    ULONG *top = sp + 96;
    struct Task *t = SysBase->ThisTask;
    int i;

    e->seq = H->n++;
    e->op = op;
    e->task = task_index();
    e->depth = H->task[e->task].depth;
    e->cls = NOCLS;
    e->method = e->obj = e->arg[0] = e->arg[1] = e->result = 0;
    e->ret[0] = sp[0];
    e->ret[1] = 0;
    if ((APTR)sp >= t->tc_SPLower && (APTR)sp < t->tc_SPUpper
        && (ULONG *)t->tc_SPUpper < top)
        top = (ULONG *)t->tc_SPUpper;
    for (i = 1; sp + i < top; i++)
        if (sp[i] != sp[0] && in_ram(sp[i]) && after_call(sp[i])) {
            e->ret[1] = sp[i];
            break;
        }
    return e;
}

LONG ms_pre(ULONG n, struct IClass *cl, Object *obj, Msg msg, ULONG *sp)
{
    struct ms_ent *e;
    ULONG m = msg->MethodID;

    if (!armed || m == OM_GET || m == OM_SET || m == MUIM_HandleEvent
        || m == MUIM_HandleInput)
        return -1;
    Forbid();
    e = new_ent(OP_METHOD, sp);
    e->cls = slot[n].cls;
    e->method = m;
    e->obj = (ULONG)obj;
    e->arg[0] = ((ULONG *)msg)[1];
    e->arg[1] = ((ULONG *)msg)[2];
    H->task[e->task].depth++;
    Permit();
    return e->seq & 0x7FFFFFFF;
}

ULONG ms_post(ULONG id, ULONG result)
{
    struct ms_ent *e = &H->rec[id % NREC];
    ULONG t;

    Forbid();
    if (e->seq == id) {
        e->result = result;
        e->op |= OP_DONE;
    }
    t = task_index();
    if (H->task[t].depth)
        H->task[t].depth--;
    Permit();
    return result;
}

/* saved: d0 a0 a1 a2 a3 of the call (datasize, base, supername, supermcc,
 * dispatcher). */
ULONG ms_ccc_done(struct MUI_CustomClass *mcc, ULONG *saved, ULONG *sp)
{
    struct MUI_CustomClass *super = (struct MUI_CustomClass *)saved[3];
    struct IClass *cl;
    struct ms_ent *e;
    ULONG i, k = NSTUB, c, sc = NOCLS;

    if (!armed || !mcc || !(all || in_process(sp[0])))
        return (ULONG)mcc;
    Forbid();
    cl = mcc->mcc_Class;
    for (i = 0; i < NSTUB; i++)
        if (!slot[i].cl) {
            k = i;
            break;
        }
    if (super)
        for (i = 0; i < H->ncls; i++)
            if (H->cls[i].ours && H->cls[i].cl == (ULONG)super->mcc_Class)
                sc = i;
    if (k < NSTUB && H->ncls < NCLS) {
        char *nm;
        ULONG v;
        c = H->ncls++;
        H->cls[c].cl = (ULONG)cl;
        H->cls[c].ours = 1;
        nm = H->cls[c].name;
        v = ++H->nours;             /* "C<v>(", v < 100 */
        *nm++ = 'C';
        if (v >= 10)
            *nm++ = '0' + v / 10 % 10;
        *nm++ = '0' + v % 10;
        *nm++ = '(';
        *nm = 0;
        if (sc != NOCLS) {          /* "C<n>" of the superclass */
            const char *s = H->cls[sc].name;
            while (*s && *s != '(')
                *nm++ = *s++;
            *nm = 0;
        } else
            copy_name(nm, super ? (super->mcc_Class->cl_ID ? (const char *)
                super->mcc_Class->cl_ID : "private") : (const char *)saved[2],
                sizeof(H->cls[c].name) - (nm - H->cls[c].name) - 1);
        strcat(nm, ")");
        slot[k].cl = (ULONG)cl;
        slot[k].cls = c;
        ms_old[k] = (ULONG)cl->cl_Dispatcher.h_Entry;
        cl->cl_Dispatcher.h_Entry = (HOOKFUNC)ms_stubs[k];
        e = new_ent(OP_CREATE, sp);
        e->cls = c;
        e->method = sc;
        e->obj = (ULONG)mcc;
        e->arg[0] = saved[0];
        e->arg[1] = saved[4];
        e->result = (ULONG)mcc;
        e->op |= OP_DONE;
    }
    Permit();
    return (ULONG)mcc;
}

static int wrapped_slot(struct IClass *cl)
{
    int i;

    for (i = 0; i < NSTUB; i++)
        if (cl && slot[i].cl == (ULONG)cl)
            return i;
    return -1;
}

void ms_lib(ULONG n, ULONG a0, ULONG d0, ULONG *sp)
{
    struct IClass *cl = NULL;
    struct ms_ent *e;
    int k;

    if (!armed || !a0)
        return;
    Forbid();
    if (n == L_DELETE) {
        cl = ((struct MUI_CustomClass *)a0)->mcc_Class;
        if ((k = wrapped_slot(cl)) >= 0) {
            e = new_ent(OP_DELETE, sp);
            e->cls = slot[k].cls;
            e->obj = a0;
            if (cl->cl_Dispatcher.h_Entry == (HOOKFUNC)ms_stubs[k])
                cl->cl_Dispatcher.h_Entry = (HOOKFUNC)ms_old[k];
            slot[k].cl = 0;
        }
    } else {
        cl = OCLASS((Object *)a0);
        if (wrapped_slot(cl) >= 0 || in_process(sp[0])) {
            e = new_ent(n == L_DISPOSE ? OP_DISPOSE : OP_REDRAW, sp);
            e->cls = class_index(cl);
            e->obj = a0;
            e->method = n == L_REDRAW ? d0 : 0;
        }
    }
    Permit();
}

/* ---- printing ---- */

#define M(x) { MUIM_##x, #x },
static const struct { ULONG id; const char *name; } methods[] = {
    { OM_NEW, "OM_NEW" }, { OM_DISPOSE, "OM_DISPOSE" },
    { OM_SET, "OM_SET" }, { OM_GET, "OM_GET" },
    { OM_ADDTAIL, "OM_ADDTAIL" }, { OM_REMOVE, "OM_REMOVE" },
    { OM_NOTIFY, "OM_NOTIFY" }, { OM_UPDATE, "OM_UPDATE" },
    { OM_ADDMEMBER, "OM_ADDMEMBER" }, { OM_REMMEMBER, "OM_REMMEMBER" },
    M(AskMinMax) M(Cleanup) M(ContextMenuBuild) M(ContextMenuChoice)
    M(CreateBubble) M(CreateShortHelp) M(DeleteBubble) M(DeleteShortHelp)
    M(DragBegin) M(DragDrop) M(DragFinish) M(DragQuery) M(DragReport)
    M(Draw) M(DrawBackground) M(HandleEvent) M(HandleInput) M(Hide)
    M(Setup) M(Show) M(GoActive) M(GoInactive) M(CreateDragImage)
    M(DeleteDragImage) M(DoDrag) M(CustomBackfill) M(ConnectParent)
    M(DisconnectParent) M(GlobalSetup) M(Layout) M(Zune_Layout)
    M(DrawParentBackground) M(CreateFrameClippingRegion)
    M(QueryFrameCharacteristics) M(CallHook) M(Export) M(FindUData)
    M(GetConfigItem) M(GetUData) M(Import) M(KillNotify) M(KillNotifyObj)
    M(MultiSet) M(NoNotifySet) M(Notify) M(Set) M(SetAsString) M(SetUData)
    M(SetUDataOnce) M(Family_AddHead) M(Family_AddTail) M(Family_Insert)
    M(Family_Remove) M(Family_Sort) M(Family_Transfer)
    M(Group_InitChange) M(Group_ExitChange) M(Group_Sort)
    M(Application_AboutMUI) M(Application_AddInputHandler)
    M(Application_CheckRefresh) M(Application_InputBuffered)
    M(Application_Load) M(Application_NewInput)
    M(Application_OpenConfigWindow) M(Application_PushMethod)
    M(Application_RemInputHandler) M(Application_ReturnID)
    M(Application_Save) M(Application_SetConfigItem)
    M(Application_ShowHelp) M(Window_AddEventHandler)
    M(Window_RemEventHandler) M(Window_ScreenToBack)
    M(Window_ScreenToFront) M(Window_ToBack)
    M(Window_ToFront) M(List_Clear) M(List_CreateImage) M(List_DeleteImage)
    M(List_Exchange) M(List_GetEntry) M(List_Insert) M(List_InsertSingle)
    M(List_Jump) M(List_Move) M(List_NextSelected) M(List_Redraw)
    M(List_Remove) M(List_Select) M(List_Sort) M(List_TestPos)
    M(List_Construct) M(List_Destruct) M(List_Display) M(List_Compare)
    M(Numeric_Decrease) M(Numeric_Increase) M(Numeric_ScaleToValue)
    M(Numeric_SetDefault) M(Numeric_Stringify) M(Numeric_ValueToScale)
};

static const char *method_name(ULONG id)
{
    static char buf[12];
    unsigned i;

    for (i = 0; i < sizeof(methods) / sizeof(methods[0]); i++)
        if (methods[i].id == id)
            return methods[i].name;
    sprintf(buf, "%08lx", (unsigned long)id);
    return buf;
}

/* Arguments worth printing in plain output: 0 none, 1 or 2 values, 'o'
 * the first is an object. */
static int plain_args(ULONG id)
{
    switch (id) {
    case MUIM_Draw: case MUIM_Numeric_Stringify:
        return 1;
    case MUIM_Set: case MUIM_NoNotifySet: case MUIM_Notify:
    case MUIM_List_Jump: case MUIM_List_Select:
        return 2;
    case MUIM_ConnectParent: case MUIM_DragQuery: case MUIM_DragDrop:
    case MUIM_DragBegin: case MUIM_DragFinish:
        return 'o';
    case OM_NEW: case OM_DISPOSE: case MUIM_Setup: case MUIM_Cleanup:
    case MUIM_AskMinMax: case MUIM_Show: case MUIM_Hide:
    case MUIM_DisconnectParent:
        return 0;
    }
    return 3;                       /* unknown: small values only */
}

static ULONG objs[512];
static int nobj;

static const char *oname(ULONG o)
{
    static char buf[2][12];
    static int b;
    int i;

    b ^= 1;
    if (!o)
        return "0";
    for (i = 0; i < nobj && objs[i] != o; i++)
        ;
    if (i == nobj && nobj < 512)
        objs[nobj++] = o;
    sprintf(buf[b], "o%d", i + 1);
    return buf[b];
}

static const char *val(ULONG v, int plain)
{
    static char buf[4][12];
    static int b;

    b = (b + 1) & 3;
    if (plain && v >= 0x10000 && v < 0x80000000UL)
        return "*";
    sprintf(buf[b], plain && v < 0x10000 ? "%lu" : "%08lx",
        (unsigned long)v);
    return buf[b];
}

static const char *cname(const struct ms_hdr *h, ULONG c)
{
    return c < h->ncls ? h->cls[c].name : "?";
}

static void print_trace(const struct ms_hdr *h, int plain)
{
    ULONG first = h->n > NREC ? h->n - NREC : 0, s;
    unsigned i;

    nobj = 0;
    printf("mccspy: muimaster %u.%u, %lu records", h->libver, h->librev,
        (unsigned long)h->n);
    if (first)
        printf(", the first %lu overwritten", (unsigned long)first);
    printf("\n");
    for (i = 0; i < h->ntask && !plain; i++)
        printf("task %u %08lx %s\n", i, (unsigned long)h->task[i].task,
            h->task[i].name);
    for (s = first; s < h->n; s++) {
        const struct ms_ent *e = &h->rec[s % NREC];
        int op = e->op & ~OP_DONE, k;

        if (!plain)
            printf("%5lu ", (unsigned long)e->seq);
        printf("%-15s ", e->task < h->ntask ? h->task[e->task].name : "?");
        for (k = 0; k < e->depth && k < 20; k++)
            printf("  ");
        switch (op) {
        case OP_METHOD:
            printf("%s %s ", cname(h, e->cls), method_name(e->method));
            if (e->method == OM_NEW)
                printf("cl");
            else
                printf("%s", oname(e->obj));
            k = plain ? plain_args(e->method) : 2;
            if (k == 'o')
                printf(" %s", oname(e->arg[0]));
            else if (k == 3) {
                if (e->arg[0] < 0x10000 || e->arg[0] >= 0x80000000UL)
                    printf(" %s", val(e->arg[0], plain));
            } else {
                if (k >= 1)
                    printf(" %s", val(e->arg[0], plain));
                if (k >= 2)
                    printf(" %s", val(e->arg[1], plain));
            }
            if (!(e->op & OP_DONE))
                printf(" -> (no return)");
            else if (e->method == OM_NEW)
                printf(" -> %s", oname(e->result));
            else
                printf(" -> %s", val(e->result, plain));
            break;
        case OP_CREATE:
            printf("MUI_CreateCustomClass %s", cname(h, e->cls));
            if (!plain)
                printf(" mcc %08lx datasize %lu dispatcher %08lx",
                    (unsigned long)e->obj, (unsigned long)e->arg[0],
                    (unsigned long)e->arg[1]);
            break;
        case OP_DELETE:
            printf("MUI_DeleteCustomClass %s", cname(h, e->cls));
            break;
        case OP_DISPOSE:
            printf("MUI_DisposeObject %s %s", oname(e->obj),
                cname(h, e->cls));
            break;
        case OP_REDRAW:
            printf("MUI_Redraw %s %s %s", oname(e->obj), cname(h, e->cls),
                val(e->method, plain));
            break;
        default:
            printf("? op %d", op);
        }
        if (!plain)
            printf("  ret %08lx %08lx", (unsigned long)e->ret[0],
                (unsigned long)e->ret[1]);
        printf("\n");
    }
}

static int read_hdr(const char *addr, int plain)
{
    struct ms_hdr *h = NULL;
    struct msport *p;

    if (addr)
        h = (struct ms_hdr *)strtoul(addr, NULL, 16);
    else {
        Forbid();
        if ((p = (struct msport *)FindPort((STRPTR)"mccspy")) != NULL)
            h = p->hdr;
        Permit();
    }
    if (!h || ((ULONG)h & 1) || TypeOfMem(h) == 0 || h->magic != MAGIC
        || h->size != sizeof(*h)) {
        printf("mccspy: no record%s%s\n", addr ? " at " : "",
            addr ? addr : " (mccspy is not running)");
        return 10;
    }
    print_trace(h, plain);
    return 0;
}

static int stop(void)
{
    struct MsgPort *p;
    int i;

    Forbid();
    if ((p = FindPort((STRPTR)"mccspy")) != NULL)
        Signal(p->mp_SigTask, SIGBREAKF_CTRL_C);
    Permit();
    if (!p) {
        printf("mccspy: not running\n");
        return 5;
    }
    for (i = 0; i < 1500; i++) {
        Forbid();
        p = FindPort((STRPTR)"mccspy");
        Permit();
        if (!p)
            return 0;
        Delay(2);
    }
    printf("mccspy: still running\n");
    return 5;
}

/* Restore the dispatchers and the library vectors; 0 if a vector was
 * patched again by someone else. */
static int unpatch(void)
{
    UBYTE *b = (UBYTE *)mb;
    int i;

    Forbid();
    if (*(ULONG *)(b - 108 + 2) != (ULONG)ms_ccc) {
        Permit();
        return 0;
    }
    for (i = 0; i < 3; i++)
        if (*(ULONG *)(b + liblvo[i] + 2) != ms_libstubs[i]) {
            Permit();
            return 0;
        }
    armed = 0;
    for (i = 0; i < NSTUB; i++)
        if (slot[i].cl) {
            struct IClass *cl = (struct IClass *)slot[i].cl;
            if (cl->cl_Dispatcher.h_Entry == (HOOKFUNC)ms_stubs[i])
                cl->cl_Dispatcher.h_Entry = (HOOKFUNC)ms_old[i];
            slot[i].cl = 0;
        }
    SetFunction(mb, -108, (APTR)ms_orig_ccc);
    for (i = 0; i < 3; i++)
        SetFunction(mb, liblvo[i], (APTR)ms_orig_lib[i]);
    Permit();
    return 1;
}

int main(int argc, char **argv)
{
    struct MemHeader *mh;
    LONG sig;
    int i, n;

    if (argc >= 2 && !strcmp(argv[1], "read")) {
        const char *addr = NULL;
        int plain = 0;
        for (i = 2; i < argc; i++)
            if (!strcmp(argv[i], "plain"))
                plain = 1;
            else
                addr = argv[i];
        return read_hdr(addr, plain);
    }
    if (argc >= 2 && !strcmp(argv[1], "stop"))
        return stop();
    if (argc >= 2 && !strcmp(argv[1], "all"))
        all = 1;
    else if (argc >= 2) {
        printf("usage: mccspy [all | stop | read [ADDR] [plain]]\n");
        return 5;
    }
    Forbid();
    if (FindPort((STRPTR)"mccspy")) {
        Permit();
        printf("mccspy: already running\n");
        return 5;
    }
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head;
         mh->mh_Node.ln_Succ && nr < 16;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ) {
        rlo[nr] = (ULONG)mh->mh_Lower;
        rhi[nr++] = (ULONG)mh->mh_Upper;
    }
    Permit();
    if (!(mb = OpenLibrary((STRPTR)"muimaster.library", 0))) {
        printf("mccspy: no muimaster.library\n");
        return 20;
    }
    if ((sig = AllocSignal(-1)) < 0
        || !(H = AllocMem(sizeof(*H), MEMF_PUBLIC | MEMF_CLEAR))) {
        printf("mccspy: no signal or memory\n");
        CloseLibrary(mb);
        return 20;
    }
    H->magic = MAGIC;
    H->size = sizeof(*H);
    H->libver = mb->lib_Version;
    H->librev = mb->lib_Revision;
    port.mp.mp_Node.ln_Type = NT_MSGPORT;
    port.mp.mp_Node.ln_Name = (char *)"mccspy";
    port.mp.mp_Flags = PA_SIGNAL;
    port.mp.mp_SigBit = sig;
    port.mp.mp_SigTask = FindTask(NULL);
    port.mp.mp_MsgList.lh_Head = (struct Node *)&port.mp.mp_MsgList.lh_Tail;
    port.mp.mp_MsgList.lh_TailPred = (struct Node *)&port.mp.mp_MsgList;
    port.hdr = H;
    AddPort(&port.mp);
    printf("rec=%08lx\n", (unsigned long)H);
    fflush(stdout);

    Forbid();
    armed = 1;
    ms_orig_ccc = (ULONG)SetFunction(mb, -108, (APTR)ms_ccc);
    for (i = 0; i < 3; i++)
        ms_orig_lib[i] = (ULONG)SetFunction(mb, liblvo[i],
            (APTR)ms_libstubs[i]);
    Permit();

    for (;;) {
        Wait(SIGBREAKF_CTRL_C);
        if (unpatch())
            break;
        printf("mccspy: muimaster was patched again after mccspy;"
            " it stays\n");
        fflush(stdout);
    }
    RemPort(&port.mp);
    for (n = 0; ms_busy; n++) {
        if (n == 250)
            printf("mccspy: waiting for %lu wrapped calls to return\n",
                (unsigned long)ms_busy);
        Delay(2);
    }
    Delay(10);                      /* a task may be past the counter */
    print_trace(H, 0);
    FreeMem(H, sizeof(*H));
    FreeSignal(sig);
    CloseLibrary(mb);
    return 0;
}

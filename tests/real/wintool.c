/* Guest helper of tests/real/run.py: starts programs one after another and
 * watches their windows. Test-only, for a disposable guest.
 *   wintool list                 print every screen and window
 *   wintool batch LIST OUTDIR    run the programs in LIST
 * LIST has one line per program: id, seconds to wait for a window,
 * directory and command, separated by tabs. For each program wintool
 * notes the free memory, starts the command asynchronously in the
 * directory (output to OUTDIR/<id>.con), waits for a window that was not
 * there before, writes the window's screen to OUTDIR/<id>.snap, asks the
 * program to quit and notes the free memory again. Quitting goes from
 * Ctrl-C to IDCMP_CLOSEWINDOW for each of its windows, then to answering
 * requesters (Amiga-V, at last Amiga-B, on a system requester, Return on
 * any other window that opened meanwhile) and picking a menu item named
 * Quit, Exit or Beenden. Free memory is noted as total, chip, largest
 * block and largest chip block. Failed OpenLibrary calls of other tasks
 * meanwhile are listed as "missing". Every step is appended as a line to
 * OUTDIR/progress.txt at once, so the host can follow and see where a
 * guest stopped. A program that does not quit ends the batch ("stuck"):
 * wintool then waits forever, as its OpenLibrary patch must stay.
 * Build: m68k-amigaos-gcc -noixemul -Os -s wintool.c -o wintool -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

extern struct ExecBase *SysBase;
extern struct IntuitionBase *IntuitionBase;
ULONG __stack = 32768;

#define MAXWIN 64
struct Win {
    struct Window *w;
    struct Screen *s;
    struct Task *task;
    WORD x, y, width, height;
    char title[64];
};

static char progress[256];
static struct MsgPort *inport;
static struct IOStdReq *inreq;

static void say(const char *fmt, ...)
{
    char buf[300];
    va_list ap;
    BPTR fh;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");
    if ((fh = Open(progress, MODE_READWRITE)))
    {
        Seek(fh, 0, OFFSET_END);
        Write(fh, buf, strlen(buf));
        Close(fh);
    }
}

static int scan(struct Win *out)
{
    struct Screen *s;
    struct Window *w;
    ULONG lock = LockIBase(0);
    int n = 0;

    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        for (w = s->FirstWindow; w && n < MAXWIN; w = w->NextWindow)
        {
            struct Win *o = &out[n++];
            o->w = w;
            o->s = s;
            o->task = w->UserPort ? w->UserPort->mp_SigTask : NULL;
            o->x = w->LeftEdge;
            o->y = w->TopEdge;
            o->width = w->Width;
            o->height = w->Height;
            strncpy(o->title, w->Title ? (char *)w->Title : "", sizeof(o->title) - 1);
            o->title[sizeof(o->title) - 1] = 0;
        }
    UnlockIBase(lock);
    return n;
}

static int known(struct Win *list, int n, struct Window *w)
{
    int i;
    for (i = 0; i < n; i++)
        if (list[i].w == w)
            return 1;
    return 0;
}

/* Windows in now[] that are not in before[], moved to the front of now[]. */
static int fresh(struct Win *before, int nb, struct Win *now, int nn)
{
    int i, k = 0;
    for (i = 0; i < nn; i++)
        if (!known(before, nb, now[i].w))
        {
            struct Win t = now[k];
            now[k++] = now[i];
            now[i] = t;
        }
    return k;
}

static void list(void)
{
    struct Win w[MAXWIN];
    int i, n = scan(w);
    struct Screen *last = NULL;

    for (i = 0; i < n; i++)
    {
        if (w[i].s != last)
        {
            last = w[i].s;
            printf("screen \"%s\" %dx%d\n", last->Title ? (char *)last->Title : "",
                last->Width, last->Height);
        }
        printf("  window \"%s\" %d,%d %dx%d\n", w[i].title, w[i].x, w[i].y,
            w[i].width, w[i].height);
    }
}

/* ---- failed OpenLibrary calls ---- */

#define NLOG 256
struct Open { char name[48]; ULONG ver; ULONG ok; };
static struct Open opens[NLOG];
static volatile int nopen, logging;
static struct Task *self;
APTR wt_origOL;

void wt_noteOL(ULONG res, ULONG ver, const char *name)
{
    int i;
    if (!logging || SysBase->ThisTask == self || !name)
        return;
    Disable();
    i = nopen < NLOG ? nopen++ : -1;
    Enable();
    if (i >= 0)
    {
        strncpy(opens[i].name, name, sizeof(opens[i].name) - 1);
        opens[i].ver = ver;
        opens[i].ok = res != 0;
    }
}

/* OpenLibrary(a1 name, d0 version): call the original, then note the result */
extern struct Library *wt_patchOL(void);
__asm__(
"   .text\n"
"   .even\n"
"_wt_patchOL:\n"
"   movem.l d0/a1,-(sp)\n"
"   move.l  _wt_origOL,a0\n"
"   jsr     (a0)\n"
"   move.l  d0,-(sp)\n"
"   jsr     _wt_noteOL\n"
"   move.l  (sp)+,d0\n"
"   addq.l  #8,sp\n"
"   rts\n");

static const char *base(const char *p)
{
    const char *b = p;
    for (; *p; p++)
        if (*p == '/' || *p == ':')
            b = p + 1;
    return b;
}

static void missing(void)
{
    int i, j, n = nopen;
    for (i = 0; i < n; i++)
    {
        int later = 0;
        if (opens[i].ok)
            continue;
        for (j = 0; j < n && !later; j++)
            if (opens[j].ok && !strcmp(base(opens[j].name), base(opens[i].name)))
                later = 1;
        for (j = 0; j < i && !later; j++)
            if (!opens[j].ok && !strcmp(base(opens[j].name), base(opens[i].name)))
                later = 1;      /* listed already */
        if (!later)
            say("missing %s %lu", base(opens[i].name), opens[i].ver);
    }
}

/* ---- the program's processes ---- */

static int clis(UBYTE *used, int max)
{
    int i, n = MaxCli();
    if (n > max)
        n = max;
    Forbid();
    for (i = 1; i < n; i++)
        used[i] = FindCliProc(i) != NULL;
    Permit();
    return n;
}

static int alive(int cli)
{
    struct Process *p;
    Forbid();
    p = FindCliProc(cli);
    Permit();
    return p != NULL;
}

static int task_exists(struct Task *t)
{
    struct Node *n;
    int found = (t == SysBase->ThisTask);
    Disable();
    for (n = SysBase->TaskReady.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        found = (struct Task *)n == t;
    for (n = SysBase->TaskWait.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        found = (struct Task *)n == t;
    Enable();
    return found;
}

/* ---- snapshot: planar screen bitmap with its palette ---- */

static void snap(struct Screen *s, const char *path)
{
    struct BitMap *bm = s->RastPort.BitMap;
    int depth = bm->Depth, w = s->Width, h = s->Height;
    int rb = (w + 7) / 8, d, y, ncol;
    ULONG *rgb;
    UBYTE *buf, *q;
    BPTR fh;
    char head[64];

    if (depth > 8)
        return;
    ncol = 1 << depth;
    rgb = AllocVec(ncol * 3 * 4, MEMF_ANY);
    buf = AllocVec(ncol * 3 + rb * depth * h, MEMF_ANY);
    if (rgb && buf)
    {
        GetRGB32(s->ViewPort.ColorMap, 0, ncol, rgb);
        for (d = 0; d < ncol * 3; d++)
            buf[d] = rgb[d] >> 24;
        q = buf + ncol * 3;
        for (y = 0; y < h; y++)
            for (d = 0; d < depth; d++, q += rb)
                CopyMem(bm->Planes[d] + y * bm->BytesPerRow, q, rb);
        if ((fh = Open(path, MODE_NEWFILE)))
        {
            sprintf(head, "SNAP %ld %ld %ld\n", (long)w, (long)h, (long)depth);
            Write(fh, head, strlen(head));
            Write(fh, buf, ncol * 3 + rb * depth * h);
            Close(fh);
        }
    }
    FreeVec(buf);
    FreeVec(rgb);
}

/* ---- quitting ---- */

static struct MsgPort *replies;
static int outstanding;

static void closewindow(struct Window *w)
{
    struct ExtIntuiMessage *m;
    ULONG lock;
    struct Screen *s;
    struct Window *x;
    int ok = 0;

    if (!(m = AllocVec(sizeof(*m), MEMF_PUBLIC | MEMF_CLEAR)))
        return;
    m->eim_IntuiMessage.ExecMessage.mn_ReplyPort = replies;
    m->eim_IntuiMessage.ExecMessage.mn_Length = sizeof(*m);
    m->eim_IntuiMessage.Class = IDCMP_CLOSEWINDOW;
    m->eim_IntuiMessage.IDCMPWindow = w;
    CurrentTime(&m->eim_IntuiMessage.Seconds, &m->eim_IntuiMessage.Micros);
    lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && !ok; s = s->NextScreen)
        for (x = s->FirstWindow; x && !ok; x = x->NextWindow)
            if (x == w && w->UserPort && (w->IDCMPFlags & IDCMP_CLOSEWINDOW))
            {
                PutMsg(w->UserPort, (struct Message *)m);
                ok = 1;
            }
    UnlockIBase(lock);
    if (ok)
        outstanding++;
    else
        FreeVec(m);
}

static void collect_replies(void)
{
    struct Message *m;
    while ((m = GetMsg(replies)))
    {
        FreeVec(m);
        outstanding--;
    }
}

/* Is w still open? Call with IBase locked. */
static int open_window(struct Window *w)
{
    struct Screen *s;
    struct Window *x;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        for (x = s->FirstWindow; x; x = x->NextWindow)
            if (x == w)
                return 1;
    return 0;
}

/* A system requester (EasyRequest, AutoRequest): buttons with text, boolean
 * gadgets or (on OS 3) BOOPSI ones. */
static int sysreq(struct Window *w)
{
    struct Gadget *g;
    ULONG lock = LockIBase(0);
    int found = 0;
    if (open_window(w))
        for (g = w->FirstGadget; g && !found; g = g->NextGadget)
            found = !(g->GadgetType & GTYP_SYSGADGET) && g->GadgetText
                && ((g->GadgetType & GTYP_GTYPEMASK) == GTYP_BOOLGADGET
                    || (g->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET);
    UnlockIBase(lock);
    return found;
}

static void key(UWORD code, UWORD qual)
{
    struct InputEvent ie;
    int up;
    if (!inreq)
        return;
    for (up = 0; up < 2; up++)
    {
        memset(&ie, 0, sizeof(ie));
        ie.ie_Class = IECLASS_RAWKEY;
        ie.ie_Code = code | (up ? IECODE_UP_PREFIX : 0);
        ie.ie_Qualifier = qual;
        inreq->io_Command = IND_WRITEEVENT;
        inreq->io_Data = &ie;
        inreq->io_Length = sizeof(ie);
        DoIO((struct IORequest *)inreq);
        Delay(2);
    }
}

/* Answer a requester: Amiga-V picks a system requester's first (positive)
 * button, Amiga-B its last (negative) one, Return the active (default)
 * button of a MUI requester. */
static void answer(struct Win *w, int negative)
{
    int sys = sysreq(w->w);
    ULONG lock = LockIBase(0);
    int ok = open_window(w->w);
    UnlockIBase(lock);
    if (!ok)
        return;
    say("answer %s", w->title);
    ActivateWindow(w->w);
    Delay(15);
    if (sys)
        key(negative ? 0x35 : 0x34, IEQUALIFIER_LCOMMAND);  /* B, V */
    else
        key(0x44, 0);                       /* Return */
}

static int quitword(const char *t)
{
    static const char *words[] = { "quit", "exit", "beenden", NULL };
    int i, n;
    while (*t == ' ')
        t++;
    for (i = 0; words[i]; i++)
    {
        n = strlen(words[i]);
        if (!strnicmp(t, words[i], n) && !((t[n] | 0x20) >= 'a' && (t[n] | 0x20) <= 'z'))
            return 1;
    }
    return 0;
}

static int quititem(struct MenuItem *it)
{
    return (it->Flags & ITEMTEXT) && (it->Flags & ITEMENABLED) && it->ItemFill
        && ((struct IntuiText *)it->ItemFill)->IText
        && quitword((char *)((struct IntuiText *)it->ItemFill)->IText);
}

/* Send w the pick of its menu item named Quit (or Exit, Beenden). */
static int menu_quit(struct Window *w)
{
    struct IntuiMessage *m;
    struct Menu *mn;
    struct MenuItem *it, *sub, *hit = NULL;
    ULONG lock;
    UWORD code = MENUNULL;
    int i, j, k;

    if (!(m = AllocVec(sizeof(struct ExtIntuiMessage), MEMF_PUBLIC | MEMF_CLEAR)))
        return 0;
    lock = LockIBase(0);
    if (open_window(w) && w->UserPort && (w->IDCMPFlags & IDCMP_MENUPICK))
        for (mn = w->MenuStrip, i = 0; mn && !hit; mn = mn->NextMenu, i++)
            if (mn->Flags & MENUENABLED)
                for (it = mn->FirstItem, j = 0; it && !hit; it = it->NextItem, j++)
                {
                    if (!it->SubItem && quititem(it))
                        hit = it, code = FULLMENUNUM(i, j, NOSUB);
                    for (sub = it->SubItem, k = 0; sub && !hit; sub = sub->NextItem, k++)
                        if (quititem(sub))
                            hit = sub, code = FULLMENUNUM(i, j, k);
                }
    if (hit)
    {
        hit->NextSelect = MENUNULL;
        m->ExecMessage.mn_ReplyPort = replies;
        m->ExecMessage.mn_Length = sizeof(struct ExtIntuiMessage);
        m->Class = IDCMP_MENUPICK;
        m->Code = code;
        m->IDCMPWindow = w;
        CurrentTime(&m->Seconds, &m->Micros);
        PutMsg(w->UserPort, (struct Message *)m);
        outstanding++;
    }
    UnlockIBase(lock);
    if (!hit)
    {
        FreeVec(m);
        return 0;
    }
    say("menu %s", (char *)((struct IntuiText *)hit->ItemFill)->IText);
    return 1;
}

/* Wait up to ticks for the program's windows and its CLI process to go. */
static int gone(struct Win *before, int nb, int cli, int ticks)
{
    struct Win now[MAXWIN];
    for (; ticks > 0; ticks -= 10)
    {
        collect_replies();
        if (!fresh(before, nb, now, scan(now)) && (cli <= 0 || !alive(cli)))
            return 1;
        Delay(10);
    }
    return 0;
}

static void saymem(const char *tag)
{
    say("%s %lu %lu %lu %lu", tag, AvailMem(MEMF_ANY), AvailMem(MEMF_CHIP),
        AvailMem(MEMF_ANY | MEMF_LARGEST), AvailMem(MEMF_CHIP | MEMF_LARGEST));
}

/* After Ctrl-C and close-window messages failed: answer requesters, pick
 * the Quit menu item, close what is left, at last answer every window (a
 * start-up error window counts as a main window), system requesters with
 * their negative button ("Please insert volume": Cancel, not Retry).
 * Returns the step that ended the program, or NULL. */
static const char *escalate(struct Win *before, int nb, struct Win *main, int nmain, int cli)
{
    static const char order[] = "amacA";
    struct Win now[MAXWIN];
    const char *s;
    int i, nf, did;

    for (s = order; *s; s++)
    {
        nf = fresh(before, nb, now, scan(now));
        for (i = did = 0; i < nf; i++)
            if (*s == 'A' || (*s == 'a' && (sysreq(now[i].w) || !known(main, nmain, now[i].w))))
            {
                answer(&now[i], *s == 'A');
                did = 1;
            }
            else if (*s == 'm' && !did)
                did = menu_quit(now[i].w);
            else if (*s == 'c')
            {
                say("close %s", now[i].title);
                closewindow(now[i].w);
                did = 1;
            }
        if (did && gone(before, nb, cli, 300))
            return *s == 'm' ? "menu" : *s == 'c' ? "close" : "answer";
    }
    return NULL;
}

/* ---- one program ---- */

static int run(char *id, int wait, char *dir, char *cmd, char *outdir)
{
    struct Win before[MAXWIN], now[MAXWIN], main[MAXWIN];
    const char *how;
    UBYTE used[128], used2[128];
    struct Task *tasks[MAXWIN];
    int nb, nn, nf, nmain, i, k, cli = 0, ntasks = 0, ncli;
    LONG t, first = -1;
    char path[256];
    BPTR lock, old, in, out;
    struct DateStamp d0, d1;

    say("start %s", id);
    nb = scan(before);
    ncli = clis(used, 128);
    nopen = 0;
    saymem("mem");
    if (!(lock = Lock(dir, ACCESS_READ)))
    {
        say("nodir %s", dir);
        return 0;
    }
    sprintf(path, "%s/%s.con", outdir, id);
    in = Open("NIL:", MODE_OLDFILE);
    out = Open(path, MODE_NEWFILE);
    old = CurrentDir(lock);
    logging = 1;
    DateStamp(&d0);
    if (SystemTags(cmd, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
                   NP_StackSize, 65536, TAG_END) == -1)
    {
        Close(in);
        Close(out);
        cli = -1;
    }
    CurrentDir(old);
    UnLock(lock);
    if (cli < 0)
    {
        logging = 0;
        say("nostart");
        return 0;
    }
    for (k = 0; k < 20 && !cli; k++)
    {
        int n = clis(used2, 128);
        for (i = 1; i < n && !cli; i++)
            if (used2[i] && (i >= ncli || !used[i]))
                cli = i;
        if (!cli)
            Delay(5);
    }
    say("cli %d", cli);

    /* wait for the first new window, or the command to end */
    for (t = 0, k = -1;; t += 5)
    {
        nn = scan(now);
        if (fresh(before, nb, now, nn))
        {
            DateStamp(&d1);
            first = (d1.ds_Minute - d0.ds_Minute) * 3000 + d1.ds_Tick - d0.ds_Tick;
            break;
        }
        if (k < 0 && cli > 0 && !alive(cli))
            k = t;          /* ended: give a detached process 5 seconds */
        if ((k >= 0 && t - k > 250) || t > wait * 50)
            break;
        Delay(5);
    }
    if (first < 0)
    {
        logging = 0;
        say(k >= 0 ? "exited %ld" : "nowindow %ld", (long)t * 20);
        missing();
        saymem("memafter");
        if (cli > 0 && alive(cli))
        {
            Signal((struct Task *)FindCliProc(cli), SIGBREAKF_CTRL_C);
            if (!gone(before, nb, cli, 500))
            {
                say("stuck");
                return 1;
            }
        }
        say("end %s", id);
        return 0;
    }
    /* let it settle: windows unchanged for two seconds, at most ten */
    for (t = 0, k = 0; t < 500 && k < 100; t += 10)
    {
        Delay(10);
        i = scan(now);
        k = (i == nn) ? k + 10 : 0;
        nn = i;
    }
    nf = fresh(before, nb, now, nn);
    say("window %ld", (long)first * 20);
    for (i = 0; i < nf; i++)
    {
        say("win %d,%d %dx%d %s", now[i].x, now[i].y, now[i].width, now[i].height,
            now[i].title);
        for (k = 0; k < ntasks && tasks[k] != now[i].task; k++)
            ;
        if (k == ntasks && now[i].task)
            tasks[ntasks++] = now[i].task;
    }
    if (!nf)
        say("closed");      /* its windows went again while settling */
    memcpy(main, now, sizeof(main[0]) * nf);
    nmain = nf;
    saymem("memopen");
    if (nf)
    {
        sprintf(path, "%s/%s.snap", outdir, id);
        snap(now[0].s, path);
        say("snap");
    }

    /* quit: Ctrl-C to its processes, then close its windows */
    Forbid();
    if (cli > 0 && FindCliProc(cli))
        Signal((struct Task *)FindCliProc(cli), SIGBREAKF_CTRL_C);
    for (k = 0; k < ntasks; k++)
        if (task_exists(tasks[k]))
            Signal(tasks[k], SIGBREAKF_CTRL_C);
    Permit();
    if (gone(before, nb, cli, 500))
        say("quit ctrlc");
    else
    {
        for (k = 0; k < 3; k++)
        {
            nn = scan(now);
            nf = fresh(before, nb, now, nn);
            for (i = 0; i < nf; i++)
            {
                say("close %s", now[i].title);
                closewindow(now[i].w);
                Delay(50);
                collect_replies();
            }
            if (gone(before, nb, cli, 300))
                break;
        }
        if (k < 3)
            say("quit close");
        else if ((how = escalate(before, nb, main, nmain, cli)))
            say("quit %s", how);
        else
        {
            nn = scan(now);
            nf = fresh(before, nb, now, nn);
            for (i = 0; i < nf; i++)
                say("left %s", now[i].title);
            logging = 0;
            missing();
            say("stuck");
            return 1;
        }
    }
    logging = 0;
    Delay(100);
    missing();
    saymem("memafter");
    say("end %s", id);
    return 0;
}

static void batch(char *listfile, char *outdir)
{
    static char line[1024];
    FILE *f = fopen(listfile, "r");
    APTR cur;
    int stuck = 0;

    sprintf(progress, "%s/progress.txt", outdir);
    if (!f || !(replies = CreateMsgPort()))
    {
        say("nolist %s", listfile);
        return;
    }
    self = FindTask(NULL);
    if ((inport = CreateMsgPort())
        && (inreq = (struct IOStdReq *)CreateIORequest(inport, sizeof(*inreq)))
        && OpenDevice("input.device", 0, (struct IORequest *)inreq, 0))
    {
        DeleteIORequest((struct IORequest *)inreq);
        inreq = NULL;
    }
    Forbid();
    wt_origOL = SetFunction((struct Library *)SysBase, -552, (APTR)wt_patchOL);
    Permit();
    say("batch");
    while (!stuck && fgets(line, sizeof(line), f))
    {
        char *id = strtok(line, "\t\n"), *w = strtok(NULL, "\t\n");
        char *dir = strtok(NULL, "\t\n"), *cmd = strtok(NULL, "\t\n");
        if (id && w && dir && cmd)
            stuck = run(id, atoi(w), dir, cmd, outdir);
    }
    fclose(f);
    Forbid();
    cur = SetFunction((struct Library *)SysBase, -552, wt_origOL);
    if (cur != (APTR)wt_patchOL)
    {
        SetFunction((struct Library *)SysBase, -552, cur);  /* patched again since */
        stuck = 1;
    }
    Permit();
    if (stuck || outstanding)
    {
        say("reboot");
        Wait(0);            /* keep the patch and the messages alive */
    }
    say("batchdone");
    DeleteMsgPort(replies);
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "list"))
        list();
    else if (argc == 4 && !strcmp(argv[1], "batch"))
        batch(argv[2], argv[3]);
    else
    {
        printf("usage: wintool list | wintool batch LIST OUTDIR\n");
        return 10;
    }
    return 0;
}

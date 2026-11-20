/*
 * borderarrows: the window-border arrows of a Scrollgroup with
 * MUIA_Scrollgroup_UseWinBorder, and a plain Scrollgroup's own arrows.
 * The program drives the mouse itself (input.device events at the
 * gadgets' places), so it needs no user. It prints:
 *   - the milliseconds of an 8-pixel scroll step (setting Top, Left);
 *   - the Virtgroup's Left/Top after a click on each border arrow
 *     (up, down, left, right, found by their place in the border);
 *   - for a held border arrow (down, right): the milliseconds after the
 *     press (from the top left corner) of every change of Top/Left and
 *     the new value, then the value 300 ms after the release (sent by a
 *     process of its own, as the program may be busy in MUI's loop);
 *   - the clicks and a hold again after the window was closed and opened;
 *   - Left/Top after two clicks on each own arrow of a plain Scrollgroup
 *     and a held own down arrow, for comparison.
 * The program polls without waiting, so the times are exact to a few ms.
 * The same output on two libraries = the same arrows.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -Ibuild/muimaster/include \
 *        tests/native/borderarrows.c -o borderarrows -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/timer.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
struct Device *TimerBase;
ULONG __stack = 65536;

static struct MsgPort *port;
static struct IOStdReq *inreq;
static struct timerequest treq;
static Object *app, *win, *vg, *win2, *sg2, *vg2;

enum { UP, DOWN, LEFT, RIGHT };
static const char *names[] = { "up", "down", "left", "right" };

static ULONG now_ms(void)
{
    struct timeval tv;
    GetSysTime(&tv);
    return tv.tv_secs * 1000 + tv.tv_micro / 1000;
}

static void send_event(struct InputEvent *ie)
{
    ie->ie_NextEvent = NULL;
    inreq->io_Command = IND_WRITEEVENT;
    inreq->io_Data = ie;
    inreq->io_Length = sizeof(*ie);
    DoIO((struct IORequest *)inreq);
}

static void run(int n)
{
    ULONG sigs = 0;
    while (n-- > 0)
    {
        Delay(2);
        DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);
    }
}

static void move(struct Screen *scr, int x, int y)
{
    struct InputEvent ie;
    struct IEPointerPixel pp;

    memset(&ie, 0, sizeof(ie));
    pp.iepp_Screen = scr;
    pp.iepp_Position.X = x;
    pp.iepp_Position.Y = y;
    ie.ie_Class = IECLASS_NEWPOINTERPOS;
    ie.ie_SubClass = IESUBCLASS_PIXEL;
    ie.ie_EventAddress = &pp;
    send_event(&ie);
}

static void button(int up)
{
    struct InputEvent ie;

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWMOUSE;
    ie.ie_Code = up ? (IECODE_LBUTTON | IECODE_UP_PREFIX) : IECODE_LBUTTON;
    ie.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
    send_event(&ie);
}

static void click(struct Screen *scr, int x, int y)
{
    move(scr, x, y);
    run(2);
    button(0);
    run(2);
    button(1);
    run(6);
}

static void pos(Object *v, LONG *l, LONG *t)
{
    IPTR a = 0, b = 0;
    get(v, MUIA_Virtgroup_Left, &a);
    get(v, MUIA_Virtgroup_Top, &b);
    *l = a;
    *t = b;
}

static void report(Object *v, const char *what)
{
    LONG l, t;
    pos(v, &l, &t);
    printf("%s: left %ld top %ld\n", what, (long)l, (long)t);
}

/* The screen place of a border arrow of the window: a small non-system
 * gadget in the right border (up above down) or in the bottom border
 * (left before right). FALSE when there is none. */
static BOOL arrow(int which, struct Screen **scr, int *px, int *py)
{
    struct Window *w = NULL;
    struct Gadget *g;
    int found = 0, best = 0;

    get(win, MUIA_Window_Window, &w);
    if (!w)
        return FALSE;
    for (g = w->FirstGadget; g; g = g->NextGadget)
    {
        int x, y;
        BOOL vert;

        if (g->GadgetType & GTYP_SYSGADGET)
            continue;
        if (g->Width <= 0 || g->Height <= 0 || g->Width > 30
            || g->Height > 30)
            continue;
        x = ((g->Flags & GFLG_RELRIGHT) ? w->Width - 1 + g->LeftEdge
            : g->LeftEdge) + g->Width / 2;
        y = ((g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 + g->TopEdge
            : g->TopEdge) + g->Height / 2;
        if (x >= w->Width - w->BorderRight
            && y >= w->BorderTop && y < w->Height - w->BorderBottom / 2)
            vert = TRUE;
        else if (y >= w->Height - w->BorderBottom
            && x < w->Width - w->BorderRight)
            vert = FALSE;
        else
            continue;
        if (vert != (which == UP || which == DOWN))
            continue;
        /* up and left: the smaller place; down and right: the larger */
        if (!found || ((which == UP || which == LEFT)
                ? (vert ? y : x) < best : (vert ? y : x) > best))
        {
            best = vert ? y : x;
            *px = x + w->LeftEdge;
            *py = y + w->TopEdge;
            *scr = w->WScreen;
            found = 1;
        }
    }
    return found;
}

static void click_arrows(const char *what)
{
    struct Screen *scr;
    int n, x, y;
    char buf[64];

    for (n = UP; n <= RIGHT; n++)
    {
        if (!arrow(n, &scr, &x, &y))
        {
            printf("%s: no %s arrow\n", what, names[n]);
            continue;
        }
        click(scr, x, y);
        sprintf(buf, "%s %s arrow", what, names[n]);
        report(vg, buf);
    }
}

/* The release comes from a process of its own, as from a user: the
 * program may be busy in MUI's input loop at that time */
static struct Task *parent;
static BYTE releasesig = -1;
static ULONG release_ms;

static void releaser(void)
{
    struct MsgPort *p = CreateMsgPort();
    struct IOStdReq *r = p ? (struct IOStdReq *)CreateIORequest(p,
        sizeof(*r)) : NULL;
    struct InputEvent ie;

    Delay(release_ms / 20);
    if (r && !OpenDevice("input.device", 0, (struct IORequest *)r, 0))
    {
        memset(&ie, 0, sizeof(ie));
        ie.ie_Class = IECLASS_RAWMOUSE;
        ie.ie_Code = IECODE_LBUTTON | IECODE_UP_PREFIX;
        ie.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
        r->io_Command = IND_WRITEEVENT;
        r->io_Data = &ie;
        r->io_Length = sizeof(ie);
        DoIO((struct IORequest *)r);
        CloseDevice((struct IORequest *)r);
    }
    if (r)
        DeleteIORequest((struct IORequest *)r);
    if (p)
        DeleteMsgPort(p);
    Forbid();
    Signal(parent, 1L << releasesig);
}

/* Press at x,y and release ms milliseconds later, polling without
 * waiting, print the time of each change of v's Left/Top */
static void hold_at(Object *v, struct Screen *scr, int x, int y, ULONG ms,
    const char *what)
{
    LONG l0, t0, l, t;
    ULONG start, el, sigs, done = 0;
    int changes = 0;

    move(scr, x, y);
    run(2);
    pos(v, &l0, &t0);
    printf("%s from left %ld top %ld:", what, (long)l0, (long)t0);
    parent = FindTask(NULL);
    release_ms = ms;
    SetSignal(0, 1L << releasesig);
    start = now_ms();
    button(0);
    if (!CreateNewProcTags(NP_Entry, (IPTR)releaser, NP_Name,
            (IPTR)"borderarrows release", TAG_DONE))
    {
        button(1);
        printf(" no process\n");
        return;
    }
    do
    {
        sigs = 0;
        DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);
        el = now_ms() - start;
        pos(v, &l, &t);
        if (l != l0 || t != t0)
        {
            printf(" %ld:%ld", (long)el, (long)(l != l0 ? l : t));
            if (++changes % 8 == 0)
                printf("\n ");
            l0 = l;
            t0 = t;
        }
        if (!done && (SetSignal(0, 0) & (1L << releasesig)))
        {
            SetSignal(0, 1L << releasesig);
            done = now_ms();
            printf(" released at %ld;", (long)(done - start));
        }
    }
    while (!done || now_ms() - done < 300);
    pos(v, &l, &t);
    printf("\n%s 300 ms after the release: left %ld top %ld\n", what,
        (long)l, (long)t);
}

/* Back to the top left corner */
static void home(Object *v)
{
    SetAttrs(v, MUIA_Virtgroup_Top, 0, MUIA_Virtgroup_Left, 0, TAG_DONE);
    run(3);
}

/* The time of ten 8-pixel scroll steps down and right, by setting the
 * Virtgroup's Top and Left */
static void step_time(Object *v)
{
    ULONG t0, t1, t2;
    int i;

    t0 = now_ms();
    for (i = 1; i <= 10; i++)
        set(v, MUIA_Virtgroup_Top, i * 8);
    t1 = now_ms();
    for (i = 1; i <= 10; i++)
        set(v, MUIA_Virtgroup_Left, i * 8);
    t2 = now_ms();
    printf("ms per 8-pixel step: down %ld right %ld\n",
        (long)(t1 - t0) / 10, (long)(t2 - t1) / 10);
    home(v);
}

static void hold(int which, ULONG ms, const char *what)
{
    struct Screen *scr;
    int x, y;

    home(vg);
    if (arrow(which, &scr, &x, &y))
        hold_at(vg, scr, x, y, ms, what);
    else
        printf("%s: no arrow\n", what);
}

/* Two clicks on each small child (the arrows) of both bars of sg2, then
 * the vertical bar's last arrow held */
static void own_arrows(void)
{
    struct Window *w = NULL;
    struct List *list = NULL;
    Object *bar, *o;
    APTR st;
    int k, i, n, hx = 0, hy = 0;
    char buf[48];

    get(win2, MUIA_Window_Window, &w);
    if (!w)
        return;
    for (k = 0; k < 2; k++)
    {
        bar = NULL;
        get(sg2, k ? MUIA_Scrollgroup_HorizBar : MUIA_Scrollgroup_VertBar,
            &bar);
        if (!bar || !get(bar, MUIA_Group_ChildList, &list) || !list)
            continue;
        st = list->lh_Head;
        n = 0;
        while ((o = NextObject(&st)))
        {
            IPTR x = 0, y = 0, ww = 0, hh = 0;
            get(o, MUIA_LeftEdge, &x);
            get(o, MUIA_TopEdge, &y);
            get(o, MUIA_Width, &ww);
            get(o, MUIA_Height, &hh);
            if (ww > 30 || hh > 30)
                continue;
            n++;
            if (!k)
            {
                hx = w->LeftEdge + x + ww / 2;
                hy = w->TopEdge + y + hh / 2;
            }
            for (i = 0; i < 2; i++)
            {
                click(w->WScreen, w->LeftEdge + x + ww / 2,
                    w->TopEdge + y + hh / 2);
                sprintf(buf, "own %s arrow %ld click %ld",
                    k ? "horiz" : "vert", (long)n, (long)i + 1);
                report(vg2, buf);
            }
        }
    }
    home(vg2);
    if (hx)
        hold_at(vg2, w->WScreen, hx, hy, 1500, "own held down");
}

static Object *cells(void)
{
    Object *body = ColGroup(4), End;
    int i;
    char buf[32];

    for (i = 0; body && i < 80; i++)
    {
        sprintf(buf, "Cell number %ld", (long)i);
        DoMethod(body, OM_ADDMEMBER, (IPTR)SimpleButton(buf));
    }
    return body;
}

int main(void)
{
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)&treq, 0))
        return 20;
    TimerBase = treq.tr_node.io_Device;
    if ((releasesig = AllocSignal(-1)) < 0)
        return 20;
    port = CreateMsgPort();
    inreq = (struct IOStdReq *)CreateIORequest(port, sizeof(*inreq));
    if (!inreq || OpenDevice("input.device", 0, (struct IORequest *)inreq, 0))
        return 20;
    app = ApplicationObject, MUIA_Application_Title, "borderarrows",
        SubWindow, win = WindowObject, MUIA_Window_Title, "borderarrows",
            MUIA_Window_Width, 220, MUIA_Window_Height, 140,
            MUIA_Window_LeftEdge, 40, MUIA_Window_TopEdge, 30,
            MUIA_Window_UseRightBorderScroller, TRUE,
            MUIA_Window_UseBottomBorderScroller, TRUE,
            WindowContents, ScrollgroupObject,
                MUIA_Scrollgroup_UseWinBorder, TRUE,
                MUIA_Scrollgroup_Contents, vg = VirtgroupObject,
                    Child, cells(),
                End,
            End,
        End,
        SubWindow, win2 = WindowObject, MUIA_Window_Title, "ownarrows",
            MUIA_Window_Width, 220, MUIA_Window_Height, 140,
            MUIA_Window_LeftEdge, 300, MUIA_Window_TopEdge, 30,
            WindowContents, sg2 = ScrollgroupObject,
                MUIA_Scrollgroup_Contents, vg2 = VirtgroupObject,
                    Child, cells(),
                End,
            End,
        End,
    End;
    if (!app)
    {
        printf("no application\n");
        return 20;
    }
    set(win, MUIA_Window_Open, TRUE);
    run(5);
    report(vg, "opened");
    step_time(vg);
    click_arrows("border");
    hold(DOWN, 2000, "held down");
    hold(RIGHT, 1200, "held right");
    set(win, MUIA_Window_Open, FALSE);
    set(win, MUIA_Window_Open, TRUE);
    run(5);
    report(vg, "reopened");
    click_arrows("reopened");
    hold(DOWN, 800, "reopened held down");
    set(win, MUIA_Window_Open, FALSE);

    set(win2, MUIA_Window_Open, TRUE);
    run(5);
    report(vg2, "own opened");
    own_arrows();
    MUI_DisposeObject(app);
    CloseDevice((struct IORequest *)inreq);
    DeleteIORequest((struct IORequest *)inreq);
    DeleteMsgPort(port);
    CloseDevice((struct IORequest *)&treq);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}

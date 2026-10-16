/* Exercise another application's window the way a user's desktop does
 * and check that every pixel comes back: cover and uncover it with a
 * window, shrink it and restore its size, send it to the back and bring
 * it to the front. After each step, polls the window's pixels until they
 * match the original again and prints how long that took (EClock ticks),
 * or how many pixels still differ after the timeout. Also prints the chip
 * and total memory taken while the window is covered (the backing store
 * of a smart refresh window). Compares smart and simple refresh builds
 * with real applications (IBrowse, YAM, Voyager).
 *   coverwin <title prefix> [cycles]
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit coverwin.c -o coverwin -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <devices/timer.h>
#include <intuition/intuitionbase.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/timer.h>

extern struct IntuitionBase *IntuitionBase;
extern struct ExecBase *SysBase;
struct Device *TimerBase;
ULONG __stack = 32768;

static struct Window *win;
static struct RastPort temprp;
static WORD bx, by, bw, bh;
static UBYTE *orig, *now_px;
static ULONG eclock_hz;

static ULONG now(void)
{
    struct EClockVal ev;

    eclock_hz = ReadEClock(&ev);
    return ev.ev_lo;
}

static struct Window *find_window(const char *prefix)
{
    struct Screen *s;
    struct Window *w, *found = NULL;
    ULONG lock = LockIBase(0);
    int n = strlen(prefix);

    for (s = IntuitionBase->FirstScreen; s && !found; s = s->NextScreen)
        for (w = s->FirstWindow; w && !found; w = w->NextWindow)
            if (w->Title && !strncmp((const char *) w->Title, prefix, n))
                found = w;
    UnlockIBase(lock);
    return found;
}

/* the window's box as it is on the screen, read from the screen */
static void snap(UBYTE *buf)
{
    struct RastPort *rp = &win->WScreen->RastPort;
    WORD y;

    for (y = 0; y < bh; y++)
        ReadPixelLine8(rp, bx, by + y, bw, buf + (LONG) y * ((bw + 15) & ~15),
            &temprp);
}

static WORD dx0, dy0, dx1, dy1;

/* differing pixels, and their bounding box relative to the window */
static LONG diffs(void)
{
    LONG n = 0, mod = (bw + 15) & ~15;
    WORD x, y;

    snap(now_px);
    dx0 = dy0 = 32767;
    dx1 = dy1 = -1;
    for (y = 0; y < bh; y++)
        for (x = 0; x < bw; x++)
            if (orig[y * mod + x] != now_px[y * mod + x])
            {
                n++;
                if (x < dx0) dx0 = x;
                if (x > dx1) dx1 = x;
                if (y < dy0) dy0 = y;
                if (y > dy1) dy1 = y;
            }
    return n;
}

/* wait until the window looks as it did, at most 'secs' seconds */
static void settle(const char *what, ULONG start, int secs)
{
    ULONG limit = eclock_hz * secs;
    LONG d;

    for (;;)
    {
        d = diffs();
        if (!d || now() - start > limit)
            break;
        Delay(1);
    }
    if (d)
        printf("%-22s %ld pixels still differ after %ld s, in %ld,%ld-%ld,%ld\n",
            what, (long) d, (long) secs, (long) dx0, (long) dy0, (long) dx1,
            (long) dy1);
    else
        printf("%-22s restored after %lu ticks\n", what,
            (unsigned long) (now() - start));
}

int main(int argc, char **argv)
{
    struct timerequest tr;
    struct BitMap *tmpbm;
    int cycles = argc > 2 ? atoi(argv[2]) : 3, i, rc = 20;
    WORD ow, oh;
    ULONG chip0, any0, chipused = 0, anyused = 0;

    if (argc < 2)
        return 20;
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *) &tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    now();
    if (!(win = find_window(argv[1])))
    {
        struct Screen *s;
        struct Window *w;
        ULONG lock = LockIBase(0);

        printf("no window titled %s*; windows:\n", argv[1]);
        for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
            for (w = s->FirstWindow; w; w = w->NextWindow)
                printf("  \"%s\"\n", w->Title ? (char *) w->Title : "");
        UnlockIBase(lock);
        goto close;
    }
    bx = win->LeftEdge;
    by = win->TopEdge;
    bw = win->Width;
    bh = win->Height;
    ow = bw;
    oh = bh;
    {
        struct Library *mui;

        Forbid();
        mui = (struct Library *) FindName(&SysBase->LibList,
            "muimaster.library");
        printf("in memory: %s", mui && mui->lib_IdString ?
            (char *) mui->lib_IdString : "no muimaster.library\n");
        if (mui)
            printf("  version %ld.%ld, %ld vectors\n", (long) mui->lib_Version,
                (long) mui->lib_Revision, (long) mui->lib_NegSize / 6);
        Permit();
    }
    printf("window \"%s\" %ldx%ld at %ld,%ld refresh %s\n", win->Title,
        (long) bw, (long) bh, (long) bx, (long) by, (win->Flags & WFLG_REFRESHBITS) == WFLG_SMART_REFRESH ?
        "smart" : (win->Flags & WFLG_REFRESHBITS) == WFLG_SIMPLE_REFRESH ?
        "simple" : "other");
    InitRastPort(&temprp);
    tmpbm = AllocBitMap((bw + 15) & ~15, 1, 8, 0, NULL);
    orig = AllocVec((LONG) bh * ((bw + 15) & ~15), MEMF_ANY);
    now_px = AllocVec((LONG) bh * ((bw + 15) & ~15), MEMF_ANY);
    if (!tmpbm || !orig || !now_px)
        goto free;
    temprp.BitMap = tmpbm;
    snap(orig);

    for (i = 0; i < cycles; i++)
    {
        struct Window *c;
        ULONG t;
        char what[32];

        chip0 = AvailMem(MEMF_CHIP);
        any0 = AvailMem(MEMF_ANY);
        c = OpenWindowTags(NULL, WA_CustomScreen, (ULONG) win->WScreen,
            WA_Left, bx + bw / 4, WA_Top, by + bh / 4,
            WA_Width, bw / 2, WA_Height, bh / 2,
            WA_SimpleRefresh, TRUE, WA_Borderless, TRUE,
            WA_Activate, FALSE, TAG_DONE);
        if (!c)
            goto free;
        if (chip0 - AvailMem(MEMF_CHIP) > chipused)
            chipused = chip0 - AvailMem(MEMF_CHIP);
        if (any0 - AvailMem(MEMF_ANY) > anyused)
            anyused = any0 - AvailMem(MEMF_ANY);
        SetAPen(c->RPort, 1);
        RectFill(c->RPort, 0, 0, c->Width - 1, c->Height - 1);
        Delay(10);
        t = now();
        CloseWindow(c);
        sprintf(what, "uncover %ld", (long) i + 1);
        settle(what, t, 5);
    }
    printf("memory taken while covered: chip %lu, total %lu bytes\n",
        (unsigned long) chipused, (unsigned long) anyused);

    if (win->Flags & WFLG_SIZEGADGET)
    {
        ChangeWindowBox(win, bx, by, ow - ow / 4, oh - oh / 4);
        Delay(100);
        ChangeWindowBox(win, bx, by, ow, oh);
        Delay(5);
        settle("shrink and restore", now(), 10);
    }

    WindowToBack(win);
    Delay(50);
    WindowToFront(win);
    settle("to back and front", now(), 5);

    rc = diffs() ? 10 : 0;
free:
    FreeVec(orig);
    FreeVec(now_px);
    if (tmpbm)
        FreeBitMap(tmpbm);
close:
    CloseDevice((struct IORequest *) &tr);
    printf(rc ? "FAIL rc=%d\n" : "PASS rc=%d\n", rc);
    return rc;
}

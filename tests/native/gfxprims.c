/* Graphics primitive timing on the test system.
 * Compares one-pixel lines drawn with RectFill and with Move/Draw, single
 * pixels with WritePixel and RectFill, and redundant SetAPen calls, in a
 * simple-refresh window on the Workbench screen.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit gfxprims.c -o gfxprims -lamiga
 */
#include <stdio.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/timer.h>
#include <string.h>
struct Device *TimerBase;
ULONG __stack = 32768;

#define N 400

static ULONG now(void)
{
    struct EClockVal ev;

    ReadEClock(&ev);
    return ev.ev_lo;
}

/* "screen": a private 640x480 depth-2 screen like perfbench's; the
 * window is then 240x420 like the benchmark's. Adds blitter-based line
 * alternatives (BltBitMapRastPort of a one-pixel-wide/high bitmap). */
int main(int argc, char **argv)
{
    struct timerequest tr;
    struct Window *w;
    struct RastPort *rp;
    struct Screen *scr = NULL;
    struct BitMap *vbm, *hbm;
    ULONG t;
    int i, depth;

    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    if (argc > 1 && !strcmp(argv[1], "screen"))
    {
        scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_Depth, 2,
            TAG_DONE);
        if (!scr)
            return 20;
    }
    w = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 20, WA_Width, scr ? 240 : 200,
        WA_Height, scr ? 420 : 120, WA_SimpleRefresh, TRUE,
        WA_Title, (ULONG)"gfxprims", scr ? WA_CustomScreen : TAG_IGNORE,
        (ULONG)scr, TAG_DONE);
    if (!w)
        return 20;
    rp = w->RPort;
    depth = rp->BitMap->Depth;
    printf("depth %d\n", depth);
    vbm = AllocBitMap(16, 50, depth, BMF_CLEAR, rp->BitMap);
    hbm = AllocBitMap(64, 1, depth, BMF_CLEAR, rp->BitMap);
    if (vbm && hbm)
    {
        struct RastPort tmp;
        InitRastPort(&tmp);
        tmp.BitMap = vbm; SetAPen(&tmp, 1); RectFill(&tmp, 0, 0, 0, 49);
        tmp.BitMap = hbm; RectFill(&tmp, 0, 0, 49, 0);
    }
    SetAPen(rp, 1);
    if (vbm && hbm)
    {
        t = now();
        for (i = 0; i < N; i++)
            BltBitMapRastPort(vbm, 0, 0, rp, 24, 34, 1, 50, 0xc0);
        printf("vline bltbmrp   %lu\n", (unsigned long)(now() - t));
        t = now();
        for (i = 0; i < N; i++)
            BltBitMapRastPort(hbm, 0, 0, rp, 20, 28, 50, 1, 0xc0);
        printf("hline bltbmrp   %lu\n", (unsigned long)(now() - t));
        t = now();
        for (i = 0; i < N; i++)
            BltBitMapRastPort(vbm, 0, 0, rp, 26, 34, 1, 20, 0xc0);
        printf("vline20 bltbmrp %lu\n", (unsigned long)(now() - t));
    }
    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 28, 34, 28, 53);
    printf("vline20 rectfill %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
    {
        Move(rp, 30, 34);
        Draw(rp, 30, 53);
    }
    printf("vline20 movedraw %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 20, 36, 39, 36);
    printf("hline20 rectfill %lu\n", (unsigned long)(now() - t));

    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 20, 30, 69, 30);
    printf("hline rectfill  %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
    {
        Move(rp, 20, 32);
        Draw(rp, 69, 32);
    }
    printf("hline movedraw  %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 20, 34, 20, 83);
    printf("vline rectfill  %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
    {
        Move(rp, 22, 34);
        Draw(rp, 22, 83);
    }
    printf("vline movedraw  %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
        WritePixel(rp, 30, 90);
    printf("pixel write     %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 32, 90, 32, 90);
    printf("pixel rectfill  %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
        SetAPen(rp, 1);
    printf("setapen same    %lu\n", (unsigned long)(now() - t));
    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 40, 40, 140, 100);
    printf("area rectfill   %lu\n", (unsigned long)(now() - t));
    printf("count %d\n", N);
    if (vbm) FreeBitMap(vbm);
    if (hbm) FreeBitMap(hbm);

    CloseWindow(w);
    if (scr) CloseScreen(scr);
    CloseDevice((struct IORequest *)&tr);
    return 0;
}

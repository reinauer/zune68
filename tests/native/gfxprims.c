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
struct Device *TimerBase;
ULONG __stack = 32768;

#define N 400

static ULONG now(void)
{
    struct EClockVal ev;

    ReadEClock(&ev);
    return ev.ev_lo;
}

int main(void)
{
    struct timerequest tr;
    struct Window *w;
    struct RastPort *rp;
    ULONG t;
    int i;

    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    w = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 40, WA_Width, 200,
        WA_Height, 120, WA_SimpleRefresh, TRUE, WA_Title, (ULONG)"gfxprims",
        TAG_DONE);
    if (!w)
        return 20;
    rp = w->RPort;
    SetAPen(rp, 1);

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

    CloseWindow(w);
    CloseDevice((struct IORequest *)&tr);
    return 0;
}

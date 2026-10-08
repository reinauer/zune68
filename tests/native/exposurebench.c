/* Whole-screen exposure latency, not client CPU time.
 * Run identical geometry over the browser document and blank Workbench.
 * No MUI objects or AppIcons are created. Foreign Window/Layer fields are
 * read only while LockIBase is held; pointers are revalidated each poll.
 */
#include <exec/types.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/layers.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Device *TimerBase;
ULONG __stack = 65536;
#define MAX_TRIALS 50
#define POLL_LIMIT 100
#define COVER_WIDTH 160
#define COVER_HEIGHT 48

struct Sample {
    ULONG open, close, settle, hold, polls;
};
static struct Sample samples[MAX_TRIALS];
static ULONG frequency;
static LONG x = 200, y = 120;

static ULONG elapsed(const struct EClockVal *start, const struct EClockVal *end)
{
    /* Each bounded phase is much shorter than one 32-bit EClock wrap. */
    return end->ev_lo - start->ev_lo;
}

static APTR observe(struct Screen *screen, APTR token, const char *prefix,
                    ULONG *layerflags, ULONG *windowflags, char *title)
{
    struct Window *window;
    APTR found = NULL;
    ULONG lock = LockIBase(0);
    for (window = screen->FirstWindow; window; window = window->NextWindow) {
        if (token && window != token) continue;
        if (!token && prefix && (!window->Title ||
            strncmp((char *)window->Title, prefix, strlen(prefix)))) continue;
        if (x < window->LeftEdge || y < window->TopEdge ||
            x + COVER_WIDTH > window->LeftEdge + window->Width ||
            y + COVER_HEIGHT > window->TopEdge + window->Height) continue;
        if (!window->WLayer) continue;
        *layerflags = window->WLayer->Flags;
        *windowflags = window->Flags;
        if (title) {
            strncpy(title, window->Title ? (char *)window->Title : "<untitled>", 95);
            title[95] = 0;
        }
        found = window;
        break;
    }
    UnlockIBase(lock);
    return found;
}

static BOOL settle(struct Screen *screen, APTR target, struct Sample *sample)
{
    ULONG layerflags = 0, windowflags = 0, quiet = 0, i;
    struct EClockVal start, end;
    ReadEClock(&start);
    for (i = 1; i <= POLL_LIMIT; i++) {
        /* Two consecutive quiet ticks are a bounded observation heuristic,
         * not proof that a client has no deferred application work.
         */
        Delay(1);
        if (!observe(screen, target, NULL, &layerflags, &windowflags, NULL)) {
            puts("STOP: target window disappeared or moved out of test rectangle");
            return FALSE;
        }
        if (layerflags & (LAYERREFRESH | LAYERUPDATING)) quiet = 0;
        else quiet++;
        if (quiet == 2) break;
    }
    ReadEClock(&end);
    sample->settle = elapsed(&start, &end);
    sample->polls = i;
    if (quiet != 2) {
        printf("TIMEOUT: refresh flags=%08lx after %u tick polls\n",
               layerflags, POLL_LIMIT);
        return FALSE;
    }
    return TRUE;
}

static BOOL trial(struct Screen *screen, APTR target, struct Sample *sample)
{
    struct EClockVal start, end;
    struct Window *cover;
    ReadEClock(&start);
    cover = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)screen,
        WA_Left, x, WA_Top, y, WA_Width, COVER_WIDTH, WA_Height, COVER_HEIGHT,
        WA_Borderless, TRUE, WA_Activate, FALSE, WA_SmartRefresh, TRUE,
        WA_IDCMP, 0, WA_RMBTrap, TRUE, TAG_DONE);
    if (!cover) { puts("FAIL: cover window allocation"); return FALSE; }
    SetAPen(cover->RPort, 0);
    RectFill(cover->RPort, 0, 0, COVER_WIDTH - 1, COVER_HEIGHT - 1);
    ReadEClock(&end);
    sample->open = elapsed(&start, &end);
    ReadEClock(&start);
    Delay(1);
    ReadEClock(&end);
    sample->hold = elapsed(&start, &end);
    ReadEClock(&start);
    CloseWindow(cover);
    ReadEClock(&end);
    sample->close = elapsed(&start, &end);
    return settle(screen, target, sample);
}

static ULONG field(const struct Sample *s, unsigned int item)
{
    if (item == 0) return s->open;
    if (item == 1) return s->close;
    if (item == 2) return s->settle;
    if (item == 3) return s->hold;
    return s->open + s->close + s->settle;
}

static void report(unsigned int count)
{
    static const char *names[] = {"open-and-fill", "close", "settle", "cover-hold",
                                 "exposure-total-excluding-hold"};
    ULONG values[MAX_TRIALS], total, extra = 0;
    unsigned int item, i, j;
    puts("All timings below are EClock ticks; divide by eclock-hz for seconds.");
    for (item = 0; item < 5; item++) {
        total = 0;
        for (i = 0; i < count; i++) {
            ULONG value = field(&samples[i], item);
            total += value;
            j = i;
            while (j && values[j - 1] > value) {
                values[j] = values[j - 1];
                j--;
            }
            values[j] = value;
        }
        printf("phase=%s count=%u total=%lu median=%lu min=%lu max=%lu\n",
               names[item], count, total, values[count / 2], values[0], values[count - 1]);
    }
    for (i = 0; i < count; i++) extra += samples[i].polls - 2;
    printf("settle-fixed-delay-ticks=%u settle-extra-delay-ticks=%lu "
           "cover-hold-delay-ticks=%u timeouts=0\n", count * 2, extra, count);
    puts("Delay ticks are 1/50 second nominal; measured EClock includes scheduling delays.");
}

int main(int argc, char **argv)
{
    struct MsgPort *port = NULL;
    struct timerequest *timer = NULL;
    struct Screen *screen = NULL;
    struct EClockVal stamp;
    struct Sample warmup;
    APTR target;
    ULONG layerflags, windowflags;
    char title[96];
    const char *prefix = NULL;
    unsigned int count = 20, i;
    int result = 20;
    if (argc != 1 && argc != 3 && argc != 4 && argc != 5) {
        puts("Usage: exposurebench [x y [trials [target-title-prefix]]]");
        return 20;
    }
    if (argc >= 3) { x = atol(argv[1]); y = atol(argv[2]); }
    if (argc >= 4) count = atoi(argv[3]);
    if (argc == 5) prefix = argv[4];
    if (count < 1 || count > MAX_TRIALS || x < 0 || y < 0) return 20;
    port = CreateMsgPort();
    if (!port) goto done;
    timer = (struct timerequest *)CreateIORequest(port, sizeof(*timer));
    if (!timer) goto done;
    if (OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)timer, 0)) goto done;
    TimerBase = timer->tr_node.io_Device;
    frequency = ReadEClock(&stamp);
    screen = LockPubScreen("Workbench");
    if (!screen) goto done;
    target = observe(screen, NULL, prefix, &layerflags, &windowflags, title);
    if (!target) { puts("FAIL: no target window contains the test rectangle"); goto done; }
    printf("exposurebench=1 x=%ld y=%ld width=%u height=%u trials=%u warmup=3 "
           "eclock-hz=%lu\n", x, y, COVER_WIDTH, COVER_HEIGHT, count, frequency);
    printf("target=%s refresh=%s initial-layer-flags=%08lx\n", title,
           windowflags & WFLG_SIMPLE_REFRESH ? "simple" :
           windowflags & WFLG_SUPER_BITMAP ? "superbitmap" : "smart", layerflags);
    puts("Measures exposure latency including client/OS scheduling, not client CPU time.");
    fflush(stdout);
    for (i = 0; i < 3; i++) if (!trial(screen, target, &warmup)) goto done;
    for (i = 0; i < count; i++) {
        if (!trial(screen, target, &samples[i])) goto done;
        if ((i + 1) % 5 == 0) {
            printf("completed=%u\n", i + 1);
            fflush(stdout);
        }
    }
    report(count);
    result = 0;
done:
    if (screen) UnlockPubScreen(NULL, screen);
    if (TimerBase) CloseDevice((struct IORequest *)timer);
    if (timer) DeleteIORequest((struct IORequest *)timer);
    if (port) DeleteMsgPort(port);
    printf("result=%d\n", result);
    return result;
}

/* Text update cost with an inherited versus an own background, nested
 * like perfbench's status text. Prints median EClock ticks per 100 sets
 * and per 100 MUI_Redraw calls.
 *   textupdate                 timings
 *   textupdate <n> set|inh     n updates of the own/inherited text, for
 *                              profiling; "sh" then holds "Status 42",
 *                              wider than the texts, for comparing how
 *                              overflowing text is cut
 *   textupdate <n>             n MUI_Redraw calls of the own text
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit textupdate.c -o textupdate -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <devices/timer.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
struct Device *TimerBase;
ULONG __stack = 65536;
static ULONG now(void) { struct EClockVal ev; ReadEClock(&ev); return ev.ev_lo; }
static int cmp(const void *a, const void *b) { ULONG x = *(const ULONG *)a, y = *(const ULONG *)b; return x < y ? -1 : x > y; }
static ULONG redraw(Object *t)
{
    ULONG s[11], t0; int i, j;
    for (i = 0; i < 11; i++) {
        t0 = now();
        for (j = 0; j < 100; j++) MUI_Redraw(t, MADF_DRAWOBJECT);
        s[i] = now() - t0;
    }
    qsort(s, 11, sizeof(s[0]), cmp);
    return s[5];
}
static ULONG run(Object *t)
{
    static char buf[32]; ULONG s[11], t0; int i, j;
    for (i = 0; i < 11; i++) {
        t0 = now();
        for (j = 0; j < 100; j++) { sprintf(buf, "Status %d", j); set(t, MUIA_Text_Contents, buf); }
        s[i] = now() - t0;
    }
    qsort(s, 11, sizeof(s[0]), cmp);
    return s[5];
}
int main(int argc, char **argv)
{
    struct timerequest tr; Object *app, *win, *a, *b, *c; ULONG open = 0;
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0)) return 20;
    TimerBase = tr.tr_node.io_Device;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "textbg",
        SubWindow, win = WindowObject, MUIA_Window_Title, "textbg",
            WindowContents, VGroup,
                Child, HGroup, Child, VGroup,
                    Child, a = TextObject, TextFrame, MUIA_Text_Contents, "Status", End,
                    Child, b = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                        MUIA_Text_Contents, "Status", End,
                    Child, c = TextObject, MUIA_Text_Contents, "Status", End,
                    End, End,
                End,
            End,
        End;
    if (!app) return 20;
    if (argc > 1) {
        set(win, MUIA_Window_Open, TRUE);
        Delay(250);
        if (argc > 2) {
            static char buf[32];
            Object *t = argv[2][0] == 'i' ? a : b;
            for (open = 0; open < (ULONG)atol(argv[1]); open++) {
                sprintf(buf, "Status %lu", open % 100);
                set(t, MUIA_Text_Contents, buf);
            }
            if (argv[2][1] == 'h') {
                set(a, MUIA_Text_Contents, "Status 42");
                set(b, MUIA_Text_Contents, "Status 42");
                set(c, MUIA_Text_Contents, "\33cStatus 42");
                Delay(600);
            }
        } else
        for (open = 0; open < (ULONG)atol(argv[1]); open++)
            MUI_Redraw(b, MADF_DRAWOBJECT);
        MUI_DisposeObject(app); CloseLibrary(MUIMasterBase);
        CloseDevice((struct IORequest *)&tr);
        return 0;
    }
    printf("closed-set %lu\n", run(b));
    set(win, MUIA_Window_Open, TRUE); get(win, MUIA_Window_Open, &open);
    if (open) {
        printf("redraw-own %lu\n", redraw(b));
        printf("redraw-inherited %lu\n", redraw(a));
        printf("inherited-frame %lu\n", run(a));
        printf("own-background %lu\n", run(b));
        printf("inherited-noframe %lu\n", run(c));
    }
    MUI_DisposeObject(app); CloseLibrary(MUIMasterBase); CloseDevice((struct IORequest *)&tr);
    return 0;
}

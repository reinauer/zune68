/*
 * vgscroll: time 8-pixel scroll steps of a Scrollgroup whose Virtgroup holds
 * a ColGroup of 80 buttons, the case where a group with more than 32
 * children painted its gaps through an O(n^2) clip region.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -Ibuild/muimaster/include \
 *        tests/native/vgscroll.c -o vgscroll -lamiga
 * Run:   vgscroll [steps [seconds]]   prints the milliseconds per step and
 *        keeps the window open for the given seconds
 */
#include <libraries/mui.h>
#include <devices/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *MUIMasterBase;
struct Device *TimerBase;

static ULONG now_us(void)
{
    struct timeval tv;
    GetSysTime(&tv);
    return tv.tv_secs * 1000000 + tv.tv_micro;
}

int main(int argc, char **argv)
{
    static char labels[80][4];
    struct timerequest tr;
    Object *app, *win, *vg, *cols, *b;
    LONG steps = argc > 1 ? atol(argv[1]) : 10, i, top = 0, height = 0;
    ULONG t0, t1;

    if (OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    cols = ColGroup(4), End;
    for (i = 0; i < 80; i++) {
        sprintf(labels[i], "%ld", (long)i);
        b = SimpleButton(labels[i]);
        DoMethod(cols, OM_ADDMEMBER, (IPTR)b);
    }
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"VGScroll",
        SubWindow, (IPTR)(win = WindowObject,
            MUIA_Window_Title, (IPTR)"VGScroll",
            MUIA_Window_Height, MUIV_Window_Height_Visible(30),
            WindowContents, VGroup,
                Child, ScrollgroupObject,
                    MUIA_Scrollgroup_Contents, (IPTR)(vg = VirtgroupObject,
                        Child, (IPTR)cols,
                    End),
                End,
            End,
        End),
    End;
    if (!app) { printf("no app\n"); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    get(vg, MUIA_Virtgroup_Height, &height);
    printf("virtual height %ld, visible %ld\n", (long)height, (long)_mheight(vg));
    t0 = now_us();
    for (i = 0; i < steps; i++) {
        top += 8;
        set(vg, MUIA_Virtgroup_Top, top);
    }
    t1 = now_us();
    printf("%ld steps, %ld ms per step\n", (long)steps,
        (long)((t1 - t0) / 1000 / (steps ? steps : 1)));
    if (argc > 2)
        Delay(50 * atol(argv[2]));
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    CloseDevice((struct IORequest *)&tr);
    return 0;
}

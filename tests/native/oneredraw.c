/* EClock time of MUI_Redraw(obj, MADF_DRAWOBJECT) on single objects: a
 * framed Text, a plain Text, a button and a string, 200 redraws each,
 * median of 9. Writes oneredraw.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit oneredraw.c -o oneredraw -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <devices/timer.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/timer.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
struct Device *TimerBase;
ULONG __stack = 65536;

static int cmp(const void *a, const void *b)
{
    ULONG x = *(const ULONG *)a, y = *(const ULONG *)b;
    return x < y ? -1 : x > y;
}

static ULONG time_redraw(Object *o)
{
    ULONG s[9];
    int r, i;

    for (r = 0; r < 9; r++)
    {
        struct EClockVal t0, t1;

        ReadEClock(&t0);
        for (i = 0; i < 200; i++)
            MUI_Redraw(o, MADF_DRAWOBJECT);
        ReadEClock(&t1);
        s[r] = t1.ev_lo - t0.ev_lo;
    }
    qsort(s, 9, sizeof(s[0]), cmp);
    return s[4];
}

int main(void)
{
    struct timerequest tr;
    Object *app, *win, *t1, *t2, *b, *str;
    ULONG open = FALSE;

    freopen("oneredraw.txt", "w", stdout);
    if (OpenDevice("timer.device", UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, VGroup,
            Child, t1 = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                MUIA_Text_Contents, "Framed text 123", End,
            Child, t2 = TextObject, MUIA_Text_Contents, "Plain text 123", End,
            Child, b = SimpleButton("Button"),
            Child, str = StringObject, StringFrame, MUIA_String_Contents, "String", End,
            End, End, End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
    {
        printf("framed text %lu\n", time_redraw(t1));
        printf("plain text  %lu\n", time_redraw(t2));
        printf("button      %lu\n", time_redraw(b));
        printf("string      %lu\n", time_redraw(str));
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    CloseDevice((struct IORequest *)&tr);
    return 0;
}

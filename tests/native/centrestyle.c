/* Where centred Text with bold or italic parts starts drawing, in a box
 * 200 pixels wide: first and last ink column per sample. Writes
 * centrestyle.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit centrestyle.c -o centrestyle -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define N 12
static const char *samples[N] = {
    "\33cRegister ONLINE!", "\33cRegister \33bONLINE!", "\33c\33bRegister ONLINE!",
    "\33c\33bRegister\33n ONLINE!", "\33cRegister \33iONLINE!", "\33cAB",
    "\33c\33bAB", "\33cA\33bB", "\33c\33iRegister ONLINE!", "\33cA\33iB",
    "\33c\33iAB", "\33cRegister \33iONLINE!\33n x",
};

int main(void)
{
    Object *app, *win, *grp, *t[N];
    ULONG open = FALSE;
    int i;

    freopen("centrestyle.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, grp = VGroup, End, End, End;
    if (!app)
        return 20;
    for (i = 0; i < N; i++)
        DoMethod(grp, OM_ADDMEMBER, t[i] = TextObject, MUIA_FixWidth, 200,
            MUIA_Text_Contents, samples[i], End);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    for (i = 0; open && i < N; i++)
    {
        LONG x, y, first = -1, last = -1;

        for (x = _mleft(t[i]); x <= _mright(t[i]); x++)
            for (y = _mtop(t[i]); y <= _mbottom(t[i]); y++)
                if (ReadPixel(_rp(t[i]), x, y) == 1)
                {
                    if (first < 0)
                        first = x - _mleft(t[i]);
                    last = x - _mleft(t[i]);
                }
        printf("%d first %ld last %ld min %d\n", i, (long)first, (long)last,
            _minwidth(t[i]));
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

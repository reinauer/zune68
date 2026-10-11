/* Weight layout of an HGroup like YAM's main panes: a weight-30 child,
 * a quiet Balance (or a plain one, or a 2 pixel Rectangle) and a
 * weight-100 child, at window widths 300..700. Writes balweight.txt. Compare native
 * and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit balweight.c -o balweight -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#ifndef MUIA_Balance_Quiet
#define MUIA_Balance_Quiet 0x80427486
#endif

struct Library *MUIMasterBase;
ULONG __stack = 65536;

int main(void)
{
    static const int widths[] = { 700, 600, 500, 400, 300 };
    int i, k;

    freopen("balweight.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (k = 0; k < 3; k++)
        for (i = 0; i < (int)(sizeof(widths) / sizeof(widths[0])); i++)
        {
            Object *app, *win, *g, *a, *b, *c;
            ULONG open = FALSE;

            app = ApplicationObject, SubWindow, win = WindowObject,
                MUIA_Window_Width, widths[i],
                WindowContents, g = HGroup, GroupSpacing(0),
                    Child, a = RectangleObject, MUIA_HorizWeight, 30,
                        End,
                    Child, b = k == 2 ? RectangleObject, MUIA_FixWidth, 2, End
                        : BalanceObject, MUIA_Balance_Quiet, k == 0, End,
                    Child, c = RectangleObject, End,
                    End,
                End, End;
            if (!app)
                return 20;
            set(win, MUIA_Window_Open, TRUE);
            get(win, MUIA_Window_Open, &open);
            if (open)
                printf("case %d width %d: a %d b %d c %d\n", k, _mwidth(g),
                    _width(a), _width(b), _width(c));
            MUI_DisposeObject(app);
        }
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

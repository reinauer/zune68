/* Sizes and pixels of Image objects for built-in image specs and for
 * unusable ones ("noimage", as NList creates), or for the specs given as
 * arguments (up to 8). Writes imgspec.txt.
 * Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit imgspec.c -o imgspec -lamiga
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
#define N 8
int main(int argc, char **argv)
{
    static const char *spec[N] = { "6:39", "6:30", "6:38", "6:22", "6:20", "6:12", "noimage", "x:yz" };
    Object *app, *win, *grp, *o[N];
    ULONG open = FALSE; int i, n = N; LONG x, y;
    if (argc > 1)
        for (n = 0; n < N && n + 1 < argc; n++)
            spec[n] = argv[n + 1];
    freopen("imgspec.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, grp = HGroup, End, End, End;
    if (!app) return 20;
    for (i = 0; i < n; i++)
        DoMethod(grp, OM_ADDMEMBER, o[i] = ImageObject, MUIA_Image_Spec, spec[i], MUIA_Image_FreeHoriz, FALSE, End);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    for (i = 0; open && i < n; i++)
    {
        printf("%s box %dx%d min %d,%d def %d,%d max %d,%d\n", spec[i], _width(o[i]), _height(o[i]),
            _minwidth(o[i]), _minheight(o[i]), _defwidth(o[i]), _defheight(o[i]), _maxwidth(o[i]), _maxheight(o[i]));
        for (y = _top(o[i]); y <= _bottom(o[i]); y++)
        {
            printf("  ");
            for (x = _left(o[i]); x <= _right(o[i]); x++)
                printf("%c", "0123456789abcdef"[ReadPixel(_rp(o[i]), x, y) & 15]);
            printf("\n");
        }
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

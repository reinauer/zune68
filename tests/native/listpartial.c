/* Pixel rows of a List with an active entry and a partly visible last
 * row. Writes listpartial.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit listpartial.c -o listpartial -lamiga
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
int main(void)
{
    static const char *e[] = { "abc", "Wxy", "jgq", "dEf", "Hij", "klm", "nop", NULL };
    Object *app, *win, *l, *lv;
    ULONG open = FALSE; LONG x, y;
    freopen("listpartial.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, lv = ListviewObject, MUIA_FixHeight, 54, MUIA_FixWidth, 60,
            MUIA_Listview_List, l = ListObject, InputListFrame, End, End, End, End;
    if (!app) return 20;
    DoMethod(l, MUIM_List_Insert, e, -1, MUIV_List_Insert_Bottom);
    set(l, MUIA_List_Active, 1);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    for (y = _top(l); open && y <= _bottom(l); y++)
    {
        printf("%2ld ", (long)(y - _top(l)));
        for (x = _left(l); x < _left(l) + 40; x++)
            printf("%c", "0123456789abcdef"[ReadPixel(_rp(l), x, y) & 15]);
        printf("\n");
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

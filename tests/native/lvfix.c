/* Sizes of Listviews and Lists with MUIA_FixWidth/MUIA_FixHeight, their
 * group and window (fixed sizes keep larger defaults). Writes lvfix.txt.
 * Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit lvfix.c -o lvfix -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static void box(const char *n, Object *o)
{
    printf("%-8s box %d,%d %dx%d inner %d,%d %dx%d min %d,%d def %d,%d max %d,%d sub %d,%d\n", n, _left(o), _top(o), _width(o), _height(o),
        _mleft(o), _mtop(o), _mwidth(o), _mheight(o), _minwidth(o), _minheight(o), _defwidth(o), _defheight(o), _maxwidth(o), _maxheight(o), _subwidth(o), _subheight(o));
}
int main(void)
{
    Object *app, *win, *lv, *l, *lv2, *l2, *grp;
    struct Window *w; ULONG open = FALSE;
    freopen("lvfix.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject, MUIA_Window_Title, "lvfix",
        WindowContents, grp = VGroup,
            Child, lv = ListviewObject, MUIA_FixWidth, 60, MUIA_FixHeight, 40,
                MUIA_Listview_List, l = ListObject, InputListFrame, End, End,
            Child, lv2 = ListviewObject, MUIA_Listview_List, l2 = ListObject, InputListFrame, MUIA_FixWidth, 60, End, End,
        End, End, End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
    {
        get(win, MUIA_Window_Window, &w);
        printf("window %dx%d\n", w->Width, w->Height);
        box("group", grp); box("lv", lv); box("list", l); box("lv2", lv2); box("list2", l2);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

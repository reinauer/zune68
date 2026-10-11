/* NListview/NList frames as drawn (corners of the input and read lists
 * and a List for comparison), inner offsets and sizes, and the pixels
 * around an NList made the active object. Writes nlframe.txt. Needs
 * NList.mcc. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit nlframe.c -o nlframe -lamiga
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
#define MUIC_NList "NList.mcc"
#define MUIC_NListview "NListview.mcc"
#define MUIA_NListview_NList 0x9d510020
#define MUIA_NList_Input 0x9d510049
static void dump(const char *n, Object *o)
{
    LONG x, y;
    ULONG fr = 0;
    get(o, MUIA_Frame, &fr);
    printf("%s box %d,%d %dx%d frame %lu inner %d %d %d %d min %d,%d def %d,%d\n", n, _left(o), _top(o), _width(o), _height(o), (unsigned long)(fr > 0x1000 ? 999 : fr),
        _mleft(o) - _left(o), _mtop(o) - _top(o), _right(o) - _mright(o), _bottom(o) - _mbottom(o), _minwidth(o), _minheight(o), _defwidth(o), _defheight(o));
    for (y = _top(o); y < _top(o) + 6; y++)
    {
        printf("  ");
        for (x = _left(o); x < _left(o) + 12; x++) printf("%ld", (long)ReadPixel(_rp(o), x, y) & 15);
        printf(" ... ");
        for (x = _right(o) - 5; x <= _right(o); x++) printf("%ld", (long)ReadPixel(_rp(o), x, y) & 15);
        printf("\n");
    }
}
int main(void)
{
    Object *app, *win, *l1, *l2, *lv1, *lv2, *l3;
    ULONG open = FALSE;
    freopen("nlframe.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, HGroup,
            Child, lv1 = MUI_NewObject(MUIC_NListview, MUIA_NListview_NList,
                l1 = MUI_NewObject(MUIC_NList, MUIA_NList_Input, TRUE, TAG_DONE), TAG_DONE),
            Child, l3 = ListObject, InputListFrame, End,
            Child, lv2 = MUI_NewObject(MUIC_NListview, MUIA_NListview_NList,
                l2 = MUI_NewObject(MUIC_NList, MUIA_NList_Input, FALSE, TAG_DONE), TAG_DONE),
        End, End, End;
    if (!app) { printf("no app\n"); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    if (open) { dump("input lv", lv1); dump("input list", l1); dump("read lv", lv2); dump("read list", l2); dump("List", l3); }
    if (open)
    {
        LONG x, y;
        set(win, MUIA_Window_ActiveObject, l1);
        DoMethod(app, MUIM_Application_InputBuffered);
        { Object *ao = NULL; get(win, MUIA_Window_ActiveObject, &ao); printf("active %s\n", ao == l1 ? "list" : ao == lv1 ? "listview" : ao ? "other" : "none"); }
        for (y = _top(lv1) - 2; y < _top(lv1) + 3; y++)
        {
            printf("  ");
            for (x = _left(lv1) - 2; x < _left(lv1) + 10; x++) printf("%ld", (long)ReadPixel(_rp(lv1), x, y) & 15);
            printf("\n");
        }
        set(win, MUIA_Window_ActiveObject, lv1);
        DoMethod(app, MUIM_Application_InputBuffered);
        { Object *ao = NULL; get(win, MUIA_Window_ActiveObject, &ao); printf("active2 %s\n", ao == l1 ? "list" : ao == lv1 ? "listview" : ao ? "other" : "none"); }
        for (y = _top(lv1) - 2; y < _top(lv1) + 3; y++)
        {
            printf("  ");
            for (x = _left(lv1) - 2; x < _left(lv1) + 10; x++) printf("%ld", (long)ReadPixel(_rp(lv1), x, y) & 15);
            printf("\n");
        }
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

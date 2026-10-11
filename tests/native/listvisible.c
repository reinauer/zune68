/* MUIA_List_Visible of lists 40..58 pixels high, with their boxes; the
 * window stays open 25 seconds for reading back the rows. Writes
 * listvisible.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit listvisible.c -o listvisible -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define N 10
int main(void)
{
    static const char *e[] = { "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z","A","B","C","D", NULL };
    Object *app, *win, *grp, *l[N], *lv[N];
    ULONG open = FALSE; int i;
    freopen("listvisible.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, grp = HGroup, End, End, End;
    if (!app) return 20;
    for (i = 0; i < N; i++)
    {
        lv[i] = ListviewObject, MUIA_FixHeight, 40 + i * 2, MUIA_Listview_List,
            l[i] = ListObject, InputListFrame, End, End;
        DoMethod(grp, OM_ADDMEMBER, lv[i]);
        DoMethod(l[i], MUIM_List_Insert, e, -1, MUIV_List_Insert_Bottom);
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    for (i = 0; open && i < N; i++)
    {
        LONG vis = 0, first = 0;
        get(l[i], MUIA_List_Visible, &vis);
        get(l[i], MUIA_List_First, &first);
        struct Window *w; get(win, MUIA_Window_Window, &w);
        if (!i) printf("window %d,%d\n", w->LeftEdge, w->TopEdge);
        printf("lv h=%d list box %d,%d %dx%d inner %d,%d %dx%d visible=%ld\n", _height(lv[i]),
            _left(l[i]), _top(l[i]), _width(l[i]), _height(l[i]),
            _mleft(l[i]), _mtop(l[i]), _mwidth(l[i]), _mheight(l[i]), (long)vis);
    }
    fflush(stdout);
    Delay(50 * 25);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

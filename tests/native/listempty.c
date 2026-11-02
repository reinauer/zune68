/* MUIA_List_Visible of empty lists 40..58 pixels high, then with three
 * entries, then after MUIM_List_Clear. Compare native and MUI 3.8 output,
 * for example with run_probe.py.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit listempty.c -o listempty -lamiga
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

static void show(const char *what, Object **l)
{
    int i;

    printf("%-7s", what);
    for (i = 0; i < N; i++)
    {
        LONG vis = 0;
        get(l[i], MUIA_List_Visible, &vis);
        printf(" %ld", (long)vis);
    }
    printf("\n");
}

int main(void)
{
    static const char *e[] = { "a", "b", "c", NULL };
    Object *app, *win, *grp, *l[N];
    ULONG open = FALSE;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, grp = HGroup, End, End, End;
    if (!app) return 20;
    for (i = 0; i < N; i++)
        DoMethod(grp, OM_ADDMEMBER, ListviewObject,
            MUIA_FixHeight, 40 + i * 2, MUIA_Listview_List,
            l[i] = ListObject, InputListFrame, End, End);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
    {
        DoMethod(app, MUIM_Application_InputBuffered);
        show("empty", l);
        for (i = 0; i < N; i++)
            DoMethod(l[i], MUIM_List_Insert, e, -1, MUIV_List_Insert_Bottom);
        show("three", l);
        for (i = 0; i < N; i++)
            DoMethod(l[i], MUIM_List_Clear);
        show("clear", l);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf(open ? "PASS listempty\n" : "FAIL window\n");
    return 0;
}

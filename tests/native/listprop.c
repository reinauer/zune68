/* List visibility and scrolling: MUIA_List_Visible and MUIA_List_First
 * after setting First, jumping to the bottom and paging, in lists 40 and
 * 45 pixels high. Writes listprop.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit listprop.c -o listprop -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
#ifndef MUIA_List_VertProp_Visible
#define MUIA_List_VertProp_Entries 0x8042a8f6
#define MUIA_List_VertProp_Visible 0x804268c5
#define MUIA_List_VertProp_First   0x8042d4b2
#endif
static Object *l;
static void show(const char *what)
{
    LONG v[6] = { 0 };
    get(l, MUIA_List_Visible, &v[0]); get(l, MUIA_List_First, &v[1]);
    get(l, MUIA_List_Entries, &v[2]); get(l, MUIA_List_VertProp_Visible, &v[3]);
    get(l, MUIA_List_VertProp_Entries, &v[4]); get(l, MUIA_List_VertProp_First, &v[5]);
    printf("%-12s visible %ld first %ld entries %ld prop vis %ld ent %ld first %ld\n", what,
        (long)v[0], (long)v[1], (long)v[2], (long)v[3], (long)v[4], (long)v[5]);
}
int main(void)
{
    static const char *e[] = { "0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19", NULL };
    Object *app, *win;
    ULONG open = FALSE; int h;
    freopen("listprop.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    for (h = 40; h <= 45; h += 5)
    {
        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, ListviewObject, MUIA_FixHeight, h, MUIA_FixWidth, 60,
                MUIA_Listview_List, l = ListObject, InputListFrame, End, End, End, End;
        if (!app) return 20;
        DoMethod(l, MUIM_List_Insert, e, -1, MUIV_List_Insert_Bottom);
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        DoMethod(app, MUIM_Application_InputBuffered);
        printf("height %d inner %d\n", h, _mheight(l));
        show("open");
        set(l, MUIA_List_First, 10); DoMethod(app, MUIM_Application_InputBuffered); show("first 10");
        set(l, MUIA_List_First, 100); DoMethod(app, MUIM_Application_InputBuffered); show("first 100");
        set(l, MUIA_List_First, 17); DoMethod(app, MUIM_Application_InputBuffered); show("first 17");
        set(l, MUIA_List_First, 16); DoMethod(app, MUIM_Application_InputBuffered); show("first 16");
        set(l, MUIA_List_First, 15); DoMethod(app, MUIM_Application_InputBuffered); show("first 15");
        set(l, MUIA_List_First, -1); DoMethod(app, MUIM_Application_InputBuffered); show("first -1");
        set(l, MUIA_List_First, 0);
        set(l, MUIA_List_Active, MUIV_List_Active_Bottom); DoMethod(app, MUIM_Application_InputBuffered); show("active bot");
        set(l, MUIA_List_Active, MUIV_List_Active_Top); DoMethod(app, MUIM_Application_InputBuffered); show("active top");
        set(l, MUIA_List_Active, MUIV_List_Active_PageDown); DoMethod(app, MUIM_Application_InputBuffered); show("pagedown");
        { LONG a = 0; get(l, MUIA_List_Active, &a); printf("  active %ld\n", (long)a); }
        set(l, MUIA_List_Active, MUIV_List_Active_PageDown); DoMethod(app, MUIM_Application_InputBuffered); show("pagedown2");
        { LONG a = 0; get(l, MUIA_List_Active, &a); printf("  active %ld\n", (long)a); }
        set(l, MUIA_List_Active, MUIV_List_Active_PageUp); DoMethod(app, MUIM_Application_InputBuffered); show("pageup");
        { LONG a = 0; get(l, MUIA_List_Active, &a); printf("  active %ld\n", (long)a); }
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

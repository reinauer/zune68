/* MUI's private mad_VirtualTop (area data offset 72, Voyager's _vtop):
 * print _top and that field for a virtual group and a child before and
 * after scrolling, on native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit virtualtop.c -o virtualtop -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define VTOP(o) (*(LONG *)((UBYTE *)muiAreaData(o) + 72))
static void show(const char *what, Object *vg, Object *body, Object *row)
{
    printf("%-10s vg top %ld vtop %ld | body top %ld vtop %ld | row top %ld vtop %ld\n",
        what, (LONG)_top(vg), (LONG)VTOP(vg), (LONG)_top(body), (LONG)VTOP(body),
        (LONG)_top(row), (LONG)VTOP(row));
}
int main(void)
{
    Object *app, *win, *vg, *body, *row = NULL, *o;
    ULONG open = 0;
    int i;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    body = VGroup, End;
    for (i = 0; i < 40; i++)
    {
        o = TextObject, MUIA_Text_Contents, "Scroll row text", End;
        DoMethod(body, OM_ADDMEMBER, o);
        if (i == 10) row = o;
    }
    app = ApplicationObject, MUIA_Application_Title, "virtualtop",
        SubWindow, win = WindowObject, MUIA_Window_Title, "virtualtop",
            MUIA_Window_Height, 120,
            WindowContents, VGroup,
                Child, vg = VirtgroupObject, VirtualFrame, Child, body, End,
            End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    show("start", vg, body, row);
    set(vg, MUIA_Virtgroup_Top, 30);
    show("top 30", vg, body, row);
    set(vg, MUIA_Virtgroup_Top, 77);
    show("top 77", vg, body, row);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

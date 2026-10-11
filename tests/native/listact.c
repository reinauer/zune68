/* The pixels of a list after activating entries near and far from the
 * view (the perfbench "active" sequence and single steps), to check the
 * cursor and the rows after each jump. Writes listact.txt. Compare
 * native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit listact.c -o listact -lamiga
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

static void dump(Object *lv, int step)
{
    LONG x, y;
    ULONG first = 0;

    get(lv, MUIA_List_First, &first);
    printf("step %d first %ld\n", step, (long)first);
    for (y = _top(lv); y <= _bottom(lv); y++)
    {
        for (x = _left(lv); x <= _right(lv); x++)
            printf("%c", "0123456789abcdef"[ReadPixel(_rp(lv), x, y) & 15]);
        printf("\n");
    }
}

int main(void)
{
    static char names[200][12];
    static const char *ptrs[201];
    static const LONG seq[] = { 7, 14, 8, 9, 3, 150, 149, 0, 199, 100 };
    Object *app, *win, *lv, *list;
    ULONG open = FALSE;
    int i;

    freopen("listact.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (i = 0; i < 200; i++)
    {
        sprintf(names[i], "Entry %ld", (long)i);
        ptrs[i] = names[i];
    }
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Width, 120,
        WindowContents, lv = ListviewObject, MUIA_Listview_List,
            list = ListObject, InputListFrame, End, End,
        End, End;
    if (!app)
        return 20;
    DoMethod(list, MUIM_List_Insert, ptrs, 200, MUIV_List_Insert_Bottom);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    for (i = 0; open && i < (int)(sizeof(seq) / sizeof(seq[0])); i++)
    {
        set(list, MUIA_List_Active, seq[i]);
        DoMethod(app, MUIM_Application_InputBuffered);
        dump(list, i);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

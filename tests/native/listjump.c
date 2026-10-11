/* Does setting MUIA_List_Active scroll the entry into view? A 200 entry
 * list, active set to entries outside the view, with and without
 * MUIA_List_AutoVisible. Writes listjump.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit listjump.c -o listjump -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

int main(void)
{
    static char names[200][8];
    static const char *ptrs[201];
    static const LONG targets[] = { 150, 3, 199, 100, 101, 0 };
    int i, av;

    freopen("listjump.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (i = 0; i < 200; i++)
    {
        sprintf(names[i], "E%ld", (long)i);
        ptrs[i] = names[i];
    }
    for (av = 0; av < 2; av++)
    {
        Object *app, *win, *list;
        ULONG open = FALSE;

        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, ListviewObject, MUIA_Listview_List,
                list = ListObject, InputListFrame,
                    MUIA_List_AutoVisible, av, End, End,
            End, End;
        if (!app)
            return 20;
        DoMethod(list, MUIM_List_Insert, ptrs, 200, MUIV_List_Insert_Bottom);
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        for (i = 0; open && i < (int)(sizeof(targets) / sizeof(targets[0])); i++)
        {
            ULONG first = 0, vis = 0, act = 0;

            set(list, MUIA_List_Active, targets[i]);
            get(list, MUIA_List_First, &first);
            get(list, MUIA_List_Visible, &vis);
            get(list, MUIA_List_Active, &act);
            printf("autovisible %d active %ld -> first %ld visible %ld\n",
                av, (long)act, (long)first, (long)vis);
        }
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}

/* Floattext wrapping in Listviews 60..120 pixels wide: the number of
 * lines and the widest line, with the lines of the narrowest, for deriving
 * MUI 3.9's wrap width and line breaking. Writes ftwidth.txt. Compare
 * native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit ftwidth.c -o ftwidth -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
/* each word is one 'x' longer than the previous, so the widest line shows the wrap width */
static const char text[] = "a bb ccc dddd eeeee ffffff ggggggg hhhhhhhh iiiiiiiii jjjjjjjjjj kkkkkkkkkkk llllllllllll mmmmmmmmmmmmm nnnnnnnnnnnnnn ooooooooooooooo pppppppppppppppp qqqqqqqqqqqqqqqqq rrrrrrrrrrrrrrrrrr";
#define N 7
int main(void)
{
    static const int widths[N] = { 60, 70, 80, 91, 100, 110, 120 };
    Object *app, *win, *grp, *ft[N];
    ULONG open = FALSE; LONG n, i, j;
    struct RastPort rp;
    freopen("ftwidth.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, grp = HGroup, End, End, End;
    if (!app) return 20;
    for (i = 0; i < N; i++)
        DoMethod(grp, OM_ADDMEMBER, ListviewObject, MUIA_Listview_Input, FALSE, MUIA_FixWidth, widths[i],
            MUIA_FixHeight, 300, MUIA_Listview_List, ft[i] = FloattextObject,
                MUIA_Frame, MUIV_Frame_ReadList, MUIA_Floattext_Text, text, End, End);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    InitRastPort(&rp);
    for (i = 0; open && i < N; i++)
    {
        LONG maxw = 0;
        SetFont(&rp, _font(ft[i]));
        get(ft[i], MUIA_List_Entries, &n);
        for (j = 0; j < n; j++)
        {
            char *e = NULL; LONG w;
            DoMethod(ft[i], MUIM_List_GetEntry, j, &e);
            w = e ? TextLength(&rp, e, strlen(e)) : 0;
            if (w > maxw) maxw = w;
            if (i == 0) printf("  [%s]\n", e ? e : "(null)");
        }
        printf("width %3d box %3d inner %3d entries %2ld widest line %3ld px\n", widths[i], _width(ft[i]), _mwidth(ft[i]), (long)n, (long)maxw);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

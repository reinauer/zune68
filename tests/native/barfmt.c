/* A titled four-column list with MUIA_List_Format "BAR,BAR,BAR," like
 * xSysInfo's: sizes and the pixels of the title and the first rows.
 * Writes barfmt.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit barfmt.c -o barfmt -lamiga
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

static const char *titles[] = { "Name", "Where", "Addr", "Ver" };
static const char *rows[][4] = {
    { "kickstart", "(512K)", "$00F80000", "V47.115" },
    { "exec", "32BitRAM", "$400008BC", "V47.13" },
};

static LONG Display(struct Hook *h __asm("a0"), char **array __asm("a2"),
    APTR entry __asm("a1"))
{
    int i;

    for (i = 0; i < 4; i++)
        array[i] = entry ? (char *)((const char **)entry)[i] : (char *)titles[i];
    return 0;
}

int main(void)
{
    static struct Hook dh;
    Object *app, *win, *lv, *list;
    ULONG open = FALSE;
    LONG x, y;

    freopen("barfmt.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    dh.h_Entry = (HOOKFUNC)Display;
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Width, 300,
        WindowContents, VGroup,
            Child, lv = ListviewObject, MUIA_Listview_List, list = ListObject,
                InputListFrame, MUIA_List_Format, "BAR,BAR,BAR,",
                MUIA_List_Title, TRUE, MUIA_List_DisplayHook, &dh, End, End,
            End, End, End;
    if (!app)
        return 20;
    DoMethod(list, MUIM_List_InsertSingle, rows[0], MUIV_List_Insert_Bottom);
    DoMethod(list, MUIM_List_InsertSingle, rows[1], MUIV_List_Insert_Bottom);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    if (open)
    {
        printf("listview box %dx%d min %d,%d def %d,%d\n", _width(lv), _height(lv),
            _minwidth(lv), _minheight(lv), _defwidth(lv), _defheight(lv));
        printf("list box %d,%d %dx%d min %d,%d def %d,%d\n", _left(list) - _left(lv),
            _top(list) - _top(lv), _width(list), _height(list),
            _minwidth(list), _minheight(list), _defwidth(list), _defheight(list));
        for (y = _top(lv); y < _top(lv) + 40; y++)
        {
            for (x = _left(lv); x < _left(lv) + 200; x++)
                printf("%c", "0123456789abcdef"[ReadPixel(_rp(lv), x, y) & 15]);
            printf("\n");
        }
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

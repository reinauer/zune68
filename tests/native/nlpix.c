/* Pixels of a titled two-column NList with a few entries, to compare
 * NList's drawing offsets on native and MUI 3.9 (YAM's folder list is
 * drawn one pixel further left natively). Writes nlpix.txt.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit nlpix.c -o nlpix -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define MUIA_NListview_NList      0x9d510020
#define MUIA_NList_Title          0x9d510062
#define MUIA_NList_Format         0x9d510058
#define MUIM_NList_InsertSingle   0x9d510076
#define MUIA_NList_ConstructHook  0x9d510003
#define MUIA_NList_DisplayHook    0x9d510053
#define MUIV_NList_Insert_Bottom  (-3)

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static LONG Display(struct Hook *h __asm("a0"), char **array __asm("a2"),
    char **e __asm("a1"))
{
    static char *title[] = { "Folder", "Total" };

    array[0] = e ? e[0] : title[0];
    array[1] = e ? e[1] : title[1];
    return 0;
}

int main(void)
{
    static struct Hook dh;
    static char *rows[][2] = { { "Incoming", "0" }, { "Drafts", "0" }, { "Sent", "" } };
    Object *app, *win, *lv, *l;
    ULONG open = FALSE;
    LONG x, y;
    int i;

    freopen("nlpix.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    dh.h_Entry = (HOOKFUNC)Display;
    l = MUI_NewObject("NList.mcc", MUIA_Frame, MUIV_Frame_InputList,
        MUIA_NList_Title, TRUE, MUIA_NList_Format, (ULONG)"BAR,",
        MUIA_NList_DisplayHook, (ULONG)&dh, TAG_DONE);
    if (!l)
        return 20;
    lv = MUI_NewObject("NListview.mcc", MUIA_NListview_NList, (ULONG)l, TAG_DONE);
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Width, 160, MUIA_Window_Height, 120,
        WindowContents, VGroup, Child, lv, End, End, End;
    if (!app)
        return 20;
    for (i = 0; i < 3; i++)
        DoMethod(l, MUIM_NList_InsertSingle, (ULONG)rows[i], MUIV_NList_Insert_Bottom);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    Delay(25);
    DoMethod(app, MUIM_Application_InputBuffered);
    if (open)
    {
        printf("nlist box %d,%d %dx%d\n", _left(l), _top(l), _width(l), _height(l));
        for (y = _top(l); y < _top(l) + 50 && y <= _bottom(l); y++)
        {
            for (x = _left(l); x <= _right(l); x++)
                printf("%c", "0123456789abcdef"[ReadPixel(_rp(l), x, y) & 15]);
            printf("\n");
        }
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

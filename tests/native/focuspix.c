/* Pixels around a listview made the window's active object (keyboard
 * focus marking), and around a button made active. Writes focuspix.txt.
 * Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit focuspix.c -o focuspix -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void dump(Object *o, const char *n)
{
    LONG x, y;

    printf("%s box %d,%d %dx%d\n", n, _left(o), _top(o), _width(o), _height(o));
    for (y = _top(o) - 3; y <= _top(o) + 2; y++)
    {
        for (x = _left(o) - 3; x <= _left(o) + 12; x++)
            printf("%c", "0123456789abcdef"[ReadPixel(_rp(o), x, y) & 15]);
        printf("\n");
    }
}

int main(void)
{
    Object *app, *win, *lv, *b;
    ULONG open = FALSE;

    freopen("focuspix.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, VGroup, GroupSpacing(6),
            Child, lv = ListviewObject, MUIA_CycleChain, 1,
                MUIA_Listview_List, ListObject, InputListFrame, End, End,
            Child, b = SimpleButton("Button"),
            End, End, End;
    if (!app)
        return 20;
    set(b, MUIA_CycleChain, 1);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
    {
        set(win, MUIA_Window_ActiveObject, lv);
        DoMethod(app, MUIM_Application_InputBuffered);
        Delay(10);
        dump(lv, "listview active");
        set(win, MUIA_Window_ActiveObject, b);
        DoMethod(app, MUIM_Application_InputBuffered);
        Delay(10);
        dump(lv, "listview inactive");
        dump(b, "button active");
        {
            struct Window *w2 = OpenWindowTags(NULL, WA_Left, 300, WA_Top, 50,
                WA_Width, 100, WA_Height, 50, WA_Title, (ULONG)"other",
                WA_Activate, TRUE, WA_DragBar, TRUE, TAG_DONE);
            Delay(25);
            DoMethod(app, MUIM_Application_InputBuffered);
            Delay(10);
            dump(b, "button, window inactive");
            if (w2) CloseWindow(w2);
            Delay(25);
            DoMethod(app, MUIM_Application_InputBuffered);
            Delay(10);
            dump(b, "button, window active again");
        }
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

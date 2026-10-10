/* Scrollgroup with window-border scrollers (MUIA_Scrollgroup_UseWinBorder)
 * around a virtual group that is filled after the window opened, as
 * Voyager lays out a page. "borderscroll hold" keeps the window open.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit borderscroll.c -o borderscroll -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
int main(int argc, char **argv)
{
    Object *app, *win, *vg, *body;
    ULONG open = 0, vh = 0;
    int i;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "borderscroll",
        SubWindow, win = WindowObject, MUIA_Window_Title, "borderscroll",
            MUIA_Window_Width, 200, MUIA_Window_Height, 120,
            MUIA_Window_UseRightBorderScroller, TRUE,
            MUIA_Window_UseBottomBorderScroller, TRUE,
            WindowContents, VGroup,
                Child, ScrollgroupObject, MUIA_Scrollgroup_UseWinBorder, TRUE,
                    MUIA_Scrollgroup_Contents, vg = VirtgroupObject, VirtualFrame, MUIA_FixHeight, 100,
                        Child, body = VGroup, Child, TextObject, MUIA_Text_Contents, "first", End, End,
                    End,
                End,
            End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    /* Fill the page after opening, as a browser does. */
    DoMethod(vg, MUIM_Group_InitChange);
    DoMethod(body, MUIM_Group_InitChange);
    for (i = 0; i < 25; i++)
        DoMethod(body, OM_ADDMEMBER, TextObject, MUIA_Text_Contents, "Scroll row text", End);
    DoMethod(body, MUIM_Group_ExitChange);
    DoMethod(vg, MUIM_Group_ExitChange);
    for (i = 0; i < 10; i++) { DoMethod(app, MUIM_Application_InputBuffered); Delay(3); }
    get(vg, MUIA_Virtgroup_Height, &vh);
    printf("virtual height %ld visible %ld\n", (LONG)vh, (LONG)_mheight(vg));
    fflush(stdout);
    if (argc > 1 && !strcmp(argv[1], "hold")) Delay(300);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

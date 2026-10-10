/* MUI's private area data after mad_Flags (offsets 40-79), as macros in
 * applications read it (Voyager: _vtop, _fontspec, _shorthelp), dumped
 * for a text object with known settings on native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit areaprivate.c -o areaprivate -lamiga
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
    Object *app, *win, *t;
    ULONG open = 0;
    UBYTE *ad;
    int i;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "areaprivate",
        SubWindow, win = WindowObject, MUIA_Window_Title, "areaprivate",
            WindowContents, VGroup,
                Child, t = TextObject, MUIA_Text_Contents, "hello",
                    MUIA_Font, MUIV_Font_Fixed, MUIA_ShortHelp, "help text",
                    MUIA_FixWidth, 50, MUIA_HorizWeight, 33, MUIA_VertWeight, 44,
                End,
            End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    ad = (UBYTE *)muiAreaData(t);
    printf("font %08lx top %ld\n", (ULONG)_font(t), (LONG)_top(t));
    for (i = 36; i < 80; i += 4)
        printf("%2d: %08lx words %d %d\n", i, *(ULONG *)(ad + i),
            *(WORD *)(ad + i), *(WORD *)(ad + i + 2));
    printf("short help at 60: %s\n", *(ULONG *)(ad + 60) ? *(char **)(ad + 60) : "(null)");
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

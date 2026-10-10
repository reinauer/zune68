/* MUIA_Weight set after creation: Voyager makes its Add/BM buttons with
 * MUI_MakeObject() and then sets MUIA_Weight to 0. Prints the horizontal
 * and vertical weights and the laid-out widths.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit weightset.c -o weightset -lamiga
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
    Object *app, *win, *str, *add, *bm;
    ULONG h = 0, v = 0, open = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    add = MUI_MakeObject(MUIO_Button, (ULONG)"Add");
    bm = MUI_MakeObject(MUIO_Button, (ULONG)"BM");
    app = ApplicationObject, MUIA_Application_Title, "weightset",
        SubWindow, win = WindowObject, MUIA_Window_Title, "weightset",
            MUIA_Window_Width, 400,
            WindowContents, HGroup,
                Child, str = StringObject, StringFrame, End,
                Child, add,
                Child, bm,
            End,
        End,
    End;
    if (!app) return 20;
    SetAttrs(add, MUIA_Weight, 0, TAG_DONE);
    SetAttrs(bm, MUIA_HorizWeight, 0, TAG_DONE);
    get(add, MUIA_HorizWeight, &h); get(add, MUIA_VertWeight, &v);
    printf("Add  set MUIA_Weight 0:      horiz %ld vert %ld\n", (LONG)h, (LONG)v);
    get(bm, MUIA_HorizWeight, &h); get(bm, MUIA_VertWeight, &v);
    printf("BM   set MUIA_HorizWeight 0: horiz %ld vert %ld\n", (LONG)h, (LONG)v);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
        printf("widths string %ld add %ld bm %ld\n", (LONG)_width(str), (LONG)_width(add), (LONG)_width(bm));
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

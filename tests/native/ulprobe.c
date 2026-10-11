/* Shortcut underlines of Text objects: MUIA_Text_HiChar and MUIA_Text_HiIndex
 * in the normal, tiny and big fonts, as pixel rows. Writes ulprobe.txt.
 * Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit ulprobe.c -o ulprobe -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#ifndef MUIA_Text_HiIndex
#define MUIA_Text_HiIndex 0x804214f5
#endif
struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define N 13
static void dump(Object *o, const char *name)
{
    LONG x, y;
    struct TextFont *f = _font(o);
    printf("%s font %s/%d baseline %d box %d,%d %dx%d inner top %d\n", name,
        f->tf_Message.mn_Node.ln_Name, f->tf_YSize, f->tf_Baseline,
        _left(o), _top(o), _width(o), _height(o), _mtop(o) - _top(o));
    for (y = _top(o); y <= _bottom(o); y++)
    {
        printf("  %2ld ", (long)(y - _top(o)));
        for (x = _left(o); x <= _left(o) + 40 && x <= _right(o); x++)
            printf("%c", ReadPixel(_rp(o), x, y) == 1 ? '#' : (ReadPixel(_rp(o), x, y) == 2 ? '+' : '.'));
        printf("\n");
    }
}
int main(void)
{
    static const char *txt[N] = { "Location", "Wide", "image", "Mix", "Location", "Wide", "image", "Mix", "Big", "jig", "_Groups", "_Wide", "_Big" };
    static const char hi[N] = { 'l', 'w', 'i', 'm', 'l', 'w', 'i', 'm', 'b', 'j', 0, 0, 0 };
    Object *app, *win, *grp, *o[N];
    ULONG open = FALSE; int i;
    freopen("ulprobe.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, grp = VGroup, End, End, End;
    if (!app) return 20;
    for (i = 0; i < N; i++)
    {
        if (i < 10)
            o[i] = TextObject, MUIA_Text_Contents, txt[i], MUIA_Text_HiChar, hi[i],
                MUIA_Font, i < 4 ? MUIV_Font_Normal : i < 8 ? MUIV_Font_Tiny : MUIV_Font_Big, MUIA_Text_SetMax, TRUE, End;
        else
            o[i] = TextObject, MUIA_Text_Contents, txt[i], MUIA_Text_HiIndex, '_',
                MUIA_Font, i == 10 ? MUIV_Font_Normal : i == 11 ? MUIV_Font_Tiny : MUIV_Font_Big, MUIA_Text_SetMax, TRUE, End;
        DoMethod(grp, OM_ADDMEMBER, o[i]);
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    DoMethod(app, MUIM_Application_InputBuffered);
    for (i = 0; open && i < N; i++) dump(o[i], txt[i]);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

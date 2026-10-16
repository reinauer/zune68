/* Size of a childless group. AmiPG reserves room for its slider legends
 * with empty groups that only carry MUIA_FixWidth and MUIA_FixHeight and
 * draws the legend graphics into them. Prints the laid out size of such a
 * group, of one without fixed sizes, and of an empty Rectangle with the
 * same fixed sizes, each next to a slider in a horizontal group, plus the
 * minimum and maximum each reports. Run on each toolkit and compare.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit emptyfixprobe.c -o emptyfixprobe -lamiga
 */
#include <stdio.h>
#include <stdarg.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static Object *new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;

    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject new_object

static void show(const char *what, Object *o)
{
    printf("%-22s %3dx%-3d min %dx%d max %dx%d\n", what, _width(o),
        _height(o), _minwidth(o), _minheight(o), _maxwidth(o),
        _maxheight(o));
}

int main(void)
{
    Object *app, *win, *fixgrp, *plaingrp, *hfixgrp, *fixrect, *slider;
    struct Window *w = NULL;
    ULONG open = 0;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Base, "EMPTYFIX",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "emptyfix",
            WindowContents, VGroup,
                Child, HGroup,
                    Child, fixgrp = VGroup,
                        MUIA_FixWidth, 16, MUIA_FixHeight, 70, End,
                    Child, slider = SliderObject, MUIA_Slider_Horiz, FALSE,
                        MUIA_Numeric_Max, 127, End,
                    End,
                Child, HGroup,
                    Child, hfixgrp = HGroup,
                        MUIA_FixWidth, 16, MUIA_FixHeight, 70, End,
                    Child, plaingrp = VGroup, End,
                    Child, fixrect = RectangleObject,
                        MUIA_FixWidth, 16, MUIA_FixHeight, 70, End,
                    End,
                End,
            End,
        End;
    if (!app)
        goto close;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &w);
    if (!open || !w)
        goto dispose;
    printf("library %d.%d\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);
    show("empty fixed VGroup", fixgrp);
    show("empty fixed HGroup", hfixgrp);
    show("empty plain VGroup", plaingrp);
    show("fixed Rectangle", fixrect);
    show("slider", slider);
    rc = 0;
dispose:
    MUI_DisposeObject(app);
close:
    CloseLibrary(MUIMasterBase);
    printf(rc ? "FAIL rc=%d\n" : "PASS rc=%d\n", rc);
    return rc;
}

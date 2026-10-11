/* Outer and inner window sizes for MUIA_Window_Width/Height given in
 * pixels, with small and large contents, with and without a size
 * gadget, and for the MUIV_Window_*_Default/MinMax/Visible forms.
 * Writes winsize.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit winsize.c -o winsize -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

struct Case
{
    const char *name;
    LONG width, height;
    LONG minw, minh;     /* content minimum, from inner spacing */
    BOOL sizegadget;
};

static const struct Case cases[] =
{
    { "80h-small",   200, 80, 20, 20, TRUE },
    { "80h-small-ns", 200, 80, 20, 20, FALSE },
    { "80h-big",     200, 80, 20, 100, TRUE },
    { "40h-small",   200, 40, 20, 20, TRUE },
    { "300w",        300, MUIV_Window_Height_Default, 20, 20, TRUE },
    { "def",         MUIV_Window_Width_Default, MUIV_Window_Height_Default,
        20, 20, TRUE },
    { "minmax50",    MUIV_Window_Width_MinMax(50),
        MUIV_Window_Height_MinMax(50), 20, 20, TRUE },
    { "visible30",   MUIV_Window_Width_Visible(30),
        MUIV_Window_Height_Visible(30), 20, 20, TRUE },
};

int main(void)
{
    int i;

    freopen("winsize.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++)
    {
        const struct Case *c = &cases[i];
        Object *app, *win, *root;
        ULONG open = FALSE;
        struct Window *w = NULL;

        app = ApplicationObject, SubWindow, win = WindowObject,
            MUIA_Window_Title, "winsize",
            MUIA_Window_Width, c->width,
            MUIA_Window_Height, c->height,
            MUIA_Window_SizeGadget, c->sizegadget,
            WindowContents, root = RectangleObject,
                MUIA_InnerLeft, c->minw / 2, MUIA_InnerRight, c->minw / 2,
                MUIA_InnerTop, c->minh / 2, MUIA_InnerBottom, c->minh / 2,
                End,
            End, End;
        if (!app)
            return 20;
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        get(win, MUIA_Window_Window, &w);
        if (open && w)
            printf("%-13s outer %3d x %3d borders %d,%d,%d,%d root %3d x %3d"
                " min %d,%d\n", c->name, w->Width, w->Height,
                w->BorderLeft, w->BorderTop, w->BorderRight,
                w->BorderBottom, _width(root), _height(root),
                _minwidth(root), _minheight(root));
        else
            printf("%-13s not open\n", c->name);
        set(win, MUIA_Window_Open, FALSE);
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

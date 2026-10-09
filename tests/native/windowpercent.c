/* Screen- and visible-relative window size comparison.
 * Opens small windows sized as screen percentages and reports their outer
 * size, borders and the screen dimensions.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit windowpercent.c -o windowpercent -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;

static const struct
{
    const char *name;
    LONG width, height;
} cases[] = {
    { "screen50", MUIV_Window_Width_Screen(50), MUIV_Window_Height_Screen(50) },
    { "screen30", MUIV_Window_Width_Screen(30), MUIV_Window_Height_Screen(30) },
    { "visible50", MUIV_Window_Width_Visible(50),
        MUIV_Window_Height_Visible(50) },
    { "minmax50", MUIV_Window_Width_MinMax(50),
        MUIV_Window_Height_MinMax(50) },
};

int main(void)
{
    ULONG i;
    int rc = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        Object *app, *win;
        struct Window *w = NULL;
        ULONG open = FALSE;

        app = ApplicationObject,
            MUIA_Application_Title, "windowpercent",
            SubWindow, win = WindowObject,
                MUIA_Window_Title, "windowpercent",
                MUIA_Window_Width, cases[i].width,
                MUIA_Window_Height, cases[i].height,
                WindowContents, VGroup,
                    Child, RectangleObject, TextFrame,
                        MUIA_FixHeight, 10, End,
                    Child, RectangleObject, End,
                    End,
                End,
            End;
        if (!app)
        {
            rc = 20;
            continue;
        }
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        if (open)
        {
            get(win, MUIA_Window_Window, &w);
            printf("%s window=%d,%d borders=%d,%d,%d,%d screen=%d,%d "
                "bar=%d\n", cases[i].name, w->Width, w->Height,
                w->BorderLeft, w->BorderTop, w->BorderRight,
                w->BorderBottom, w->WScreen->Width, w->WScreen->Height,
                w->WScreen->BarHeight);
        }
        else
            rc = 20;
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

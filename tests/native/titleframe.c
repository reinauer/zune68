/* Titled group frame spacing comparison.
 * Reports frame sizes and first-child offsets of titled and untitled
 * group frames with default and overridden inner spacing.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit titleframe.c -o titleframe -lamiga
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

#define COUNT 5

static Object *group(Object **child, const char *title, LONG top, LONG bottom)
{
    return VGroup,
        MUIA_Frame, MUIV_Frame_Group,
        title ? MUIA_FrameTitle : TAG_IGNORE, title,
        top >= 0 ? MUIA_InnerTop : TAG_IGNORE, top,
        bottom >= 0 ? MUIA_InnerBottom : TAG_IGNORE, bottom,
        Child, *child = RectangleObject, MUIA_FixHeight, 10, End,
        End;
}

int main(void)
{
    Object *app, *win, *groups[COUNT], *children[COUNT];
    struct TextFont *f;
    ULONG open = FALSE, i;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "titleframe",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "titleframe",
            WindowContents, VGroup,
                Child, groups[0] = group(&children[0], "Title", -1, -1),
                Child, groups[1] = group(&children[1], NULL, -1, -1),
                Child, groups[2] = group(&children[2], "Title", 0, -1),
                Child, groups[3] = group(&children[3], "Title", 10, -1),
                Child, groups[4] = group(&children[4], "Title", -1, 0),
                End,
            End,
        End;
    if (!app)
        goto done;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    Delay(25);
    for (i = 0; i < COUNT; i++)
    {
        f = _font(groups[i]);
        printf("group%lu font=%s/%d base=%d top=%d height=%d sub=%d,%d "
            "child=%d,%d min=%d\n", (unsigned long)i,
            f->tf_Message.mn_Node.ln_Name, f->tf_YSize, f->tf_Baseline,
            _top(groups[i]), _height(groups[i]), _subwidth(groups[i]),
            _subheight(groups[i]), _top(children[i]) - _top(groups[i]),
            _bottom(groups[i]) - _bottom(children[i]),
            _minheight(groups[i]));
    }
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

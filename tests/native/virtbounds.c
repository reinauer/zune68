/* Scrollgroup and virtual group MinMax comparison.
 * Each case opens its own window so the window default size reflects the
 * scrollgroup's default dimensions.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit virtbounds.c -o virtbounds -lamiga
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

static const char text[] =
    "line 01\nline 02\nline 03\nline 04\nline 05\nline 06\nline 07\n"
    "line 08\nline 09\nline 10\nline 11\nline 12\nline 13\nline 14\n"
    "line 15\nline 16\nline 17\nline 18\nline 19\nline 20\nline 21\n"
    "line 22\nline 23\nline 24\nline 25\nline 26\nline 27\nline 28\n"
    "line 29\nline 30 is quite a lot wider than all the other lines";

static void report(const char *name, Object *o)
{
    printf("  %s box=%d,%d,%d,%d min=%d,%d def=%d,%d max=%d,%d sub=%d,%d\n",
        name, _left(o), _top(o), _width(o), _height(o), _minwidth(o),
        _minheight(o), _defwidth(o), _defheight(o), _maxwidth(o),
        _maxheight(o), _subwidth(o), _subheight(o));
}

static Object *contents(int kind)
{
    switch (kind)
    {
    case 0:
        return VirtgroupObject, VirtualFrame,
            Child, TextObject, MUIA_Text_Contents, text, End,
            End;
    case 1:
        return VGroupV, VirtualFrame,
            Child, RectangleObject, TextFrame,
                MUIA_FixWidth, 300, MUIA_FixHeight, 400, End,
            End;
    default:
        return VGroupV, VirtualFrame,
            Child, RectangleObject, TextFrame,
                MUIA_FixWidth, 40, MUIA_FixHeight, 30, End,
            End;
    }
}

int main(void)
{
    static const char *const names[] = {
        "text", "big", "small", "small-autobars"
    };
    int kind, rc = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (kind = 0; kind < 4; kind++)
    {
        Object *app, *win, *sg, *virt;
        struct Window *w = NULL;
        ULONG open = FALSE;

        app = ApplicationObject,
            MUIA_Application_Title, "virtbounds",
            SubWindow, win = WindowObject,
                MUIA_Window_Title, "virtbounds",
                WindowContents, VGroup,
                    Child, sg = ScrollgroupObject,
                        MUIA_Scrollgroup_AutoBars, kind == 3,
                        MUIA_Scrollgroup_Contents, virt = contents(kind),
                        End,
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
            DoMethod(app, MUIM_Application_InputBuffered);
            Delay(10);
            get(win, MUIA_Window_Window, &w);
            printf("%s window=%d,%d\n", names[kind], w->Width, w->Height);
            report("scrollgroup", sg);
            report("virtual", virt);
            {
                Object *vbar = NULL;
                ULONG shown = 0;

                get(sg, MUIA_Scrollgroup_VertBar, &vbar);
                if (vbar)
                {
                    get(vbar, MUIA_ShowMe, &shown);
                    printf("  vertbar shown=%lu\n", (unsigned long)shown);
                }
            }
        }
        else
            rc = 20;
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

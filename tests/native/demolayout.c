/* MUI-Demo main window layout comparison.
 * Rebuilds the SDK demo's main window and reports every object's box,
 * MinMax and frame size, walking groups recursively.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit demolayout.c -o demolayout -lamiga
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

static const char info[] =
    "\tWelcome to the MUI demonstration program. This little toy will "
    "show you how easy it is to create graphical user interfaces with MUI "
    "and how powerful the results are.\n\tMUI is based on BOOPSI, Amiga's "
    "basic object oriented programming system.";

static Object *button(const char *label)
{
    return SimpleButton(label);
}

static void walk(Object *o, int depth)
{
    struct List *children = NULL;
    Object *child;
    APTR state;

    printf("%*sbox=%d,%d,%d,%d min=%d,%d def=%d,%d max=%d,%d "
        "sub=%d,%d\n", depth * 2, "", _left(o), _top(o), _width(o),
        _height(o), _minwidth(o), _minheight(o), _defwidth(o),
        _defheight(o), _maxwidth(o), _maxheight(o), _subwidth(o),
        _subheight(o));
    if (depth >= 4 || !get(o, MUIA_Group_ChildList, &children) || !children)
        return;
    state = children->lh_Head;
    while ((child = NextObject(&state)) != NULL)
        walk(child, depth + 1);
}

int main(void)
{
    Object *app, *win, *root;
    ULONG open = FALSE;
    struct Window *w = NULL;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "demolayout",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "demolayout",
            WindowContents, root = VGroup,
                Child, TextObject, GroupFrame,
                    MUIA_Background, MUII_SHADOWFILL,
                    MUIA_Text_Contents, "\33c\0338MUI - \33bM\33nagic"
                        "\33bU\33nser\33bI\33nnterface\n"
                        "written 1992-95 by Stefan Stuntz",
                    End,
                Child, ListviewObject,
                    MUIA_Weight, 50,
                    MUIA_Listview_Input, FALSE,
                    MUIA_Listview_List, FloattextObject,
                        MUIA_Frame, MUIV_Frame_ReadList,
                        MUIA_Background, MUII_ReadListBack,
                        MUIA_Floattext_Text, info,
                        MUIA_Floattext_TabSize, 4,
                        MUIA_Floattext_Justify, TRUE,
                        End,
                    End,
                Child, VGroup, GroupFrameT("Available Demos"),
                    Child, HGroup, MUIA_Group_SameWidth, TRUE,
                        Child, button("_Groups"),
                        Child, button("_Frames"),
                        Child, button("_Backfill"),
                        End,
                    Child, HGroup, MUIA_Group_SameWidth, TRUE,
                        Child, button("_Notify"),
                        Child, button("_Listviews"),
                        Child, button("_Cycle"),
                        End,
                    End,
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
    get(win, MUIA_Window_Window, &w);
    printf("window %d %d borders %d,%d,%d,%d\n", w->Width, w->Height,
        w->BorderLeft, w->BorderTop, w->BorderRight, w->BorderBottom);
    walk(root, 0);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

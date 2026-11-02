/* Calltips.mcc: a borderless tip window placed against its source object. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <libraries/mui.h>
#include <graphics/modeid.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include "../../workbench/classes/zune/calltips/Calltips_mcc.h"
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static int failures;
static void check(BOOL ok, const char *label)
{
    if (!ok) { printf("FAIL %s\n", label); failures++; }
}
static ULONG attr(Object *obj, ULONG id)
{
    ULONG value = 0;
    GetAttr(id, obj, &value);
    return value;
}
int main(int argc, char **argv)
{
    struct Screen *screen = NULL;
    struct Rect32 rectangle = {0, 30, 150, 60};
    Object *app, *win, *source, *tip, *text;
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    if (argc > 1)
        screen = OpenScreenTags(NULL, SA_DisplayID, HIRES_KEY, SA_Width, 640,
            SA_Height, 256, SA_Depth, 2, TAG_DONE);
    source = MO(MUIC_Text, MUIA_Text_Contents, (ULONG)"source", MUIA_FixHeight, 20,
        TAG_DONE);
    win = MO(MUIC_Window, MUIA_Window_Screen, (ULONG)screen,
        MUIA_Window_RootObject, (ULONG)source, MUIA_Window_Width, 200, TAG_DONE);
    app = MO(MUIC_Application, MUIA_Application_Window, (ULONG)win, TAG_DONE);
    check(app != NULL, "application");
    if (!app) return 20;
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    check(attr(win, MUIA_Window_Open), "source window open");

    check(MO(MUIC_Calltips, TAG_DONE) == NULL, "a tip needs a source");

    text = MO(MUIC_Text, MUIA_Text_Contents, (ULONG)"tip", TAG_DONE);
    tip = MO(MUIC_Calltips, MUIA_Calltips_Source, (ULONG)source,
        MUIA_Calltips_Rectangle, (ULONG)&rectangle,
        MUIA_Window_RootObject, (ULONG)text, TAG_DONE);
    check(tip != NULL, "tip creation");
    if (tip)
    {
        struct Window *parent = (struct Window *)attr(win, MUIA_Window_Window), *window;
        DoMethod(app, OM_ADDMEMBER, tip);
        DoMethod(tip, MUIM_Calltips_ParentShow);
        check(attr(tip, MUIA_Window_Open), "shown");
        window = (struct Window *)attr(tip, MUIA_Window_Window);
        check(window != NULL, "tip has a window");
        if (window && parent)
        {
            LONG left = window->LeftEdge, top = window->TopEdge;
            printf("parent=%d,%d %dx%d tip=%ld,%ld %dx%d\n", parent->LeftEdge,
                parent->TopEdge, parent->Width, parent->Height, left, top,
                window->Width, window->Height);
            check(top >= parent->TopEdge, "tip below the parent's top");
            check(!(window->Flags & (WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_SIZEGADGET)),
                "tip has no gadgets");
            check(!(window->Flags & WFLG_WINDOWACTIVE), "tip takes no focus");
            DoMethod(tip, MUIM_Calltips_SetRectangle, 10, 10, 99, 49);
            Delay(10);
            printf("moved tip=%d,%d %dx%d\n", window->LeftEdge, window->TopEdge,
                window->Width, window->Height);
            check(window->Width == 90 && window->Height == 40, "rectangle sets the size");
            check(window->LeftEdge != left || window->TopEdge != top, "rectangle moves the tip");
        }
        DoMethod(tip, MUIM_Calltips_ParentHide);
        check(!attr(tip, MUIA_Window_Open), "hidden");
        DoMethod(tip, MUIM_Calltips_ParentShow);
        check(attr(tip, MUIA_Window_Open), "shown again");
        DoMethod(tip, MUIM_Calltips_ParentWindowArranged);
        DoMethod(tip, MUIM_Calltips_ParentHide);
    }
    MUI_DisposeObject(app);
    if (screen) CloseScreen(screen);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    CloseLibrary(MUIMasterBase);
    return failures ? 20 : 0;
}

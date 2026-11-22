/* The spacing between an NList's frame and its inner box. NList clears
 * MADF_DRAWOBJECT before calling Area and never paints that ring itself.
 * The NList box is filled with pen 3 and redrawn in several ways; each
 * line counts the pen-3 pixels left in the ring and in the inner box.
 * MUI 3.8 leaves none in the ring when a parent redraws the list (the
 * frame comes with its spacing) and the whole ring when the list redraws
 * itself. Writes nlring.txt. Needs NList.mcc. Compare native and MUI 3.8.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit nlring.c -o nlring -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define MUIC_NList "NList.mcc"
#define MUIC_NListview "NListview.mcc"
#define MUIA_NListview_NList 0x9d510020
#define MUIA_NList_SourceArray 0x9d510061
static FILE *out;
static Object *app;
static const char *lines[] = { "one", "two", "three", "four", NULL };

static void settle(void)
{
    int i;

    for (i = 0; i < 10; i++) {
        DoMethod(app, MUIM_Application_InputBuffered);
        Delay(3);
    }
}

static void paint(Object *o)
{
    SetAPen(_rp(o), 3);
    RectFill(_rp(o), _left(o), _top(o), _right(o), _bottom(o));
}

static void count(const char *what, Object *o)
{
    LONG ring = 0, inner = 0, x, y;

    for (y = _top(o); y <= _bottom(o); y++)
        for (x = _left(o); x <= _right(o); x++)
            if (ReadPixel(_rp(o), x, y) == 3) {
                if (x >= _mleft(o) && x <= _mright(o) && y >= _mtop(o)
                    && y <= _mbottom(o))
                    inner++;
                else
                    ring++;
            }
    fprintf(out, "%-26s ring %4ld inner %4ld\n", what, ring, inner);
}

static Object *nlistview(Object **nl)
{
    return MUI_NewObject(MUIC_NListview,
        MUIA_NListview_NList, *nl = MUI_NewObject(MUIC_NList,
            InputListFrame,
            MUIA_NList_SourceArray, lines,
            TAG_DONE),
        TAG_DONE);
}

int main(void)
{
    Object *win, *grp, *nlv, *nl, *pages, *black, *page, *nl2;
    ULONG open = 0;

    if (!(out = fopen("nlring.txt", "w")))
        return 20;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, MUIA_Application_Title, "nlring",
        SubWindow, win = WindowObject, MUIA_Window_Title, "nlring",
            MUIA_Window_Width, 220, MUIA_Window_Height, 200,
            WindowContents, VGroup,
                Child, grp = VGroup,
                    Child, nlv = nlistview(&nl),
                End,
                Child, pages = VGroup,
                    Child, black = RectangleObject,
                        MUIA_Background, MUII_SHADOW, End,
                End,
            End,
        End,
    End;
    page = VGroup, Child, nlistview(&nl2), End;
    if (!app || !page) {
        fprintf(out, "no application\n");
        fclose(out);
        return 10;
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open) {
        settle();
        fprintf(out, "nlist box %d,%d %dx%d inner %d,%d,%d,%d\n",
            _left(nl), _top(nl), _width(nl), _height(nl),
            _mleft(nl) - _left(nl), _mtop(nl) - _top(nl),
            _right(nl) - _mright(nl), _bottom(nl) - _mbottom(nl));
        count("after open", nl);

        paint(nl);
        MUI_Redraw(nl, MADF_DRAWOBJECT);
        settle();
        count("Redraw(nlist, OBJECT)", nl);

        paint(nl);
        MUI_Redraw(nlv, MADF_DRAWOBJECT);
        settle();
        count("Redraw(nlistview, OBJECT)", nl);

        paint(nl);
        MUI_Redraw(grp, MADF_DRAWOBJECT);
        settle();
        count("Redraw(group, OBJECT)", nl);

        paint(nl);
        MUI_Redraw(nl, MADF_DRAWUPDATE);
        settle();
        count("Redraw(nlist, UPDATE)", nl);

        /* Prefs/Zune's page switch */
        DoMethod(pages, MUIM_Group_InitChange);
        DoMethod(pages, OM_REMMEMBER, black);
        DoMethod(pages, OM_ADDMEMBER, page);
        DoMethod(pages, MUIM_Group_ExitChange);
        settle();
        count("page swap from black", nl2);
        MUI_DisposeObject(black);
        set(win, MUIA_Window_Open, FALSE);
    }
    else
        fprintf(out, "window did not open\n");
    MUI_DisposeObject(app);
    if (!open)
        MUI_DisposeObject(page);
    CloseLibrary(MUIMasterBase);
    fclose(out);
    return 0;
}

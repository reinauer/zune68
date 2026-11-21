/* A List format that cannot be used (more columns than a list takes):
 * which format the list reports afterwards, and that it still shows its
 * entries in two columns (the width of each column's text after a
 * redraw). Then a usable format again.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit \
 *        listfmt.c -o listfmt -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static char wide[400];
static const char good[] = "BAR,";
static const char good2[] = "P=\33r,";

static LONG Display(struct Hook *h __asm("a0"), char **array __asm("a2"),
    char *entry __asm("a1"))
{
    if (!entry)
    {
        array[0] = "Name";
        array[1] = "Size";
        return 0;
    }
    array[0] = entry;
    array[1] = "12";
    return 0;
}

static struct Hook display_hook = { { 0 }, (HOOKFUNC)Display, 0, 0 };

static const char *fmtname(STRPTR f)
{
    if (f == (STRPTR)good)
        return "first";
    if (f == (STRPTR)good2)
        return "second";
    if (f == (STRPTR)wide)
        return "too wide";
    return f ? "other" : "NULL";
}

static void show(const char *when, Object *list)
{
    STRPTR f = NULL;
    IPTR n = 0;

    get(list, MUIA_List_Format, &f);
    get(list, MUIA_List_Entries, &n);
    printf("%-18s format %s, %ld entries\n", when, fmtname(f), (long)n);
}

int main(void)
{
    static const char *entries[] = { "alpha", "beta", "gamma", NULL };
    Object *app, *win, *list;
    ULONG open = FALSE;
    int i;

    for (i = 0; i < 99; i++)
        strcat(wide, "BAR,");
    strcat(wide, "BAR");
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Title, "listfmt",
        WindowContents, VGroup,
            Child, ListviewObject, MUIA_Listview_List, list = ListObject,
                InputListFrame, MUIA_List_Format, good,
                MUIA_List_DisplayHook, &display_hook,
                MUIA_List_SourceArray, entries, MUIA_List_Title, TRUE,
                End, End,
            End, End, End;
    if (!app)
        return 20;
    show("new", list);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %s\n", open ? "yes" : "no");
    if (open)
    {
        set(list, MUIA_List_Format, wide);
        show("100 columns", list);
        DoMethod(list, MUIM_List_Redraw, MUIV_List_Redraw_All);
        DoMethod(app, MUIM_Application_InputBuffered);
        set(list, MUIA_List_Format, good2);
        show("two columns again", list);
        DoMethod(list, MUIM_List_InsertSingle, "delta", MUIV_List_Insert_Bottom);
        show("inserted", list);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}

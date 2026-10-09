/* List, Listview and Floattext MinMax comparison.
 * Reports each object's box and MinMax for empty, short and long
 * contents, including a Floattext that only formats after layout.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit listbounds.c -o listbounds -lamiga
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

static const char longtext[] =
    "\tWelcome to the MUI demonstration program. This little toy will "
    "show you how easy it is to create graphical user interfaces with MUI "
    "and how powerful the results are.\n\tMUI is based on BOOPSI, Amiga's "
    "basic object oriented programming system. For details about "
    "programming, see the 'ReadMe' file and the documented source code of "
    "this demo. Only one thing so far: it's really easy!\n\tNow go on, "
    "click around and watch this demo. Or use your keyboard (TAB, Return, "
    "Cursor-Keys) if you like that better. Hint: play around with the MUI "
    "preferences program and customize every pixel to fit your personal "
    "taste.";
static const char *const entries[] = {
    "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
    "seventeen", "eighteen", "nineteen", "twenty", NULL
};

static void report(const char *name, Object *o)
{
    printf("%s box=%d,%d,%d,%d min=%d,%d def=%d,%d max=%d,%d sub=%d,%d\n",
        name, _left(o), _top(o), _width(o), _height(o), _minwidth(o),
        _minheight(o), _defwidth(o), _defheight(o), _maxwidth(o),
        _maxheight(o), _subwidth(o), _subheight(o));
}

int main(int argc, char **argv)
{
    Object *app, *win, *ft, *ftlv, *empty, *emptylv, *few, *fewlv, *many,
        *manylv, *noscroll, *big, *bigfew, *noframe, *scroller, *hscroller;
    ULONG open = FALSE;
    LONG entries_n = 0;
    struct Window *w = NULL;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "listbounds",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "listbounds",
            WindowContents, HGroup,
                Child, ftlv = ListviewObject,
                    MUIA_Listview_Input, FALSE,
                    MUIA_Listview_List, ft = FloattextObject,
                        MUIA_Frame, MUIV_Frame_ReadList,
                        MUIA_Background, MUII_ReadListBack,
                        MUIA_Floattext_Text, longtext,
                        MUIA_Floattext_TabSize, 4,
                        MUIA_Floattext_Justify, TRUE,
                        End,
                    End,
                Child, emptylv = ListviewObject,
                    MUIA_Listview_List, empty = ListObject,
                        InputListFrame, End,
                    End,
                Child, fewlv = ListviewObject,
                    MUIA_Listview_List, few = ListObject,
                        InputListFrame, End,
                    End,
                Child, manylv = ListviewObject,
                    MUIA_Listview_List, many = ListObject,
                        InputListFrame, End,
                    End,
                Child, VGroup,
                    Child, ListviewObject,
                        MUIA_Listview_ScrollerPos,
                            MUIV_Listview_ScrollerPos_None,
                        MUIA_Listview_List, noscroll = ListObject,
                            InputListFrame, End,
                        End,
                    Child, ListviewObject,
                        MUIA_Listview_List, noframe = ListObject, End,
                        End,
                    End,
                Child, ListviewObject,
                    MUIA_Listview_List, big = ListObject,
                        InputListFrame, MUIA_Font, MUIV_Font_Big, End,
                    End,
                Child, ListviewObject,
                    MUIA_Listview_List, bigfew = ListObject,
                        InputListFrame, MUIA_Font, MUIV_Font_Big, End,
                    End,
                Child, VGroup,
                    Child, scroller = ScrollbarObject, End,
                    Child, hscroller = ScrollbarObject,
                        MUIA_Group_Horiz, TRUE, End,
                    End,
            End,
        End,
    End;
    if (!app)
        goto done;
    DoMethod(few, MUIM_List_Insert, entries, 3, MUIV_List_Insert_Bottom);
    DoMethod(many, MUIM_List_Insert, entries, -1, MUIV_List_Insert_Bottom);
    DoMethod(bigfew, MUIM_List_Insert, entries, 3, MUIV_List_Insert_Bottom);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    Delay(25);
    get(win, MUIA_Window_Window, &w);
    printf("window %d %d\n", w->Width, w->Height);
    report("ftlv", ftlv);
    report("ft", ft);
    get(ft, MUIA_List_Entries, &entries_n);
    printf("ft entries=%ld\n", (long)entries_n);
    report("emptylv", emptylv);
    report("empty", empty);
    report("fewlv", fewlv);
    report("few", few);
    report("manylv", manylv);
    report("many", many);
    {
        LONG visible = 0;

        get(many, MUIA_List_Visible, &visible);
        printf("many visible=%ld\n", (long)visible);
    }
    report("noscroll", noscroll);
    report("noframe", noframe);
    report("big", big);
    report("bigfew", bigfew);
    report("scroller", scroller);
    report("hscroller", hscroller);
    {
        Object *sb[2], *child;
        struct List *children = NULL;
        APTR state;
        int k, n;

        sb[0] = scroller;
        sb[1] = hscroller;
        for (k = 0; k < 2; k++)
        {
            get(sb[k], MUIA_Group_ChildList, &children);
            if (!children)
                continue;
            state = children->lh_Head;
            for (n = 0; (child = NextObject(&state)) != NULL; n++)
            {
                char name[16];

                sprintf(name, "%s.%d", k ? "h" : "v", n);
                report(name, child);
            }
        }
    }
    /* An argument keeps the window open for pixel inspection. */
    if (argc > 1)
    {
        fflush(stdout);
        Delay(1500);
    }
    (void)argv;
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

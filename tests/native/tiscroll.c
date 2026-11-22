/* Textinputscroll.mcc: its object tree and sizes, and how the scroll bars
 * follow the text (MUIA_Textinput_Lines, MUIA_Textinput_TopLine, the bars'
 * Prop attributes), for a multiline field of 20 lines, a multiline
 * Textinput without bars and a vertical-bar-only one. Writes tiscroll.txt.
 * Compare native and MUI 3.8 with the original Textinput.mcc 29.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit tiscroll.c -o tiscroll -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>


struct Library *MUIMasterBase;
ULONG __stack = 65536;
static FILE *out;
static Object *bars[8];
static int nbars;

static const char *classname(Object *o)
{
    static char buf[64];
    struct IClass *cl = OCLASS(o);
    int depth = 0;

    while (cl && !cl->cl_ID)
        cl = cl->cl_Super, depth++;
    if (!cl)
        return "?";
    if (!depth)
        return (const char *)cl->cl_ID;
    snprintf(buf, sizeof(buf), "%s+%d", (const char *)cl->cl_ID, depth);
    return buf;
}

static void dump(Object *o, int level)
{
    struct List *children = NULL;
    Object *child;
    APTR state;
    const char *name = classname(o);

    fprintf(out, "%*s%s box=%d,%d %dx%d inner=%d,%d min=%dx%d def=%dx%d max=%dx%d\n",
        level * 2, "", name, _left(o), _top(o), _width(o), _height(o),
        _mleft(o) - _left(o), _mtop(o) - _top(o), _minwidth(o),
        _minheight(o), _defwidth(o), _defheight(o), _maxwidth(o),
        _maxheight(o));
    if (!strncmp(name, "Prop", 4) && nbars < 8)
        bars[nbars++] = o;
    if (!GetAttr(MUIA_Group_ChildList, o, (IPTR *)&children) || !children)
        return;
    state = children->lh_Head;
    while ((child = NextObject(&state)))
        dump(child, level + 1);
}

static void scrollstate(const char *what, Object *ts)
{
    IPTR lines = 0, top = 0;
    ULONG okl, okt;
    int i;

    okl = GetAttr(MUIA_Textinput_Lines, ts, &lines);
    okt = GetAttr(MUIA_Textinput_TopLine, ts, &top);
    fprintf(out, "%s: lines=%lu(%lu) topline=%lu(%lu)", what,
        (unsigned long)lines, (unsigned long)okl, (unsigned long)top,
        (unsigned long)okt);
    for (i = 0; i < nbars; i++) {
        IPTR first = 0, entries = 0, visible = 0, horiz = 0;
        GetAttr(MUIA_Prop_First, bars[i], &first);
        GetAttr(MUIA_Prop_Entries, bars[i], &entries);
        GetAttr(MUIA_Prop_Visible, bars[i], &visible);
        GetAttr(MUIA_Prop_Horiz, bars[i], &horiz);
        fprintf(out, " | %s first=%lu entries=%lu visible=%lu",
            horiz ? "h" : "v", (unsigned long)first,
            (unsigned long)entries, (unsigned long)visible);
    }
    fprintf(out, "\n");
}

static void settle(Object *app)
{
    int i;

    for (i = 0; i < 10; i++) {
        DoMethod(app, MUIM_Application_InputBuffered);
        Delay(2);
    }
}

int main(void)
{
    static char text[1024];
    Object *app, *win, *ts, *ti, *tv;
    ULONG open = 0;
    int i;

    for (i = 1, text[0] = 0; i <= 20; i++)
        sprintf(text + strlen(text), "%sline %d of the text", i > 1 ? "\n" : "", i);
    if (!(out = fopen("tiscroll.txt", "w")))
        return 20;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, MUIA_Application_Title, "tiscroll",
        SubWindow, win = WindowObject, MUIA_Window_Title, "tiscroll",
            WindowContents, VGroup,
                Child, ts = MUI_NewObject("Textinputscroll.mcc",
                    MUIA_Textinput_Multiline, TRUE,
                    MUIA_Textinput_Contents, text, TAG_DONE),
                Child, ti = MUI_NewObject("Textinput.mcc",
                    MUIA_Textinput_Multiline, TRUE,
                    MUIA_Textinput_Contents, "one\ntwo\nthree", TAG_DONE),
                Child, tv = MUI_NewObject("Textinputscroll.mcc",
                    MUIA_Textinput_Multiline, TRUE,
                    MUIA_Textinputscroll_VertScrollerOnly, TRUE,
                    MUIA_Textinput_Contents, "a\nb", TAG_DONE),
            End,
        End,
    End;
    if (!app) {
        fprintf(out, "no application\n");
        return 10;
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open) {
        settle(app);
        dump(ts, 0);
        scrollstate("open", ts);
        set(ts, MUIA_Textinput_TopLine, 5);
        settle(app);
        scrollstate("topline 5", ts);
        for (i = 0; i < nbars; i++) {
            IPTR horiz = 0;
            GetAttr(MUIA_Prop_Horiz, bars[i], &horiz);
            if (!horiz)
                set(bars[i], MUIA_Prop_First, 10);
        }
        settle(app);
        scrollstate("bar 10", ts);
        nbars = 0;
        dump(ti, 0);
        dump(tv, 0);
    }
    else
        fprintf(out, "window did not open\n");
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    fclose(out);
    return 0;
}

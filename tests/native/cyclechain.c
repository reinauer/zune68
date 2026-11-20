/*
 * cyclechain: the window's Tab order (cycle chain), black box. Six chain
 * objects A..F (0..5: button, string, button, a page group of two
 * buttons D and E, listview). Prints the index of the object that
 * MUIA_Window_ActiveObject Next/Prev reach (-1 none, 99 another object):
 * from none, from each object, past the ends, with C disabled, with page E
 * shown (D hidden), after reopening the window, with B out of the chain
 * (set while closed, and while open, then back in while open), after
 * MUIM_Window_SetCycleChain(F, C, A, B) and the MUIA_CycleChain values it
 * leaves, and what a hidden or disabled object set active gives.
 * The same output on two libraries = the same Tab order.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -DMUI_OBSOLETE \
 *        -Ibuild/muimaster/include tests/native/cyclechain.c \
 *        -o cyclechain -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define N 6
static Object *o[N], *win, *pages;

static int which(Object *a)
{
    int i;
    for (i = 0; i < N; i++)
        if (o[i] == a)
            return i;
    return a ? 99 : -1;
}

static int active(void)
{
    Object *a = NULL;
    get(win, MUIA_Window_ActiveObject, &a);
    return which(a);
}

static void none(void)
{
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
}

static void walk(const char *what, IPTR dir, int steps)
{
    printf("%s:", what);
    while (steps-- > 0)
    {
        set(win, MUIA_Window_ActiveObject, dir);
        printf(" %d", active());
    }
    printf("\n");
}

static void from_each(void)
{
    int i;
    for (i = 0; i < N; i++)
    {
        set(win, MUIA_Window_ActiveObject, o[i]);
        printf("from %d:", i);
        set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_Next);
        printf(" next %d", active());
        set(win, MUIA_Window_ActiveObject, o[i]);
        set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_Prev);
        printf(" prev %d\n", active());
    }
}

static void reopen(void)
{
    set(win, MUIA_Window_Open, FALSE);
    set(win, MUIA_Window_Open, TRUE);
}

int main(void)
{
    Object *app;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "cyclechain",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "cyclechain",
            WindowContents, VGroup,
                Child, o[0] = SimpleButton("_A"),
                Child, o[1] = StringObject, StringFrame, End,
                Child, o[2] = SimpleButton("_C"),
                Child, pages = PageGroup,
                    Child, o[3] = SimpleButton("_D"),
                    Child, o[4] = SimpleButton("_E"),
                End,
                Child, o[5] = ListviewObject,
                    MUIA_Listview_List, ListObject, InputListFrame, End,
                End,
            End,
        End,
    End;
    if (!app)
    {
        printf("no application\n");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    for (i = 0; i < N; i++)
        set(o[i], MUIA_CycleChain, 1);
    set(win, MUIA_Window_Open, TRUE);
    walk("next from none", MUIV_Window_ActiveObject_Next, 8);
    none();
    walk("prev from none", MUIV_Window_ActiveObject_Prev, 8);
    from_each();

    set(o[2], MUIA_Disabled, TRUE);
    none();
    walk("C disabled, next", MUIV_Window_ActiveObject_Next, 7);
    walk("C disabled, prev", MUIV_Window_ActiveObject_Prev, 7);
    set(o[2], MUIA_Disabled, FALSE);

    set(pages, MUIA_Group_ActivePage, 1);
    none();
    walk("page E, next", MUIV_Window_ActiveObject_Next, 7);
    walk("page E, prev", MUIV_Window_ActiveObject_Prev, 7);

    reopen();
    none();
    walk("reopened, next", MUIV_Window_ActiveObject_Next, 7);

    set(win, MUIA_Window_Open, FALSE);
    set(o[1], MUIA_CycleChain, 0);
    set(win, MUIA_Window_Open, TRUE);
    none();
    walk("B out while closed, next", MUIV_Window_ActiveObject_Next, 7);
    set(o[1], MUIA_CycleChain, 1);
    none();
    walk("B in while open, next", MUIV_Window_ActiveObject_Next, 7);
    set(o[0], MUIA_CycleChain, 0);
    set(o[0], MUIA_CycleChain, 1);
    none();
    walk("A out and in while open, next", MUIV_Window_ActiveObject_Next, 7);

    DoMethod(win, MUIM_Window_SetCycleChain, (IPTR)o[5], (IPTR)o[2],
        (IPTR)o[0], (IPTR)o[1], (IPTR)NULL);
    none();
    walk("explicit, next", MUIV_Window_ActiveObject_Next, 7);
    walk("explicit, prev", MUIV_Window_ActiveObject_Prev, 7);
    reopen();
    none();
    walk("explicit reopened, next", MUIV_Window_ActiveObject_Next, 7);
    printf("cyclechain attrs:");
    for (i = 0; i < N; i++)
    {
        IPTR cc = 99;
        get(o[i], MUIA_CycleChain, &cc);
        printf(" %d", (int)cc);
    }
    printf("\n");

    /* a hidden object set active, the active object hidden by a page
     * switch, a disabled object set active */
    set(win, MUIA_Window_ActiveObject, o[3]);
    printf("hidden D set active: %d\n", active());
    set(win, MUIA_Window_ActiveObject, o[4]);
    printf("E set active: %d\n", active());
    set(pages, MUIA_Group_ActivePage, 0);
    printf("E hidden by its page: %d\n", active());
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_Next);
    printf("next from hidden E: %d\n", active());
    set(pages, MUIA_Group_ActivePage, 1);
    set(win, MUIA_Window_ActiveObject, o[4]);
    set(pages, MUIA_Group_ActivePage, 0);
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_Prev);
    printf("prev from hidden E: %d\n", active());
    set(o[2], MUIA_Disabled, TRUE);
    set(win, MUIA_Window_ActiveObject, o[2]);
    printf("disabled C set active: %d\n", active());
    set(o[2], MUIA_Disabled, FALSE);
    set(win, MUIA_Window_ActiveObject, o[2]);
    set(o[2], MUIA_Disabled, TRUE);
    printf("active C disabled: %d\n", active());
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_Next);
    printf("next from disabled C: %d\n", active());
    set(o[2], MUIA_Disabled, FALSE);

    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}

/* Programmatic list scrolling must not queue IDCMP_IDCMPUPDATE messages
 * from the list's scrollbar gadget. Fills a list with 200 entries, opens
 * its window, consumes pending input, makes 50 activations that scroll,
 * then counts the IDCMPUPDATE messages waiting on the window's port
 * without removing them. Compare two library builds.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit listscrollmsgs.c -o listscrollmsgs -lamiga
 */
#include <stdio.h>
#include <stdarg.h>
#include <exec/execbase.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static Object *new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;

    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject new_object

static const char *entries[] = { "One", "Two", "Three", "Four", "Five",
    "Six", "Seven", "Eight", "Nine", "Ten", NULL };

static int pending_updates(struct MsgPort *port, int *all)
{
    struct Node *n;
    int count = 0;

    *all = 0;
    Forbid();
    for (n = port->mp_MsgList.lh_Head; n->ln_Succ; n = n->ln_Succ)
    {
        ++*all;
        if (((struct IntuiMessage *)n)->Class == IDCMP_IDCMPUPDATE)
            count++;
    }
    Permit();
    return count;
}

int main(void)
{
    Object *app, *win, *list;
    struct Window *w = NULL;
    ULONG open = 0, first = 0;
    int i, all, updates, rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        SubWindow, win = WindowObject,
            MUIA_Window_Height, 150,
            WindowContents, VGroup,
                Child, ListviewObject,
                    MUIA_Listview_List, list = ListObject,
                        InputListFrame, End,
                    End,
                End,
            End,
        End;
    if (!app)
        goto close;
    for (i = 0; i < 20; i++)
        DoMethod(list, MUIM_List_Insert, entries, -1, MUIV_List_Insert_Bottom);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &w);
    if (!open || !w)
        goto dispose;
    DoMethod(app, MUIM_Application_InputBuffered);
    for (i = 0; i < 50; i++)
        set(list, MUIA_List_Active, (i * 7) % 200);
    get(list, MUIA_List_First, &first);
    updates = pending_updates(w->UserPort, &all);
    printf("after 50 activations: first=%lu queued=%d idcmpupdate=%d\n",
        (unsigned long)first, all, updates);
    rc = updates ? 10 : 0;
dispose:
    MUI_DisposeObject(app);
close:
    CloseLibrary(MUIMasterBase);
    printf(rc ? "FAIL rc=%d\n" : "PASS rc=%d\n", rc);
    return rc;
}

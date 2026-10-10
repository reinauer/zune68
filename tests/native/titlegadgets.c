/* Which windows get MUI's Settings/Iconify title bar gadgets: lists the
 * system and non-system gadgets in the title bar of windows opened with
 * and without MUIA_Window_ID, with NoMenus, and fixed size.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit titlegadgets.c -o titlegadgets -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void list(const char *name, Object *win)
{
    struct Window *w = NULL;
    struct Gadget *g;
    int n = 0;

    get(win, MUIA_Window_Window, &w);
    printf("%-10s", name);
    if (w)
        printf(" %3dx%-3d flags %08lx", w->Width, w->Height, w->Flags);
    if (!w)
    {
        printf(" not open\n");
        return;
    }
    for (g = w->FirstGadget; g; g = g->NextGadget)
        if (g->TopEdge < w->BorderTop && (g->Flags & GFLG_RELRIGHT))
        {
            printf(" %s%d@%d", (g->GadgetType & GTYP_SYSGADGET) ? "sys"
                : "app", g->Width, -g->LeftEdge);
            n++;
        }
    printf(" (%d)\n", n);
}

static Object *window(const char *title, ULONG id, BOOL nomenus, BOOL fixed,
    BOOL nosize)
{
    return WindowObject,
        MUIA_Window_Title, title,
        MUIA_Window_SizeGadget, !nosize,
        id ? MUIA_Window_ID : TAG_IGNORE, id,
        MUIA_Window_NoMenus, nomenus,
        WindowContents, fixed ? (Object *)TextObject,
            MUIA_Text_Contents, "fixed size", End
            : (Object *)ListviewObject, MUIA_Listview_List, ListObject,
                End, End,
        End;
}

int main(void)
{
    Object *app, *w[7];
    static const char *names[] = { "plain", "id", "nomenus", "fixed",
        "fixed id", "nosize", "fixed ns" };
    int rc = 20, i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "titlegadgets",
        SubWindow, w[0] = window("plain", 0, FALSE, FALSE, FALSE),
        SubWindow, w[1] = window("id", MAKE_ID('T','G','I','D'), FALSE, FALSE,
            FALSE),
        SubWindow, w[2] = window("nomenus", 0, TRUE, FALSE, FALSE),
        SubWindow, w[3] = window("fixed", 0, FALSE, TRUE, FALSE),
        SubWindow, w[4] = window("fixed id", MAKE_ID('T','G','F','X'), FALSE,
            TRUE, FALSE),
        SubWindow, w[5] = window("nosize", 0, FALSE, FALSE, TRUE),
        SubWindow, w[6] = window("fixed ns", 0, FALSE, TRUE, TRUE),
        End;
    if (!app)
        goto done;
    for (i = 0; i < 7; i++)
    {
        set(w[i], MUIA_Window_Open, TRUE);
        list(names[i], w[i]);
        set(w[i], MUIA_Window_Open, FALSE);
    }
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

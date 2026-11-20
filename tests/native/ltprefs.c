/* A Listtree.mcc with a few open and closed nodes, drawn with the settings
 * of the Listtree page (Listtree.mcp): run it, change the page in the
 * settings program (Save or Use), run it again and compare. Quit: close
 * the window, Ctrl-C, or after SECONDS when given ("ltprefs 10").
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -idirafter <SDK>/include \
 *        -Wl,-u,___stkinit ltprefs.c -o ltprefs -lamiga
 */
#include <stdlib.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <mui/Listtree_mcc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define ROOT MUIV_Listtree_Insert_ListNode_Root
#define TAIL MUIV_Listtree_Insert_PrevNode_Tail

static APTR add(Object *tree, const char *name, APTR parent, ULONG flags)
{
    return (APTR)DoMethod(tree, MUIM_Listtree_Insert, (IPTR)name, 0,
        (IPTR)parent, TAIL, flags);
}

int main(int argc, char **argv)
{
    Object *app, *win, *tree;
    ULONG sigs = 0, ticks = argc > 1 ? atol(argv[1]) * 50 : 0;
    APTR comp, sys, de;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"LTPrefs",
        MUIA_Application_Base, (IPTR)"LTPREFS",
        SubWindow, (IPTR)(win = WindowObject,
            MUIA_Window_Title, (IPTR)"Listtree settings",
            MUIA_Window_ID, MAKE_ID('L','T','P','R'),
            WindowContents, (IPTR)(VGroup,
                Child, (IPTR)(ListviewObject,
                    MUIA_Listview_List, (IPTR)(tree = MUI_NewObject(MUIC_Listtree,
                        InputListFrame, TAG_DONE)),
                    End),
                End),
            End),
        End;
    if (!app || !tree)
    {
        Printf("no %s\n", (IPTR)(tree ? "application" : "Listtree.mcc"));
        if (app)
            MUI_DisposeObject(app);
        CloseLibrary(MUIMasterBase);
        return 10;
    }
    comp = add(tree, "comp", ROOT, TNF_LIST | TNF_OPEN);
    sys = add(tree, "sys", comp, TNF_LIST | TNF_OPEN);
    add(tree, "amiga", sys, 0);
    add(tree, "mac", sys, 0);
    add(tree, "lang", add(tree, "lang", comp, TNF_LIST), 0);
    de = add(tree, "de", ROOT, TNF_LIST | TNF_OPEN);
    add(tree, "comm", de, 0);
    add(tree, "comp", add(tree, "rec", de, TNF_LIST), 0);
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    set(win, MUIA_Window_Open, TRUE);
    while (DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs)
        != MUIV_Application_ReturnID_Quit)
    {
        if (ticks)
        {
            Delay(1);
            if (!--ticks || (SetSignal(0, 0) & SIGBREAKF_CTRL_C))
                break;
        }
        else if (sigs && (Wait(sigs | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C))
            break;
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

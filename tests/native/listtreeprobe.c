/* Listtree.mcc behavior: inserts, lookups, rename, find, active node,
 * open/close and removal, printed so native and MUI 3.9 runs can be
 * diffed. "listtreeprobe hold" keeps the window open for screenshots.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -idirafter <SDK>/include \
 *        -Wl,-u,___stkinit listtreeprobe.c -o listtreeprobe -lamiga
 */
#include <stdio.h>
#include <string.h>
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
static const char *name(APTR p)
{
    struct MUIS_Listtree_TreeNode *tn = p;
    return tn ? (const char *)tn->tn_Name : "(null)";
}
static void show(const char *what, APTR p)
{
    struct MUIS_Listtree_TreeNode *tn = p;
    if (tn)
        printf("%-28s %s flags=%x user=%ld\n", what, tn->tn_Name,
            tn->tn_Flags & (TNF_LIST | TNF_OPEN), (LONG)tn->tn_User);
    else
        printf("%-28s (null)\n", what);
    fflush(stdout);
}
static void entries(Object *lt, const char *what)
{
    LONG i;
    printf("%-28s", what);
    for (i = 0; i < 12; i++) {
        APTR tn = (APTR)DoMethod(lt, MUIM_Listtree_GetEntry,
            MUIV_Listtree_GetEntry_ListNode_Root, i,
            MUIV_Listtree_GetEntry_Flags_Visible);
        if (!tn) break;
        printf(" %s", name(tn));
    }
    printf("\n");
    fflush(stdout);
}
int main(int argc, char **argv)
{
    Object *app, *win, *lt;
    APTR a, a1, a2, b, c, c1, tn;
    ULONG v = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "listtreeprobe",
        SubWindow, win = WindowObject, MUIA_Window_Title, "listtreeprobe",
            MUIA_Window_Width, 240, MUIA_Window_Height, 160,
            WindowContents, VGroup,
                Child, ListviewObject, MUIA_Listview_List,
                    lt = ListtreeObject, InputListFrame, End,
                End,
            End,
        End,
    End;
    if (!app) { printf("no application (Listtree.mcc missing?)\n"); return 20; }
    a = (APTR)DoMethod(lt, MUIM_Listtree_Insert, "A", 1, ROOT, TAIL, TNF_LIST | TNF_OPEN);
    a1 = (APTR)DoMethod(lt, MUIM_Listtree_Insert, "A1", 2, a, TAIL, 0);
    a2 = (APTR)DoMethod(lt, MUIM_Listtree_Insert, "A2", 3, a, TAIL, 0);
    b = (APTR)DoMethod(lt, MUIM_Listtree_Insert, "B", 4, ROOT, TAIL, 0);
    c = (APTR)DoMethod(lt, MUIM_Listtree_Insert, "C", 5, ROOT, TAIL, TNF_LIST);
    c1 = (APTR)DoMethod(lt, MUIM_Listtree_Insert, "C1", 6, c, TAIL, 0);
    show("insert A", a); show("insert A1", a1); show("insert A2", a2);
    show("insert B", b); show("insert C", c); show("insert C1", c1);
    entries(lt, "visible");
    show("entry root head", (APTR)DoMethod(lt, MUIM_Listtree_GetEntry,
        MUIV_Listtree_GetEntry_ListNode_Root, MUIV_Listtree_GetEntry_Position_Head, 0));
    show("entry root tail", (APTR)DoMethod(lt, MUIM_Listtree_GetEntry,
        MUIV_Listtree_GetEntry_ListNode_Root, MUIV_Listtree_GetEntry_Position_Tail, 0));
    show("entry A head", (APTR)DoMethod(lt, MUIM_Listtree_GetEntry,
        a, MUIV_Listtree_GetEntry_Position_Head, 0));
    show("entry A1 next", (APTR)DoMethod(lt, MUIM_Listtree_GetEntry,
        a1, MUIV_Listtree_GetEntry_Position_Next, 0));
    show("entry A2 parent", (APTR)DoMethod(lt, MUIM_Listtree_GetEntry,
        a2, MUIV_Listtree_GetEntry_Position_Parent, 0));
    show("entry C1 parent", (APTR)DoMethod(lt, MUIM_Listtree_GetEntry,
        c1, MUIV_Listtree_GetEntry_Position_Parent, 0));
    printf("%-28s %ld %ld %ld\n", "nr A2/B/C", (LONG)DoMethod(lt, MUIM_Listtree_GetNr, a2, 0),
        (LONG)DoMethod(lt, MUIM_Listtree_GetNr, b, 0), (LONG)DoMethod(lt, MUIM_Listtree_GetNr, c, 0));
    printf("%-28s %ld %ld\n", "count all/list",
        (LONG)DoMethod(lt, MUIM_Listtree_GetNr, NULL, MUIV_Listtree_GetNr_Flags_CountAll),
        (LONG)DoMethod(lt, MUIM_Listtree_GetNr, NULL, MUIV_Listtree_GetNr_Flags_CountList));
    show("find A2", (APTR)DoMethod(lt, MUIM_Listtree_FindName,
        MUIV_Listtree_FindName_ListNode_Root, "A2", 0));
    show("find A2 same level", (APTR)DoMethod(lt, MUIM_Listtree_FindName,
        MUIV_Listtree_FindName_ListNode_Root, "A2", MUIV_Listtree_FindName_Flags_SameLevel));
    show("find C1 in C", (APTR)DoMethod(lt, MUIM_Listtree_FindName, c, "C1", 0));
    show("rename A2", (APTR)DoMethod(lt, MUIM_Listtree_Rename, a2, "A2x", 0));
    set(lt, MUIA_Listtree_Active, b);
    get(lt, MUIA_Listtree_Active, &v);
    show("active", (APTR)v);
    get(lt, MUIA_List_Active, &v);
    printf("%-28s %ld\n", "list active", (LONG)v);
    DoMethod(lt, MUIM_Listtree_Open, MUIV_Listtree_Open_ListNode_Root, c, 0);
    entries(lt, "after open C");
    DoMethod(lt, MUIM_Listtree_Close, MUIV_Listtree_Close_ListNode_Root, a, 0);
    entries(lt, "after close A");
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &v);
    printf("%-28s %ld\n", "window open", (LONG)v);
    fflush(stdout);
    if (argc > 1 && !strcmp(argv[1], "hold")) Delay(400);
    DoMethod(lt, MUIM_Listtree_Remove, a, a1, 0);
    entries(lt, "after remove A1");
    DoMethod(lt, MUIM_Listtree_Open, MUIV_Listtree_Open_ListNode_Root, a, 0);
    entries(lt, "after open A");
    DoMethod(lt, MUIM_Listtree_Remove, MUIV_Listtree_Remove_ListNode_Root,
        MUIV_Listtree_Remove_TreeNode_All, 0);
    tn = (APTR)DoMethod(lt, MUIM_Listtree_GetEntry, MUIV_Listtree_GetEntry_ListNode_Root,
        MUIV_Listtree_GetEntry_Position_Head, 0);
    show("after remove all", tn);
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}

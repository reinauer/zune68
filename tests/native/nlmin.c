/* Minimum sizes of NList, NListtree (with and without a title) and
 * their NListviews in a window. Run with LIBS: as YAM's (the same
 * NList classes on both systems). Writes nlmin.txt.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit nlmin.c -o nlmin -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define MUIA_NListview_NList 0x9d510020
#define MUIA_NList_Title     0x9d510011

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void show(const char *n, Object *o)
{
    printf("%-14s min %d,%d def %d,%d max %d,%d sub %d,%d box %dx%d\n", n,
        _minwidth(o), _minheight(o), _defwidth(o), _defheight(o),
        _maxwidth(o), _maxheight(o), _subwidth(o), _subheight(o),
        _width(o), _height(o));
}

int main(void)
{
    static const char *cls[] = { "NList.mcc", "NListtree.mcc", "NList.mcc" };
    static const char *names[] = { "nlist", "nlisttree", "nlist-title" };
    int i;

    freopen("nlmin.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (i = 0; i < 3; i++)
    {
        Object *app, *win, *lv, *l;
        ULONG open = FALSE;

        l = MUI_NewObject(cls[i], MUIA_Frame, MUIV_Frame_InputList,
            i == 2 ? MUIA_NList_Title : TAG_IGNORE, (ULONG)"Title", TAG_DONE);
        if (!l) { printf("%s: no object\n", names[i]); continue; }
        lv = MUI_NewObject("NListview.mcc", MUIA_NListview_NList, (ULONG)l,
            TAG_DONE);
        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, VGroup, Child, lv, End, End, End;
        if (!app) { printf("%s: no app\n", names[i]); continue; }
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        if (open)
        {
            printf("%s\n", names[i]);
            show("  listview", lv);
            show("  list", l);
        }
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}

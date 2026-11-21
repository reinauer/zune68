/* What a group's layout hook finds in lm_MinMax at MUILM_MINMAX before it
 * fills anything, for a plain group, a framed group, a group with children
 * and a virtual group. The hook fills nothing and lays out nothing.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit layoutmsg.c -o layoutmsg -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static struct MUI_MinMax seen[4];
static int count[4];

static ULONG hookfunc(struct Hook *h __asm("a0"), Object *obj __asm("a2"),
    struct MUI_LayoutMsg *lm __asm("a1"))
{
    int i = (int)h->h_Data;

    if (lm->lm_Type == MUILM_MINMAX && !count[i]++)
        seen[i] = lm->lm_MinMax;
    return lm->lm_Type == MUILM_LAYOUT;
}

static struct Hook hooks[4] = {
    {{0}, (HOOKFUNC)hookfunc, 0, (APTR)0}, {{0}, (HOOKFUNC)hookfunc, 0, (APTR)1},
    {{0}, (HOOKFUNC)hookfunc, 0, (APTR)2}, {{0}, (HOOKFUNC)hookfunc, 0, (APTR)3}};
static const char *names[4] = {"plain", "framed", "children", "virtual"};

int main(void)
{
    Object *app, *win;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, VGroup,
            Child, VGroup, MUIA_Group_LayoutHook, &hooks[0], End,
            Child, VGroup, GroupFrame, MUIA_Group_LayoutHook, &hooks[1], End,
            Child, VGroup, MUIA_Group_LayoutHook, &hooks[2],
                Child, TextObject, MUIA_Text_Contents, "text", End,
                Child, SimpleButton("button"), End,
            Child, VirtgroupObject, MUIA_Group_LayoutHook, &hooks[3], End,
        End, End, End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    for (i = 0; i < 4; i++)
        printf("%-8s calls=%s min=%dx%d def=%dx%d max=%dx%d\n", names[i],
            count[i] ? "yes" : "no", seen[i].MinWidth, seen[i].MinHeight,
            seen[i].DefWidth, seen[i].DefHeight, seen[i].MaxWidth,
            seen[i].MaxHeight);
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

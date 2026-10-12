/* Form elements measured the way Voyager 3.2 does: a temporary object gets
 * the parent's global info, MUIM_ConnectParent-style setup, MUIM_Setup and
 * MUIM_AskMinMax with a NULL MinMaxInfo; then its private min/max is read.
 * Writes vmeasure.txt.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit vmeasure.c -o vmeasure -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#ifndef MUIM_GlobalSetup
#define MUIM_GlobalSetup 0x8042d532
#endif
#ifndef _parent
#define _parent(obj) (*(Object **)&muiNotifyData(obj)->priv2)
#endif
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static const char *entries[] = { "First", "Second", NULL };

static void children(const char *name, Object *obj, int depth)
{
    struct List *list = NULL;
    Object *child;
    APTR state;
    get(obj, MUIA_Group_ChildList, &list);
    if (!list) return;
    state = list->lh_Head;
    while ((child = NextObject(&state)))
    {
        printf("%-24s %*schild min %2d,%2d def %2d,%2d max %5d,%5d add %d,%d sub %d,%d font %d\n", name,
            depth * 2, "", _minwidth(child), _minheight(child), _defwidth(child), _defheight(child),
            _maxwidth(child), _maxheight(child), _addleft(child), _addtop(child),
            _subwidth(child), _subheight(child), _font(child) ? _font(child)->tf_YSize : -1);
        if (depth < 2) children(name, child, depth + 1);
    }
}

static void measure(const char *name, Object *root, Object *obj)
{
    if (!obj) { printf("%-24s none\n", name); return; }
    muiGlobalInfo(obj) = muiGlobalInfo(root);
    DoMethod(obj, MUIM_GlobalSetup, (ULONG)muiGlobalInfo(root));
    _parent(obj) = root;
    muiRenderInfo(obj) = muiRenderInfo(root);
    DoMethod(obj, MUIM_Setup, (ULONG)muiRenderInfo(root));
    DoMethod(obj, MUIM_AskMinMax, (ULONG)NULL);
    printf("%-24s min %2d,%2d def %2d,%2d max %5d,%5d font %d add %d,%d sub %d,%d\n", name,
        _minwidth(obj), _minheight(obj), _defwidth(obj), _defheight(obj),
        _maxwidth(obj), _maxheight(obj), _font(obj) ? _font(obj)->tf_YSize : -1,
        _addleft(obj), _addtop(obj), _subwidth(obj), _subheight(obj));
    if (name[0] == 'c' && name[1] == 'y')
        children(name, obj, 1);
    DoMethod(obj, MUIM_Cleanup);
    MUI_DisposeObject(obj);
}

int main(void)
{
    Object *app, *win, *root;
    ULONG open = 0;
    freopen("vmeasure.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, root = VGroup, Child, RectangleObject, End, End, End, End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open) return 20;
    measure("check-voyager-big", root, ImageObject, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Image_FreeVert, TRUE,
        MUIA_Image_FreeHoriz, TRUE, MUIA_CycleChain, 1, MUIA_Font, MUIV_Font_Big, End);
    measure("check-voyager-normal", root, ImageObject, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Image_FreeVert, TRUE,
        MUIA_Image_FreeHoriz, TRUE, MUIA_CycleChain, 1, End);
    measure("check-voyager-framed", root, ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Image_FreeVert, TRUE,
        MUIA_Image_FreeHoriz, TRUE, MUIA_CycleChain, 1, MUIA_Font, MUIV_Font_Big, End);
    measure("check-toggle-font-big", root, ImageObject, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Font, MUIV_Font_Big, End);
    measure("check-font-big", root, ImageObject, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_Font, MUIV_Font_Big, End);
    measure("check-toggle-font-normal", root, ImageObject, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Font, MUIV_Font_Normal, End);
    measure("check-toggle-font-tiny", root, ImageObject, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Font, MUIV_Font_Tiny, End);
    measure("check-toggle-only", root, ImageObject, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_InputMode, MUIV_InputMode_Toggle, End);
    measure("check-plain", root, ImageObject, MUIA_Image_Spec, MUII_CheckMark, End);
    measure("makeobj-check", root, MUI_MakeObject(MUIO_Checkmark, NULL));
    measure("cycle-big", root, CycleObject, MUIA_Cycle_Entries, entries, MUIA_Font, MUIV_Font_Big, End);
    measure("cycle", root, CycleObject, MUIA_Cycle_Entries, entries, End);
    {
        Object *cy = CycleObject, MUIA_Cycle_Entries, entries, End;
        DoMethod(root, MUIM_Group_InitChange);
        DoMethod(root, OM_ADDMEMBER, cy);
        DoMethod(root, MUIM_Group_ExitChange);
        set(win, MUIA_Window_Open, FALSE);
        set(win, MUIA_Window_Open, TRUE);
        printf("%-24s min %2d,%2d def %2d,%2d max %5d,%5d add %d,%d sub %d,%d\n", "cycle-in-window",
            _minwidth(cy), _minheight(cy), _defwidth(cy), _defheight(cy), _maxwidth(cy), _maxheight(cy),
            _addleft(cy), _addtop(cy), _subwidth(cy), _subheight(cy));
        children("cycle-in-window", cy, 1);
    }
    measure("string", root, StringObject, StringFrame, MUIA_String_Contents, "Edit me", End);
    measure("text", root, TextObject, MUIA_Text_Contents, "Check box", MUIA_Font, MUIV_Font_Big, End);
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

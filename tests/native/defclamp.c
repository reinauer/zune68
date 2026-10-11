/* A subclass that raises only MinWidth/MinHeight (or lowers MaxWidth) in
 * AskMinMax: does MUI move the default into [min, max]? Writes
 * defclamp.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit defclamp.c -o defclamp -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static int mode;

static ULONG Dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"),
    Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_AskMinMax)
    {
        struct MUI_MinMax *mm;

        DoSuperMethodA(cl, obj, msg);
        mm = ((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        if (mode == 0) { mm->MinWidth += 40; mm->MinHeight += 20; }
        if (mode == 1) { mm->MinWidth += 40; mm->MinHeight += 20; mm->MaxWidth = 50; mm->MaxHeight = 30; }
        if (mode == 2) { mm->DefWidth = 500; mm->DefHeight = 300; mm->MaxWidth = 100; mm->MaxHeight = 60; }
        if (mode == 3) { mm->MinWidth = 80; mm->MaxWidth = 20; }
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(void)
{
    struct MUI_CustomClass *mcc;

    freopen("defclamp.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 4, (APTR)Dispatch);
    for (mode = 0; mcc && mode < 4; mode++)
    {
        Object *app, *win, *o;
        ULONG open = FALSE;

        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, VGroup,
                Child, o = NewObject(mcc->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_String, TAG_DONE),
                End, End, End;
        if (!app)
            return 20;
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        printf("mode %d open %lu min %d,%d def %d,%d max %d,%d box %dx%d\n", mode, open,
            _minwidth(o), _minheight(o), _defwidth(o), _defheight(o),
            _maxwidth(o), _maxheight(o), _width(o), _height(o));
        set(win, MUIA_Window_Open, FALSE);
        MUI_DisposeObject(app);
    }
    if (mcc) MUI_DeleteCustomClass(mcc);
    CloseLibrary(MUIMasterBase);
    return 0;
}

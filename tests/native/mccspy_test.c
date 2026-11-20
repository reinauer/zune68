/*
 * mccspy_test: a small MUI program with custom classes, for mccspy.
 *
 * Box is an Area subclass with AskMinMax and Draw, Box2 a subclass of Box,
 * Pane a Group subclass holding one of each. The program opens a window
 * with them, calls MUI_Redraw and a method of its own on the boxes,
 * closes the window, disposes the application and deletes the classes.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s -Ibuild/muimaster/include \
 *        tests/native/mccspy_test.c -o mccspy_test -lamiga
 */
#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

#define MUIM_Box_Poke 0xF0010001UL

struct Library *MUIMasterBase;
ULONG __stack = 32768;

static IPTR box_dispatch(struct IClass *cl __asm("a0"),
                         Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    switch (msg->MethodID) {
    case MUIM_AskMinMax: {
        struct MUI_MinMax *mm;
        /* MUI 3.8 sends MinMaxInfo NULL; Area's AskMinMax sets it. */
        DoSuperMethodA(cl, obj, msg);
        mm = ((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        mm->MinWidth += 20;
        mm->DefWidth += 40;
        mm->MaxWidth += 100;
        mm->MinHeight += 10;
        mm->DefHeight += 20;
        mm->MaxHeight += 100;
        return 0;
    }
    case MUIM_Draw:
        DoSuperMethodA(cl, obj, msg);   /* MUI 3.8 and Zune68 send flags 0 */
        SetAPen(_rp(obj), _dri(obj)->dri_Pens[SHINEPEN]);
        RectFill(_rp(obj), _mleft(obj), _mtop(obj),
            _mright(obj), _mbottom(obj));
        return 0;
    case MUIM_Box_Poke:
        return 42;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static IPTR box2_dispatch(struct IClass *cl __asm("a0"),
                          Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_Box_Poke)
        return DoSuperMethodA(cl, obj, msg) + 1;
    return DoSuperMethodA(cl, obj, msg);
}

static IPTR pane_dispatch(struct IClass *cl __asm("a0"),
                          Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    return DoSuperMethodA(cl, obj, msg);
}

int main(void)
{
    struct MUI_CustomClass *box, *box2, *pane;
    Object *app, *win, *b1, *b2;
    ULONG open = 0, r1, r2;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    box = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 0, box_dispatch);
    box2 = box ? MUI_CreateCustomClass(NULL, NULL, box, 0, box2_dispatch)
        : NULL;
    pane = MUI_CreateCustomClass(NULL, MUIC_Group, NULL, 0, pane_dispatch);
    if (!box || !box2 || !pane)
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "mccspy_test",
        MUIA_Application_Base, "MCCSPYTEST",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "mccspy test",
            WindowContents, NewObject(pane->mcc_Class, NULL,
                MUIA_Group_Horiz, TRUE,
                Child, b1 = NewObject(box->mcc_Class, NULL,
                    MUIA_Frame, MUIV_Frame_Text, TAG_DONE),
                Child, b2 = NewObject(box2->mcc_Class, NULL,
                    MUIA_Frame, MUIV_Frame_Text, TAG_DONE),
                TAG_DONE),
            End,
        End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
        MUI_Redraw(b1, MADF_DRAWOBJECT);
    r1 = DoMethod(b1, MUIM_Box_Poke);
    r2 = DoMethod(b2, MUIM_Box_Poke);
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    MUI_DeleteCustomClass(pane);
    MUI_DeleteCustomClass(box2);
    MUI_DeleteCustomClass(box);
    printf("open=%lu poke=%lu,%lu\n", open, r1, r2);
    CloseLibrary(MUIMasterBase);
    return 0;
}

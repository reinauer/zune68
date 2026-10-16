/* What does a window resize repaint behind objects that do not paint
 * their own background? IBrowse's toolbar group has a custom backfill
 * that leaves its whole box to the window background; after its window
 * shrinks and grows again, MUI 3.8 and 3.9 show clean background there.
 * Three areas at the left of the window, which neither move nor change
 * size while the window shrinks by 60 pixels and grows back:
 *   nodraw  - an object whose MUIM_Draw paints nothing at all
 *   nofill  - an object with MUIA_FillArea FALSE
 *   bargap  - the gap right of the buttons in a group whose custom
 *             backfill does nothing for its whole box (IBrowse)
 * The probe scribbles into each, resizes from inside the application,
 * lets MUI handle the events, and counts scribbled pixels left over.
 * MUI 3.8 (smart refresh) clears all three; MUI 3.9 (simple refresh)
 * keeps all three. Zune68 clears smart refresh windows like MUI 3.8 and
 * leaves simple refresh windows like MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit resizeclear.c -o resizeclear -lamiga
 */
#include <stdio.h>
#include <stdarg.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define MUIA_CustomBackfill 0x80420a63
#define MUIM_CustomBackfill 0x80428d73
struct CustomBackfillMsg { ULONG MethodID; LONG left, top, right, bottom,
    xoffset, yoffset; };

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

/* fixed 40x20 box; with user data 1 MUIM_Draw paints nothing */
static ULONG box_dispatch(register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"), register Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_AskMinMax)
    {
        struct MUI_MinMax *mm;

        DoSuperMethodA(cl, obj, msg);
        mm = ((struct MUIP_AskMinMax *) msg)->MinMaxInfo;
        mm->MinWidth += 40;
        mm->DefWidth += 40;
        mm->MaxWidth += 40;
        mm->MinHeight += 20;
        mm->DefHeight += 20;
        mm->MaxHeight += 20;
        return 0;
    }
    if (msg->MethodID == MUIM_Draw && muiUserData(obj) == 1)
        return 0;
    return DoSuperMethodA(cl, obj, msg);
}

/* IBrowse's toolbar: as wide as it may get, leaves its whole box to the
 * window background and passes smaller requests on */
static ULONG bar_dispatch(register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"), register Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_AskMinMax)
    {
        DoSuperMethodA(cl, obj, msg);
        ((struct MUIP_AskMinMax *) msg)->MinMaxInfo->MaxWidth = MUI_MAXMAX;
        return 0;
    }
    if (msg->MethodID == MUIM_CustomBackfill)
    {
        struct CustomBackfillMsg *m = (struct CustomBackfillMsg *) msg;

        if (m->left <= _mleft(obj) && m->top <= _mtop(obj)
            && m->right >= _mright(obj) && m->bottom >= _mbottom(obj))
            return TRUE;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void pump(Object *app, int ticks)
{
    while (ticks-- > 0)
    {
        DoMethod(app, MUIM_Application_InputBuffered);
        Delay(1);
    }
}

struct Spot { const char *name; WORD l, t, r, b; LONG before; };

static LONG count_pen(struct RastPort *rp, struct Spot *s, LONG pen)
{
    LONG n = 0;
    WORD x, y;

    for (y = s->t; y <= s->b; y++)
        for (x = s->l; x <= s->r; x++)
            n += ReadPixel(rp, x, y) == pen;
    return n;
}

int main(void)
{
    struct MUI_CustomClass *boxcc, *barcc;
    Object *app, *win, *nodraw, *nofill, *bar, *b;
    struct Window *w = NULL;
    struct Spot spots[3];
    ULONG open = 0;
    int rc = 20, i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    boxcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 4,
        (APTR) box_dispatch);
    barcc = MUI_CreateCustomClass(NULL, MUIC_Group, NULL, 4,
        (APTR) bar_dispatch);
    if (!boxcc || !barcc)
        goto close;
    app = ApplicationObject,
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "resizeclear",
            MUIA_Window_LeftEdge, 40,
            MUIA_Window_TopEdge, 40,
            MUIA_Window_Width, 300,
            WindowContents, VGroup,
                Child, HGroup,
                    Child, nodraw = NewObject(boxcc->mcc_Class, NULL,
                        MUIA_UserData, 1, TAG_DONE),
                    Child, nofill = NewObject(boxcc->mcc_Class, NULL,
                        MUIA_FillArea, FALSE, TAG_DONE),
                    Child, HSpace(0),
                    End,
                Child, bar = NewObject(barcc->mcc_Class, NULL,
                    MUIA_Group_Horiz, TRUE, MUIA_CustomBackfill, TRUE,
                    MUIA_Group_Child, TextObject, ButtonFrame,
                        MUIA_FixWidth, 30, MUIA_Text_Contents, "A", End,
                    MUIA_Group_Child, b = TextObject, ButtonFrame,
                        MUIA_FixWidth, 30, MUIA_Text_Contents, "B", End,
                    TAG_DONE),
                Child, SimpleButton("Below"),
                End,
            End,
        End;
    if (!app)
        goto class;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &w);
    if (!open || !w)
        goto dispose;
    pump(app, 25);
    printf("library %ld.%ld refresh %s, window %ldx%ld\n",
        (long) MUIMasterBase->lib_Version, (long) MUIMasterBase->lib_Revision,
        (w->Flags & WFLG_REFRESHBITS) == WFLG_SMART_REFRESH ? "smart" :
        "simple", (long) w->Width, (long) w->Height);

    spots[0].name = "nodraw";
    spots[0].l = _left(nodraw); spots[0].t = _top(nodraw);
    spots[0].r = _right(nodraw); spots[0].b = _bottom(nodraw);
    spots[1].name = "nofill";
    spots[1].l = _left(nofill); spots[1].t = _top(nofill);
    spots[1].r = _right(nofill); spots[1].b = _bottom(nofill);
    spots[2].name = "bargap";
    spots[2].l = _right(b) + 4; spots[2].t = _mtop(bar);
    spots[2].r = _right(b) + 40; spots[2].b = _mbottom(bar);
    printf("toolbar %ld..%ld, buttons end at %ld\n", (long) _mleft(bar),
        (long) _mright(bar), (long) _right(b));

    SetAPen(w->RPort, 3);
    for (i = 0; i < 3; i++)
    {
        RectFill(w->RPort, spots[i].l, spots[i].t, spots[i].r, spots[i].b);
        spots[i].before = count_pen(w->RPort, &spots[i], 3);
    }
    ChangeWindowBox(w, w->LeftEdge, w->TopEdge, w->Width - 60, w->Height);
    pump(app, 50);
    ChangeWindowBox(w, w->LeftEdge, w->TopEdge, w->Width + 60, w->Height);
    pump(app, 50);
    for (i = 0; i < 3; i++)
        printf("%-7s %ld scribbled pixels, %ld left after shrink and grow\n",
            spots[i].name, (long) spots[i].before,
            (long) count_pen(w->RPort, &spots[i], 3));
    rc = 0;
dispose:
    MUI_DisposeObject(app);
class:
    MUI_DeleteCustomClass(barcc);
    MUI_DeleteCustomClass(boxcc);
close:
    CloseLibrary(MUIMasterBase);
    printf(rc ? "FAIL rc=%d\n" : "PASS rc=%d\n", rc);
    return rc;
}

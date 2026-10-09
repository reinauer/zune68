/* When does a MUIA_Disabled change redraw a custom subclass?
 * The subclass forwards OM_SET to its superclass first and only then
 * updates the state its Draw method uses. If the library redraws inside
 * the superclass OM_SET, the stale appearance remains on screen.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit disabletiming.c -o disabletiming -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;

static int look_disabled;   /* state used by Draw, updated after Super */
static int draws_in_super;  /* Draw calls made while OM_SET's Super runs */
static int in_super;

static ULONG dispatcher(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    switch (msg->MethodID)
    {
    case MUIM_AskMinMax:
    {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;

        DoSuperMethodA(cl, obj, msg);
        m->MinMaxInfo->MinWidth += 24;
        m->MinMaxInfo->DefWidth += 24;
        m->MinMaxInfo->MaxWidth += 24;
        m->MinMaxInfo->MinHeight += 24;
        m->MinMaxInfo->DefHeight += 24;
        m->MinMaxInfo->MaxHeight += 24;
        return 0;
    }
    case MUIM_Draw:
        if (in_super)
            draws_in_super++;
        DoSuperMethodA(cl, obj, msg);
        if (look_disabled)
        {
            SetAPen(_rp(obj), 1);
            RectFill(_rp(obj), _mleft(obj), _mtop(obj), _mright(obj),
                _mbottom(obj));
        }
        return 0;
    case OM_SET:
    {
        struct TagItem *tag = FindTagItem(MUIA_Disabled,
            ((struct opSet *)msg)->ops_AttrList);
        ULONG rc;

        in_super = 1;
        rc = DoSuperMethodA(cl, obj, msg);
        in_super = 0;
        if (tag)
            look_disabled = tag->ti_Data ? 1 : 0;
        return rc;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

static LONG sample(Object *o)
{
    LONG x, y, n = 0;

    for (y = _mtop(o); y <= _mbottom(o); y++)
        for (x = _mleft(o); x <= _mright(o); x++)
            if (ReadPixel(_rp(o), x, y) == 1)
                n++;
    return n;
}

int main(void)
{
    struct MUI_CustomClass *mcc;
    Object *app = NULL, *win, *obj;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 4, (APTR)dispatcher);
    if (!mcc)
        goto done;
    app = ApplicationObject,
        MUIA_Application_Title, "disabletiming",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "disabletiming",
            WindowContents, HGroup,
                Child, obj = NewObject(mcc->mcc_Class, NULL,
                    MUIA_FillArea, TRUE, TAG_DONE),
                End,
            End,
        End;
    if (!app)
        goto done;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);

    set(obj, MUIA_Disabled, TRUE);
    DoMethod(app, MUIM_Application_InputBuffered);
    printf("disable: black=%ld draws-in-super=%d\n", (long)sample(obj),
        draws_in_super);
    draws_in_super = 0;
    set(obj, MUIA_Disabled, FALSE);
    DoMethod(app, MUIM_Application_InputBuffered);
    printf("enable: black=%ld draws-in-super=%d\n", (long)sample(obj),
        draws_in_super);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    if (mcc)
        MUI_DeleteCustomClass(mcc);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

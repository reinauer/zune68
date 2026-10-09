/* Background ownership when a custom Draw skips its superclass.
 * A custom Area subclass first paints black, then redraws without calling
 * DoSuperMethod. The pixel left behind shows whether the redraw path
 * painted the object's background before dispatching MUIM_Draw.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit nosuperdraw.c -o nosuperdraw -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;

static int mode;    /* 1: super + black fill, 2: super only, 0: nothing */

static ULONG dispatcher(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_AskMinMax)
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
    if (msg->MethodID == MUIM_Draw)
    {
        if (mode)
            DoSuperMethodA(cl, obj, msg);
        if (mode == 1)
        {
            SetAPen(_rp(obj), 1);
            RectFill(_rp(obj), _mleft(obj), _mtop(obj), _mright(obj),
                _mbottom(obj));
        }
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Count pen-1 pixels over the object's inner area */
static LONG sample(Object *o)
{
    LONG x, y, n = 0;

    for (y = _mtop(o); y <= _mbottom(o); y++)
        for (x = _mleft(o); x <= _mright(o); x++)
            if (ReadPixel(_rp(o), x, y) == 1)
                n++;
    return n;
}

/* Print a pen histogram of the object's inner area */
static void histogram(const char *name, Object *o)
{
    LONG x, y, counts[16] = { 0 };
    int i;

    for (y = _mtop(o); y <= _mbottom(o); y++)
        for (x = _mleft(o); x <= _mright(o); x++)
            counts[ReadPixel(_rp(o), x, y) & 15]++;
    printf("%s pens:", name);
    for (i = 0; i < 16; i++)
        if (counts[i])
            printf(" %d=%ld", i, (long)counts[i]);
    printf("\n");
}

int main(void)
{
    struct MUI_CustomClass *mcc;
    Object *app, *win, *plain, *disabled, *nofill;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 4, (APTR)dispatcher);
    if (!mcc)
        goto done;
    mode = 1;
    app = ApplicationObject,
        MUIA_Application_Title, "nosuperdraw",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "nosuperdraw",
            WindowContents, HGroup,
                Child, plain = NewObject(mcc->mcc_Class, NULL,
                    MUIA_FillArea, TRUE, TAG_DONE),
                Child, disabled = NewObject(mcc->mcc_Class, NULL,
                    MUIA_FillArea, TRUE, TAG_DONE),
                Child, nofill = NewObject(mcc->mcc_Class, NULL,
                    MUIA_FillArea, FALSE, TAG_DONE),
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
    printf("after paint: plain=%ld disabled=%ld nofill=%ld\n",
        (long)sample(plain), (long)sample(disabled), (long)sample(nofill));

    mode = 0;
    MUI_Redraw(plain, MADF_DRAWOBJECT);
    MUI_Redraw(nofill, MADF_DRAWOBJECT);
    set(disabled, MUIA_Disabled, TRUE);
    printf("after empty redraw: plain=%ld disabled=%ld nofill=%ld\n",
        (long)sample(plain), (long)sample(disabled), (long)sample(nofill));

    /* Re-enabling with a superclass draw must remove the ghost pattern */
    mode = 2;
    set(disabled, MUIA_Disabled, FALSE);
    printf("after enable: disabled=%ld\n", (long)sample(disabled));

    /* Disabled object whose Draw calls only the superclass: count the
     * ghost pattern pixels the built-in drawing leaves behind */
    set(disabled, MUIA_Disabled, TRUE);
    printf("disabled super-only: disabled=%ld\n", (long)sample(disabled));
    histogram("disabled super-only", disabled);
    histogram("enabled plain", plain);
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

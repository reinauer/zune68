/* Which classes draw the disabled ghost pattern.
 * Disables a text button, an image button, a rectangle and custom Area
 * and Text subclasses whose Draw only calls the superclass, then prints
 * a pen histogram of each object's inner area.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit ghostclasses.c -o ghostclasses -lamiga
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
        m->MinMaxInfo->MinHeight += 12;
        m->MinMaxInfo->DefHeight += 12;
        m->MinMaxInfo->MaxHeight += 12;
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

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

#define COUNT 10

int main(void)
{
    static const char *const names[COUNT] = {
        "button", "image", "rectangle", "custom-area", "custom-text",
        "cycle", "group", "string", "checkmark", "slider"
    };
    static const char *entries[] = { "One", "Two", NULL };
    struct MUI_CustomClass *area_mcc, *text_mcc;
    Object *app, *win, *objs[COUNT];
    ULONG open = FALSE;
    int i, rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    area_mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 4,
        (APTR)dispatcher);
    text_mcc = MUI_CreateCustomClass(NULL, MUIC_Text, NULL, 4,
        (APTR)dispatcher);
    if (!area_mcc || !text_mcc)
        goto done;
    app = ApplicationObject,
        MUIA_Application_Title, "ghostclasses",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "ghostclasses",
            WindowContents, HGroup,
                Child, objs[0] = SimpleButton("Ghost"),
                Child, objs[1] = ImageObject, ImageButtonFrame,
                    MUIA_Image_Spec, MUII_ArrowUp, End,
                Child, objs[2] = RectangleObject, TextFrame,
                    MUIA_FixWidth, 24, End,
                Child, objs[3] = NewObject(area_mcc->mcc_Class, NULL,
                    MUIA_FillArea, TRUE, TAG_DONE),
                Child, objs[4] = NewObject(text_mcc->mcc_Class, NULL,
                    MUIA_Text_Contents, "Text", TAG_DONE),
                Child, VGroup,
                    Child, objs[5] = CycleObject,
                        MUIA_Cycle_Entries, entries, End,
                    Child, objs[6] = HGroup, GroupFrame,
                        Child, TextObject, MUIA_Text_Contents, "In", End,
                        End,
                    Child, objs[7] = StringObject, StringFrame,
                        MUIA_String_Contents, "Str", End,
                    Child, objs[8] = CheckMark(TRUE),
                    Child, objs[9] = SliderObject, End,
                    End,
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
    for (i = 0; i < COUNT; i++)
        histogram(names[i], objs[i]);
    for (i = 0; i < COUNT; i++)
        set(objs[i], MUIA_Disabled, TRUE);
    DoMethod(app, MUIM_Application_InputBuffered);
    for (i = 0; i < COUNT; i++)
    {
        char name[32];

        sprintf(name, "disabled-%s", names[i]);
        histogram(name, objs[i]);
    }
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    if (area_mcc)
        MUI_DeleteCustomClass(area_mcc);
    if (text_mcc)
        MUI_DeleteCustomClass(text_mcc);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

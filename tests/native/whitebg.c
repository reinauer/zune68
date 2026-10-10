/* Widgets on a white parent background, as on an IBrowse page: prints a
 * pen histogram per object, so pixels a widget leaves to its parent show
 * up as the white pen. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit whitebg.c -o whitebg -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void histogram(const char *name, Object *o)
{
    LONG x, y, counts[16] = { 0 };
    int i;

    if (!o)
    {
        printf("%-14s missing\n", name);
        return;
    }
    for (y = _top(o); y <= _bottom(o); y++)
        for (x = _left(o); x <= _right(o); x++)
            counts[ReadPixel(_rp(o), x, y) & 15]++;
    printf("%-14s %3dx%-3d pens:", name, _width(o), _height(o));
    for (i = 0; i < 16; i++)
        if (counts[i])
            printf(" %d=%ld", i, (long)counts[i]);
    printf("\n");
}

/* All pens of the object, one text row per pixel row */
static void dump(const char *name, Object *o)
{
    LONG x, y;

    printf("%s:\n", name);
    for (y = _top(o); y <= _bottom(o); y++)
    {
        for (x = _left(o); x <= _right(o); x++)
            printf("%lx", (long)(ReadPixel(_rp(o), x, y) & 15));
        printf("\n");
    }
}

/* One row of pens across the middle of the object */
static void row(const char *name, Object *o)
{
    LONG x, y;

    if (!o)
        return;
    y = _top(o) + _height(o) / 2;
    printf("%-14s mid:", name);
    for (x = _left(o); x <= _right(o); x++)
        printf("%lx", (long)(ReadPixel(_rp(o), x, y) & 15));
    printf("\n");
}

int main(void)
{
    Object *app, *win, *radio0, *radio1, *check0, *str, *nstr, *bstr;
    Object *rsel, *rbgsel, *rbg, *isel, *nnf, *nvirt, *rvirt, *svirt;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "whitebg",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "whitebg",
            WindowContents, VGroup,
                MUIA_Background, "2:ffffffff,ffffffff,ffffffff",
                Child, HGroup,
                    Child, radio0 = ImageObject,
                        MUIA_Image_Spec, MUII_RadioButton,
                        MUIA_InputMode, MUIV_InputMode_Immediate,
                        MUIA_Selected, FALSE, End,
                    Child, radio1 = ImageObject,
                        MUIA_Image_Spec, MUII_RadioButton,
                        MUIA_InputMode, MUIV_InputMode_Immediate,
                        MUIA_Selected, TRUE, End,
                    Child, check0 = ImageObject,
                        MUIA_Image_Spec, MUII_CheckMark,
                        MUIA_InputMode, MUIV_InputMode_Toggle,
                        MUIA_Selected, FALSE, End,
                    End,
                Child, HGroup,
                    /* selected objects without and with an own background */
                    Child, rsel = RectangleObject, MUIA_FixWidth, 12,
                        MUIA_FixHeight, 8, MUIA_InputMode,
                        MUIV_InputMode_Toggle, MUIA_Selected, TRUE, End,
                    Child, rbgsel = RectangleObject, MUIA_FixWidth, 12,
                        MUIA_FixHeight, 8, MUIA_InputMode,
                        MUIV_InputMode_Toggle, MUIA_Selected, TRUE,
                        MUIA_Background, MUII_ButtonBack, End,
                    Child, rbg = RectangleObject, MUIA_FixWidth, 12,
                        MUIA_FixHeight, 8, MUIA_InputMode,
                        MUIV_InputMode_Toggle,
                        MUIA_Background, MUII_ButtonBack, End,
                    Child, isel = HSpace(0),
                    End,
                Child, str = StringObject, StringFrame,
                    MUIA_String_Contents, "Amiga test", End,
                Child, nstr = MUI_NewObject("Newstring.mcc", StringFrame,
                    MUIA_String_Contents, "Amiga test", TAG_DONE),
                Child, bstr = MUI_NewObject("BetterString.mcc", StringFrame,
                    MUIA_String_Contents, "Amiga test", TAG_DONE),
                Child, nnf = MUI_NewObject("Newstring.mcc",
                    MUIA_String_Contents, "Amiga test", TAG_DONE),
                Child, ScrollgroupObject, MUIA_Scrollgroup_Contents,
                    VirtgroupObject,
                        MUIA_Background, "2:ffffffff,ffffffff,ffffffff",
                        Child, nvirt = MUI_NewObject("Newstring.mcc",
                            StringFrame, MUIA_String_Contents, "Amiga test",
                            TAG_DONE),
                        Child, rvirt = RectangleObject, MUIA_FixWidth, 12,
                            MUIA_FixHeight, 8, MUIA_FillArea, TRUE, End,
                        Child, svirt = StringObject, StringFrame,
                            MUIA_String_Contents, "Amiga test", End,
                        End,
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
    histogram("radio off", radio0);
    histogram("radio on", radio1);
    histogram("checkmark off", check0);
    histogram("rect sel", rsel);
    histogram("rect bg sel", rbgsel);
    histogram("rect bg", rbg);
    histogram("string", str);
    histogram("newstring", nstr);
    histogram("betterstring", bstr);
    histogram("newstr noframe", nnf);
    histogram("newstr virtual", nvirt);
    histogram("rect virtual", rvirt);
    histogram("string virtual", svirt);
    dump("radio off", radio0);
    dump("radio on", radio1);
    row("radio off", radio0);
    row("string", str);
    row("newstring", nstr);
    row("newstr noframe", nnf);
    row("newstr virtual", nvirt);
    dump("string virtual", svirt);
    /* The private background slot (MUI's mad_reserved) */
    printf("bg slot: newstring %08lx virtual %08lx string %08lx\n",
        ((ULONG *)nstr)[8], ((ULONG *)nvirt)[8], ((ULONG *)str)[8]);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

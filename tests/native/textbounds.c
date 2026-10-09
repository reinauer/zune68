/* Text minimum width and styled-text measurement comparison.
 * Opens the MUI-Demo header Text and simpler variants, then reports each
 * object's box, inner width and MinMax against measured string widths.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit textbounds.c -o textbounds -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define HEADER "\33c\0338MUI - \33bM\33nagic\33bU\33nser\33bI\33nnterface\n" \
    "written 1992-95 by Stefan Stuntz"
struct sample
{
    const char *contents;   /* Text contents */
    const char *plain;      /* contents without control sequences */
    ULONG frame;            /* MUIA_Frame */
    ULONG setmin;           /* MUIA_Text_SetMin */
    LONG inner;             /* MUIA_InnerLeft/Right, or -1 for default */
    LONG font;              /* MUIA_Font, or 0 for default */
};
static const struct sample samples[] = {
    { HEADER, "written 1992-95 by Stefan Stuntz", MUIV_Frame_Group, TRUE, -1, 0 },
    { "written 1992-95 by Stefan Stuntz", "written 1992-95 by Stefan Stuntz",
        MUIV_Frame_Text, TRUE, -1, 0 },
    { "\33bBold text width check\33n", "Bold text width check",
        MUIV_Frame_Text, TRUE, -1, 0 },
    { "plain\33bbold\33nplain", "plainboldplain", MUIV_Frame_Text, TRUE, -1, 0 },
    { "\33cCentered single line", "Centered single line",
        MUIV_Frame_Text, TRUE, -1, 0 },
    { "No frame text", "No frame text", MUIV_Frame_None, TRUE, -1, 0 },
    { "Button frame text", "Button frame text", MUIV_Frame_Button, TRUE, -1, 0 },
    { "No SetMin text", "No SetMin text", MUIV_Frame_Text, FALSE, -1, 0 },
    { "a\33bb\33nc\33bd\33ne\33bf\33ng", "abcdefg", MUIV_Frame_None, TRUE, -1, 0 },
    { "\33iItalic text\33n", "Italic text", MUIV_Frame_None, TRUE, -1, 0 },
    { "\33uUnderlined\33n", "Underlined", MUIV_Frame_None, TRUE, -1, 0 },
    { "Two\nlines here", "lines here", MUIV_Frame_None, TRUE, -1, 0 },
    { "No SetMin text", "No SetMin text", MUIV_Frame_None, FALSE, -1, 0 },
    { "No SetMin text", "No SetMin text", MUIV_Frame_Group, FALSE, -1, 0 },
    { "ab", "ab", MUIV_Frame_Text, FALSE, -1, 0 },
    { "A much longer text without minimum width",
        "A much longer text without minimum width", MUIV_Frame_Text, FALSE, -1, 0 },
    { "Zero inner text", "Zero inner text", MUIV_Frame_Text, TRUE, 0, 0 },
    { "Wide inner text", "Wide inner text", MUIV_Frame_Text, TRUE, 10, 0 },
    { "Wide inner none", "Wide inner none", MUIV_Frame_None, TRUE, 10, 0 },
    { "Fixed font", "Fixed font", MUIV_Frame_Text, TRUE, -1, MUIV_Font_Fixed },
    { "Fixed font", "Fixed font", MUIV_Frame_None, FALSE, -1, MUIV_Font_Fixed },
    { "Big font", "Big font", MUIV_Frame_None, FALSE, -1, MUIV_Font_Big },
};
#define COUNT (sizeof(samples) / sizeof(samples[0]))

static LONG width(struct TextFont *font, const char *s, ULONG style)
{
    struct RastPort rp;
    struct TextExtent te;

    InitRastPort(&rp);
    SetFont(&rp, font);
    SetSoftStyle(&rp, style, AskSoftStyle(&rp));
    TextExtent(&rp, (STRPTR)s, strlen(s), &te);
    return te.te_Width;
}

int main(void)
{
    Object *app, *win, *group, *texts[COUNT];
    ULONG i, open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "textbounds",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "textbounds",
            WindowContents, group = VGroup, End,
        End,
    End;
    if (!app)
        goto done;
    for (i = 0; i < COUNT; i++)
    {
        texts[i] = TextObject,
            MUIA_Frame, samples[i].frame,
            MUIA_Text_SetMin, samples[i].setmin,
            MUIA_Text_Contents, samples[i].contents,
            samples[i].font ? MUIA_Font : TAG_IGNORE, samples[i].font,
            samples[i].inner >= 0 ? MUIA_InnerLeft : TAG_IGNORE,
                samples[i].inner,
            samples[i].inner >= 0 ? MUIA_InnerRight : TAG_IGNORE,
                samples[i].inner,
            End;
        if (!texts[i])
            goto done;
        if (i == 0)
            set(texts[i], MUIA_Background, MUII_SHADOWFILL);
        DoMethod(group, OM_ADDMEMBER, texts[i]);
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    /* Let the first refresh complete before sampling geometry. */
    DoMethod(app, MUIM_Application_InputBuffered);
    Delay(25);
    {
        struct Window *w = NULL;

        get(win, MUIA_Window_Window, &w);
        printf("window %d %d\n", w->Width, w->Height);
    }
    for (i = 0; i < COUNT; i++)
    {
        Object *o = texts[i];
        struct TextFont *f = _font(o);

        printf("text%lu font=%s/%d box=%d,%d,%d,%d inner=%d,%d "
            "min=%d def=%d max=%d plain=%ld bold=%ld\n",
            (unsigned long)i, f->tf_Message.mn_Node.ln_Name,
            f->tf_YSize, _left(o), _top(o), _width(o), _height(o),
            _mleft(o), _mwidth(o), _minwidth(o), _defwidth(o),
            _maxwidth(o), (long)width(f, samples[i].plain, FS_NORMAL),
            (long)width(f, samples[i].plain, FSF_BOLD));
    }
    {
        static const char *const glyphs[] = { "M", "W", "m", "n", "x", "0", "X" };
        struct TextFont *f = _font(texts[COUNT - 1]);

        printf("glyphs xsize=%d", f->tf_XSize);
        for (i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++)
            printf(" %s=%ld", glyphs[i], (long)width(f, glyphs[i], FS_NORMAL));
        printf("\n");
    }
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

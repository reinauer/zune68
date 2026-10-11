/* Width MUI gives a frameless Text for styled strings, minus the plain
 * TextLength of the visible characters, for working out how MUI 3.9
 * measures bold and italic chunks.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit textstyles.c -o textstyles -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static const char *const samples[][2] = {
    { "\33bBBBB", "BBBB" },
    { "\33bBBBB\33n", "BBBB" },
    { "pp\33bBB", "ppBB" },
    { "pp\33bBB\33npp", "ppBBpp" },
    { "\33bBB\33npp", "BBpp" },
    { "\33bB\33np\33bB\33np", "BpBp" },
    { "\33bB\33np\33bB", "BpB" },
    { "\33iIIII", "IIII" },
    { "\33iIIII\33n", "IIII" },
    { "pp\33iII", "ppII" },
    { "pp\33iII\33npp", "ppIIpp" },
    { "\33iII\33npp", "IIpp" },
    { "\33i\33bXX", "XX" },
    { "\33bBB\nplainline", "plainline" },
    { "plainline\n\33bBB", "plainline" },
    { "\33bBBBBBBBBBB\nplain", "BBBBBBBBBB" },
    { "\33uUUUU", "UUUU" },
    { "plain", "plain" },
};
#define COUNT (sizeof(samples) / sizeof(samples[0]))

int main(void)
{
    Object *app, *win, *group, *texts[COUNT];
    struct RastPort rp;
    ULONG i, open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "textstyles",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "textstyles",
            WindowContents, group = VGroup, End,
        End,
    End;
    if (!app)
        goto done;
    for (i = 0; i < COUNT; i++)
    {
        texts[i] = TextObject, MUIA_Text_Contents, samples[i][0],
            MUIA_Text_SetMin, TRUE, End;
        DoMethod(group, OM_ADDMEMBER, texts[i]);
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    InitRastPort(&rp);
    SetFont(&rp, _font(texts[0]));
    for (i = 0; i < COUNT; i++)
    {
        const char *plain = samples[i][1];
        LONG len = TextLength(&rp, (STRPTR)plain, strlen(plain));

        printf("%2lu min %3d height %2d plain %3ld extra %+ld\n", i,
            _minwidth(texts[i]), _minheight(texts[i]), (long)len,
            (long)(_minwidth(texts[i]) - len));
    }
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

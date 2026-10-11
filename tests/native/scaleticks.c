/* How many ticks a horizontal Scale draws at a range of widths, in the
 * default and the big font, read back from the window, with the widths
 * of the percentage labels, for deriving MUI 3.9's labelling rule.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit scaleticks.c -o scaleticks -lamiga
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

static const int widths[] = { 38, 50, 60, 70, 80, 90, 100, 120, 140, 160,
    180, 200, 230, 260, 300, 340, 380, 420 };
#define COUNT (sizeof(widths) / sizeof(widths[0]))

static void ticks(Object *o, int w, const char *font)
{
    LONG x, y = _mtop(o), n = 0, labels = 0;
    BOOL prev = FALSE;

    for (x = _mleft(o); x <= _mright(o); x++)
    {
        BOOL on = ReadPixel(_rp(o), x, y) == 1;

        if (on && !prev)
            n++;
        prev = on;
    }
    /* label runs: columns with any text pen below the bar */
    prev = FALSE;
    for (x = _mleft(o); x <= _mright(o); x++)
    {
        BOOL on = FALSE;

        for (y = _mtop(o) + 5; y <= _mbottom(o); y++)
            if (ReadPixel(_rp(o), x, y) == 1)
                on = TRUE;
        if (on && !prev)
            labels++;
        prev = on;
    }
    printf("%-5s width %3d ticks %2ld inkruns %2ld\n", font, w, (long)n,
        (long)labels);
}

int main(void)
{
    static const char *pct[] = { "0%", "12%", "25%", "37%", "50%", "62%",
        "75%", "87%", "100%" };
    Object *app, *win, *grp, *s[COUNT * 2];
    struct RastPort rp;
    ULONG open = FALSE;
    int rc = 20, i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "scaleticks",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "scaleticks",
            WindowContents, grp = VGroup, End,
        End,
    End;
    if (!app)
        goto done;
    for (i = 0; i < (int)COUNT; i++)
    {
        s[i] = ScaleObject, MUIA_FixWidth, widths[i], End;
        s[COUNT + i] = ScaleObject, MUIA_FixWidth, widths[i],
            MUIA_Font, MUIV_Font_Big, End;
        DoMethod(grp, OM_ADDMEMBER, s[i]);
        DoMethod(grp, OM_ADDMEMBER, s[COUNT + i]);
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    for (i = 0; i < 2; i++)
    {
        int j;

        InitRastPort(&rp);
        SetFont(&rp, _font(s[i * COUNT]));
        printf("%s font %d:", i ? "big" : "small", _font(s[i * COUNT])->tf_YSize);
        for (j = 0; j < 9; j++)
            printf(" %s=%ld", pct[j], (long)TextLength(&rp, (STRPTR)pct[j],
                strlen(pct[j])));
        printf("\n");
    }
    for (i = 6; i < (int)COUNT; i += 6)
    {
        LONG x, y;

        printf("rows of width %d:\n", widths[i]);
        for (y = _mtop(s[i]); y <= _mbottom(s[i]); y++)
        {
            for (x = _mleft(s[i]); x <= _mright(s[i]); x++)
                printf("%c", ReadPixel(_rp(s[i]), x, y) == 1 ? '#' : '.');
            printf("\n");
        }
    }
    for (i = 0; i < (int)COUNT; i++)
        ticks(s[i], widths[i], "small");
    for (i = 0; i < (int)COUNT; i++)
        ticks(s[COUNT + i], widths[i], "big");
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

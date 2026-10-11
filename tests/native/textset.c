/* Pixels of Text objects after a series of MUIA_Text_Contents sets:
 * plain, centred, bold and right-aligned preparses, a narrow one that
 * cuts its text, contents that grow, shrink, gain escapes, become empty
 * and plain again. Writes textset.txt. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit textset.c -o textset -lamiga
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

#define N 5
static Object *t[N];

static void dump(int step)
{
    LONG x, y;
    int i;

    for (i = 0; i < N; i++)
    {
        printf("step %d text %d box %dx%d\n", step, i, _width(t[i]), _height(t[i]));
        for (y = _top(t[i]); y <= _bottom(t[i]); y++)
        {
            for (x = _left(t[i]); x <= _right(t[i]); x++)
                printf("%c", "0123456789abcdef"[ReadPixel(_rp(t[i]), x, y) & 15]);
            printf("\n");
        }
    }
}

int main(void)
{
    static const char *seq[] = { "Status 1", "Status 12345", "St", "\33bBold now",
        "", "Plain again", "A rather long status line here", "x" };
    Object *app, *win;
    ULONG open = FALSE;
    int s, i;

    freopen("textset.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_Width, 200,
        WindowContents, VGroup,
            Child, t[0] = TextObject, TextFrame, MUIA_Text_Contents, "Status", End,
            Child, t[1] = TextObject, MUIA_Text_PreParse, "\33c", MUIA_Text_Contents, "Status", End,
            Child, t[2] = TextObject, TextFrame, MUIA_Text_PreParse, "\33b", MUIA_Text_Contents, "Status", End,
            Child, t[3] = TextObject, MUIA_Text_PreParse, "\33r", MUIA_Text_Contents, "Status", End,
            Child, HGroup, Child, t[4] = TextObject, TextFrame, MUIA_FixWidth, 40,
                MUIA_Text_Contents, "Status", End, Child, HSpace(0), End,
            End, End, End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        return 20;
    for (s = 0; s < (int)(sizeof(seq) / sizeof(seq[0])); s++)
    {
        for (i = 0; i < N; i++)
            set(t[i], MUIA_Text_Contents, seq[s]);
        dump(s);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

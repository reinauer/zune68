/*
 * groupset: two Group behaviours compared with MUI 3.8.
 *
 * 1. A set() of MUIA_Group_Spacing or MUIA_Group_Columns on a group, made
 *    before its window opens: does a child group take the value too? The
 *    probe prints the gap between the inner group's two buttons and whether
 *    they stand side by side.
 * 2. A group with MUIA_FillArea FALSE and a background of its own: are the
 *    gaps between its children painted? The probe prints the pen in the
 *    middle of the gap and the background's pen (FILLPEN).
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -Ibuild/muimaster/include \
 *        tests/native/groupset.c -o groupset -lamiga
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;

static Object *bar(void)
{
    return RectangleObject, MUIA_Background, MUII_SHINE,
        MUIA_FixHeight, 8, MUIA_FixWidth, 40, End;
}

static Object *fillgroup(BOOL fill, Object **top, Object **bottom)
{
    return VGroup, MUIA_FillArea, fill, MUIA_Background, MUII_FILL,
        MUIA_Group_Spacing, 10,
        Child, (IPTR)(*top = bar()),
        Child, (IPTR)(*bottom = bar()),
    End;
}

static void gap_pen(const char *name, Object *top, Object *bottom)
{
    LONG x = _left(top) + _width(top) / 2;
    LONG y = (_bottom(top) + _top(bottom)) / 2;

    printf("%s: gap pen %ld, FILLPEN %ld, window back pen %ld\n", name,
        (long)ReadPixel(_rp(top), x, y), (long)_dri(top)->dri_Pens[FILLPEN],
        (long)_dri(top)->dri_Pens[BACKGROUNDPEN]);
}

int main(void)
{
    Object *app, *win, *spaced, *columns, *s1, *s2, *c1, *c2;
    Object *f1, *f2, *t1, *t2;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"GroupSet",
        SubWindow, (IPTR)(win = WindowObject,
            MUIA_Window_Title, (IPTR)"GroupSet",
            WindowContents, HGroup,
                Child, (IPTR)(spaced = VGroup, GroupFrame,
                    Child, VGroup, MUIA_Group_Spacing, 1,
                        Child, (IPTR)(s1 = SimpleButton("S1")),
                        Child, (IPTR)(s2 = SimpleButton("S2")),
                    End,
                End),
                Child, (IPTR)(columns = VGroup, GroupFrame,
                    Child, VGroup,
                        Child, (IPTR)(c1 = SimpleButton("C1")),
                        Child, (IPTR)(c2 = SimpleButton("C2")),
                    End,
                End),
                Child, fillgroup(FALSE, &f1, &f2),
                Child, fillgroup(TRUE, &t1, &t2),
            End,
        End),
    End;
    if (!app) { printf("no app\n"); return 20; }
    set(spaced, MUIA_Group_Spacing, 20);
    set(columns, MUIA_Group_Columns, 2);
    set(win, MUIA_Window_Open, TRUE);
    printf("Spacing 20 on the outer group: inner gap %ld (1 kept)\n",
        (long)(_top(s2) - _bottom(s1) - 1));
    printf("Columns 2 on the outer group: inner buttons %s\n",
        _top(c1) == _top(c2) ? "side by side (passed on)" : "stacked (kept)");
    gap_pen("FillArea FALSE", f1, f2);
    gap_pen("FillArea TRUE", t1, t2);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

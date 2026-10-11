/* How a horizontal group shares its width among children with different
 * minimum, default and maximum widths and weights (Voyager's fast link
 * row, YAM's panes). Each case lays out a row of custom objects at
 * several window widths and prints their widths to hdist.txt ("hdist v":
 * a column at several window heights, into vdist.txt). Compare native
 * and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit hdist.c -o hdist -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define TAG_MIN (0x4d440000 | 1)
#define TAG_DEF (0x4d440000 | 2)
#define TAG_MAX (0x4d440000 | 3)

struct Data { LONG min, def, max; };
static int vertical;

static ULONG Dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    struct Data *d;

    switch (msg->MethodID)
    {
    case OM_NEW:
        if ((obj = (Object *)DoSuperMethodA(cl, obj, msg)))
        {
            struct TagItem *t = ((struct opSet *)msg)->ops_AttrList;

            d = INST_DATA(cl, obj);
            d->min = d->def = 0;
            d->max = MUI_MAXMAX;
            for (; t && t->ti_Tag != TAG_DONE; t++)
                if (t->ti_Tag == TAG_MIN) d->min = t->ti_Data;
                else if (t->ti_Tag == TAG_DEF) d->def = t->ti_Data;
                else if (t->ti_Tag == TAG_MAX) d->max = t->ti_Data;
        }
        return (ULONG)obj;
    case MUIM_AskMinMax:
    {
        struct MUI_MinMax *mm;

        /* MUI points MinMaxInfo at the object's own minmax during the
         * super call */
        DoSuperMethodA(cl, obj, msg);
        mm = ((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        d = INST_DATA(cl, obj);
        if (vertical)
        {
            mm->MinHeight += d->min;
            mm->DefHeight += d->def;
            mm->MaxHeight += d->max;
            mm->MinWidth += 10;
            mm->DefWidth += 10;
            mm->MaxWidth += 10;
        }
        else
        {
            mm->MinWidth += d->min;
            mm->DefWidth += d->def;
            mm->MaxWidth += d->max;
            mm->MinHeight += 10;
            mm->DefHeight += 10;
            mm->MaxHeight += 10;
        }
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

struct Kid { LONG min, def, max, weight; };
struct Case { const char *name; int n; struct Kid kids[8]; };

static const struct Case cases[] =
{
    { "mins", 3, { { 10, 10, 9999, 100 }, { 50, 50, 9999, 100 },
        { 100, 100, 9999, 100 } } },
    { "mins+4", 3, { { 10, 14, 9999, 100 }, { 50, 54, 9999, 100 },
        { 100, 104, 9999, 100 } } },
    { "defs", 3, { { 0, 10, 9999, 100 }, { 0, 50, 9999, 100 },
        { 0, 100, 9999, 100 } } },
    { "bigdefs", 3, { { 10, 300, 9999, 100 }, { 50, 300, 9999, 100 },
        { 100, 300, 9999, 100 } } },
    { "weights", 2, { { 0, 0, 9999, 30 }, { 0, 0, 9999, 100 } } },
    { "weights-min", 2, { { 20, 20, 9999, 30 }, { 60, 60, 9999, 100 } } },
    { "w1", 4, { { 27, 31, 9999, 1 }, { 48, 52, 9999, 1 },
        { 43, 47, 9999, 1 }, { 32, 36, 9999, 1 } } },
    { "maxed", 3, { { 10, 10, 40, 100 }, { 50, 50, 9999, 100 },
        { 100, 100, 9999, 100 } } },
    { "fix0", 3, { { 0, 0, 9999, 30 }, { 2, 2, 2, 0 }, { 0, 0, 9999, 100 } } },
    { "fix100", 3, { { 0, 0, 9999, 30 }, { 2, 2, 2, 100 },
        { 0, 0, 9999, 100 } } },
    { "fix50w", 3, { { 0, 0, 9999, 30 }, { 50, 50, 50, 100 },
        { 0, 0, 9999, 100 } } },
    { "fixlast", 3, { { 0, 0, 9999, 30 }, { 0, 0, 9999, 100 },
        { 2, 2, 2, 100 } } },
    { "fixfirst", 3, { { 2, 2, 2, 100 }, { 0, 0, 9999, 30 },
        { 0, 0, 9999, 100 } } },
    { "w30-100-100", 3, { { 0, 0, 9999, 30 }, { 0, 0, 9999, 100 },
        { 0, 0, 9999, 100 } } },
};

int main(int argc, char **argv)
{
    static const int widths[] = { 200, 223, 246, 260, 269, 292, 315, 330,
        338, 361, 384, 400, 407, 430, 453, 476, 499, 517, 522, 545, 568,
        591, 614, 637, 640, 660, 683, 700 };
    struct MUI_CustomClass *mcc;
    int c, i, k;

    int vert = argc > 1 && argv[1][0] == 'v';

    freopen(vert ? "vdist.txt" : "hdist.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    vertical = vert;
    mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct Data),
        (APTR)Dispatch);
    if (!mcc)
        return 20;
    for (c = 0; c < (int)(sizeof(cases) / sizeof(cases[0])); c++)
        for (i = 0; i < (int)(sizeof(widths) / sizeof(widths[0])); i++)
        {
            Object *app, *win, *g, *kid[8];
            ULONG open = FALSE;

            struct TagItem gtags[12];

            if (vert && widths[i] > 520)
                continue;

            for (k = 0; k < cases[c].n; k++)
            {
                kid[k] = NewObject(mcc->mcc_Class, NULL,
                    TAG_MIN, cases[c].kids[k].min,
                    TAG_DEF, cases[c].kids[k].def,
                    TAG_MAX, cases[c].kids[k].max,
                    vert ? MUIA_VertWeight : MUIA_HorizWeight,
                    cases[c].kids[k].weight, TAG_DONE);
                gtags[k].ti_Tag = MUIA_Group_Child;
                gtags[k].ti_Data = (ULONG)kid[k];
            }
            gtags[k].ti_Tag = MUIA_Group_Horiz;
            gtags[k++].ti_Data = !vert;
            gtags[k].ti_Tag = MUIA_Group_Spacing;
            gtags[k++].ti_Data = 0;
            gtags[k].ti_Tag = TAG_DONE;
            g = MUI_NewObjectA(MUIC_Group, gtags);
            app = ApplicationObject, SubWindow, win = WindowObject,
                vert ? MUIA_Window_Height : MUIA_Window_Width, widths[i],
                WindowContents, g,
                End, End;
            if (!app)
                return 20;
            set(win, MUIA_Window_Open, TRUE);
            get(win, MUIA_Window_Open, &open);
            if (open)
            {
                printf("%-11s %3d:", cases[c].name,
                    vert ? _mheight(g) : _mwidth(g));
                for (k = 0; k < cases[c].n; k++)
                    printf(" %3d", vert ? _height(kid[k]) : _width(kid[k]));
                printf("\n");
                fflush(stdout);
            }
            set(win, MUIA_Window_Open, FALSE);
            MUI_DisposeObject(app);
        }
    MUI_DeleteCustomClass(mcc);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

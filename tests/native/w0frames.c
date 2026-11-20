/* w0frames TYPES SIZES [FLAGS]: framepix's first form (before 2026-11-17): Rectangles of the frame
 * types in TYPES ("0123456789abc") at the sizes in SIZES (indexes into 0x0, 1x1, 2x0, 0x2,
 * 3x2, 5x3, 12x6 inner pixels; an inner size 0 is weight 0 on that axis), each wrapped as
 * HGroup(VGroup(obj, VSpace(0)), HSpace(0)) in a ColGroup; prints each box. FLAGS: s on
 * framepix's 640x480 screen of 16 colours, t thick frames in the settings, l the objects
 * added after the application was made (as framepix did). OpenMUI's lean
 * hung opening the full set (a box across the screen); MUI 3.8 opens it.
 * From OpenMUI (tests/lean/probe/w0frames.c).
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include w0frames.c -o w0frames -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>
#include <graphics/modeid.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static const UBYTE sizes[7][2] = { {0, 0}, {1, 1}, {2, 0}, {0, 2}, {3, 2}, {5, 3}, {12, 6} };
static Object *objs[200];
static char specs[200][8];

int main(int argc, char **argv)
{
    const char *types = argc > 1 ? argv[1] : "0123456789abc", *sz = argc > 2 ? argv[2] : "0123456";
    Object *app, *win, *grid;
    IPTR open = 0;
    int n = 0, i, t, s;
    const char *z;

    struct Screen *scr = NULL;
    int late = argc > 3 && strchr(argv[3], 'l') != NULL;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    if (argc > 3 && strchr(argv[3], 't'))
    {
        Object *c = MUI_NewObject("Configdata.mui", TAG_DONE);
        if (c)
        {
            DoMethod(c, 0x8042f4b6 /* MUIM_Configdata_SetULong */, 0x50b /* Frame_Thickness */, 2);
            DoMethod(c, 0x80422c43 /* MUIM_Configdata_Save */, (IPTR)"ENV:OpenMUI/W0F.1.prefs");
            MUI_DisposeObject(c);
        }
    }
    if (argc > 3 && strchr(argv[3], 's'))
    {
        static UWORD pens[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, (UWORD)~0 };
        scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_DisplayID, HIRESLACE_KEY,
            SA_Depth, 4, SA_Pens, (IPTR)pens, SA_SharePens, TRUE, SA_Title, (IPTR)"W0F",
            SA_Type, CUSTOMSCREEN, TAG_DONE);
    }
    grid = ColGroup(2 * strlen(sz)), MUIA_Group_Spacing, 2, End;
    if (!late)
    {
    for (t = 0; types[t]; t++)
        for (s = 0; s < 2; s++)
            for (z = sz; *z && n < 200; z++, n++)
            {
                i = *z - '0';
                strcpy(specs[n], "000000");
                specs[n][0] = types[t];
                specs[n][1] = '0' + s;
                objs[n] = RectangleObject, MUIA_Frame, (IPTR)specs[n],
                    sizes[i][0] ? MUIA_FixWidth : MUIA_HorizWeight, sizes[i][0],
                    sizes[i][1] ? MUIA_FixHeight : MUIA_VertWeight, sizes[i][1], End;
                DoMethod(grid, OM_ADDMEMBER, (IPTR)(sizes[i][0] && sizes[i][1] ? objs[n]
                    : HGroup, MUIA_Group_Spacing, 0, Child, (IPTR)(VGroup,
                        MUIA_Group_Spacing, 0, Child, (IPTR)objs[n], Child,
                        (IPTR)VSpace(0), End), Child, (IPTR)HSpace(0), End));
            }
    }
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"W0F", MUIA_Application_Base, (IPTR)"W0F",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"W0F", MUIA_Window_ID, 0,
            scr ? MUIA_Window_Screen : TAG_IGNORE, (IPTR)scr, WindowContents, (IPTR)grid, End), End;
    if (!app) { printf("no app\n"); return 20; }
    if (late)
    {
    for (t = 0; types[t]; t++)
        for (s = 0; s < 2; s++)
            for (z = sz; *z && n < 200; z++, n++)
            {
                i = *z - '0';
                strcpy(specs[n], "000000");
                specs[n][0] = types[t];
                specs[n][1] = '0' + s;
                objs[n] = RectangleObject, MUIA_Frame, (IPTR)specs[n],
                    sizes[i][0] ? MUIA_FixWidth : MUIA_HorizWeight, sizes[i][0],
                    sizes[i][1] ? MUIA_FixHeight : MUIA_VertWeight, sizes[i][1], End;
                DoMethod(grid, OM_ADDMEMBER, (IPTR)(sizes[i][0] && sizes[i][1] ? objs[n]
                    : HGroup, MUIA_Group_Spacing, 0, Child, (IPTR)(VGroup,
                        MUIA_Group_Spacing, 0, Child, (IPTR)objs[n], Child,
                        (IPTR)VSpace(0), End), Child, (IPTR)HSpace(0), End));
            }
    }
    printf("objects %d, opening\n", n);
    fflush(stdout);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %ld\n", (long)open);
    for (i = 0; open && i < n; i++)
        printf("%s %c: box %d,%d %dx%d min %dx%d\n", specs[i], sz[i % strlen(sz)], _left(objs[i]),
            _top(objs[i]), _width(objs[i]), _height(objs[i]), _minwidth(objs[i]), _minheight(objs[i]));
    fflush(stdout);
    Delay(25);
    MUI_DisposeObject(app);
    if (scr) CloseScreen(scr);
    CloseLibrary(MUIMasterBase);
    return 0;
}

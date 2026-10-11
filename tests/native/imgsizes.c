/* Minimum, default and maximum sizes of Image objects for the built-in
 * image specs 6:0 to 6:47 (fixed size, not free). Writes imgsizes.txt.
 * Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit imgsizes.c -o imgsizes -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define FIRST 0
#define COUNT 48
#define PERWIN 12

int main(void)
{
    int base, i;

    freopen("imgsizes.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (base = FIRST; base < FIRST + COUNT; base += PERWIN)
    {
        static char specs[PERWIN][8];
        Object *app, *win, *grp, *o[PERWIN];
        ULONG open = FALSE;

        app = ApplicationObject, SubWindow, win = WindowObject,
            WindowContents, grp = HGroup, End, End, End;
        if (!app)
            return 20;
        for (i = 0; i < PERWIN; i++)
        {
            sprintf(specs[i], "6:%ld", (long)(base + i));
            DoMethod(grp, OM_ADDMEMBER, o[i] = ImageObject,
                MUIA_Image_Spec, specs[i], MUIA_Image_FreeHoriz, FALSE,
                MUIA_Image_FreeVert, FALSE, End);
        }
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        if (open)
            for (i = 0; i < PERWIN; i++)
                printf("%-5s min %2d,%2d def %2d,%2d max %5d,%5d\n",
                    specs[i], _minwidth(o[i]), _minheight(o[i]),
                    _defwidth(o[i]), _defheight(o[i]),
                    _maxwidth(o[i]), _maxheight(o[i]));
        set(win, MUIA_Window_Open, FALSE);
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

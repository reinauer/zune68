/* Sizes of check mark images as a form checkbox would use them.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit imgdef.c -o imgdef -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

int main(void)
{
    Object *app, *win, *grp, *o[14];
    static const char *names[14] = { "makeobj-check", "check-freevert", "check-fixed",
        "check-freeboth", "check-noframe-freevert", "check-freevert-fontmatch",
        "popup-freevert", "check-freevert-toggle-nobg", "check-freevert-big",
        "check-freevert-big-inner1", "check-fixed-big", "cycle-big",
        "check-fontmatch-big", "check-fontmatchw-big" };
    ULONG open = FALSE;
    int i;

    freopen("imgdef.txt", "w", stdout);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        WindowContents, grp = HGroup, End, End, End;
    if (!app)
        return 20;
    o[0] = MUI_MakeObject(MUIO_Checkmark, NULL);
    o[1] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_Image_FreeVert, TRUE, MUIA_InputMode, MUIV_InputMode_Toggle,
        MUIA_Background, MUII_ButtonBack, MUIA_ShowSelState, FALSE, End;
    o[2] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_InputMode, MUIV_InputMode_Toggle, End;
    o[3] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_Image_FreeVert, TRUE, MUIA_Image_FreeHoriz, TRUE, End;
    o[4] = ImageObject, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_Image_FreeVert, TRUE, End;
    o[5] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_Image_FreeVert, TRUE, MUIA_Image_FontMatchHeight, TRUE, End;
    o[6] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_PopUp,
        MUIA_Image_FreeVert, TRUE, End;
    o[7] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
        MUIA_Image_FreeVert, TRUE, MUIA_InputMode, MUIV_InputMode_Toggle, End;
    {
        static const char *entries[] = { "First", "Second", NULL };
        o[8] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
            MUIA_Image_FreeVert, TRUE, MUIA_Font, MUIV_Font_Big, End;
        o[9] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
            MUIA_Image_FreeVert, TRUE, MUIA_Font, MUIV_Font_Big,
            MUIA_InnerLeft, 1, MUIA_InnerRight, 1, MUIA_InnerTop, 1, MUIA_InnerBottom, 1, End;
        o[10] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
            MUIA_Font, MUIV_Font_Big, End;
        o[11] = CycleObject, MUIA_Cycle_Entries, entries, MUIA_Font, MUIV_Font_Big, End;
        o[12] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
            MUIA_Image_FreeVert, TRUE, MUIA_Image_FontMatchHeight, TRUE, MUIA_Font, MUIV_Font_Big, End;
        o[13] = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
            MUIA_Image_FreeHoriz, TRUE, MUIA_Image_FontMatchWidth, TRUE, MUIA_Font, MUIV_Font_Big, End;
    }
    for (i = 0; i < 14; i++)
        if (o[i])
            DoMethod(grp, OM_ADDMEMBER, o[i]);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
        for (i = 0; i < 14; i++)
            if (o[i])
                printf("%-26s min %2d,%2d def %2d,%2d max %5d,%5d box %dx%d font %d add %d,%d sub %d,%d\n",
                    names[i], _minwidth(o[i]), _minheight(o[i]),
                    _defwidth(o[i]), _defheight(o[i]),
                    _maxwidth(o[i]), _maxheight(o[i]), _width(o[i]), _height(o[i]),
                    _font(o[i]) ? _font(o[i])->tf_YSize : -1,
                    _addleft(o[i]), _addtop(o[i]), _subwidth(o[i]), _subheight(o[i]));
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

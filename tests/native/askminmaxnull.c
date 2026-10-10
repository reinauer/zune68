/* MUIM_AskMinMax with a NULL MinMaxInfo, as Voyager measures form fields:
 * set up an object against an open window, ask, then read _minwidth().
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit askminmaxnull.c -o askminmaxnull -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#ifndef MUIM_GlobalSetup
#define MUIM_GlobalSetup 0x8042d532 /* private */
#endif
#ifndef _parent
#define _parent(obj) (*(Object **)&muiNotifyData(obj)->priv2) /* private */
#endif
struct Library *MUIMasterBase;
ULONG __stack = 65536;
int main(void)
{
    Object *app, *win, *root, *obj;
    ULONG open = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "askminmaxnull",
        SubWindow, win = WindowObject, MUIA_Window_Title, "askminmaxnull",
            WindowContents, root = VGroup, Child, RectangleObject, End, End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open) return 20;
    obj = StringObject, StringFrame, MUIA_String_Contents, "Amiga test", End;
    muiGlobalInfo(obj) = muiGlobalInfo(root);
    DoMethod(obj, MUIM_GlobalSetup, (ULONG)muiGlobalInfo(root));
    _parent(obj) = root;
    muiRenderInfo(obj) = muiRenderInfo(root);
    DoMethod(obj, MUIM_Setup, (ULONG)muiRenderInfo(root));
    DoMethod(obj, MUIM_AskMinMax, (ULONG)NULL);
    printf("string min %ldx%ld max %ldx%ld def %ldx%ld\n",
        (LONG)_minwidth(obj), (LONG)_minheight(obj), (LONG)_maxwidth(obj),
        (LONG)_maxheight(obj), (LONG)_defwidth(obj), (LONG)_defheight(obj));
    {
        /* With a caller's buffer: where do the results go, and does the
         * message keep the caller's pointer? */
        struct MUI_MinMax mine = { 7777, 7777, 7777, 7777, 7777, 7777 };
        struct MUIP_AskMinMax amm;
        amm.MethodID = MUIM_AskMinMax;
        amm.MinMaxInfo = &mine;
        _minwidth(obj) = 5555;
        DoMethodA(obj, (Msg)&amm);
        printf("own buffer: msg keeps it %d, buffer min %ldx%ld, _minwidth %ld\n",
            amm.MinMaxInfo == &mine, (LONG)mine.MinWidth, (LONG)mine.MinHeight,
            (LONG)_minwidth(obj));
    }
    DoMethod(obj, MUIM_Cleanup);
    MUI_DisposeObject(obj);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

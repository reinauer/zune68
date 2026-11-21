/* Two programs with the same application base and window ID (TheBar's
 * demos all use THEBAREXAMPLE and MAIN): the first one's window is large,
 * the second one's fixed and small. The size the second window opens at
 * after the first was closed and disposed, and Boopsi's answer to
 * get(MUIA_Boopsi_Object).
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit \
 *        winclamp.c -o winclamp -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static Object *make(LONG w, LONG h, Object **win, Object **rect)
{
    return ApplicationObject, MUIA_Application_Base, "WCLAMP",
        SubWindow, *win = WindowObject, MUIA_Window_Title, "winclamp",
            MUIA_Window_ID, MAKE_ID('M','A','I','N'),
            WindowContents, VGroup,
                Child, *rect = RectangleObject, MUIA_FixWidth, w,
                    MUIA_FixHeight, h, End,
                End, End, End;
}

static void run(const char *name, LONG w, LONG h)
{
    Object *app, *win, *rect;
    struct Window *iw = NULL;
    ULONG open = FALSE;

    if (!(app = make(w, h, &win, &rect)))
    {
        printf("%s: no application\n", name);
        return;
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &iw);
    if (open && iw)
        printf("%-6s window %dx%d contents %dx%d\n", name, iw->Width,
            iw->Height, _width(rect), _height(rect));
    else
        printf("%-6s not open\n", name);
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
}

int main(void)
{
    Object *b;
    IPTR val = 0;
    ULONG rc;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    run("large", 300, 150);
    run("small", 100, 40);
    b = BoopsiObject, MUIA_Boopsi_ClassID, "propgclass", End;
    if (b)
    {
        rc = GetAttr(MUIA_Boopsi_Object, b, &val);
        printf("boopsi object get: answer %s, value %s\n", rc ? "TRUE" : "FALSE",
            val ? "set" : "NULL");
        MUI_DisposeObject(b);
    }
    else
        printf("no boopsi object\n");
    CloseLibrary(MUIMasterBase);
    return 0;
}

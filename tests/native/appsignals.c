/* Signal bits a MUI application uses: counts the task's allocated signals
 * before creating an application, after creating it (with and without
 * MUIA_Application_UseCommodities/UseRexx) and with a window open.
 * IBrowse runs out of signals for OpenClipboard() if MUI uses more than
 * MUI 3.9 does. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit appsignals.c -o appsignals -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static int used(void)
{
    ULONG mask = FindTask(NULL)->tc_SigAlloc;
    int n = 0, i;

    for (i = 0; i < 32; i++)
        if (mask & (1UL << i))
            n++;
    return n;
}

static void one(const char *name, BOOL cx, BOOL rexx)
{
    Object *app, *win;
    int before = used(), created, opened;

    app = ApplicationObject,
        MUIA_Application_Title, "appsignals",
        MUIA_Application_Base, "APPSIGNALS",
        MUIA_Application_UseCommodities, cx,
        MUIA_Application_UseRexx, rexx,
        SubWindow, win = WindowObject, MUIA_Window_Title, "appsignals",
            WindowContents, RectangleObject, End,
        End,
    End;
    if (!app)
    {
        printf("%-10s no application\n", name);
        return;
    }
    created = used();
    set(win, MUIA_Window_Open, TRUE);
    opened = used();
    set(win, MUIA_Window_Open, FALSE);
    printf("%-10s before %d app %+d window %+d\n", name, before,
        created - before, opened - created);
    MUI_DisposeObject(app);
}

int main(void)
{
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    one("default", TRUE, TRUE);
    one("no cx", FALSE, TRUE);
    one("no rexx", TRUE, FALSE);
    one("neither", FALSE, FALSE);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}

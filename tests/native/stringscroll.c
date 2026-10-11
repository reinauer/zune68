/* String cursor and display position of a text longer than the field:
 * after creation, after setting the contents, after activating, and
 * after closing and reopening the window (IBrowse's Location field).
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit stringscroll.c -o stringscroll -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static Object *app, *win, *str, *other;

static void show(const char *step)
{
    ULONG buf = 0, disp = 0;

    DoMethod(app, MUIM_Application_InputBuffered);
    get(str, MUIA_String_BufferPos, &buf);
    get(str, MUIA_String_DisplayPos, &disp);
    printf("%-16s bufferpos %3lu displaypos %3lu\n", step, buf, disp);
}

static void cycle(const char *step)
{
    set(win, MUIA_Window_Open, FALSE);
    set(win, MUIA_Window_Open, TRUE);
    show(step);
}

int main(void)
{
    static const char url[] = "http://127.0.0.1:8768/index.html?page=1";
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "stringscroll",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "stringscroll",
            WindowContents, VGroup,
                Child, str = StringObject, StringFrame, MUIA_FixWidthTxt,
                    "http://127.0.0.1", MUIA_String_Contents, url, End,
                Child, other = StringObject, StringFrame, End,
                End,
            End,
        End;
    if (!app)
        goto done;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    show("created");
    cycle("reopened");
    set(str, MUIA_String_Contents, url);
    show("set contents");
    cycle("reopened");
    set(win, MUIA_Window_ActiveObject, str);
    show("activated");
    set(win, MUIA_Window_ActiveObject, other);
    show("deactivated");
    cycle("reopened");
    set(str, MUIA_String_BufferPos, 0);
    show("bufferpos 0");
    cycle("reopened");
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

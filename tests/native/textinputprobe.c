/* Textinput.mcc as Voyager uses it for form fields: StringFrame with
 * MUII_TextBack, plus a plain one and a multiline one, for comparing
 * native and MUI 3.9 rendering. "textinputprobe hold" keeps it open.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit textinputprobe.c -o textinputprobe -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
int main(int argc, char **argv)
{
    Object *app, *win, *a, *b, *c;
    ULONG open = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "textinputprobe",
        SubWindow, win = WindowObject, MUIA_Window_Title, "textinputprobe",
            MUIA_Window_Width, 260,
            WindowContents, VGroup,
                Child, a = MUI_NewObject(MUIC_Textinput,
                    MUIA_Textinput_Contents, "plain", TAG_DONE),
                Child, b = MUI_NewObject(MUIC_Textinput, StringFrame,
                    MUIA_Background, MUII_TextBack,
                    MUIA_Textinput_Contents, "Amiga test", TAG_DONE),
                Child, c = MUI_NewObject(MUIC_Textinput, StringFrame,
                    MUIA_Background, MUII_TextBack,
                    MUIA_Textinput_Multiline, TRUE,
                    MUIA_Textinput_Contents, "Editable notes", TAG_DONE),
            End,
        End,
    End;
    if (!app) { printf("no application\n"); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %ld a %p b %p c %p\n", (LONG)open, a, b, c);
    fflush(stdout);
    if (argc > 1 && !strcmp(argv[1], "hold")) Delay(400);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

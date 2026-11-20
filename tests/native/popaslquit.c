/* popaslquit: dispose an application while its Popasl requester is still
 * open, as a program that does not check MUIA_Popasl_Active does when it
 * quits. Zune used to free the requester under the process still showing
 * it. Prints when the requester is open and when the dispose returned.
 * On asl.library 44+ the dispose aborts the requester; before that it
 * waits until the user closes it. From OpenMUI (tests/lean/popaslquit.c). */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;

int main(void)
{
    Object *app, *win, *pop;
    IPTR active = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"PopaslQuit",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, (IPTR)"PopaslQuit",
            WindowContents, VGroup,
                Child, pop = PopaslObject,
                    MUIA_Popstring_String, StringObject, StringFrame, End,
                    MUIA_Popstring_Button, PopButton(MUII_PopFile),
                End,
            End,
        End,
    End;
    if (!app)
    {
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    set(win, MUIA_Window_Open, TRUE);
    DoMethod(pop, MUIM_Popstring_Open);
    Delay(150);
    get(pop, MUIA_Popasl_Active, &active);
    printf("requester open: %ld\n", (long)active);
    fflush(stdout);
    MUI_DisposeObject(app);
    printf("disposed\n");
    CloseLibrary(MUIMasterBase);
    return 0;
}

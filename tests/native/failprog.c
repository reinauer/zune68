/*
 * failprog: a program for failn sweeps. It makes an application with a
 * menustrip and a window holding most built-in classes (groups, a
 * register, buttons, strings, a cycle, a slider, a listview, a popobject,
 * a popasl, a scrollgroup), opens and closes the window, disposes the
 * application and closes the library, whatever failed on the way, so that
 * memory a failure path keeps shows in failn's figures.
 *
 * Exit code: 0 all made and shown, 10 the window did not open, 20 no
 * application, 30 no muimaster.library.
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s -Ibuild/muimaster/include \
 *        tests/native/failprog.c -o failprog -lamiga
 */
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;

static const char *pages[] = { "Simple", "Lists", NULL };
static const char *cycle[] = { "One", "Two", "Three", NULL };
static const char *entries[] = { "Alpha", "Beta", "Gamma", NULL };
static struct NewMenu menu[] = {
    { NM_TITLE, (STRPTR)"Project", 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"About", (STRPTR)"?", 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Quit", (STRPTR)"Q", 0, 0, 0 },
    { NM_END, NULL, 0, 0, 0, 0 },
};

int main(void)
{
    Object *app, *win;
    LONG open = FALSE;
    int rc;

    if (!(MUIMasterBase = OpenLibrary((STRPTR)"muimaster.library", 19)))
        return 30;
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"FailProg",
        MUIA_Application_Base, (IPTR)"FAILPROG",
        MUIA_Application_Menustrip, (IPTR)MUI_MakeObject(MUIO_MenustripNM,
            (IPTR)menu, 0),
        SubWindow, (IPTR)(win = WindowObject,
            MUIA_Window_Title, (IPTR)"FailProg",
            MUIA_Window_ID, MAKE_ID('F','A','I','L'),
            WindowContents, VGroup,
                Child, (IPTR)(RegisterGroup(pages),
                    Child, (IPTR)(ColGroup(2),
                        Child, (IPTR)Label2("String"),
                        Child, (IPTR)(StringObject, StringFrame, End),
                        Child, (IPTR)Label1("Cycle"),
                        Child, (IPTR)MUI_MakeObject(MUIO_Cycle, NULL,
                            (IPTR)cycle),
                        Child, (IPTR)Label("Slider"),
                        Child, (IPTR)MUI_MakeObject(MUIO_Slider, NULL, 0, 10),
                        Child, (IPTR)Label2("File"),
                        Child, (IPTR)(PopaslObject,
                            MUIA_Popstring_String, (IPTR)(StringObject,
                                StringFrame, End),
                            MUIA_Popstring_Button, (IPTR)PopButton(MUII_PopFile),
                        End),
                        Child, (IPTR)Label2("Pick"),
                        Child, (IPTR)(PopobjectObject,
                            MUIA_Popstring_String, (IPTR)(StringObject,
                                StringFrame, End),
                            MUIA_Popstring_Button, (IPTR)PopButton(MUII_PopUp),
                            MUIA_Popobject_Object, (IPTR)(ListviewObject,
                                MUIA_Listview_List, (IPTR)(ListObject,
                                    InputListFrame,
                                    MUIA_List_SourceArray, (IPTR)entries,
                                End),
                            End),
                        End),
                    End),
                    Child, (IPTR)(HGroup,
                        Child, (IPTR)(ListviewObject,
                            MUIA_Listview_List, (IPTR)(ListObject,
                                InputListFrame,
                                MUIA_List_SourceArray, (IPTR)entries,
                            End),
                        End),
                        Child, (IPTR)(ScrollgroupObject,
                            MUIA_Scrollgroup_Contents, (IPTR)(VirtgroupObject,
                                VirtualFrame,
                                Child, (IPTR)TextObject,
                                    MUIA_Text_Contents, (IPTR)"Virtual",
                                End,
                            End),
                        End),
                    End),
                End),
                Child, (IPTR)(HGroup,
                    Child, (IPTR)SimpleButton("_Ok"),
                    Child, (IPTR)HSpace(0),
                    Child, (IPTR)SimpleButton("_Cancel"),
                End),
            End,
        End),
    End;
    if (!app)
        rc = 20;
    else {
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        set(win, MUIA_Window_Open, FALSE);
        rc = open ? 0 : 10;
        MUI_DisposeObject(app);
    }
    CloseLibrary(MUIMasterBase);
    return rc;
}

/* Every class built from workbench/classes/zune opens and shows a window. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <graphics/modeid.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
struct Library *MUIMasterBase;
ULONG __stack = 65536;
enum { LIBRARY, AREA, GROUP, WINDOW, CONTENTS, QRCODE };
#define MUIA_QRCode_Text 0x90470b01 /* MUIB_AROS | 0xb01, zune/qrcode.h */
#define MUIA_QRCode_Status 0x90470b04
#define QRCODE_TEXT "https://github.com/reinauer/zune68"
static const struct { const char *name; int kind; } classes[] = {
    {"Calendar.mcc", AREA}, {"Calltips.mcc", LIBRARY}, {"Clock.mcc", AREA},
    {"Flexgroup.mcc", GROUP}, {"Graph.mcc", AREA}, {"IconImage.mcc", LIBRARY},
    {"Lamp.mcc", AREA}, {"PrefsEditor.mcc", AREA}, {"Aboutbox.mcc", WINDOW},
    {"AboutWindow.mcc", WINDOW}, {"LoginWindow.mcc", WINDOW},
    {"PrefsWindow.mcc", CONTENTS}, {"SystemPrefsWindow.mcc", CONTENTS},
    {"QRCode.mcc", QRCODE},
};
static struct Screen *screen;
static int failures;
static void check(BOOL ok, const char *name, const char *label)
{
    if (!ok) { printf("FAIL %s %s\n", name, label); failures++; }
}
static void show(const char *name, Object *win, LONG hold)
{
    ULONG open = 0;
    Object *app = MO(MUIC_Application, MUIA_Application_Window, (ULONG)win, TAG_DONE);
    check(app != NULL, name, "application");
    if (!app) return;
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, win, &open);
    check(open, name, "window open");
    if (hold) Delay(hold);
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    MUI_DisposeObject(app);
}
static void exercise(const char *name, int kind, LONG hold)
{
    Object *object, *win;
    char path[40];
    struct Library *base;
    sprintf(path, "MUI/%s", name);
    base = OpenLibrary(path, 0);
    check(base != NULL, name, "library");
    if (!base) return;
    printf("%s %d.%d\n", name, base->lib_Version, base->lib_Revision);
    CloseLibrary(base);
    if (kind == LIBRARY) return;
    if (kind == WINDOW)
        win = MO(name, MUIA_Window_Screen, (ULONG)screen, TAG_DONE);
    else if (kind == CONTENTS)
        win = MO(name, MUIA_Window_Screen, (ULONG)screen, MUIA_Window_RootObject,
            !strcmp(name, "SystemPrefsWindow.mcc")
                ? (ULONG)MO("PrefsEditor.mcc", TAG_DONE)
                : (ULONG)MO(MUIC_Text, MUIA_Text_Contents, (ULONG)"contents", TAG_DONE),
            TAG_DONE);
    else
    {
        object = kind == GROUP
            ? MO(name, MUIA_Group_Child,
                 (ULONG)MO(MUIC_Text, MUIA_Text_Contents, (ULONG)"child", TAG_DONE), TAG_DONE)
            : kind == QRCODE
            ? MO(name, MUIA_QRCode_Text, (ULONG)QRCODE_TEXT, TAG_DONE)
            : MO(name, TAG_DONE);
        check(object != NULL, name, "object");
        if (!object) return;
        if (kind == QRCODE)
        {
            ULONG status = 0;
            GetAttr(MUIA_QRCode_Status, object, &status);
            check(status == 1, name, "status Ready"); /* MUIV_QRCode_Status_Ready */
        }
        win = MO(MUIC_Window, MUIA_Window_Screen, (ULONG)screen,
            MUIA_Window_RootObject, (ULONG)object, TAG_DONE);
    }
    check(win != NULL, name, "window");
    if (win) show(name, win, hold);
}
int main(int argc, char **argv)
{
    int i;
    LONG hold = argc > 2 ? 100 : 0;
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    if (argc > 1)
        screen = OpenScreenTags(NULL, SA_DisplayID, HIRES_KEY, SA_Width, 640,
            SA_Height, 256, SA_Depth, 2, TAG_DONE);
    for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i++)
    {
        printf("-- %s\n", classes[i].name); fflush(stdout);
        exercise(classes[i].name, classes[i].kind, hold);
    }
    if (screen) CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 5 : 0;
}

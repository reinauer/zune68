/* Exercise the native toolbar classes using their public SDK interfaces.
 * Run normally for the interactive example, or with CHECK for a short
 * construction, attribute, window and disposal workload.
 */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <mui/TheBar_mcc.h>
#include <mui/Toolbar_mcc.h>
#include <stdio.h>
#include <string.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static const char version[] = "$VER: TheBarDemo 1.0 (07.10.2026)";
static struct MUIS_TheBar_Button buttons[] = {
    { 0, 10, "_First", "First toolbar button", 0, 0, NULL, NULL },
    { 1, 20, "_Second", "Second toolbar button", 0, 0, NULL, NULL },
    { MUIV_TheBar_End, 0, NULL, NULL, 0, 0, NULL, NULL }
};
static struct MUIP_Toolbar_Description legacy[] = {
    { TDT_BUTTON, 0, 0, "_Legacy", "Toolbar compatibility wrapper", 0 },
    { TDT_END, 0, 0, NULL, NULL, 0 }
};

static ULONG value(Object *obj, ULONG attr)
{
    ULONG result = 0;
    GetAttr(attr, obj, &result);
    return result;
}

static int run(BOOL check)
{
    Object *bar, *virt, *button, *toolbar, *group, *window, *app;
    ULONG signals = 0, result, opened;
    int passed = 1;

    /* Create containers without children, then transfer ownership only
     * after each constructor succeeds. A failed constructor may return
     * before consuming any of the child tags supplied to it.
     */
    app = MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "TheBarDemo",
        MUIA_Application_Version, version,
        MUIA_Application_Base, "THEBARDEMO", TAG_DONE);
    if (!app) return 0;
    window = MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, "Zune68 toolbar classes",
        MUIA_Window_ID, 0x54424152UL, TAG_DONE);
    if (!window) {
        MUI_DisposeObject(app);
        return 0;
    }
    DoMethod(app, OM_ADDMEMBER, window);
    group = MUI_NewObject(MUIC_Group, TAG_DONE);
    if (!group) {
        MUI_DisposeObject(app);
        return 0;
    }
    SetAttrs(window, MUIA_Window_RootObject, group, TAG_DONE);

    bar = MUI_NewObject(MUIC_TheBar,
        MUIA_TheBar_Buttons, buttons,
        MUIA_TheBar_ViewMode, MUIV_TheBar_ViewMode_Text,
        MUIA_TheBar_IgnoreAppearance, TRUE, TAG_DONE);
    printf("TheBar construct=%d\n", bar != NULL); fflush(stdout);
    virt = MUI_NewObject(MUIC_TheBarVirt,
        MUIA_TheBar_Buttons, buttons,
        MUIA_TheBar_ViewMode, MUIV_TheBar_ViewMode_Text,
        MUIA_TheBar_IgnoreAppearance, TRUE, TAG_DONE);
    printf("TheBarVirt construct=%d\n", virt != NULL); fflush(stdout);
    button = MUI_NewObject(MUIC_TheButton,
        MUIA_TheButton_Label, "Standalone button",
        MUIA_TheButton_ViewMode, MUIV_TheButton_ViewMode_Text, TAG_DONE);
    printf("TheButton construct=%d\n", button != NULL); fflush(stdout);
    toolbar = MUI_NewObject(MUIC_Toolbar,
        MUIA_Toolbar_Description, legacy, TAG_DONE);
    printf("Toolbar construct=%d\n", toolbar != NULL); fflush(stdout);
    if (!bar || !virt || !button || !toolbar) {
        if (bar) MUI_DisposeObject(bar);
        if (virt) MUI_DisposeObject(virt);
        if (button) MUI_DisposeObject(button);
        if (toolbar) MUI_DisposeObject(toolbar);
        MUI_DisposeObject(app);
        return 0;
    }
    result = DoMethod(bar, MUIM_TheBar_GetObject, 10);
    passed &= result != 0;
    if (result) {
        DoMethod(bar, MUIM_TheBar_SetAttr, 10, MUIV_TheBar_Attr_Disabled, TRUE);
        passed &= value((Object *)result, MUIA_Disabled) != 0;
        DoMethod(bar, MUIM_TheBar_SetAttr, 10, MUIV_TheBar_Attr_Disabled, FALSE);
        passed &= value((Object *)result, MUIA_Disabled) == 0;
    }
    printf("button lookup and disable=%s\n", passed ? "PASS" : "FAIL");
    fflush(stdout);
    DoMethod(group, OM_ADDMEMBER, bar);
    DoMethod(group, OM_ADDMEMBER, virt);
    DoMethod(group, OM_ADDMEMBER, button);
    DoMethod(group, OM_ADDMEMBER, toolbar);
    DoMethod(window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
        app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    opened = value(window, MUIA_Window_Open);
    passed &= opened != 0;
    printf("window open=%lu\n", opened); fflush(stdout);
    if (check && opened) {
        for (int frame = 0; frame < 25; frame++) {
            DoMethod(app, MUIM_Application_NewInput, &signals);
            Delay(1);
        }
    } else if (opened) {
        while ((LONG)DoMethod(app, MUIM_Application_NewInput, &signals) !=
               MUIV_Application_ReturnID_Quit) {
            if (signals && (Wait(signals | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C))
                break;
        }
    }
    SetAttrs(window, MUIA_Window_Open, FALSE, TAG_DONE);
    MUI_DisposeObject(app);
    printf("disposed=%s\n", passed ? "PASS" : "FAIL"); fflush(stdout);
    return passed;
}

int main(int argc, char **argv)
{
    BOOL check = argc > 1 && strcmp(argv[1], "CHECK") == 0;
    int passed = 1;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return RETURN_FAIL;
    for (int iteration = 0; iteration < (check ? 3 : 1); iteration++)
        if (!run(check)) { passed = 0; break; }
    CloseLibrary(MUIMasterBase);
    printf("TheBarDemo %s\n", passed ? "PASS" : "FAIL");
    return passed ? RETURN_OK : RETURN_FAIL;
}

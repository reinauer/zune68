/* Observe Colorfield on an isolated screen; never supply a system pen. */
#include <exec/types.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void snapshot(const char *phase, struct Screen *screen,
                     Object *field, LONG spare)
{
    struct ColorMap *cm = screen->ViewPort.ColorMap;
    ULONG pen = 0xdeadbeef, rgb[3], channels[3] = {0, 0, 0};
    ULONG ok = 0, depth, count, i;
    LONG nfree = -1;
    depth = GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH);
    count = depth < 8 ? 1UL << depth : 256;
    if (count > cm->Count) count = cm->Count;
    if (cm->PalExtra) {
        ObtainSemaphoreShared(&cm->PalExtra->pe_Semaphore);
        nfree = cm->PalExtra->pe_NFree;
        ReleaseSemaphore(&cm->PalExtra->pe_Semaphore);
    }
    if (field) {
        ok = GetAttr(MUIA_Colorfield_Pen, field, &pen);
        GetAttr(MUIA_Colorfield_Red, field, &channels[0]);
        GetAttr(MUIA_Colorfield_Green, field, &channels[1]);
        GetAttr(MUIA_Colorfield_Blue, field, &channels[2]);
    }
    printf("%s get=%lu pen=%08lx spare=%ld depth=%lu free=%ld "
           "rgb=%08lx,%08lx,%08lx\n", phase, ok, pen, spare,
           depth, nfree, channels[0], channels[1], channels[2]);
    for (i = 0; i < count; i++) {
        GetRGB32(cm, i, 1, rgb);
        printf("palette %lu %08lx %08lx %08lx\n",
               i, rgb[0], rgb[1], rgb[2]);
    }
    fflush(stdout);
}

static int scenario(struct Screen *screen, int mode)
{
    struct ColorMap *cm = screen->ViewPort.ColorMap;
    Object *field = NULL, *window = NULL, *app = NULL;
    LONG spare;
    ULONG opened = 0;
    int result = 20;
    ULONG initial[3] = {0x33333333, 0x55555555, 0x77777777};
    ULONG changed[3] = {0x99999999, 0x44444444, 0x22222222};
    ULONG reset_rgb[3] = {0x22222222, 0x77777777, 0x44444444};

    printf("case=%d %s\n", mode, mode == 0 ? "automatic" :
           mode == 1 ? "explicit-constructor" : "explicit-live-set");
    snapshot("before-spare", screen, NULL, -1);
    spare = ObtainPen(cm, (ULONG)-1, 0x66666666, 0x22222222,
                      0x44444444, PENF_EXCLUSIVE);
    if (spare == -1) {
        puts("SKIP: no exclusive spare pen available");
        return 0;
    }
    snapshot("spare-owned", screen, NULL, spare);
    if (mode == 1)
        field = MUI_NewObject((char *)MUIC_Colorfield,
            MUIA_Colorfield_Pen, spare, MUIA_Colorfield_RGB, (ULONG)initial,
            MUIA_FixWidth, 32, MUIA_FixHeight, 24, TAG_DONE);
    else
        field = MUI_NewObject((char *)MUIC_Colorfield,
            MUIA_Colorfield_RGB, (ULONG)initial,
            MUIA_FixWidth, 32, MUIA_FixHeight, 24, TAG_DONE);
    if (!field) { puts("FAIL: Colorfield constructor"); goto done; }
    snapshot("before-setup", screen, field, spare);
    window = MUI_NewObject((char *)MUIC_Window,
        MUIA_Window_Screen, (ULONG)screen, MUIA_Window_Borderless, TRUE,
        MUIA_Window_CloseGadget, FALSE, MUIA_Window_SizeGadget, FALSE,
        MUIA_Window_DepthGadget, FALSE, MUIA_Window_DragBar, FALSE,
        MUIA_Window_RootObject, (ULONG)field, TAG_DONE);
    if (!window) { puts("FAIL: Window constructor"); field = NULL; goto done; }
    app = MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Title, (ULONG)"Colorfield lifecycle probe",
        MUIA_Application_Base, (ULONG)"COLORFIELDPROBE",
        MUIA_Application_Window, (ULONG)window, TAG_DONE);
    if (!app) { puts("FAIL: Application constructor"); window = NULL;
                field = NULL; goto done; }
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, window, &opened);
    if (!opened) { puts("FAIL: opening window"); goto done; }
    snapshot("after-setup", screen, field, spare);
    if (mode == 2) {
        SetAttrs(field, MUIA_Colorfield_Pen, spare, TAG_DONE);
        snapshot("after-explicit-set", screen, field, spare);
    }
    SetAttrs(field, MUIA_Colorfield_RGB, changed, TAG_DONE);
    snapshot("after-rgb-change", screen, field, spare);
    SetAttrs(field, MUIA_Colorfield_Pen, (ULONG)-1, TAG_DONE);
    snapshot("after-minus-one", screen, field, spare);
    SetAttrs(field, MUIA_Colorfield_RGB, reset_rgb, TAG_DONE);
    snapshot("after-reset-rgb", screen, field, spare);
    SetAttrs(window, MUIA_Window_Open, FALSE, TAG_DONE);
    snapshot("after-cleanup", screen, field, spare);
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, window, &opened);
    printf("reopen=%lu\n", opened);
    snapshot("after-reopen", screen, field, spare);
    SetAttrs(window, MUIA_Window_Open, FALSE, TAG_DONE);
    result = 0;
done:
    if (app) MUI_DisposeObject(app);
    else if (window) MUI_DisposeObject(window);
    else if (field) MUI_DisposeObject(field);
    snapshot("after-dispose-spare-still-owned", screen, NULL, spare);
    ReleasePen(cm, spare);
    snapshot("after-owner-release", screen, NULL, spare);
    return result;
}

int main(void)
{
    struct Screen *screen;
    int mode, result = 0;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    screen = OpenScreenTags(NULL,
        SA_Depth, 4, SA_Width, 640, SA_Height, 256,
        SA_Quiet, TRUE, SA_Type, CUSTOMSCREEN, SA_SharePens, TRUE,
        SA_Title, (ULONG)"Colorfield lifecycle probe", TAG_DONE);
    if (!screen) { CloseLibrary(MUIMasterBase); return 20; }
    puts("screen=private-custom-640x256x4 share-pens=TRUE probe=3");
    printf("muimaster %u.%u; all pen transitions below are observations\n",
           MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    for (mode = 0; mode < 3 && !result; mode++)
        result = scenario(screen, mode);
    CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("result=%d\n", result);
    return result;
}

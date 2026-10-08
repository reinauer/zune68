/* Observe MUI pen handles on an isolated screen, using original MUI headers.
 * Reference counters below are diagnostic only: WORD entries were verified
 * in ReleasePen of the AmigaOS 3.2.3 ROM used for this comparison. They are
 * private graphics.library data, not a portable application interface.
 */
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
#include <string.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void snapshot(const char *phase, struct Screen *screen)
{
    struct PaletteExtra *extra = screen->ViewPort.ColorMap->PalExtra;
    UWORD counts[16], nfree = 0, nshared = 0;
    unsigned int i;
    if (!extra) { printf("%s no-PaletteExtra\n", phase); return; }
    ObtainSemaphoreShared(&extra->pe_Semaphore);
    nfree = extra->pe_NFree;
    nshared = extra->pe_NShared;
    for (i = 0; i < 16; i++)
        counts[i] = ((UWORD *)extra->pe_RefCnt)[i];
    ReleaseSemaphore(&extra->pe_Semaphore);
    printf("%s free=%u shared=%u refs-os323-words=", phase, nfree, nshared);
    for (i = 0; i < 16; i++) printf("%s%u", i ? "," : "", counts[i]);
    putchar('\n');
    fflush(stdout);
}

static void scenario(struct Screen *screen, struct MUI_RenderInfo *mri,
                     const char *text)
{
    struct MUI_PenSpec spec;
    LONG first, second;
    ULONG rgb[3];
    memset(&spec, 0, sizeof(spec));
    strcpy(spec.buf, text);
    printf("spec=%s\n", text);
    snapshot("before", screen);
    first = MUI_ObtainPen(mri, &spec, 0);
    printf("obtain1=%08lx\n", (ULONG)first);
    if (first != -1 && ((ULONG)first & 0xffff) < 16) {
        GetRGB32(screen->ViewPort.ColorMap, (ULONG)first & 0xffff, 1, rgb);
        printf("rgb=%08lx,%08lx,%08lx\n", rgb[0], rgb[1], rgb[2]);
    }
    snapshot("after-obtain1", screen);
    second = MUI_ObtainPen(mri, &spec, 0);
    printf("obtain2=%08lx\n", (ULONG)second);
    snapshot("after-obtain2", screen);
    if (first != -1) MUI_ReleasePen(mri, first);
    snapshot("after-release1", screen);
    if (second != -1) MUI_ReleasePen(mri, second);
    snapshot("after-release2", screen);
}

int main(void)
{
    static const char *specs[] = {
        "s0", "s1", "s3", "m0", "m2", "p0", "p3",
        "rffa997", "rffffffff,00000000,00000000"
    };
    struct Screen *screen = NULL;
    Object *root = NULL, *window = NULL, *app = NULL;
    ULONG opened = 0;
    unsigned int i;
    int result = 20;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    screen = OpenScreenTags(NULL, SA_Depth, 4, SA_Width, 640,
        SA_Height, 256, SA_Quiet, TRUE, SA_Type, CUSTOMSCREEN,
        SA_SharePens, TRUE, SA_Title, (ULONG)"Pen handle probe", TAG_DONE);
    if (!screen) goto done;
    puts("penhandles=1 screen=private-custom-640x256x4 share-pens=TRUE");
    puts("private counters require the verified AmigaOS 3.2.3 ROM");
    printf("muimaster=%u.%u graphics=%u.%u\n",
        MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision,
        ((struct Library *)GfxBase)->lib_Version,
        ((struct Library *)GfxBase)->lib_Revision);
    snapshot("screen-open", screen);
    root = MUI_NewObject((char *)MUIC_Rectangle,
        MUIA_FixWidth, 32, MUIA_FixHeight, 24, TAG_DONE);
    if (!root) goto done;
    window = MUI_NewObject((char *)MUIC_Window,
        MUIA_Window_Screen, (ULONG)screen,
        MUIA_Window_Borderless, TRUE, MUIA_Window_CloseGadget, FALSE,
        MUIA_Window_SizeGadget, FALSE, MUIA_Window_DepthGadget, FALSE,
        MUIA_Window_DragBar, FALSE,
        MUIA_Window_RootObject, (ULONG)root, TAG_DONE);
    if (!window) { root = NULL; goto done; }
    app = MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Title, (ULONG)"Pen handle probe",
        MUIA_Application_Base, (ULONG)"PENHANDLES",
        MUIA_Application_Window, (ULONG)window, TAG_DONE);
    if (!app) { window = root = NULL; goto done; }
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, window, &opened);
    if (!opened) goto done;
    printf("mui-pens=");
    for (i = 0; i < 8; i++) printf("%s%u", i ? "," : "", _pens(root)[i]);
    putchar('\n');
    snapshot("window-open", screen);
    for (i = 0; i < sizeof(specs) / sizeof(specs[0]); i++)
        scenario(screen, muiRenderInfo(root), specs[i]);
    SetAttrs(window, MUIA_Window_Open, FALSE, TAG_DONE);
    snapshot("window-closed", screen);
    result = 0;
done:
    if (app) MUI_DisposeObject(app);
    else if (window) MUI_DisposeObject(window);
    else if (root) MUI_DisposeObject(root);
    if (screen) {
        snapshot("after-dispose", screen);
        CloseScreen(screen);
    }
    CloseLibrary(MUIMasterBase);
    printf("result=%d\n", result);
    return result;
}

/* Repeated public window/iconify operations, with custom Window hooks.
 * Memory/palette samples are observations: shared caches and other tasks can
 * change them. A falling sample alone does not prove a leak.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG setups, cleanups, failures;

/* Window subclass hooks introduced after the original SDK's public list. */
#define WINDOW_SETUP   0x8042c34cUL
#define WINDOW_CLEANUP 0x8042ab26UL

static ULONG dispatch(struct IClass *cl __asm("a0"),
                      Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    ULONG before = 0, after = 0, result;
    if (msg->MethodID != WINDOW_SETUP && msg->MethodID != WINDOW_CLEANUP)
        return DoSuperMethodA(cl, obj, msg);
    GetAttr(MUIA_Window_Screen, obj, &before);
    result = DoSuperMethodA(cl, obj, msg);
    GetAttr(MUIA_Window_Screen, obj, &after);
    if (!before || !after || before != after) {
        printf("FAIL hook=%08lx screen-before=%08lx after=%08lx\n",
               msg->MethodID, before, after);
        failures++;
    }
    if (msg->MethodID == WINDOW_SETUP) setups++;
    else cleanups++;
    return result;
}

static void settle(Object *app, ULONG ticks)
{
    ULONG signals = 0, i;
    for (i = 0; i < ticks; i++) {
        if (app) DoMethod(app, MUIM_Application_NewInput, &signals);
        Delay(1);
    }
}

static void snapshot(const char *phase, ULONG cycle, struct Screen *screen)
{
    struct PaletteExtra *extra = screen->ViewPort.ColorMap->PalExtra;
    ULONG rgb[12], chip, fast, chipmax, fastmax, chiptotal, fasttotal;
    LONG nfree = -1, nshared = -1;
    if (extra) {
        ObtainSemaphoreShared(&extra->pe_Semaphore);
        nfree = extra->pe_NFree;
        nshared = extra->pe_NShared;
        ReleaseSemaphore(&extra->pe_Semaphore);
    }
    GetRGB32(screen->ViewPort.ColorMap, 0, 4, rgb);
    chip = AvailMem(MEMF_CHIP); fast = AvailMem(MEMF_FAST);
    chipmax = AvailMem(MEMF_CHIP | MEMF_LARGEST);
    fastmax = AvailMem(MEMF_FAST | MEMF_LARGEST);
    chiptotal = AvailMem(MEMF_CHIP | MEMF_TOTAL);
    fasttotal = AvailMem(MEMF_FAST | MEMF_TOTAL);
    printf("sample=%s cycle=%lu chip-free=%lu largest=%lu total=%lu "
           "fast-free=%lu largest=%lu total=%lu pens-free=%ld shared=%ld "
           "setups=%lu cleanups=%lu failures=%lu\n", phase, cycle,
           chip, chipmax, chiptotal, fast, fastmax, fasttotal,
           nfree, nshared, setups, cleanups, failures);
    for (ULONG i = 0; i < 4; i++)
        printf("rgb%lu=%08lx,%08lx,%08lx\n", i,
               rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    fflush(stdout);
}

static BOOL opened(Object *window, BOOL value)
{
    ULONG actual = 0;
    SetAttrs(window, MUIA_Window_Open, value, TAG_DONE);
    GetAttr(MUIA_Window_Open, window, &actual);
    if (!!actual != !!value) {
        printf("FAIL open requested=%d actual=%lu\n", value, actual);
        failures++;
        return FALSE;
    }
    return TRUE;
}

static BOOL cycle(Object *app, Object **windows)
{
    ULONG actual = 0, i;
    for (i = 0; i < 2; i++) if (!opened(windows[i], TRUE)) return FALSE;
    settle(app, 1);
    for (i = 0; i < 2; i++) opened(windows[i], FALSE);
    settle(app, 1);
    for (i = 0; i < 2; i++) if (!opened(windows[i], TRUE)) return FALSE;
    SetAttrs(app, MUIA_Application_Iconified, TRUE, TAG_DONE);
    settle(app, 1);
    GetAttr(MUIA_Application_Iconified, app, &actual);
    if (!actual) { puts("FAIL iconify refused"); failures++; return FALSE; }
    SetAttrs(app, MUIA_Application_Iconified, FALSE, TAG_DONE);
    settle(app, 1);
    GetAttr(MUIA_Application_Iconified, app, &actual);
    if (actual) { puts("FAIL uniconify refused"); failures++; return FALSE; }
    for (i = 0; i < 2; i++) {
        GetAttr(MUIA_Window_Open, windows[i], &actual);
        if (!actual) { puts("FAIL window not restored"); failures++; return FALSE; }
        opened(windows[i], FALSE);
    }
    settle(app, 1);
    return failures == 0;
}

int main(void)
{
    struct Screen *screen = NULL;
    struct MUI_CustomClass *cc = NULL;
    Object *app = NULL, *windows[2] = {NULL, NULL}, *root;
    ULONG i, completed = 0;
    int result = 20;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    screen = LockPubScreen("Workbench");
    if (!screen) goto done;
    printf("windowcycles=1 library=%u.%u windows=2 warmup=5 measured=100\n",
           MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    puts("Each cycle: open/close both, reopen, iconify/uniconify own app, close.");
    puts("Samples follow settling; memory changes may include shared caches.");
    settle(NULL, 10);
    snapshot("before-objects", 0, screen);
    cc = MUI_CreateCustomClass(NULL, (char *)MUIC_Window,
                              NULL, 0, (APTR)dispatch);
    if (!cc) goto done;
    for (i = 0; i < 2; i++) {
        root = MUI_NewObject((char *)MUIC_Text,
            MUIA_Text_Contents, (ULONG)"Window lifecycle stress",
            MUIA_FixWidth, 180, MUIA_FixHeight, 24, TAG_DONE);
        if (!root) goto done;
        windows[i] = NewObject(cc->mcc_Class, NULL,
            MUIA_Window_Title, (ULONG)"Window lifecycle stress",
            MUIA_Window_PublicScreen, (ULONG)"Workbench",
            MUIA_Window_RootObject, (ULONG)root, TAG_DONE);
        if (!windows[i]) goto done;
    }
    app = MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Title, (ULONG)"Window lifecycle stress",
        MUIA_Application_Base, (ULONG)"WINDOWCYCLES",
        MUIA_Application_Window, (ULONG)windows[0],
        MUIA_Application_Window, (ULONG)windows[1], TAG_DONE);
    if (!app) { windows[0] = windows[1] = NULL; goto done; }
    for (i = 0; i < 5; i++) if (!cycle(app, windows)) goto done;
    settle(app, 10);
    snapshot("after-warmup", 0, screen);
    for (i = 1; i <= 100; i++) {
        if (!cycle(app, windows)) goto done;
        completed = i;
        if (i == 10 || i == 100) {
            settle(app, 10);
            snapshot("measured", i, screen);
        }
    }
    if (setups != cleanups) {
        puts("FAIL setup/cleanup counts differ"); failures++;
    }
    result = failures ? 20 : 0;
done:
    if (app) MUI_DisposeObject(app);
    else for (i = 0; i < 2; i++)
        if (windows[i]) MUI_DisposeObject(windows[i]);
    if (cc) MUI_DeleteCustomClass(cc);
    if (screen) {
        settle(NULL, 10);
        snapshot("after-dispose", completed, screen);
        UnlockPubScreen(NULL, screen);
    }
    CloseLibrary(MUIMasterBase);
    printf("result=%d\n", result);
    return result;
}

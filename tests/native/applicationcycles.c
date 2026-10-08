/* Compare whole application lifetimes with and without pending methods.
 * Memory samples are observations: other tasks and shared caches can change
 * them. A repeatable slope after warmup matters more than the first sample.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define PROBE_METHOD 0x81726831UL
static ULONG calls, failures, created, disposed, queued;
static ULONG baseline_chip, baseline_fast;

static ULONG dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == PROBE_METHOD) {
        calls++;
        return 1;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void snapshot(const char *phase, ULONG cycle, BOOL baseline)
{
    ULONG chip, fast, chipmax, fastmax;
    /* Allow Workbench and DOS notification housekeeping to settle. */
    Delay(10);
    chip = AvailMem(MEMF_CHIP);
    fast = AvailMem(MEMF_FAST);
    chipmax = AvailMem(MEMF_CHIP | MEMF_LARGEST);
    fastmax = AvailMem(MEMF_FAST | MEMF_LARGEST);
    if (baseline) {
        baseline_chip = chip;
        baseline_fast = fast;
    }
    printf("sample=%s cycle=%lu chip-free=%lu largest=%lu delta=%ld "
           "fast-free=%lu largest=%lu delta=%ld created=%lu disposed=%lu "
           "queued=%lu calls=%lu failures=%lu\n", phase, cycle,
           chip, chipmax, (LONG)chip - (LONG)baseline_chip,
           fast, fastmax, (LONG)fast - (LONG)baseline_fast,
           created, disposed, queued, calls, failures);
    fflush(stdout);
}

static BOOL cycle(Object *destination, BOOL pending)
{
    Object *app = MUI_NewObject((char *)MUIC_Application,
        MUIA_Application_Title, (ULONG)"Application lifetime stress",
        MUIA_Application_Base, (ULONG)"APPLICATIONCYCLES", TAG_DONE);
    ULONG before = calls;
    BOOL ok = TRUE;

    if (!app) {
        puts("FAIL application construction");
        failures++;
        return FALSE;
    }
    created++;
    if (pending) {
        /* Stay within the original SDK's seven-word PushMethod limit. */
        ULONG ids[3];
        ids[0] = DoMethod(app, MUIM_Application_PushMethod,
                         destination, 1, PROBE_METHOD);
        ids[1] = DoMethod(app, MUIM_Application_PushMethod,
                         destination, 2, PROBE_METHOD, 0x11223344UL);
        ids[2] = DoMethod(app, MUIM_Application_PushMethod,
                         destination, 4, PROBE_METHOD,
                         0x55667788UL, 0x99aabbccUL, 0xddeeff00UL);
        for (ULONG i = 0; i < 3; i++) {
            if (ids[i]) queued++;
            else {
                puts("FAIL PushMethod refused");
                failures++;
                ok = FALSE;
            }
        }
    }
    /* Deliberately no NewInput: these destinations must not be invoked. */
    MUI_DisposeObject(app);
    disposed++;
    if (calls != before) {
        puts("FAIL pending method invoked during disposal");
        failures++;
        ok = FALSE;
    }
    return ok;
}

int main(void)
{
    struct MUI_CustomClass *cc = NULL;
    Object *destination = NULL;
    ULONG i, mode;
    int result = 20;

    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    printf("applicationcycles=1 library=%u.%u warmup=10-per-mode "
           "measured=1000-per-mode windows=0\n",
           MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    puts("One persistent custom Notify destination; fresh application each cycle.");
    puts("Compare control and queued slopes after warmup, not only first-use loss.");
    cc = MUI_CreateCustomClass(NULL, (char *)MUIC_Notify, NULL, 0,
                              (APTR)dispatch);
    if (!cc) goto done;
    destination = NewObject(cc->mcc_Class, NULL, TAG_DONE);
    if (!destination) goto done;
    /* Check the destination hook without executing a queued method. */
    DoMethod(destination, PROBE_METHOD);
    if (calls != 1) { failures++; goto done; }
    calls = 0;
    snapshot("before-warmup", 0, TRUE);
    for (mode = 0; mode < 2; mode++)
        for (i = 0; i < 10; i++)
            if (!cycle(destination, mode != 0)) goto done;
    snapshot("after-warmup", 0, FALSE);
    for (mode = 0; mode < 2; mode++) {
        snapshot(mode ? "queued-baseline" : "control-baseline", 0, TRUE);
        for (i = 1; i <= 1000; i++) {
            if (!cycle(destination, mode != 0)) goto done;
            if (i == 10 || i == 100 || i == 1000)
                snapshot(mode ? "queued" : "control", i, FALSE);
        }
    }
    result = failures || calls || created != disposed ? 20 : 0;
done:
    if (destination) MUI_DisposeObject(destination);
    if (cc) MUI_DeleteCustomClass(cc);
    snapshot("after-destination-dispose", 0, FALSE);
    CloseLibrary(MUIMasterBase);
    printf("result=%d\n", result);
    return result;
}

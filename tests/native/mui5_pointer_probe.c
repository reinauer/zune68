/* Compare with the MUI 5 OS3 SDK and runtime. Input comes through input.device.
 * Build with -m68000 -msoft-float -noixemul and -Wl,-u,___stkinit.
 * The temporary Intuition vector hook records only this test's window and
 * chains to the original function. It is restored before exiting.
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <libraries/mui.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/pointerclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

_Static_assert(MUIA_PointerType == 0x8042b467UL, "pointer attribute ID");
_Static_assert(MUIM_WhichPointerType == 0x8042e212UL, "pointer method ID");
_Static_assert(sizeof(struct MUIP_WhichPointerType) == 12, "pointer message");
_Static_assert(offsetof(struct MUIP_WhichPointerType, mx) == 4, "mouse X");
_Static_assert(offsetof(struct MUIP_WhichPointerType, my) == 8, "mouse Y");

struct Library *MUIMasterBase;
ULONG __stack = 65536;
APTR pointer_probe_original;
static struct Window *watched;
static ULONG setcalls, lastbusy, lastpointer, queries;
static LONG query_x, query_y;
static BOOL override;
static ULONG override_type;
static Object *close_on_query;
static int failures;

/* Avoid allocation, I/O and library calls inside the system vector hook. */
void pointer_probe_record(struct Window *win, struct TagItem *tags)
{
    int count = 0;
    if (win != watched) return;
    setcalls++;
    lastbusy = lastpointer = 0;
    while (tags && tags->ti_Tag && count++ < 64)
    {
        if (tags->ti_Tag == TAG_MORE) { tags = (APTR)tags->ti_Data; continue; }
        if (tags->ti_Tag == TAG_SKIP) { tags += tags->ti_Data + 1; continue; }
        if (tags->ti_Tag == WA_BusyPointer) lastbusy = tags->ti_Data;
        if (tags->ti_Tag == WA_Pointer) lastpointer = tags->ti_Data;
        tags++;
    }
}

void pointer_probe_hook(void);
__asm__(".text\n.globl _pointer_probe_hook\n_pointer_probe_hook:\n"
    "movem.l d0-d1/a0-a1/a6,-(sp)\nmove.l a1,-(sp)\nmove.l a0,-(sp)\n"
    "jsr _pointer_probe_record\naddq.l #8,sp\n"
    "movem.l (sp)+,d0-d1/a0-a1/a6\n"
    "move.l _pointer_probe_original,-(sp)\nrts\n");

static ULONG dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_WhichPointerType)
    {
        struct MUIP_WhichPointerType *m = (APTR)msg;
        queries++;
        query_x = m->mx;
        query_y = m->my;
        if (close_on_query)
        {
            Object *win = close_on_query;
            close_on_query = NULL;
            SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
            return MUIV_PointerType_Normal;
        }
        if (override) return override_type;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void check(BOOL condition, const char *what)
{
    if (!condition) { failures++; printf("FAIL %s\n", what); }
}

static void pump(Object *app)
{
    int i;
    for (i = 0; i < 6; i++)
    {
        Delay(2);
        DoMethod(app, MUIM_Application_InputBuffered);
    }
}

static void move(Object *app, struct IOStdReq *io, LONG x, LONG y)
{
    struct InputEvent ev = {0};
    struct IEPointerPixel pos = {0};
    pos.iepp_Screen = watched->WScreen;
    pos.iepp_Position.X = watched->LeftEdge + x;
    pos.iepp_Position.Y = watched->TopEdge + y;
    ev.ie_Class = IECLASS_NEWPOINTERPOS;
    ev.ie_SubClass = IESUBCLASS_PIXEL;
    ev.ie_EventAddress = &pos;
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &ev;
    io->io_Length = sizeof(ev);
    check(DoIO((struct IORequest *)io) == 0, "input injection");
    pump(app);
}

static void attr(Object *obj, ULONG type)
{
    ULONG got = 0;
    SetAttrs(obj, MUIA_PointerType, type, TAG_DONE);
    check(GetAttr(MUIA_PointerType, obj, &got) && got == type, "attribute");
    check(DoMethod(obj, MUIM_WhichPointerType, 17, 23) == type, "method");
}

int main(int argc, char **argv)
{
    struct MUI_CustomClass *cc = NULL;
    Object *child = NULL, *other, *root, *win, *app = NULL, *custom = NULL;
    struct MsgPort *port = NULL;
    struct IOStdReq *io = NULL;
    struct Screen *screen = NULL;
    BOOL device = FALSE, hooked = FALSE;
    struct BitMap bitmap = {0};
    UWORD planes[32] = {0xffff, 0x8001, 0xffff};
    struct TagItem empty[] = {{TAG_DONE, 0}};
    LONG x, y;
    ULONG i, got, before;

    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    printf("library=%u.%u\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);
    fflush(stdout);
    /* Permit a 68000 run without a CPU-specific Workbench installation. */
    if (argc > 1 && !strcmp(argv[1], "screen"))
    {
        screen = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256,
            SA_Depth, 2, SA_Title, "Pointer probe", TAG_DONE);
        if (!screen) goto done;
    }
    cc = MUI_CreateCustomClass(NULL, MUIC_Rectangle, NULL, 0, (APTR)dispatch);
    if (!cc) goto done;
    child = NewObjectA(cc->mcc_Class, NULL, empty);
    if (!child) goto done;
    got = 0;
    check(GetAttr(MUIA_PointerType, child, &got) && got == (ULONG)-1,
        "default Parent");
    check(DoMethod(child, MUIM_WhichPointerType, 17, 23) == (ULONG)-1,
        "method preserves Parent");
    for (i = 0; i < MUIV_PointerType_Count; i++) attr(child, i);
    attr(child, -1);
    attr(child, -2); /* Storage is not an enum clamp. Never render invalid data. */
    attr(child, -1);
    other = MUI_NewObjectA(MUIC_Rectangle, empty);
    if (!other) goto done;
    {
        struct TagItem rt[] = {{MUIA_Group_Child, (ULONG)child},
            {MUIA_Group_Child, (ULONG)other}, {TAG_DONE, 0}};
        root = MUI_NewObjectA(MUIC_Group, rt);
    }
    if (!root) { MUI_DisposeObject(other); goto done; }
    SetAttrs(root, MUIA_Group_Forward, FALSE,
        MUIA_PointerType, MUIV_PointerType_Link, TAG_DONE);
    check(DoMethod(child, MUIM_WhichPointerType, 17, 23) == (ULONG)-1,
        "inheritance resolved by window");
    {
        struct TagItem wt[] = {{MUIA_Window_Title, (ULONG)"Pointer probe"},
            {MUIA_Window_RootObject, (ULONG)root}, {MUIA_Window_Width, 240},
            {MUIA_Window_Height, 140},
            {MUIA_Window_Screen, (ULONG)screen}, {TAG_DONE, 0}};
        win = MUI_NewObjectA(MUIC_Window, wt);
    }
    if (!win) { MUI_DisposeObject(root); child = NULL; goto done; }
    {
        struct TagItem at[] = {{MUIA_Application_Window, (ULONG)win},
            {TAG_DONE, 0}};
        app = MUI_NewObjectA(MUIC_Application, at);
    }
    if (!app) { MUI_DisposeObject(win); child = NULL; goto done; }
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Window, win, (ULONG *)&watched);
    if (!watched) goto done;
    pump(app);
    port = CreateMsgPort();
    if (!port) goto done;
    io = (APTR)CreateIORequest(port, sizeof(*io));
    if (!io || OpenDevice("input.device", 0, (struct IORequest *)io, 0))
        goto done;
    device = TRUE;
    bitmap.BytesPerRow = 2; bitmap.Rows = 16; bitmap.Depth = 2;
    bitmap.Planes[0] = (PLANEPTR)planes;
    bitmap.Planes[1] = (PLANEPTR)(planes + 16);
    {
        struct TagItem pt[] = {{POINTERA_BitMap, (ULONG)&bitmap},
            {POINTERA_WordWidth, 1}, {TAG_DONE, 0}};
        custom = NewObjectA(NULL, "pointerclass", pt);
    }
    if (!custom) goto done;
    Forbid();
    pointer_probe_original = SetFunction((struct Library *)IntuitionBase,
        -816, (APTR)pointer_probe_hook);
    Permit();
    hooked = TRUE;
    x = _left(child) + 4; y = _top(child) + 4;
    move(app, io, 1, 1);
    queries = 0;
    move(app, io, x, y);
    check(queries && query_x == x && query_y == y, "window coordinates");
    check(lastpointer && !lastbusy, "inherited link image");
    for (i = 0; i < MUIV_PointerType_Count; i++)
    {
        attr(child, i);
        move(app, io, x + 2 + (i & 1), y);
        if (i == MUIV_PointerType_Normal)
            check(!lastpointer && !lastbusy, "normal system pointer");
        else if (i == MUIV_PointerType_Busy)
            check(lastbusy != 0, "busy system pointer");
        else
            check(lastpointer && !lastbusy, "built-in image");
    }
    attr(child, (ULONG)custom);
    move(app, io, x + 2, y);
    check(lastpointer == (ULONG)custom, "borrowed pointer image");
    override = TRUE; override_type = MUIV_PointerType_Normal;
    move(app, io, x + 3, y);
    check(!lastpointer && !lastbusy, "custom method override");
    override = FALSE;
    move(app, io, x + 4, y);
    check(lastpointer == (ULONG)custom, "custom image restored");
    SetAttrs(win, MUIA_Window_Sleep, TRUE, TAG_DONE);
    SetAttrs(win, MUIA_Window_Sleep, TRUE, TAG_DONE);
    before = queries;
    move(app, io, x + 5, y);
    check(lastbusy && queries == before, "sleep suppresses hover queries");
    SetAttrs(win, MUIA_Window_Sleep, FALSE, TAG_DONE);
    move(app, io, x + 6, y);
    check(lastbusy && queries == before, "nested sleep");
    SetAttrs(win, MUIA_Window_Sleep, FALSE, TAG_DONE);
    move(app, io, 1, 1);
    check(!lastpointer && !lastbusy, "border resets pointer");
    move(app, io, x, y);
    check(lastpointer == (ULONG)custom, "wake and reenter");
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Window, win, (ULONG *)&watched);
    check(watched != NULL, "reopen");
    if (watched)
    {
        move(app, io, 1, 1);
        move(app, io, _left(child) + 4, _top(child) + 4);
        check(lastpointer == (ULONG)custom, "borrowed pointer survives close");
        close_on_query = win;
        move(app, io, _left(child) + 5, _top(child) + 4);
        got = 1;
        GetAttr(MUIA_Window_Open, win, &got);
        check(!got && !close_on_query, "close from pointer override");
    }
    printf("pointer tests: failures=%d sets=%lu queries=%lu\n",
        failures, setcalls, queries);
    fflush(stdout);

done:
    if (!hooked) { puts("FAIL setup"); failures++; }
    if (app) MUI_DisposeObject(app);
    else if (child) MUI_DisposeObject(child);
    if (hooked)
    {
        Forbid();
        SetFunction((struct Library *)IntuitionBase, -816, pointer_probe_original);
        Permit();
    }
    if (custom) DisposeObject(custom);
    if (device) CloseDevice((struct IORequest *)io);
    if (io) DeleteIORequest((struct IORequest *)io);
    if (port) DeleteMsgPort(port);
    if (cc) MUI_DeleteCustomClass(cc);
    if (screen) CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 20 : 0;
}

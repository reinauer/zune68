/* Inspect and exercise only the local IBrowse fixture, using input.device. */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <intuition/intuitionbase.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include "mui5_fixture.h"
struct Library *MUIMasterBase, *KeymapBase;
extern struct IntuitionBase *IntuitionBase;
ULONG __stack = 65536;
static Object *url, *form;
static unsigned strings, newstrings;
static struct Window *window;
static void walk(Object *obj, unsigned depth)
{
    struct IClass *cl;
    struct List *children = NULL;
    BOOL group = FALSE, string = FALSE, legacy = FALSE;
    if (depth > 32) return;
    for (cl = OCLASS(obj); cl; cl = cl->cl_Super)
        if (cl->cl_ID)
        {
            if (!strcmp((char *)cl->cl_ID, MUIC_Group)) group = TRUE;
            if (!strcmp((char *)cl->cl_ID, MUIC_String)) string = TRUE;
            if (!strcmp((char *)cl->cl_ID, "Newstring.mcc")) legacy = TRUE;
        }
    if (string || legacy)
    {
        STRPTR contents = NULL;
        if (string) strings++; else newstrings++;
        printf("string ancestry:");
        for (cl = OCLASS(obj); cl; cl = cl->cl_Super)
            if (cl->cl_ID) printf(" %s", (char *)cl->cl_ID);
        puts("");
        GetAttr(MUIA_String_Contents, obj, (ULONG *)&contents);
        if (contents && !strncmp((char *)contents, "file:///Bench:mui5-", 18)) url = obj;
        if (contents && !strcmp((char *)contents, "seed")) form = obj;
    }
    if (group && GetAttr(MUIA_Group_ChildList, obj, (ULONG *)&children) && children)
    {
        APTR state = children->lh_Head;
        Object *child;
        while ((child = NextObject(&state))) walk(child, depth + 1);
    }
}
static void event(struct IOStdReq *io, ULONG type, ULONG code, ULONG qual,
    LONG x, LONG y)
{
    struct InputEvent ev = {0};
    struct IEPointerPixel pos = {0};
    ev.ie_Class = type; ev.ie_Code = code; ev.ie_Qualifier = qual;
    if (type == IECLASS_NEWPOINTERPOS)
    {
        pos.iepp_Screen = window->WScreen;
        pos.iepp_Position.X = window->LeftEdge + x;
        pos.iepp_Position.Y = window->TopEdge + y;
        ev.ie_SubClass = IESUBCLASS_PIXEL; ev.ie_EventAddress = &pos;
    }
    io->io_Command = IND_WRITEEVENT; io->io_Data = &ev; io->io_Length = sizeof(ev);
    DoIO((APTR)io);
    Delay(3);
}
static void key(struct IOStdReq *io, ULONG code, ULONG qual)
{
    event(io, IECLASS_RAWKEY, code, qual, 0, 0);
    event(io, IECLASS_RAWKEY, code | IECODE_UP_PREFIX, qual, 0, 0);
}
static void edit(struct IOStdReq *io, Object *obj, const char *text)
{
    UBYTE map[8];
    event(io, IECLASS_NEWPOINTERPOS, 0, 0, _mleft(obj) + 8, _mtop(obj) + 3);
    event(io, IECLASS_RAWMOUSE, IECODE_LBUTTON, 0, 0, 0);
    event(io, IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
    key(io, 0x4f, IEQUALIFIER_LSHIFT); /* beginning */
    key(io, 0x46, IEQUALIFIER_LSHIFT); /* delete to end */
    if (obj == url)
    {
        STRPTR current = NULL;
        GetAttr(MUIA_String_Contents, obj, (ULONG *)&current);
        printf("url after clear=[%.100s]\n", current ? (char *)current : "NULL");
    }
    while (*text)
    {
        LONG count = MapANSI((STRPTR)text++, 1, map, sizeof(map), NULL);
        for (LONG i = 0; i < count; i++) key(io, map[2*i], map[2*i+1]);
    }
}
int main(int argc, char **argv)
{
    struct Screen *screen;
    Object *root = NULL;
    struct MsgPort *port;
    struct IOStdReq *io;
    STRPTR contents = NULL;
    int failures = 0;
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    KeymapBase = OpenLibrary("keymap.library", 37);
    if (!MUIMasterBase || !KeymapBase) return 20;
    Forbid();
    for (screen = IntuitionBase->FirstScreen; screen && !window; screen = screen->NextScreen)
        for (struct Window *w = screen->FirstWindow; w; w = w->NextWindow)
        {
            if (w->Title) printf("guest window=%.80s data=%p\n", (char *)w->Title, w->UserData);
            if (w->Title && (strstr((char *)w->Title, "MUI5 fixture") || strstr((char *)w->Title, "IBrowse")) && w->UserData)
            { window = w; break; }
        }
    if (window)
    {
        struct MUI5Fixture *fixture = (APTR)FindPort(MUI5_FIXTURE_PORT);
        struct List *windows = NULL;
        if (fixture && fixture->magic == MUI5_FIXTURE_MAGIC && fixture->application)
            GetAttr(MUIA_Application_WindowList, fixture->application, (ULONG *)&windows);
        if (windows)
        {
            APTR state = windows->lh_Head;
            Object *obj;
            while ((obj = NextObject(&state)))
            {
                struct Window *native = NULL;
                GetAttr(MUIA_Window_Window, obj, (ULONG *)&native);
                if (native == window)
                    GetAttr(MUIA_Window_RootObject, obj, (ULONG *)&root);
            }
        }
    }
    if (root) walk(root, 0);
    printf("window=%p root=%p\n", window, root);
    Permit();
    printf("builtin-strings=%u newstring=%u url=%d form=%d\n",
        strings, newstrings, url != NULL, form != NULL);
    fflush(stdout);
    if (!url || !form || (argc > 1 && !strcmp(argv[1], "modern") && newstrings))
    { failures++; goto done; }
    port = CreateMsgPort();
    io = port ? (APTR)CreateIORequest(port, sizeof(*io)) : NULL;
    if (!io || OpenDevice("input.device", 0, (APTR)io, 0)) return 20;
    edit(io, form, "edited");
    Forbid();
    GetAttr(MUIA_String_Contents, form, (ULONG *)&contents);
    if (!contents || strcmp((char *)contents, "edited")) failures++;
    Permit();
    printf("form-edit=%s\n", failures ? "FAIL" : "PASS");
    edit(io, url, "file:///Bench:mui5-second.html");
    Forbid();
    GetAttr(MUIA_String_Contents, url, (ULONG *)&contents);
    if (!contents || strcmp((char *)contents, "file:///Bench:mui5-second.html")) failures++;
    Permit();
    printf("url-edit=%s contents=[%.100s]\n", failures ? "FAIL" : "PASS", contents ? (char *)contents : "NULL");
    key(io, 0x44, 0); /* acknowledge and navigate */
    Delay(100);
    CloseDevice((APTR)io); DeleteIORequest((APTR)io); DeleteMsgPort(port);
done:
    CloseLibrary(KeymapBase); CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 20 : 0;
}

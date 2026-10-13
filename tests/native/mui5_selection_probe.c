/* Input and rendering coverage for Selectgroup and Textdata. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
static int failures;
static void check(BOOL ok, const char *label)
{ if (!ok) { printf("FAIL %s\n", label); failures++; } }
static ULONG attr(Object *o, ULONG a)
{ ULONG v = 0xdeadbeef; GetAttr(a, o, &v); return v; }
static ULONG pixels(Object *o)
{
    ULONG hash = 2166136261UL;
    for (LONG y = _mtop(o); y <= _mbottom(o); y++)
        for (LONG x = _mleft(o); x <= _mright(o); x++)
            hash = (hash ^ ReadPixel(_rp(o), x, y)) * 16777619UL;
    return hash;
}
static struct Window *native;
static void input(Object *app, ULONG type, ULONG code, ULONG qual, LONG x, LONG y)
{
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(APTR)CreateIORequest(port,sizeof(*io)):NULL;
    if(io && !OpenDevice("input.device",0,(APTR)io,0)) {
        struct InputEvent event={0}; struct IEPointerPixel pixel={0};
        event.ie_Class=type;event.ie_Code=code;event.ie_Qualifier=qual;
        if(type==IECLASS_NEWPOINTERPOS){pixel.iepp_Screen=native->WScreen;
            pixel.iepp_Position.X=native->LeftEdge+x;pixel.iepp_Position.Y=native->TopEdge+y;
            event.ie_SubClass=IESUBCLASS_PIXEL;event.ie_EventAddress=&pixel;}
        io->io_Command=IND_WRITEEVENT;io->io_Data=&event;io->io_Length=sizeof(event);
        DoIO((APTR)io);CloseDevice((APTR)io);
    }
    if(io)DeleteIORequest((APTR)io);
    if(port)DeleteMsgPort(port);
    for(int j=0;j<5;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
}
int main(int argc, char **argv)
{
    struct Screen *screen = NULL;
    Object *buttons[3], *labels[3], *sel, *text, *expected, *win, *app, *edit;
    setvbuf(stdout, NULL, _IONBF, 0);
    MUIMasterBase = OpenLibrary("muimaster.library", 21);
    if (!MUIMasterBase) return 20;
    if (argc > 1 && !strcmp(argv[1], "screen"))
        screen = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2, TAG_DONE);
    for (int i = 0; i < 3; i++)
    {
        buttons[i] = MO(MUIC_Image, MUIA_Image_Spec, MUII_RadioButton,
            MUIA_Image_FontMatch, TRUE, MUIA_ShowSelState, FALSE,
            MUIA_InputMode, MUIV_InputMode_Immediate,
            MUIA_UserData, MUIA_Selectgroup_Active, TAG_DONE);
        labels[i] = MO(MUIC_Text, MUIA_Text_Contents, "Option", TAG_DONE);
    }
    sel = MO(MUIC_Selectgroup, MUIA_Group_Columns, 2,
        MUIA_Selectgroup_Active, 1, MUIA_CycleChain, 1,
        MUIA_Group_Child, buttons[0], MUIA_Group_Child, labels[0],
        MUIA_Group_Child, buttons[1], MUIA_Group_Child, labels[1],
        MUIA_Group_Child, buttons[2], MUIA_Group_Child, labels[2], TAG_DONE);
    check(sel != NULL, "Selectgroup creation");
    if (!sel) { CloseLibrary(MUIMasterBase); return 20; }
    check(attr(sel, MUIA_Selectgroup_Active) == 1, "initial selection");
    Object *data = MO(MUIC_Textdata, MUIA_Textdata_Contents, "A\xc3\xa4",
        MUIA_Textdata_Encoding, MIBENUM_UTF_8, TAG_DONE);
    text = MO(MUIC_Text, MUIA_Text_Data, data,
        MUIA_Font, MUIV_Font_Fixed, MUIA_FixWidth, 80, TAG_DONE);
    expected = MO(MUIC_Text, MUIA_Text_Contents, "A\xe4",
        MUIA_Font, MUIV_Font_Fixed, MUIA_FixWidth, 80, TAG_DONE);
    Object *row = MO(MUIC_Group, MUIA_Group_Horiz, TRUE,
        MUIA_Group_Child, text, MUIA_Group_Child, expected, TAG_DONE);
    edit = MO(MUIC_String, MUIA_String_Contents, "cursor", MUIA_CycleChain, 1, TAG_DONE);
    Object *root = MO(MUIC_Group, MUIA_Group_Child, sel,
        MUIA_Group_Child, row, MUIA_Group_Child, edit, TAG_DONE);
    win = MO(MUIC_Window, MUIA_Window_Title, "Data input",
        MUIA_Window_Screen, screen, MUIA_Window_RootObject, root, TAG_DONE);
    app = MO(MUIC_Application, MUIA_Application_Window, win, TAG_DONE);
    check(attr(win, MUIA_Window_Opacity) == 255, "default window opacity");
    SetAttrs(win, MUIA_Window_Opacity, 128, TAG_DONE);
    check(attr(win, MUIA_Window_Opacity) == 128, "stored OS3 window opacity");
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    check(attr(win, MUIA_Window_Open), "window opens");
    GetAttr(MUIA_Window_Window, win, (ULONG *)&native);
    if (native)
    {
        for (int i=0;i<4;i++) { DoMethod(app,MUIM_Application_InputBuffered); Delay(2); }
        SetAttrs(sel, MUIA_Selectgroup_Active, MUIV_Selectgroup_Active_Next, TAG_DONE);
        printf("next active=%ld\n", (LONG)attr(sel, MUIA_Selectgroup_Active));
        if (strstr((char *)MUIMasterBase->lib_IdString, "Zune68"))
            check(attr(sel, MUIA_Selectgroup_Active) == 2, "next selection");
        SetAttrs(sel, MUIA_Selectgroup_Active, MUIV_Selectgroup_Active_Next, TAG_DONE);
        printf("next again active=%ld\n", (LONG)attr(sel, MUIA_Selectgroup_Active));
        if (strstr((char *)MUIMasterBase->lib_IdString, "Zune68"))
            check(attr(sel, MUIA_Selectgroup_Active) == 0, "next wraps");
        SetAttrs(sel, MUIA_Selectgroup_Active, MUIV_Selectgroup_Active_Prev, TAG_DONE);
        printf("previous active=%ld\n", (LONG)attr(sel, MUIA_Selectgroup_Active));
        if (strstr((char *)MUIMasterBase->lib_IdString, "Zune68"))
            check(attr(sel, MUIA_Selectgroup_Active) == 2, "previous wraps");
        if (strstr((char *)MUIMasterBase->lib_IdString, "Zune68"))
        {
            ULONG rgb[3];
            GetRGB32(native->WScreen->ViewPort.ColorMap, _pens(text)[MPEN_TEXT], 1, rgb);
            check(attr(text, MUIA_TextColor) == (((rgb[0] >> 8) & 0xff0000) |
                ((rgb[1] >> 16) & 0xff00) | (rgb[2] >> 24)), "TextColor matches screen pen");
            check(DoMethod(text, MUIM_Relayout, 0), "visible object relayout");
        }
        check(pixels(text) == pixels(expected), "Textdata rendering matches converted text");
        LONG x = _left(labels[0]) + 2, y = _top(labels[0]) + 2;
        input(app, IECLASS_NEWPOINTERPOS, 0, 0, x, y);
        input(app, IECLASS_RAWMOUSE, IECODE_LBUTTON, 0, x, y);
        input(app, IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, 0, x, y);
        check(attr(sel, MUIA_Selectgroup_Active) == 0, "label click selects its option");
        check(attr(buttons[0], MUIA_Selected) && !attr(buttons[1], MUIA_Selected) &&
            !attr(buttons[2], MUIA_Selected), "radio images reflect selection");
        if (strstr((char *)MUIMasterBase->lib_IdString, "Zune68"))
        {
            SetAttrs(win, MUIA_Window_ActiveObject, edit, TAG_DONE);
            input(app, IECLASS_RAWKEY, 0x4e, 0, 0, 0);
            input(app, IECLASS_RAWKEY, 0x4e | IECODE_UP_PREFIX, 0, 0, 0);
            check(attr(sel, MUIA_Selectgroup_Active) == 0,
                "arrows in another object leave selection unchanged");
            SetAttrs(win, MUIA_Window_ActiveObject, sel, TAG_DONE);
            input(app, IECLASS_RAWKEY, 0x4e, 0, 0, 0);
            input(app, IECLASS_RAWKEY, 0x4e | IECODE_UP_PREFIX, 0, 0, 0);
            check(attr(sel, MUIA_Selectgroup_Active) == 1,
                "focused selection accepts arrow keys");
        }
    }
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    if (strstr((char *)MUIMasterBase->lib_IdString, "Zune68"))
    {
        check(attr(text, MUIA_TextColor) == 0, "TextColor cleared after cleanup");
        check(!DoMethod(text, MUIM_Relayout, 0), "hidden object relayout rejected");
    }
    MUI_DisposeObject(app);
    if (screen) CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 20 : 0;
}

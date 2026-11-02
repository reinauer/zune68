/* Textinput API of String.mui and the Textinput.mcc/Textinputscroll.mcc
 * classes, with the results of MUI 21.227. Key input goes through
 * input.device to a String subclass that logs the Textinput methods MUI
 * calls. "screen" opens a private planar screen. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <graphics/modeid.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define TI(x) MCC_TI_ID(x)
#define MO(name, ...) \
    ({ ULONG tags[] = {__VA_ARGS__}; MUI_NewObjectA(name, (struct TagItem *)tags); })
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static Object *app;
static struct IOStdReq *io;
static int failures;

static ULONG g(Object *o, ULONG id) { ULONG v = 0xdeadbeef; GetAttr(id, o, &v); return v; }
static const char *s(ULONG v) { return v && v != 0xdeadbeef ? (const char *)v : "(null)"; }

/* contents, cursor, inclusive mark start/end */
static void expect(const char *label, Object *o, const char *c, LONG cur,
    LONG ms, LONG me)
{
    const char *ac = s(g(o, MUIA_Textinput_Contents));
    LONG acur = g(o, MUIA_Textinput_CursorPos);
    LONG ams = g(o, MUIA_Textinput_MarkStart), ame = g(o, MUIA_Textinput_MarkEnd);
    BOOL ok = !strcmp(ac, c) && acur == cur && ams == ms && ame == me;
    printf("%s %-16s c=\"%s\" cur=%ld ms=%ld me=%ld\n", ok ? "ok  " : "FAIL",
        label, ac, (long)acur, (long)ams, (long)ame);
    if (!ok)
    {
        printf("     expected c=\"%s\" cur=%ld ms=%ld me=%ld\n", c, (long)cur,
            (long)ms, (long)me);
        failures++;
    }
}
static void check(const char *label, BOOL ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", label);
    if (!ok) failures++;
}
static void prep(Object *o, const char *text, ULONG cursor)
{
    SetAttrs(o, MUIA_Textinput_Contents, (ULONG)text, TAG_DONE);
    SetAttrs(o, MUIA_Textinput_CursorPos, cursor, TAG_DONE);
}
static void pump(void)
{
    for (int i = 0; i < 6; i++) { Delay(2); DoMethod(app, MUIM_Application_InputBuffered); }
}
static void key(ULONG raw, ULONG qual)
{
    struct InputEvent e = {0};
    e.ie_Class = IECLASS_RAWKEY; e.ie_Code = raw; e.ie_Qualifier = qual;
    io->io_Command = IND_WRITEEVENT; io->io_Data = &e; io->io_Length = sizeof(e);
    DoIO((APTR)io); pump();
    e.ie_Code |= IECODE_UP_PREFIX;
    DoIO((APTR)io); pump();
}

/* String subclass logging Textinput methods. MUI 5 also calls
 * TranslateEvent before HandleChar; it is not logged. */
static char log[200];
static ULONG logdisp(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    ULONG id = msg->MethodID;
    if (id >= TI(1) && id <= TI(99) && id != MUIM_Textinput_TranslateEvent
        && strlen(log) < sizeof(log) - 12)
    {
        char b[12];
        if (id == MUIM_Textinput_HandleChar)
            sprintf(b, " HC%lu", ((ULONG *)msg)[1]);
        else
            sprintf(b, " %lu", id - MCC_TI_TAGBASE);
        strcat(log, b);
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(int argc, char **argv)
{
    struct Screen *screen = NULL;
    struct MsgPort *port;
    struct MUI_CustomClass *mcc;
    Object *str, *sub, *ti, *tis, *win, *grp;
    struct Library *lib;
    ULONG v = 0;

    MUIMasterBase = OpenLibrary("muimaster.library", 20);
    if (!MUIMasterBase) { printf("FAIL muimaster.library 20\n"); return 20; }
    if (argc > 1 && !strcmp(argv[1], "screen"))
        screen = OpenScreenTags(NULL, SA_DisplayID, HIRES_KEY,
            SA_Width, 640, SA_Height, 256, SA_Depth, 2, TAG_DONE);
    if ((lib = OpenLibrary("mui/Textinput.mcc", 0)))
    { v = lib->lib_Version; CloseLibrary(lib); }
    check("Textinput.mcc version 29 or later", v >= 29);
    v = 0;
    if ((lib = OpenLibrary("mui/Textinputscroll.mcc", 0)))
    { v = lib->lib_Version; CloseLibrary(lib); }
    check("Textinputscroll.mcc version 29 or later", v >= 29);

    mcc = MUI_CreateCustomClass(NULL, MUIC_String, NULL, 4, (APTR)logdisp);
    str = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_Contents, (ULONG)"hello world", TAG_DONE);
    sub = mcc ? (Object *)NewObject(mcc->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_String,
        MUIA_CycleChain, 1, TAG_DONE) : NULL;
    ti = MO(MUIC_Textinput, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_Textinput_Contents, (ULONG)"textinput", TAG_DONE);
    tis = MO(MUIC_Textinputscroll, MUIA_Textinput_Multiline, TRUE,
        MUIA_Textinput_Contents, (ULONG)"alpha", TAG_DONE);
    grp = MO(MUIC_Group, MUIA_Group_Child, (ULONG)str, MUIA_Group_Child, (ULONG)sub,
        MUIA_Group_Child, (ULONG)ti, MUIA_Group_Child, (ULONG)tis, TAG_DONE);
    win = MO(MUIC_Window, MUIA_Window_Title, (ULONG)"Textinput probe",
        MUIA_Window_Screen, (ULONG)screen, MUIA_Window_RootObject, (ULONG)grp, TAG_DONE);
    app = MO(MUIC_Application, MUIA_Application_Window, (ULONG)win, TAG_DONE);
    if (!app || !sub) { printf("FAIL objects\n"); return 20; }

    check("Textinput object version", g(ti, MUIA_Version) >= 29);
    check("Textinput and String contents",
        !strcmp(s(g(ti, MUIA_String_Contents)), "textinput"));
    expect("default", str, "hello world", 0, -1, -1);
    check("defaults", g(str, MUIA_Textinput_Editable) == 1
        && g(str, MUIA_Textinput_NoInput) == 0 && g(str, MUIA_Textinput_Multiline) == 0
        && g(str, MUIA_Textinput_Secret) == 0 && g(str, MUIA_Textinput_WordWrap) == 70
        && g(str, MUIA_Textinput_ResetMarkOnCursor) == 0
        && g(str, MUIA_Textinput_Changed) == 1);
    check("Textinputscroll contents", !strcmp(s(g(tis, MUIA_Textinput_Contents)), "alpha")
        && !strcmp(s(g(tis, MUIA_String_Contents)), "alpha"));
    SetAttrs(tis, MUIA_Textinput_Contents, (ULONG)"gamma", TAG_DONE);
    DoMethod(tis, MUIM_Textinput_AppendText, (ULONG)"-delta", -1);
    check("Textinputscroll set and append",
        !strcmp(s(g(tis, MUIA_Textinput_Contents)), "gamma-delta"));

    /* Marks are inclusive, independent of the cursor, normalized and
     * clamped when read; a negative bound removes them. */
    prep(str, "hello world", 3); expect("cursor3", str, "hello world", 3, -1, -1);
    SetAttrs(str, MUIA_String_BufferPos, 5, TAG_DONE);
    expect("bufferpos5", str, "hello world", 5, -1, -1);
    SetAttrs(str, MUIA_Textinput_MarkStart, 1, TAG_DONE); expect("ms1", str, "hello world", 5, 1, 1);
    SetAttrs(str, MUIA_Textinput_MarkEnd, 4, TAG_DONE); expect("me4", str, "hello world", 5, 1, 4);
    SetAttrs(str, MUIA_Textinput_MarkStart, 6, MUIA_Textinput_MarkEnd, 2, TAG_DONE);
    expect("ms6me2", str, "hello world", 5, 2, 6);
    SetAttrs(str, MUIA_Textinput_MarkEnd, -1, TAG_DONE); expect("me-1", str, "hello world", 5, -1, -1);
    SetAttrs(str, MUIA_Textinput_MarkStart, 0, MUIA_Textinput_MarkEnd, 99, TAG_DONE);
    expect("ms0me99", str, "hello world", 5, 0, 11);
    SetAttrs(str, MUIA_Textinput_MarkStart, -1, TAG_DONE); expect("ms-1", str, "hello world", 5, -1, -1);
    DoMethod(str, MUIM_Textinput_DoMarkAll); expect("DoMarkAll", str, "hello world", 11, 0, 11);
    SetAttrs(str, MUIA_Textinput_CursorPos, 2, TAG_DONE);
    expect("markall+cur2", str, "hello world", 2, 0, 11);
    prep(str, "hello world", 3); DoMethod(str, MUIM_Textinput_DoMarkStart);
    expect("DoMarkStart", str, "hello world", 3, -1, -1);

    /* Clipboard: Copy copies the inclusive mark and removes it; Cut without
     * a mark clears the contents and leaves the clipboard alone. */
    prep(str, "hello world", 3);
    SetAttrs(str, MUIA_Textinput_MarkStart, 1, MUIA_Textinput_MarkEnd, 4, TAG_DONE);
    DoMethod(str, MUIM_Textinput_DoCopy); expect("DoCopy", str, "hello world", 3, -1, -1);
    DoMethod(str, MUIM_Textinput_DoCut); expect("DoCut", str, "", 0, -1, -1);
    prep(str, "XY", 1); DoMethod(str, MUIM_Textinput_DoPaste); expect("DoPaste", str, "XelloY", 5, -1, -1);

    static const struct { ULONG method; const char *c; LONG cur; } edits[] = {
        { MUIM_Textinput_DoDel, "helo world", 3 }, { MUIM_Textinput_DoBS, "helo world", 2 },
        { MUIM_Textinput_DoDelEOL, "hel", 3 }, { MUIM_Textinput_DoBSSOL, "lo world", 0 },
        { MUIM_Textinput_DoDelLine, "", 0 }, { MUIM_Textinput_DoBSWord, "lo world", 0 },
        { MUIM_Textinput_DoDelWord, "helworld", 3 }, { MUIM_Textinput_DoLeft, "hello world", 2 },
        { MUIM_Textinput_DoRight, "hello world", 4 }, { MUIM_Textinput_DoLineStart, "hello world", 0 },
        { MUIM_Textinput_DoLineEnd, "hello world", 11 }, { MUIM_Textinput_DoPrevWord, "hello world", 0 },
        { MUIM_Textinput_DoNextWord, "hello world", 6 },
        { MUIM_Textinput_DoToggleCase, "helLo world", 4 },
        { MUIM_Textinput_DoToggleCaseEOW, "helLO world", 5 },
        { MUIM_Textinput_DoUndo, "hello world", 3 },
    };
    for (int i = 0; i < (int)(sizeof(edits) / sizeof(edits[0])); i++)
    {
        char label[24];
        sprintf(label, "method %lu", edits[i].method - MCC_TI_TAGBASE);
        prep(str, "hello world", 3); DoMethod(str, edits[i].method);
        expect(label, str, edits[i].c, edits[i].cur, -1, -1);
    }
    prep(str, "a19b", 2); DoMethod(str, MUIM_Textinput_DoIncrementDec);
    expect("DoIncrementDec", str, "a20b", 2, -1, -1);
    prep(str, "a19b", 2); DoMethod(str, MUIM_Textinput_DoDecrementDec);
    expect("DoDecrementDec", str, "a18b", 2, -1, -1);
    prep(str, "hello world", 3); DoMethod(str, MUIM_Textinput_HandleChar, 'x', 0);
    expect("HandleChar", str, "helxlo world", 4, -1, -1);
    prep(str, "hello world", 3); DoMethod(str, MUIM_Textinput_InsertText, (ULONG)"ZZ", -1);
    expect("InsertText", str, "helZZlo world", 5, -1, -1);
    prep(str, "hello world", 3); DoMethod(str, MUIM_Textinput_AppendText, (ULONG)"abcdef", 2);
    expect("AppendText", str, "hello worldab", 3, -1, -1);
    prep(str, "hello", 5); DoMethod(str, MUIM_Textinput_Acknowledge, (ULONG)"ack");
    check("Acknowledge reports the contents",
        !strcmp(s(g(str, MUIA_String_Acknowledge)), "hello"));
    SetAttrs(str, MUIA_Textinput_Integer, 42, TAG_DONE);
    check("Integer", !strcmp(s(g(str, MUIA_Textinput_Contents)), "42")
        && g(str, MUIA_String_Integer) == 42);
    SetAttrs(str, MUIA_Textinput_Changed, FALSE, TAG_DONE);
    check("Changed cleared", g(str, MUIA_Textinput_Changed) == 0);
    SetAttrs(str, MUIA_String_Contents, (ULONG)"via string", TAG_DONE);
    check("Changed by String contents", g(str, MUIA_Textinput_Changed) == 1);
    SetAttrs(str, MUIA_Textinput_Secret, TRUE, TAG_DONE);
    check("Secret", g(str, MUIA_Textinput_Secret) == 1 && g(str, MUIA_String_Secret) == 1);
    SetAttrs(str, MUIA_Textinput_Secret, FALSE, TAG_DONE);

    port = CreateMsgPort();
    io = port ? (APTR)CreateIORequest(port, sizeof(*io)) : NULL;
    if (!io || OpenDevice("input.device", 0, (APTR)io, 0))
    {
        printf("FAIL input.device\n");
        return 20;
    }
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    if (!g(win, MUIA_Window_Open)) { printf("FAIL window\n"); return 20; }

    /* Methods MUI calls for key input, and their result */
    static const struct { const char *n; ULONG raw, qual; const char *log, *c; LONG cur; } keys[] = {
        {"a", 0x20, 0, " HC97", "abca def", 4},
        {"A", 0x20, IEQUALIFIER_LSHIFT, " HC65", "abcA def", 4},
        {"space", 0x40, 0, " HC32", "abc  def", 4},
        {"bs", 0x41, 0, " 38", "ab def", 2},
        {"del", 0x46, 0, " 36", "abcdef", 3},
        {"left", 0x4f, 0, " 23", "abc def", 2},
        {"right", 0x4e, 0, " 24", "abc def", 4},
        {"shift-left", 0x4f, IEQUALIFIER_LSHIFT, " 27", "abc def", 0},
        {"shift-right", 0x4e, IEQUALIFIER_LSHIFT, " 28", "abc def", 7},
        {"up", 0x4c, 0, "", "abc def", 3},
        {"shift-bs", 0x41, IEQUALIFIER_LSHIFT, " 39", " def", 0},
        {"shift-del", 0x46, IEQUALIFIER_LSHIFT, " 37", "abc", 3},
        {"ctrl-x", 0x32, IEQUALIFIER_CONTROL, " 12", "", 0},
        {"amiga-c", 0x33, IEQUALIFIER_RCOMMAND, " 13", "abc def", 3},
        {"amiga-v", 0x34, IEQUALIFIER_RCOMMAND, " 14", "abcabc def def", 10},
        {"amiga-a", 0x20, IEQUALIFIER_RCOMMAND, "", "abc def", 3},
        {"amiga-z", 0x31, IEQUALIFIER_RCOMMAND, " 56", "abc def", 3},
        {"tab", 0x42, 0, "", "abc def", 3},
        {"return", 0x44, 0, " HC13 20", "abc def", 3},
    };
    for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++)
    {
        char label[24];
        SetAttrs(sub, MUIA_Textinput_Contents, (ULONG)"abc def", TAG_DONE);
        SetAttrs(sub, MUIA_Textinput_CursorPos, 3, TAG_DONE);
        SetAttrs(win, MUIA_Window_ActiveObject, (ULONG)sub, TAG_DONE);
        pump();
        log[0] = 0;
        key(keys[i].raw, keys[i].qual);
        sprintf(label, "key %s", keys[i].n);
        if (strcmp(log, keys[i].log))
        {
            printf("FAIL %s methods \"%s\" expected \"%s\"\n", label, log, keys[i].log);
            failures++;
        }
        expect(label, sub, keys[i].c, keys[i].cur, -1, -1);
    }
    check("Return clears Changed", g(sub, MUIA_Textinput_Changed) == 0);

    /* Cursor keys remove a mark, typing replaces it, with and without
     * ResetMarkOnCursor; reversed bounds as set by an autocompleter. */
    for (int r = 0; r < 2; r++)
    {
        SetAttrs(sub, MUIA_Textinput_ResetMarkOnCursor, r,
            MUIA_Textinput_Contents, (ULONG)"abc def", TAG_DONE);
        SetAttrs(win, MUIA_Window_ActiveObject, (ULONG)sub, TAG_DONE); pump();
        SetAttrs(sub, MUIA_Textinput_CursorPos, 5, MUIA_Textinput_MarkStart, 2,
            MUIA_Textinput_MarkEnd, 4, TAG_DONE);
        expect("marked", sub, "abc def", 5, 2, 4);
        key(0x4e, 0); expect("  right", sub, "abc def", 6, -1, -1);
        SetAttrs(sub, MUIA_Textinput_CursorPos, 5, MUIA_Textinput_MarkStart, 2,
            MUIA_Textinput_MarkEnd, 4, TAG_DONE);
        key(0x20, 0); expect("  typed a", sub, "abaef", 3, -1, -1);
    }
    SetAttrs(sub, MUIA_Textinput_Contents, (ULONG)"www.example.com",
        MUIA_Textinput_CursorPos, 5, MUIA_Textinput_MarkEnd, 5,
        MUIA_Textinput_MarkStart, 15, TAG_DONE);
    expect("completion", sub, "www.example.com", 5, 5, 15);
    key(0x41, 0); expect("  bs", sub, "www.e", 5, -1, -1);
    SetAttrs(win, MUIA_Window_ActiveObject, (ULONG)str, TAG_DONE); pump();
    SetAttrs(str, MUIA_Textinput_Contents, (ULONG)"gone", TAG_DONE);
    SetAttrs(str, MUIA_Textinput_MarkStart, 0, MUIA_Textinput_MarkEnd, 3, TAG_DONE);
    check("active marks", g(str, MUIA_Textinput_MarkStart) == 0
        && g(str, MUIA_Textinput_MarkEnd) == 3);

    CloseDevice((APTR)io);
    DeleteIORequest((APTR)io);
    DeleteMsgPort(port);
    MUI_DisposeObject(app);
    MUI_DeleteCustomClass(mcc);
    if (screen) CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf(failures ? "FAIL textinput %d\n" : "PASS textinput\n", failures);
    return failures ? 5 : 0;
}

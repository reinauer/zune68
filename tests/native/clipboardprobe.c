/* Interactive clipboard baseline using only public MUI 3.8 attributes.
 * No selection-range extension or application keyboard shortcut is used.
 * Run with redirected output; Dump records both gadgets and OS FTXT data.
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
#include <datatypes/textclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/iffparse.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase, *IFFParseBase;
ULONG __stack = 65536;

#ifndef CLIPBOARD_CLASS
#define CLIPBOARD_CLASS MUIC_String
#endif

static void escaped(const UBYTE *text, LONG length)
{
    LONG i;
    putchar('"');
    for (i = 0; i < length; i++) {
        unsigned c = text[i];
        if (c >= 32 && c < 127 && c != '\\' && c != '"')
            putchar(c);
        else
            printf("\\x%02x", c);
    }
    putchar('"');
}

static void clipboard(int seed)
{
    struct IFFHandle *iff = AllocIFF();
    LONG result = -1;
    if (iff) {
        iff->iff_Stream = (ULONG)OpenClipboard(0);
        if (iff->iff_Stream) {
            InitIFFasClip(iff);
            result = OpenIFF(iff, seed ? IFFF_WRITE : IFFF_READ);
            if (!result) {
                if (seed) {
                    result = PushChunk(iff, ID_FTXT, ID_FORM,
                        IFFSIZE_UNKNOWN);
                    if (!result) {
                        result = PushChunk(iff, ID_FTXT, ID_CHRS, 6);
                        if (!result) {
                            result = WriteChunkBytes(iff, "Seed68", 6);
                            PopChunk(iff);
                        }
                        PopChunk(iff);
                    }
                } else {
                    result = StopChunk(iff, ID_FTXT, ID_CHRS);
                    if (!result) result = ParseIFF(iff, IFFPARSE_SCAN);
                    if (!result) {
                        struct ContextNode *cn = CurrentChunk(iff);
                        UBYTE bytes[256];
                        LONG length = cn ? cn->cn_Size : -1;
                        if (length >= 0) {
                            LONG wanted = length > 256 ? 256 : length;
                            result = ReadChunkBytes(iff, bytes, wanted);
                            printf("clipboard size=%ld read=%ld text=",
                                length, result);
                            if (result >= 0) escaped(bytes, result);
                            putchar('\n');
                        }
                    }
                }
                CloseIFF(iff);
            }
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
    if (seed || result < 0)
        printf("clipboard %s result=%ld\n", seed ? "seed" : "read", result);
}

static void dump(const char *phase, Object *source, Object *destination)
{
    Object *objects[2] = {source, destination};
    int i;
    printf("phase=%s\n", phase);
    for (i = 0; i < 2; i++) {
        STRPTR text = NULL;
        LONG pos = -1, length = 0;
        GetAttr(MUIA_String_Contents, objects[i], (ULONG *)&text);
        GetAttr(MUIA_String_BufferPos, objects[i], (ULONG *)&pos);
        if (text) while (length < 256 && text[length]) length++;
        printf("%s cursor=%ld text=", i ? "destination" : "source", pos);
        escaped((UBYTE *)text, length);
        putchar('\n');
    }
    clipboard(0);
    fflush(stdout);
}

int main(void)
{
    Object *app, *window, *source, *destination, *root, *buttons;
    Object *reset, *seed, *target, *report, *quit;
    struct DateStamp start, now;
    ULONG signals = 0, id, opened = 0;
    int timeout = 0;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    IFFParseBase = OpenLibrary("iffparse.library", 37);
    if (!MUIMasterBase || !IFFParseBase) goto failed;
    source = MUI_NewObject(CLIPBOARD_CLASS, MUIA_Frame, MUIV_Frame_String,
        MUIA_String_Contents, (ULONG)"Clipboard68", MUIA_String_MaxLen, 128,
        MUIA_CycleChain, 1, TAG_DONE);
    destination = MUI_NewObject(CLIPBOARD_CLASS, MUIA_Frame, MUIV_Frame_String,
        MUIA_String_Contents, (ULONG)"", MUIA_String_MaxLen, 128,
        MUIA_CycleChain, 1, TAG_DONE);
    if (!source || !destination) {
        printf("Cannot create %s gadgets\n", CLIPBOARD_CLASS);
        fflush(stdout);
        if (source) MUI_DisposeObject(source);
        if (destination) MUI_DisposeObject(destination);
        goto failed;
    }
    reset = MUI_MakeObject(MUIO_Button, (ULONG)"Reset");
    seed = MUI_MakeObject(MUIO_Button, (ULONG)"Seed");
    target = MUI_MakeObject(MUIO_Button, (ULONG)"Target");
    report = MUI_MakeObject(MUIO_Button, (ULONG)"Dump");
    quit = MUI_MakeObject(MUIO_Button, (ULONG)"Quit");
    buttons = MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
        MUIA_Group_Child, (ULONG)reset, MUIA_Group_Child, (ULONG)seed,
        MUIA_Group_Child, (ULONG)target,
        MUIA_Group_Child, (ULONG)report, MUIA_Group_Child, (ULONG)quit,
        TAG_DONE);
    root = MUI_NewObject(MUIC_Group,
        MUIA_Group_Child, (ULONG)MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents,
            (ULONG)"Copy: select source with mouse, then Right Amiga C.",
            TAG_DONE),
        MUIA_Group_Child, (ULONG)source,
        MUIA_Group_Child, (ULONG)MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents,
            (ULONG)"Paste: click empty destination, then Right Amiga V.",
            TAG_DONE),
        MUIA_Group_Child, (ULONG)destination,
        MUIA_Group_Child, (ULONG)MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents,
            (ULONG)"Seed writes Seed68 and focuses destination.", TAG_DONE),
        MUIA_Group_Child, (ULONG)buttons, TAG_DONE);
    window = MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, (ULONG)"Clipboard probe (300 second limit)",
        MUIA_Window_RootObject, (ULONG)root, TAG_DONE);
    app = MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, (ULONG)"Clipboard probe",
        MUIA_Application_Base, (ULONG)"CLIPBOARDPROBE",
        MUIA_Application_Window, (ULONG)window, TAG_DONE);
    if (!app) goto failed;
    DoMethod(reset, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, 10);
    DoMethod(seed, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, 11);
    DoMethod(report, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, 12);
    DoMethod(target, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, 13);
    DoMethod(quit, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
        app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    set(window, MUIA_Window_Open, TRUE);
    GetAttr(MUIA_Window_Open, window, &opened);
    if (!opened) { MUI_DisposeObject(app); goto failed; }
    set(window, MUIA_Window_ActiveObject, source);
    set(source, MUIA_String_BufferPos, 0);
    printf("library=%u.%u class=%s timeout=300\n",
        MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision,
        CLIPBOARD_CLASS);
    dump("initial", source, destination);
    DateStamp(&start);
    for (;;) {
        id = DoMethod(app, MUIM_Application_NewInput, &signals);
        if (id == (ULONG)MUIV_Application_ReturnID_Quit) break;
        if (id == 10) {
            set(source, MUIA_String_Contents, "Clipboard68");
            set(destination, MUIA_String_Contents, "");
            set(window, MUIA_Window_ActiveObject, source);
            set(source, MUIA_String_BufferPos, 0);
            dump("reset", source, destination);
        } else if (id == 11) {
            clipboard(1);
            set(destination, MUIA_String_Contents, "");
            set(window, MUIA_Window_ActiveObject, destination);
            set(destination, MUIA_String_BufferPos, 0);
            dump("seed", source, destination);
        } else if (id == 12) dump("dump", source, destination);
        else if (id == 13) {
            set(window, MUIA_Window_ActiveObject, destination);
            dump("target", source, destination);
        }
        DateStamp(&now);
        if ((now.ds_Days - start.ds_Days) * 86400L
            + (now.ds_Minute - start.ds_Minute) * 60L
            + (now.ds_Tick - start.ds_Tick) / TICKS_PER_SECOND >= 300) {
            timeout = 1;
            break;
        }
        Delay(1);
    }
    dump(timeout ? "timeout" : "quit", source, destination);
    set(window, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(IFFParseBase);
    CloseLibrary(MUIMasterBase);
    return 0;
failed:
    if (IFFParseBase) CloseLibrary(IFFParseBase);
    if (MUIMasterBase) CloseLibrary(MUIMasterBase);
    return 20;
}

/* Compare fallback images, copied specs and String hint rendering. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(name, ...) \
    ({ ULONG tags[] = {__VA_ARGS__}; MUI_NewObjectA(name, (struct TagItem *)tags); })
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG pixels(Object *obj)
{
    ULONG value = 2166136261UL;
    for (LONG y = 0; y < _mheight(obj); y++)
        for (LONG x = 0; x < _mwidth(obj); x++)
            value = (value ^ ReadPixel(_rp(obj), _mleft(obj)+x, _mtop(obj)+y)) * 16777619UL;
    return value;
}
static void pump(Object *app)
{
    for (int i = 0; i < 5; i++)
    { DoMethod(app, MUIM_Application_InputBuffered); Delay(2); }
}
int main(int argc, char **argv)
{
    Object *images[3], *strings[2], *root, *win, *app;
    struct Screen *screen = NULL;
    ULONG hash[3], blank, hint, actual;
    char spec[] = "6:19";
    STRPTR contents = NULL;
    int failures = 0;
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    if (argc > 1 && !strcmp(argv[1], "screen"))
        screen = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2, TAG_DONE);
    printf("library=%u.%u\n", MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision);
    images[0] = MO(MUIC_Image, MUIA_Image_Spec, MUII_PopFile, MUIA_FixWidth, 32,
        MUIA_FixHeight, 24, TAG_DONE);
    images[1] = MO(MUIC_Image, MUIA_Image_Spec, "5:RAM:missing-zune68-image",
        MUIA_Image_BuiltinSpec, MUII_PopFile, MUIA_FixWidth, 32, MUIA_FixHeight, 24, TAG_DONE);
    images[2] = MO(MUIC_Image, MUIA_Image_Spec, spec, MUIA_Image_CopySpec, TRUE,
        MUIA_FixWidth, 32, MUIA_FixHeight, 24, TAG_DONE);
    spec[3] = '0';
    strings[0] = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String,
        MUIA_CycleChain, 1, MUIA_String_Placeholder, "Type here", TAG_DONE);
    strings[1] = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String,
        MUIA_CycleChain, 1, TAG_DONE);
    root = MO(MUIC_Group, MUIA_Group_Child,
        MO(MUIC_Group, MUIA_Group_Horiz, TRUE,
            MUIA_Group_Child, images[0], MUIA_Group_Child, images[1],
            MUIA_Group_Child, images[2], TAG_DONE),
        MUIA_Group_Child, strings[0], MUIA_Group_Child, strings[1], TAG_DONE);
    win = MO(MUIC_Window, MUIA_Window_Title, "Image and hint probe",
        MUIA_Window_Screen, screen, MUIA_Window_RootObject, root, TAG_DONE);
    app = MO(MUIC_Application, MUIA_Application_Window, win, TAG_DONE);
    if (!app) return 20;
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    SetAttrs(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None, TAG_DONE);
    pump(app);
    for (int i = 0; i < 3; i++)
    {
        hash[i] = pixels(images[i]);
        printf("image %d box=%dx%d hash=%08lx\n", i, _mwidth(images[i]), _mheight(images[i]), hash[i]);
    }
    if (hash[0] != hash[1] || hash[0] != hash[2]) failures++;
    hint = pixels(strings[0]); blank = pixels(strings[1]);
    if (hint == blank) failures++;
    SetAttrs(strings[0], MUIA_String_Contents, "actual", TAG_DONE);
    SetAttrs(strings[1], MUIA_String_Contents, "actual", TAG_DONE);
    pump(app);
    actual = pixels(strings[0]);
    if (actual != pixels(strings[1])) failures++;
    SetAttrs(strings[0], MUIA_String_Contents, "", TAG_DONE);
    pump(app);
    if (pixels(strings[0]) != hint) failures++;
    GetAttr(MUIA_String_Contents, strings[0], (ULONG *)&contents);
    if (!contents || *contents) failures++;
    SetAttrs(strings[0], MUIA_String_InactiveContents, NULL, TAG_DONE);
    pump(app);
    if (pixels(strings[0]) != blank) failures++;
    printf("hint=%08lx blank=%08lx actual=%08lx\n", hint, blank, actual);
    MUI_DisposeObject(app);
    if (screen) CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 20 : 0;
}

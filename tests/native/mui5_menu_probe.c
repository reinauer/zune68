/* Native menu API and BOOPSI image lifetime comparison with MUI 5. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <intuition/imageclass.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define MUI_NewObject(name, ...) \
    ({ ULONG tags[] = {__VA_ARGS__}; MUI_NewObjectA(name, (struct TagItem *)tags); })

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG disposed;
static int failures;
static void check(int ok, const char *name)
{
    if (!ok) { printf("FAIL %s\n", name); failures++; }
}
static ULONG image_dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == OM_DISPOSE) disposed++;
    return DoSuperMethodA(cl, obj, msg);
}
int main(void)
{
    struct IClass *ic;
    Object *obj, *image1, *image2, *strip;
    ULONG got, before;
    char title[32] = "Title", shortcut[8] = "T", aiss[16] = "copy";
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    printf("library=%u.%u\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);
    ic = MakeClass(NULL, "imageclass", NULL, 0, 0);
    if (!ic) { CloseLibrary(MUIMasterBase); return 20; }
    ic->cl_Dispatcher.h_Entry = (HOOKFUNC)image_dispatch;
    image1 = NewObject(ic, NULL, IA_Width, 12, IA_Height, 8, TAG_DONE);
    image2 = NewObject(ic, NULL, IA_Width, 16, IA_Height, 10, TAG_DONE);
    obj = MUI_NewObject(MUIC_Menuitem, MUIA_Menuitem_Title, title,
        MUIA_Menuitem_Shortcut, shortcut, MUIA_Menuitem_CopyStrings, TRUE,
        MUIA_Menuitem_Image, image1, MUIA_Menuitem_AISSName, aiss, TAG_DONE);
    check(obj != NULL, "create");
    if (!obj) goto done;
    title[0] = 'X'; shortcut[0] = 'X'; aiss[0] = 'X';
    got = 0; GetAttr(MUIA_Menuitem_Title, obj, &got);
    check(got && !strcmp((char *)got, "Title"), "copied title");
    got = 0; GetAttr(MUIA_Menuitem_Shortcut, obj, &got);
    check(got && !strcmp((char *)got, "T"), "copied shortcut");
    got = 0; GetAttr(MUIA_Menuitem_AISSName, obj, &got);
    check(got == (ULONG)aiss && !strcmp((char *)got, "Xopy"), "borrowed AISS name");
    got = 0;
    check(GetAttr(MUIA_Menuitem_Image, obj, &got) && got == (ULONG)image1,
        "image get");
    before = disposed;
    SetAttrs(obj, MUIA_Menuitem_Image, image2, TAG_DONE);
    check(disposed == before + 1, "replacement disposes owned image");
    SetAttrs(obj, MUIA_Menuitem_Title, "Replaced", TAG_DONE);
    MUI_DisposeObject(obj);
    check(disposed == before + 2, "dispose owns image");
    image1 = NewObject(ic, NULL, TAG_DONE);
    obj = MUI_NewObject(MUIC_Menuitem, MUIA_Menuitem_Image, image1,
        MUIA_Menuitem_FreeImage, FALSE, TAG_DONE);
    before = disposed;
    SetAttrs(obj, MUIA_Menuitem_Image, 0, TAG_DONE);
    MUI_DisposeObject(obj);
    check(disposed == before, "borrowed image retained");
    DisposeObject(image1);
    obj = MUI_NewObject(MUIC_Menuitem, MUIA_Menuitem_Title, "Q\0Quit",
        MUIA_Menuitem_Shortcut, MUIV_Menuitem_Shortcut_Check, TAG_DONE);
    got = 0; GetAttr(MUIA_Menuitem_Title, obj, &got);
    printf("encoded title=%s\n", got ? (char *)got : "NULL");
    got = 0; GetAttr(MUIA_Menuitem_Shortcut, obj, &got);
    printf("encoded shortcut=%08lx\n", got);
    MUI_DisposeObject(obj);
    strip = MUI_NewObject(MUIC_Menustrip,
        MUIA_Menustrip_CaseSensitive, TRUE, TAG_DONE);
    got = 0;
    check(GetAttr(MUIA_Menustrip_CaseSensitive, strip, &got) && got,
        "case sensitive");
    got = 0;
    check(GetAttr(MUIA_Menustrip_Enabled, strip, &got) && got, "strip enabled");
    printf("init=%lu\n", DoMethod(strip, MUIM_Menustrip_InitChange));
    printf("nested init=%lu\n", DoMethod(strip, MUIM_Menustrip_InitChange));
    printf("nested exit=%lu\n", DoMethod(strip, MUIM_Menustrip_ExitChange));
    printf("exit=%lu\n", DoMethod(strip, MUIM_Menustrip_ExitChange));
    MUI_DisposeObject(strip);
done:
    check(FreeClass(ic), "all images freed");
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 20 : 0;
}

/* Build against the MUI 5 OS3 SDK, then run unchanged on both runtimes. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define MUI_NewObject(name, ...) \
    ({ ULONG tags[] = {__VA_ARGS__}; MUI_NewObjectA(name, (struct TagItem *)tags); })

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static int failures;
static void check(int ok, const char *name)
{
    if (!ok) { printf("FAIL %s\n", name); failures++; }
}
static void contents(Object *obj, const char *expected)
{
    STRPTR text = NULL;
    GetAttr(MUIA_String_Contents, obj, (ULONG *)&text);
    printf("contents=%s expected=%s\n", text ? (char *)text : "NULL", expected);
    check(text && !strcmp(text, expected), "contents");
}
int main(void)
{
    Object *obj;
    ULONG got;
    long long value = -4294967297LL;
    char placeholder[] = "Search here", inactive[] = "Inactive";
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 20;
    printf("library=%u.%u\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);
    obj = MUI_NewObject(MUIC_String, MUIA_String_MaxLen, 80,
        MUIA_String_Placeholder, placeholder, TAG_DONE);
    check(obj != NULL, "create");
    if (!obj) goto done;
    got = 0;
    check(GetAttr(MUIA_String_Placeholder, obj, &got) &&
        got == (ULONG)placeholder, "placeholder borrowed");
    got = 0;
    GetAttr(MUIA_String_InactiveContents, obj, &got);
    printf("inactive aliases placeholder=%d\n", got == (ULONG)placeholder);
    SetAttrs(obj, MUIA_String_InactiveContents, inactive, TAG_DONE);
    got = 0;
    check(GetAttr(MUIA_String_InactiveContents, obj, &got) &&
        got == (ULONG)inactive, "inactive borrowed");
    got = 0;
    GetAttr(MUIA_String_Placeholder, obj, &got);
    printf("placeholder aliases inactive=%d\n", got == (ULONG)inactive);
    contents(obj, "");
    SetAttrs(obj, MUIA_String_Integer64, &value, TAG_DONE);
    contents(obj, "-4294967297");
    got = 0;
    GetAttr(MUIA_String_Integer64, obj, &got);
    printf("get64 pointer=%08lx value=%08lx%08lx\n", got,
        got ? ((ULONG *)got)[0] : 0, got ? ((ULONG *)got)[1] : 0);
    if (MUIMasterBase->lib_Version == 21 && MUIMasterBase->lib_Revision == 227)
    {
        check(got && *(long long *)got == -2147483648LL,
            "reference getter clamps to 32 bits");
        puts("REFERENCE LIMIT: Integer64 getter clamps to signed 32 bits");
    }
    else check(got && *(long long *)got == value, "integer64 borrowed value");
    {
        static const struct { long long number; const char *text; } cases[] = {
            {-9223372036854775807LL - 1, "-9223372036854775808"},
            {9223372036854775807LL, "9223372036854775807"},
            {4294967296LL, "4294967296"}, {0, "0"}
        };
        for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); i++)
        {
            value = cases[i].number;
            SetAttrs(obj, MUIA_String_Integer64, &value, TAG_DONE);
            contents(obj, cases[i].text);
            got = 0; GetAttr(MUIA_String_Integer64, obj, &got);
            if (MUIMasterBase->lib_Version == 19)
                check(got && *(long long *)got == value, "integer64 boundary");
        }
    }
    check(DoMethod(obj, MUIM_WhichPointerType, 0, 0) ==
        MUIV_PointerType_Text, "default text pointer");
    printf("pointer method=%ld\n", DoMethod(obj, MUIM_WhichPointerType, 0, 0));
    got = 0;
    GetAttr(MUIA_ContextMenu, obj, &got);
    printf("default context=%08lx\n", got);
    printf("context build=%08lx\n", DoMethod(obj, MUIM_ContextMenuBuild, 0, 0));
    MUI_DisposeObject(obj);
done:
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 20 : 0;
}

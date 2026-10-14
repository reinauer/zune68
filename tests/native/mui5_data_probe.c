/* Original SDK client for map ownership, iteration and encoded Text data. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
static int failures, disposed;
static struct Screen *screen;
static void check(BOOL ok, const char *name)
{
    if (!ok) { printf("FAIL %s\n", name); failures++; }
}
static ULONG attr(Object *o, ULONG a)
{
    ULONG value = 0xdeadbeef;
    GetAttr(a, o, &value);
    return value;
}
static ULONG dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == OM_DISPOSE) disposed++;
    return DoSuperMethodA(cl, obj, msg);
}
static void maps(void)
{
    APTR pool = CreatePool(MEMF_ANY, 4096, 4096);
    Object *map = MO(MUIC_Datamap, MUIA_Datamap_AutoLock, TRUE,
        MUIA_Datamap_Pool, pool, TAG_DONE);
    check(map != NULL, "Datamap creation");
    if (map)
    {
        ULONG size = 999, counter = 0, seen = 0;
        LONG value = 0x12345678;
        char key[] = "beta", previous[16] = "";
        check(DoMethod(map, MUIM_Datamap_Set, &value, sizeof(value), key), "map insertion");
        key[0] = 'x'; value = 0;
        LONG *stored = (APTR)DoMethod(map, MUIM_Datamap_Get, "beta", &size);
        check(stored && !((ULONG)stored & 1) && *stored == 0x12345678 && size == 4,
            "copied key and aligned copied data");
        DoMethod(map, MUIM_Datamap_Set, "A", 2, "alpha");
        DoMethod(map, MUIM_Datamap_Set, "Z", 2, "zeta");
        check(attr(map, MUIA_Datamap_Count) == 3, "map count");
        DoMethod(map, MUIM_Datamap_Set, "B", 2, "beta");
        check(attr(map, MUIA_Datamap_Count) == 3, "replacement count");
        check(!DoMethod(map, MUIM_Datamap_Get, "absent", &size), "missing map key");
        printf("missing size=%lu\n", size);
        DoMethod(map, MUIM_Semaphore_Obtain);
        while (DoMethod(map, MUIM_Datamap_Iterate, &counter))
        {
            char *current = (APTR)DoMethod(map, MUIM_Datamap_IterationKey, &counter);
            check(current && strcmp(previous, current) < 0, "sorted iteration");
            if (!current) break;
            strcpy(previous, current);
            check(DoMethod(map, MUIM_Datamap_Remove, current), "remove iterator current");
            seen++;
        }
        DoMethod(map, MUIM_Semaphore_Release);
        check(seen == 3 && attr(map, MUIA_Datamap_Count) == 0, "iteration after removal");
        DoMethod(map, MUIM_Datamap_Set, "reset", 6, "key");
        DoMethod(map, MUIM_Datamap_Clear);
        check(attr(map, MUIA_Datamap_Count) == 0, "map clear");
        DoMethod(map, MUIM_Datamap_Set, "reused", 7, "key");
        check(DoMethod(map, MUIM_Datamap_Find, "key") != 0, "reuse cleared map");
        MUI_DisposeObject(map);
    }
    /* External pools remain owned by their caller. */
    APTR block = AllocPooled(pool, 32);
    check(block != NULL, "caller pool survives map");
    if (block) FreePooled(pool, block, 32);
    DeletePool(pool);
    map = MO(MUIC_Datamap, MUIA_Datamap_CopyKeys, FALSE, TAG_DONE);
    if (map)
    {
        char key[] = "borrowed";
        ULONG counter = 0;
        DoMethod(map, MUIM_Datamap_Set, "value", 6, key);
        DoMethod(map, MUIM_Datamap_Iterate, &counter);
        check(DoMethod(map, MUIM_Datamap_IterationKey, &counter) == (ULONG)key,
            "borrowed map key identity");
        MUI_DisposeObject(map);
    }
    struct MUI_CustomClass *cc = MUI_CreateCustomClass(NULL, MUIC_Notify, NULL, 0, (APTR)dispatch);
    map = MO(MUIC_Objectmap, TAG_DONE);
    check(map && cc, "Objectmap creation");
    if (map && cc)
    {
        Object *a = NewObject(cc->mcc_Class, NULL, TAG_DONE);
        Object *b = NewObject(cc->mcc_Class, NULL, TAG_DONE);
        disposed = 0;
        check(DoMethod(map, MUIM_Objectmap_Set, a, "one"), "object insertion");
        check(DoMethod(map, MUIM_Objectmap_Set, b, "one"), "object replacement");
        printf("replacement disposals=%d\n", disposed);
        check(disposed == 1, "replacement disposes old object");
        check(DoMethod(map, MUIM_Objectmap_Find, "one") == (ULONG)b, "object lookup");
        check(DoMethod(map, MUIM_Objectmap_Remove, "one"), "object removal");
        check(disposed == 1, "removal returns ownership");
        MUI_DisposeObject(b);
        b = NewObject(cc->mcc_Class, NULL, TAG_DONE);
        DoMethod(map, MUIM_Objectmap_Set, b, "two");
        MUI_DisposeObject(map); map = NULL;
        check(disposed == 3, "map disposal owns remaining objects");
    }
    if (map) MUI_DisposeObject(map);
    if (cc) MUI_DeleteCustomClass(cc);
}
static void textdata(void)
{
    char utf8[] = "A\xc3\xa4";
    const UBYTE ucs2[] = {0, 'A', 0, 0xe4, 0, 0};
    Object *data = MO(MUIC_Textdata, MUIA_Textdata_Contents, utf8,
        MUIA_Textdata_Encoding, MIBENUM_UTF_8, TAG_DONE);
    check(data != NULL, "Textdata creation");
    if (!data) return;
    check(attr(data, MUIA_Textdata_Contents) == (ULONG)utf8, "borrowed Textdata source");
    check(attr(data, MUIA_Textdata_Encoding) == 106 && _encoding(data) == 106 &&
        _text(data) == utf8, "Textdata public instance ABI");
    char *latin = (APTR)DoMethod(data, MUIM_Textdata_Convert, 4);
    check(latin && !strcmp(latin, "A\xe4"), "UTF8 to Latin1");
    UBYTE *wide = (APTR)DoMethod(data, MUIM_Textdata_Convert, 1000);
    check(wide && !memcmp(wide, ucs2, sizeof(ucs2)), "UTF8 to UCS2");
    check(DoMethod(data, MUIM_Textdata_Convert, 106) == (ULONG)utf8, "identity conversion");
    /* Conversion storage may be replaced by the next conversion. */
    Object *text = MO(MUIC_Text, MUIA_Text_Contents, "fallback", MUIA_Text_Data, data, TAG_DONE);
    check(text && attr(text, MUIA_Text_Data) == (ULONG)data, "Text accepts Textdata");
    if (text)
    {
        printf("Text contents before detach=%s\n", (char *)attr(text, MUIA_Text_Contents));
        SetAttrs(text, MUIA_Text_Data, NULL, TAG_DONE);
        printf("Text contents after detach=%s\n", (char *)attr(text, MUIA_Text_Contents));
        MUI_DisposeObject(text);
        check(attr(data, MUIA_Textdata_Contents) == (ULONG)utf8, "Text detachment returns ownership");
    }
    MUI_DisposeObject(data);
    /* 21.227 interprets Contents using Encoding; SourceEncoding does not
     * transcode or take ownership of the supplied bytes. */
    for (ULONG source = 4; source <= 106; source += 102)
    {
        data = MO(MUIC_Textdata, MUIA_Textdata_Contents, utf8,
            MUIA_Textdata_Encoding, 106, MUIA_Textdata_SourceEncoding, source, TAG_DONE);
        check(data != NULL, "SourceEncoding construction");
        if (data)
        {
            check(attr(data, MUIA_Textdata_Contents) == (ULONG)utf8,
                "SourceEncoding keeps borrowed source");
            latin = (APTR)DoMethod(data, MUIM_Textdata_Convert, 4);
            check(latin && !strcmp(latin, "A\xe4"), "Encoding determines source decoding");
            MUI_DisposeObject(data);
        }
    }
    data = MO(MUIC_Textdata, MUIA_Textdata_Contents, utf8,
        MUIA_Textdata_Encoding, 106, MUIA_Textdata_SourceBytes, 1, TAG_DONE);
    if (data)
    {
        latin = (APTR)DoMethod(data, MUIM_Textdata_Convert, 4);
        check(latin && !strcmp(latin, "A"), "SourceBytes conversion boundary");
        MUI_DisposeObject(data);
    }
}
static void containers(void)
{
    Object *space = MO(MUIC_Dataspace, TAG_DONE);
    ULONG size = 999;
    DoMethod(space, MUIM_Dataspace_Add, "first", 6, 1);
    DoMethod(space, MUIM_Dataspace_Add, "second", 7, 2);
    char *value = (APTR)DoMethod(space, MUIM_Dataspace_Get, 1, &size);
    check(value && !strcmp(value, "first") && size == 6, "Dataspace Get");
    check(attr(space, MUIA_Dataspace_Count) == 2, "Dataspace count");
    DoMethod(space, MUIM_Dataspace_Clear);
    check(attr(space, MUIA_Dataspace_Count) == 0, "Dataspace clear count");
    check(!DoMethod(space, MUIM_Dataspace_Find, 1), "Dataspace clear lookup");
    DoMethod(space, MUIM_Dataspace_Add, "reused", 7, 1);
    check(attr(space, MUIA_Dataspace_Count) == 1, "Dataspace reuse");
    MUI_DisposeObject(space);

    Object *a = MO(MUIC_Rectangle, TAG_DONE), *b = MO(MUIC_Rectangle, TAG_DONE);
    Object *c = MO(MUIC_Rectangle, TAG_DONE), *d = MO(MUIC_Rectangle, TAG_DONE);
    Object *group = MO(MUIC_Group, MUIA_Group_Child, a, MUIA_Group_Child, b,
        MUIA_Group_Child, c, MUIA_Group_Child, d, TAG_DONE);
    DoMethod(group, MUIM_Group_InitChange);
    DoMethod(group, MUIM_Group_MoveMember, a, -2);
    DoMethod(group, MUIM_Group_ExitChange);
    struct List *children = (APTR)attr(group, MUIA_Group_ChildList);
    Object *state = (APTR)children->lh_Head;
    check(NextObject(&state) == b && NextObject(&state) == c &&
        NextObject(&state) == a && NextObject(&state) == d, "Group MoveMember from end");
    DoMethod(group, MUIM_Group_InitChange);
    DoMethod(group, MUIM_Group_Reorder, NULL, d, b, NULL);
    DoMethod(group, MUIM_Group_ExitChange);
    state = (APTR)children->lh_Head;
    { Object *x; while ((x = NextObject(&state))) printf("order %c\n", x==a?'a':x==b?'b':x==c?'c':'d'); }
    state = (APTR)children->lh_Head;
    check(NextObject(&state) == b && NextObject(&state) == d &&
        NextObject(&state) == c && NextObject(&state) == a, "Group partial Reorder");
    DoMethod(group, MUIM_Group_Reorder, c, d, b, NULL);
    state = (APTR)children->lh_Head;
    { Object *x; while ((x = NextObject(&state))) printf("after anchor %c\n", x==a?'a':x==b?'b':x==c?'c':'d'); }
    DoMethod(group, MUIM_Group_Reorder, (Object *)-1, d, c, NULL);
    state = (APTR)children->lh_Head;
    { Object *x; while ((x = NextObject(&state))) printf("after tail %c\n", x==a?'a':x==b?'b':x==c?'c':'d'); }
    check(attr(group, MUIA_Group_ChildCount) == 4, "Group child count");
    MUI_DisposeObject(group);

    Object *implicit = MO(MUIC_List, MUIA_List_MaxColumns, 3, TAG_DONE);
    check(implicit != NULL, "MaxColumns without a format");
    if (implicit) MUI_DisposeObject(implicit);

    const char *titles[] = {"Name", "Rank"};
    char first[] = "beta";
    const char *row1[] = {first, "1"}, *row2[] = {"alpha", "2"};
    Object *list = MO(MUIC_List, MUIA_List_MaxColumns, 2,
        MUIA_List_TitleArray, titles, MUIA_List_Format, ",",
        MUIA_List_ConstructHook, MUIV_List_ConstructHook_StringArray,
        MUIA_List_DestructHook, MUIV_List_DestructHook_StringArray,
        MUIA_List_DisplayHook, MUIV_List_DisplayHook_StringArray,
        MUIA_List_CompareHook, MUIV_List_CompareHook_StringArray, TAG_DONE);
    check(list != NULL, "List StringArray creation");
    if (list)
    {
        DoMethod(list, MUIM_List_InsertSingle, row1, MUIV_List_Insert_Bottom);
        DoMethod(list, MUIM_List_InsertSingle, row2, MUIV_List_Insert_Bottom);
        first[0] = 'X';
        char **entry = NULL;
        DoMethod(list, MUIM_List_GetEntry, 0, &entry);
        check(entry && !strcmp(entry[0], "beta") && !strcmp(entry[1], "1"),
            "StringArray deep copy");
        char **copy = (APTR)attr(list, MUIA_List_TitleArray);
        printf("title copy=%p original=%p values=%s,%s\n",copy,titles,copy?copy[0]:"NULL",copy?copy[1]:"NULL");
        check(copy && copy[0] == titles[0] && copy[1] == titles[1],
            "TitleArray copies pointers only");
        SetAttrs(list, MUIA_List_SortColumn, 0, TAG_DONE);
        DoMethod(list, MUIM_List_Sort);
        DoMethod(list, MUIM_List_GetEntry, 0, &entry);
        check(entry && !strcmp(entry[0], "alpha"), "StringArray sort first column");
        SetAttrs(list, MUIA_List_SortColumn, 1, TAG_DONE);
        DoMethod(list, MUIM_List_Sort);
        DoMethod(list, MUIM_List_GetEntry, 0, &entry);
        check(entry && !strcmp(entry[1], "1"), "StringArray sort second column");
        ULONG order[4] = {0xcccccccc,0xcccccccc,0xcccccccc,0xcccccccc};
        GetAttr(MUIA_List_ColumnOrder, list, order);
        check(order[0] == 0x0001cccc && order[1] == 0xcccccccc,
            "ColumnOrder writes caller byte array");
        UBYTE reverse[] = {1, 0};
        SetAttrs(list, MUIA_List_ColumnOrder, reverse, TAG_DONE);
        GetAttr(MUIA_List_ColumnOrder, list, order);
        check(order[0] == 0x0100cccc && order[1] == 0xcccccccc,
            "ColumnOrder changes permutation");
        MUI_DisposeObject(list);
    }
}

static void directory_sizes(void)
{
    BPTR lock = CreateDir("T:mui5-dir-probe");
    check(lock != 0, "create private directory fixture");
    if (!lock) return;
    UnLock(lock);
    BPTR file = Open("T:mui5-dir-probe/one", MODE_NEWFILE);
    check(file != 0, "create first file");
    if (file) { Write(file, "123", 3); Close(file); }
    file = Open("T:mui5-dir-probe/two", MODE_NEWFILE);
    check(file != 0, "create second file");
    if (file) { Write(file, "123456", 6); Close(file); }
    Object *dir = MO(MUIC_Dirlist, TAG_DONE);
    check(dir != NULL, "Dirlist creation");
    if (dir)
    {
        Object *group = MO(MUIC_Group, MUIA_Group_Child, dir, TAG_DONE);
        Object *win = MO(MUIC_Window, MUIA_Window_Screen, screen, MUIA_Window_RootObject, group, TAG_DONE);
        Object *app = MO(MUIC_Application, MUIA_Application_Window, win, TAG_DONE);
        check(app != NULL, "directory application");
        SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
        SetAttrs(dir, MUIA_Dirlist_Directory, "T:mui5-dir-probe", TAG_DONE);
        /* Deliver completion messages from the reference reader task. */
        for (int i = 0; i < 100 && (i < 10 || attr(dir, MUIA_Dirlist_Status) == MUIV_Dirlist_Status_Reading); i++) { DoMethod(app, MUIM_Application_InputBuffered); Delay(2); }
        printf("directory open=%lu status=%lu path=%s\n", attr(win, MUIA_Window_Open), attr(dir, MUIA_Dirlist_Status), (char *)attr(dir, MUIA_Dirlist_Directory));
        unsigned long long *bytes = (APTR)attr(dir, MUIA_Dirlist_NumBytes64);
        printf("directory files=%lu bytes=%lu pointer=%p\n", attr(dir, MUIA_Dirlist_NumFiles), attr(dir, MUIA_Dirlist_NumBytes), bytes);
        ULONG total = attr(dir, MUIA_Dirlist_NumBytes);
        check(bytes && *bytes == total && total >= 9,
            "32 and 64 bit directory totals");
        DoMethod(dir, MUIM_Dirlist_ReRead);
        for (int i = 0; i < 100 && (i < 10 || attr(dir, MUIA_Dirlist_Status) == MUIV_Dirlist_Status_Reading); i++) { DoMethod(app, MUIM_Application_InputBuffered); Delay(2); }
        check(attr(dir, MUIA_Dirlist_NumFiles) == 2 && bytes && *bytes == total,
            "directory reread resets counters");
        MUI_DisposeObject(app);
    }
    DeleteFile("T:mui5-dir-probe/one");
    DeleteFile("T:mui5-dir-probe/two");
    DeleteFile("T:mui5-dir-probe");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    MUIMasterBase = OpenLibrary("muimaster.library", 21);
    if (!MUIMasterBase) { puts("FAIL OpenLibrary version 21"); return 20; }
    printf("library=%u.%u %s\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision, (char *)MUIMasterBase->lib_IdString);
    Object *o = MO(MUIC_Text, TAG_DONE);
    check(o && attr(o, MUIA_Version) >= 21, "built-in class version gate");
    if (o) MUI_DisposeObject(o);
    if (argc > 1 && !strcmp(argv[1], "screen"))
        screen = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2, TAG_DONE);
    maps(); textdata(); containers(); directory_sizes();
    if (screen) CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 20 : 0;
}

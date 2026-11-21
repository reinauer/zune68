/*
 * fixpaths: paths fixed after OpenMUI's fault injection and code audit,
 * each printed as one line.
 *
 *   nochild    MUI_NewObject() of a class that does not exist, given a
 *              MUIA_Group_Child: is the child disposed? (MUI 3.8: no)
 *   nullbutton a Popobject given a NULL button (a failed PopButton()), a
 *              string and an object: how often each is disposed (MUI 3.8:
 *              string once, object never, lost; Zune68: both once)
 *   dataspace  a record whose data has no terminator, added where an
 *              earlier record of 0xFF bytes was: the byte after its data
 *              (0 when each record ends with a zero byte)
 *   gauge      MUIA_Gauge_InfoText set to NULL (survives)
 *   popobject  a Popobject in a window on a screen of the probe's own:
 *              the screen its popup window opens on
 *   listwrap   (only with the argument "listwrap", last: may crash the
 *              library before the fix) MUIM_List_Insert with a count no
 *              memory can hold: the result and the entries afterwards
 *
 * Build: m68k-amigaos-gcc -noixemul -Os -s -Ibuild/muimaster/include \
 *        tests/native/fixpaths.c -o fixpaths -lamiga
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

struct Library *MUIMasterBase;

#ifndef MUIM_Dataspace_Add
#define MUIM_Dataspace_Add    0x80423366
#define MUIM_Dataspace_Find   0x8042832c
#define MUIM_Dataspace_Remove 0x8042dce1
#endif

static int disposed;
static Object *watched[2];
static int watched_disposed[2];

static IPTR tracked_dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == OM_DISPOSE)
    {
        disposed++;
        if (obj == watched[0]) watched_disposed[0]++;
        if (obj == watched[1]) watched_disposed[1]++;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void nochild(void)
{
    struct MUI_CustomClass *mcc;
    Object *child, *o;

    mcc = MUI_CreateCustomClass(NULL, MUIC_Rectangle, NULL, 4,
        (APTR)tracked_dispatch);
    if (!mcc) { printf("nochild: no class\n"); return; }
    child = NewObject(mcc->mcc_Class, NULL, TAG_DONE);
    o = MUI_NewObject("NoSuchClass.mui", MUIA_Group_Child, (IPTR)child,
        TAG_DONE);
    printf("nochild: object %s, child %s\n", o ? "made" : "NULL",
        disposed ? "disposed" : "kept");
    if (!disposed)
        MUI_DisposeObject(child);
    MUI_DeleteCustomClass(mcc);
}

static void nullbutton(void)
{
    struct MUI_CustomClass *mcc;
    Object *o;

    mcc = MUI_CreateCustomClass(NULL, MUIC_Rectangle, NULL, 4,
        (APTR)tracked_dispatch);
    if (!mcc) { printf("nullbutton: no class\n"); return; }
    watched[0] = NewObject(mcc->mcc_Class, NULL, TAG_DONE);
    watched[1] = NewObject(mcc->mcc_Class, NULL, TAG_DONE);
    if (!watched[0] || !watched[1])
    {
        printf("nullbutton: no objects\n");
        MUI_DisposeObject(watched[0]);
        MUI_DisposeObject(watched[1]);
        MUI_DeleteCustomClass(mcc);
        return;
    }
    o = MUI_NewObject(MUIC_Popobject,
        MUIA_Popstring_String, (IPTR)watched[0],
        MUIA_Popstring_Button, 0,
        MUIA_Popobject_Object, (IPTR)watched[1],
        TAG_DONE);
    printf("nullbutton: object %s, string disposed %d, object disposed %d\n",
        o ? "made" : "NULL", watched_disposed[0], watched_disposed[1]);
    if (o)
        MUI_DisposeObject(o);
    else
    {
        if (!watched_disposed[0]) MUI_DisposeObject(watched[0]);
        if (!watched_disposed[1]) MUI_DisposeObject(watched[1]);
    }
    watched[0] = watched[1] = NULL;
    MUI_DeleteCustomClass(mcc);
}

static void dataspace(void)
{
    static UBYTE ff[12], five[5] = { 'A', 'B', 'C', 'D', 'E' };
    Object *ds = MUI_NewObject(MUIC_Dataspace, TAG_DONE);
    UBYTE *p;

    if (!ds) { printf("dataspace: no object\n"); return; }
    memset(ff, 0xFF, sizeof(ff));
    DoMethod(ds, MUIM_Dataspace_Add, (IPTR)ff, sizeof(ff), 1);
    DoMethod(ds, MUIM_Dataspace_Remove, 1);
    DoMethod(ds, MUIM_Dataspace_Add, (IPTR)five, sizeof(five), 2);
    p = (UBYTE *)DoMethod(ds, MUIM_Dataspace_Find, 2);
    if (p)
        printf("dataspace: byte after the data 0x%02x\n", p[sizeof(five)]);
    else
        printf("dataspace: record not found\n");
    MUI_DisposeObject(ds);
}

static void gauge(void)
{
    Object *o = GaugeObject, MUIA_Gauge_Horiz, TRUE,
        MUIA_Gauge_InfoText, (IPTR)"%ld %%", End;

    if (!o) { printf("gauge: no object\n"); return; }
    set(o, MUIA_Gauge_InfoText, (IPTR)NULL);
    set(o, MUIA_Gauge_Current, 10);
    printf("gauge: survived\n");
    MUI_DisposeObject(o);
}

static void popobject(void)
{
    struct Screen *scr;
    Object *app, *win, *pop, *lv, *pw = NULL;
    struct Window *w = NULL;

    scr = OpenScreenTags(NULL, SA_Depth, 2, SA_Title, (IPTR)"fixpaths",
        SA_Type, CUSTOMSCREEN, SA_LikeWorkbench, TRUE, TAG_DONE);
    if (!scr) { printf("popobject: no screen\n"); return; }
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"FixPaths",
        SubWindow, (IPTR)(win = WindowObject,
            MUIA_Window_Title, (IPTR)"FixPaths",
            MUIA_Window_Screen, (IPTR)scr,
            WindowContents, VGroup,
                Child, (IPTR)(pop = PopobjectObject,
                    MUIA_Popstring_String, (IPTR)StringObject, StringFrame, End,
                    MUIA_Popstring_Button, (IPTR)PopButton(MUII_PopUp),
                    MUIA_Popobject_Object, (IPTR)(lv = ListviewObject,
                        MUIA_Listview_List, (IPTR)(ListObject, InputListFrame,
                            MUIA_List_SourceArray, (IPTR)(const char *[]){"One", "Two", NULL},
                        End),
                    End),
                End),
            End,
        End),
    End;
    if (!app) { printf("popobject: no app\n"); CloseScreen(scr); return; }
    set(win, MUIA_Window_Open, TRUE);
    DoMethod(pop, MUIM_Popstring_Open);
    get(lv, MUIA_WindowObject, &pw);
    if (pw)
        get(pw, MUIA_Window_Window, &w);
    if (!w)
        printf("popobject: no popup window\n");
    else
        printf("popobject: popup on %s\n", w->WScreen == scr ? "the parent's screen" :
            w->WScreen->Title ? (char *)w->WScreen->Title : "another screen");
    DoMethod(pop, MUIM_Popstring_Close, TRUE);
    MUI_DisposeObject(app);
    CloseScreen(scr);
}

static void listwrap(void)
{
    static const char *entries[] = { "one", "two", NULL };
    Object *o = ListObject, MUIA_List_SourceArray, (IPTR)entries, End;
    LONG n = -1;
    IPTR r;

    if (!o) { printf("listwrap: no object\n"); return; }
    printf("listwrap: inserting\n");
    fflush(stdout);
    r = DoMethod(o, MUIM_List_Insert, (IPTR)entries, 0x40000000,
        MUIV_List_Insert_Bottom);
    get(o, MUIA_List_Entries, &n);
    printf("listwrap: insert returned %ld, entries %ld\n", (long)r, (long)n);
    MUI_DisposeObject(o);
}

int main(int argc, char **argv)
{
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    printf("muimaster.library %ld.%ld\n", (long)MUIMasterBase->lib_Version,
        (long)MUIMasterBase->lib_Revision);
    nochild();
    nullbutton();
    dataspace();
    gauge();
    popobject();
    if (argc > 1 && !strcmp(argv[1], "listwrap"))
        listwrap();
    CloseLibrary(MUIMasterBase);
    return 0;
}

/* Which attributes a subclass's OM_SET sees while the superclass's OM_NEW
 * runs. A class that applies its own attributes in OM_NEW with
 * set(obj, ...) dispatches them to the subclass first, whose instance data
 * is not set up yet; MUI 3.8 applies them without the subclass. One
 * subclass per class; a line per class with the attribute ids its OM_SET
 * got during OM_NEW ("none" when there were none). Volumelist, Dirlist and
 * Popscreen are left out: MUI 3.8 sets their hooks through the subclass
 * too, but not the same hooks as Zune68.
 * After OpenMUI's tests/lean/probe/newset.c by Tobias Karlsson.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit \
 *        newset.c -o newset -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static int in_new;
static ULONG seen[64];
static int nseen;

static ULONG Dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"),
    Msg msg __asm("a1"))
{
    if (msg->MethodID == OM_NEW)
    {
        ULONG r;

        in_new++;
        r = DoSuperMethodA(cl, obj, msg);
        in_new--;
        return r;
    }
    if (msg->MethodID == OM_SET && in_new)
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList, *tag;

        while ((tag = NextTagItem(&tags)))
            if (nseen < 64)
                seen[nseen++] = tag->ti_Tag;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void make(const char *name, const char *super, struct TagItem *tags)
{
    struct MUI_CustomClass *mcc;
    Object *o;
    int i;

    nseen = 0;
    mcc = MUI_CreateCustomClass(NULL, (char *)super, NULL, 4, (APTR)Dispatch);
    if (!mcc)
    {
        printf("%-10s no class\n", name);
        return;
    }
    o = NewObjectA(mcc->mcc_Class, NULL, tags);
    printf("%-10s %s OM_SET during OM_NEW:", name, o ? "made" : "failed");
    if (!nseen)
        printf(" none");
    for (i = 0; i < nseen; i++)
        printf(" %08lx", seen[i]);
    printf("\n");
    if (o)
        MUI_DisposeObject(o);
    MUI_DeleteCustomClass(mcc);
}

static Object *str(void)
{
    return StringObject, End;
}

static Object *button(ULONG spec)
{
    return ImageObject, MUIA_Image_Spec, spec, End;
}

int main(void)
{
    static const char *array[] = { "one", "two", NULL };

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 11)))
        return 20;
    {
        struct TagItem t[] = { { MUIA_Disabled, TRUE }, { MUIA_Group_Child, 0 }, { TAG_DONE, 0 } };
        t[1].ti_Data = (IPTR)(TextObject, MUIA_Text_Contents, "x", End);
        make("Group", MUIC_Group, t);
    }
    {
        struct TagItem t[] = { { MUIA_List_DragSortable, TRUE }, { MUIA_Listview_DragType, 1 }, { TAG_DONE, 0 } };
        make("List", MUIC_List, t);
    }
    {
        struct TagItem t[] = { { MUIA_Floattext_Text, (IPTR)"a b c" }, { TAG_DONE, 0 } };
        make("Floattext", MUIC_Floattext, t);
    }
    {
        struct TagItem t[] = { { TAG_DONE, 0 } };
        make("Scale", MUIC_Scale, t);
    }
    {
        struct TagItem t[] = { { MUIA_Popstring_String, 0 }, { MUIA_Popstring_Button, 0 },
            { MUIA_Popobject_Object, 0 }, { TAG_DONE, 0 } };
        t[0].ti_Data = (IPTR)str();
        t[1].ti_Data = (IPTR)button(MUII_PopUp);
        t[2].ti_Data = (IPTR)(ListviewObject, MUIA_Listview_List, ListObject, End, End);
        make("Popobject", MUIC_Popobject, t);
    }
    {
        struct TagItem t[] = { { MUIA_Popstring_String, 0 }, { MUIA_Popstring_Button, 0 }, { TAG_DONE, 0 } };
        t[0].ti_Data = (IPTR)str();
        t[1].ti_Data = (IPTR)button(MUII_PopFile);
        make("Popasl", MUIC_Popasl, t);
    }
    {
        struct TagItem t[] = { { MUIA_Popstring_String, 0 }, { MUIA_Popstring_Button, 0 },
            { MUIA_Poplist_Array, (IPTR)array }, { TAG_DONE, 0 } };
        t[0].ti_Data = (IPTR)str();
        t[1].ti_Data = (IPTR)button(MUII_PopUp);
        make("Poplist", MUIC_Poplist, t);
    }
    {
        struct TagItem t[] = { { MUIA_Application_Base, (IPTR)"NEWSET" }, { TAG_DONE, 0 } };
        make("Application", MUIC_Application, t);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}

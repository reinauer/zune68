/* Walk another application's MUI window and print each object's class,
 * box, min/max height and virtual size, to compare layouts on native and
 * MUI 3.9. Usage: objtree <window title substring>
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit objtree.c -o objtree -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <intuition/intuitionbase.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
extern struct IntuitionBase *IntuitionBase;
ULONG __stack = 65536;
static const char *cname(Object *o)
{
    struct IClass *c = OCLASS(o);
    while (c && !c->cl_ID) c = c->cl_Super;
    return c ? (const char *)c->cl_ID : "?";
}
static int isgroup(Object *o)
{
    struct IClass *c;
    for (c = OCLASS(o); c; c = c->cl_Super)
        if (c->cl_ID && !strcmp((char *)c->cl_ID, MUIC_Group)) return 1;
    return 0;
}
static void walk(Object *o, int depth)
{
    ULONG vh = 0, vw = 0;
    struct List *l = NULL;
    printf("%*s%-14s %4ld,%4ld %4ldx%4ld minh %4ld maxh %5ld", depth * 2, "", cname(o),
        (LONG)_left(o), (LONG)_top(o), (LONG)_width(o), (LONG)_height(o),
        (LONG)_minheight(o), (LONG)_maxheight(o));
    if (GetAttr(MUIA_Virtgroup_Height, o, &vh) && GetAttr(MUIA_Virtgroup_Width, o, &vw) && isgroup(o))
        printf(" virt %ldx%ld", (LONG)vw, (LONG)vh);
    if (!strcmp(cname(o), MUIC_Prop))
    {
        ULONG first = 0, entries = 0, visible = 0;
        GetAttr(MUIA_Prop_First, o, &first);
        GetAttr(MUIA_Prop_Entries, o, &entries);
        GetAttr(MUIA_Prop_Visible, o, &visible);
        printf(" prop %ld/%ld/%ld", (LONG)first, (LONG)entries, (LONG)visible);
    }
    printf("\n");
    if (depth > 6 || !isgroup(o)) return;
    if (GetAttr(MUIA_Group_ChildList, o, (ULONG *)&l) && l)
    {
        APTR cstate = l->lh_Head;
        Object *c;
        int n = 0;
        while ((c = NextObject(&cstate)) && n++ < 12)
            walk(c, depth + 1);
    }
}
int main(int argc, char **argv)
{
    struct Screen *s;
    struct Window *w, *found = NULL;
    Object *wobj = NULL, *root = NULL;
    if (argc < 2) return 20;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    Forbid();
    for (s = IntuitionBase->FirstScreen; s && !found; s = s->NextScreen)
        for (w = s->FirstWindow; w; w = w->NextWindow)
            if (w->Title && strstr((char *)w->Title, argv[1])) { found = w; break; }
    if (found)
    {
        wobj = (Object *)found->UserData;
        if (wobj) GetAttr(MUIA_Window_RootObject, wobj, (ULONG *)&root);
        printf("window %p %ldx%ld wobj %p root %p\n", found, (LONG)found->Width, (LONG)found->Height, wobj, root);
        if (root) walk(root, 0);
    }
    Permit();
    CloseLibrary(MUIMasterBase);
    return 0;
}

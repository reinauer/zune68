/* Print the MUI object tree of another application's window: class (the
 * first named class up the chain for custom classes), box in window
 * coordinates, inner box and area flags. Read-only, without locking, for
 * a test guest. With x,y, prints only the objects containing that point.
 *   wintree <title prefix> [x y]
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit wintree.c -o wintree -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <intuition/intuitionbase.h>
#include <intuition/classes.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>

extern struct IntuitionBase *IntuitionBase;
ULONG __stack = 32768;

static LONG px = -1, py = -1;

static const char *class_name(Object *obj, int *custom)
{
    struct IClass *cl = OCLASS(obj);

    *custom = 0;
    while (cl && !cl->cl_ID)
    {
        cl = cl->cl_Super;
        *custom = 1;
    }
    return cl ? (const char *) cl->cl_ID : "?";
}

static void walk(Object *obj, int depth)
{
    struct List *children = NULL;
    int custom;
    const char *name = class_name(obj, &custom);
    BOOL inside = px < 0 || (px >= _left(obj) && px <= _right(obj)
        && py >= _top(obj) && py <= _bottom(obj));

    if (inside)
        printf("%*s%s%s %08lx box %ld,%ld %ldx%ld inner %ld,%ld %ldx%ld "
            "flags %08lx\n", depth * 2, "", custom ? "*" : "", name,
            (unsigned long) obj, (long) _left(obj), (long) _top(obj),
            (long) _width(obj), (long) _height(obj), (long) _mleft(obj),
            (long) _mtop(obj), (long) _mwidth(obj), (long) _mheight(obj),
            (unsigned long) _flags(obj));
    if (!inside)
        return;
    if (get(obj, MUIA_Group_ChildList, &children) && children)
    {
        APTR state = children->lh_Head;
        Object *child;

        while ((child = NextObject(&state)))
            walk(child, depth + 1);
    }
}

int main(int argc, char **argv)
{
    struct Screen *s;
    struct Window *w, *found = NULL;
    Object *mwin, *root = NULL;
    ULONG lock;
    int n;

    if (argc < 2)
        return 20;
    if (argc > 3)
    {
        px = atol(argv[2]);
        py = atol(argv[3]);
    }
    n = strlen(argv[1]);
    lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && !found; s = s->NextScreen)
        for (w = s->FirstWindow; w && !found; w = w->NextWindow)
            if (w->Title && !strncmp((const char *) w->Title, argv[1], n))
                found = w;
    UnlockIBase(lock);
    if (!found || !(mwin = (Object *) found->UserData))
    {
        printf("no MUI window titled %s*\nFAIL rc=20\n", argv[1]);
        return 20;
    }
    printf("window %ld,%ld %ldx%ld borders %ld,%ld,%ld,%ld\n",
        (long) found->LeftEdge, (long) found->TopEdge, (long) found->Width,
        (long) found->Height, (long) found->BorderLeft,
        (long) found->BorderTop, (long) found->BorderRight,
        (long) found->BorderBottom);
    get(mwin, MUIA_Window_RootObject, &root);
    if (root)
        walk(root, 0);
    printf("PASS rc=0\n");
    return 0;
}

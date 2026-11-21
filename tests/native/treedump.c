/* treedump: the object tree of another program's open windows, with each
 * object's class, box, inner box and minimum/maximum size. The program is
 * found through MUI's application list (private LVO -264 and its private
 * find method, as AmIRC uses them), by its MUIA_Application_Base or
 * MUIA_Application_Title.
 *   treedump base|title STRING
 * Test-only: it reads another task's objects without locking them.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit treedump.c -o treedump -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static Object *applist(void)
{
    register struct Library *a6 __asm("a6") = MUIMasterBase;
    register Object *d0 __asm("d0");
    __asm volatile ("jsr -264(%%a6)" : "=r"(d0) : "r"(a6)
        : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static const char *classname(Object *o)
{
    static char buf[64];
    struct IClass *cl = OCLASS(o);
    int depth = 0;

    while (cl && !cl->cl_ID)
        cl = cl->cl_Super, depth++;
    if (!cl)
        return "?";
    if (!depth)
        return (const char *)cl->cl_ID;
    snprintf(buf, sizeof(buf), "%s+%d", (const char *)cl->cl_ID, depth);
    return buf;
}

static void dump(Object *o, int level)
{
    struct List *children = NULL;
    Object *child;
    APTR state;

    printf("%*s%s box=%d,%d %dx%d inner=%d,%d %dx%d min=%dx%d max=%dx%d\n",
        level * 2, "", classname(o), _left(o), _top(o), _width(o),
        _height(o), _mleft(o) - _left(o), _mtop(o) - _top(o), _mwidth(o),
        _mheight(o), _minwidth(o), _minheight(o), _maxwidth(o),
        _maxheight(o));
    if (!GetAttr(MUIA_Group_ChildList, o, (IPTR *)&children) || !children)
        return;
    state = children->lh_Head;
    while ((child = NextObject(&state)))
        dump(child, level + 1);
}

int main(int argc, char **argv)
{
    Object *list, *app, *win, *root;
    struct List *windows = NULL;
    APTR state;
    ULONG attr;

    if (argc != 3)
        return 10;
    attr = !strcmp(argv[1], "base") ? MUIA_Application_Base
        : MUIA_Application_Title;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    list = applist();
    app = list ? (Object *)DoMethod(list, 0x8042e50f, attr, argv[2]) : NULL;
    if (!app) {
        printf("no application %s\n", argv[2]);
        CloseLibrary(MUIMasterBase);
        return 5;
    }
    GetAttr(MUIA_Application_WindowList, app, (IPTR *)&windows);
    state = windows ? windows->lh_Head : NULL;
    while (state && (win = NextObject(&state))) {
        IPTR open = 0, w = 0, h = 0;
        root = NULL;
        GetAttr(MUIA_Window_Open, win, &open);
        GetAttr(MUIA_Window_Width, win, &w);
        GetAttr(MUIA_Window_Height, win, &h);
        GetAttr(MUIA_Window_RootObject, win, (IPTR *)&root);
        printf("window open=%lu inner=%lux%lu\n", (unsigned long)open,
            (unsigned long)w, (unsigned long)h);
        if (open && root)
            dump(root, 1);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}

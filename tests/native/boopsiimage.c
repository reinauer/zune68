/* boopsiimage [SPEC...]: image specs naming external BOOPSI image classes
 * ("3:<name>.image"). A window with one Image object per spec at its own
 * size and one free in both directions, the first spec also as a toggle
 * button (drawn normal, then selected) and stretched into a fixed 40x24
 * box. Prints each object's minimum,
 * maximum and default size, the size it got, and its pixels as pen numbers
 * (one character per pixel), how many copies of each class library are in
 * memory and how often they are open, then quits. Without arguments: kmel and XEN
 * images from MUI:Images, one by its full path, and two that do not exist.
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include boopsiimage.c
 *   -o boopsiimage -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <exec/execbase.h>
#include <stdio.h>
#include <string.h>

extern struct ExecBase *SysBase;
struct Library *MUIMasterBase;
ULONG __stack = 32768;

static const char *const defaults[] = {
    "3:kmel/kmel_arrowdown.image", "3:kmel/kmel_checkmark.image",
    "3:kmel/kmel_cycle.image", "3:kmel/kmel_popdrawer.image",
    "3:kmel/kmel_knob2.image", "3:XEN/Scrollbars/xenbar.image",
    "3:Misc/mui-stdcycle.image", "3:MUI:Images/kmel/kmel_arrowup.image",
    "3:kmel/nothere.image", "3:NOVOLUME:x/nothere.image",
};

#define MAXIMG 12

/* How many libraries of the spec's file name are in memory, and how often
 * the first is open (a second copy would mean the file was loaded twice) */
static void count_libs(const char *spec)
{
    const char *name = FilePart((STRPTR)spec + 2);
    struct Node *n;
    int copies = 0, opens = 0;

    Forbid();
    for (n = SysBase->LibList.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (n->ln_Name && !strcmp(n->ln_Name, name))
            if (!copies++)
                opens = ((struct Library *)n)->lib_OpenCnt;
    Permit();
    printf("%s: %d in memory, open %d\n", name, copies, opens);
}

static void dump(Object *o, const char *what)
{
    struct RastPort *rp = _rp(o);
    int x, y, w = _width(o), h = _height(o);
    char row[100];

    printf("%s: min %dx%d max %dx%d def %dx%d got %dx%d\n", what,
        _minwidth(o), _minheight(o), _maxwidth(o), _maxheight(o),
        _defwidth(o), _defheight(o), w, h);
    if (w > 96)
        w = 96;
    if (h > 40)
        h = 40;
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            LONG p = ReadPixel(rp, _left(o) + x, _top(o) + y);
            row[x] = p < 10 ? '0' + p : p < 36 ? 'a' + p - 10 : '#';
        }
        row[w] = 0;
        printf("  %s\n", row);
    }
}

int main(int argc, char **argv)
{
    Object *app, *win, *grp, *img[MAXIMG], *freeimg[MAXIMG], *button, *stretched;
    const char *specs[MAXIMG];
    int n = 0, i;
    IPTR open = 0;

    for (i = 1; i < argc && n < MAXIMG; i++)
        specs[n++] = argv[i];
    if (!n)
        for (i = 0; i < (int)(sizeof(defaults) / sizeof(defaults[0])); i++)
            specs[n++] = defaults[i];
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    grp = ColGroup(3), End;
    for (i = 0; i < n; i++)
    {
        img[i] = ImageObject, MUIA_Image_Spec, (IPTR)specs[i], End;
        freeimg[i] = ImageObject, MUIA_Image_Spec, (IPTR)specs[i],
            MUIA_Image_FreeHoriz, TRUE, MUIA_Image_FreeVert, TRUE, End;
        if (grp && img[i] && freeimg[i])
        {
            DoMethod(grp, OM_ADDMEMBER, (IPTR)(TextObject,
                MUIA_Text_Contents, (IPTR)specs[i], End));
            DoMethod(grp, OM_ADDMEMBER, (IPTR)img[i]);
            DoMethod(grp, OM_ADDMEMBER, (IPTR)freeimg[i]);
        }
        else
        {
            printf("%s: no object\n", specs[i]);
            img[i] = NULL;
        }
    }
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"boopsiimage",
        MUIA_Application_Base, (IPTR)"BOOPSIIMAGE",
        SubWindow, (IPTR)(win = WindowObject,
            MUIA_Window_Title, (IPTR)"boopsiimage",
            MUIA_Window_ID, 0,
            WindowContents, (IPTR)(VGroup,
                Child, (IPTR)grp,
                Child, (IPTR)(HGroup,
                    Child, (IPTR)(button = ImageObject,
                        MUIA_Image_Spec, (IPTR)specs[0],
                        MUIA_InputMode, MUIV_InputMode_Toggle,
                        MUIA_Frame, MUIV_Frame_ImageButton, End),
                    Child, (IPTR)(stretched = ImageObject,
                        MUIA_Image_Spec, (IPTR)specs[0],
                        MUIA_Image_FreeHoriz, TRUE, MUIA_Image_FreeVert, TRUE,
                        MUIA_FixWidth, 40, MUIA_FixHeight, 24, End),
                    Child, (IPTR)HSpace(0),
                End),
            End),
        End),
    End;
    if (!app)
    {
        printf("no application\n");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %ld\n", (long)open);
    if (open)
    {
        Delay(10);
        for (i = 0; i < n; i++)
            if (img[i])
            {
                dump(img[i], specs[i]);
                dump(freeimg[i], "  free");
            }
        dump(button, "button");
        dump(stretched, "stretched");
        set(button, MUIA_Selected, TRUE);
        Delay(10);
        dump(button, "button selected");
        for (i = 0; i < n; i++)
            count_libs(specs[i]);
        set(win, MUIA_Window_Open, FALSE);
        printf("closed:\n");
        for (i = 0; i < n; i++)
            count_libs(specs[i]);
    }
    fflush(stdout);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

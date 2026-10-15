/* Corner pixels of bevel and recessed frames on a parent with a white
 * (MUII_SHINE) background: is a frame corner the parent's background, the
 * object's own background or always MPEN_BACKGROUND? Prints, per object
 * and state, the four corner pens, the pen just outside the top-left
 * corner and the pen just inside it. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<SDK>/MUI/C/include \
 *        -Wl,-u,___stkinit cornerbg.c -o cornerbg -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdarg.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static Object *new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;

    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject new_object

static void corners(const char *name, Object *o)
{
    struct RastPort *rp = _rp(o);
    WORD l = _left(o), t = _top(o), r = _right(o), b = _bottom(o);

    printf("%-10s %3dx%-3d tl=%ld tr=%ld bl=%ld br=%ld outside=%ld inside=%ld\n",
        name, _width(o), _height(o),
        (long)ReadPixel(rp, l, t), (long)ReadPixel(rp, r, t),
        (long)ReadPixel(rp, l, b), (long)ReadPixel(rp, r, b),
        (long)ReadPixel(rp, l - 1, t - 1), (long)ReadPixel(rp, l + 2, t + 2));
}

int main(void)
{
    Object *app, *win, *b1, *b2, *str, *txt, *img, *b3, *b4;
    struct Screen *scr;
    ULONG open = 0;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_Depth, 4,
        TAG_DONE);
    if (!scr)
        goto close;
    app = ApplicationObject,
        SubWindow, win = WindowObject,
            MUIA_Window_Screen, scr,
            WindowContents, VGroup,
                Child, VGroup, MUIA_Background, MUII_SHINE,
                    InnerSpacing(6, 6),
                    Child, b1 = SimpleButton("Normal"),
                    Child, b2 = SimpleButton("Pressed"),
                    Child, str = StringObject, StringFrame,
                        MUIA_String_Contents, "text", End,
                    Child, txt = TextObject, TextFrame,
                        MUIA_Text_Contents, "frame", End,
                    Child, img = ImageObject, ImageButtonFrame,
                        MUIA_Image_Spec, MUII_ArrowUp, End,
                    End,
                Child, VGroup, InnerSpacing(6, 6),
                    Child, b3 = SimpleButton("Grey"),
                    Child, b4 = SimpleButton("GreyPressed"),
                    End,
                End,
            End,
        End;
    if (!app)
        goto screen;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto dispose;
    DoMethod(app, MUIM_Application_InputBuffered);
    puts("state normal");
    corners("button", b1);
    corners("string", str);
    corners("textframe", txt);
    corners("imagebtn", img);
    corners("greybtn", b3);
    set(b2, MUIA_Selected, TRUE);
    set(b4, MUIA_Selected, TRUE);
    puts("state pressed");
    corners("button", b2);
    corners("greybtn", b4);
    set(b2, MUIA_Selected, FALSE);
    set(b4, MUIA_Selected, FALSE);
    puts("state released");
    corners("button", b2);
    corners("greybtn", b4);
    rc = 0;
dispose:
    MUI_DisposeObject(app);
screen:
    CloseScreen(scr);
close:
    CloseLibrary(MUIMasterBase);
    printf(rc ? "FAIL rc=%d\n" : "PASS rc=%d\n", rc);
    return rc;
}

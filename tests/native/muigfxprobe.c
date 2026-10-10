/* muigfx.library parity: the calls TearOffPanel.mcc and Voyager make,
 * printed so MUI 3.9's muigfx 20.606 and Zune68's built-in version can
 * be diffed. "muigfxprobe hold" keeps the window with drawn frames open.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit muigfxprobe.c -o muigfxprobe -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase, *MUIGfxBase;
ULONG __stack = 65536;

static APTR GetImage(APTR mri, CONST_STRPTR spec, ULONG flags)
{
    register APTR a0 __asm("a0") = mri;
    register CONST_STRPTR a1 __asm("a1") = spec;
    register ULONG d0 __asm("d0") = flags;
    register struct Library *a6 __asm("a6") = MUIGfxBase;
    __asm volatile ("jsr a6@(-114:W)" : "+r"(d0), "+r"(a0), "+r"(a1)
        : "r"(a6) : "d1", "cc", "memory");
    return (APTR)d0;
}
static void ReleaseImage(APTR image)
{
    register APTR a0 __asm("a0") = image;
    register struct Library *a6 __asm("a6") = MUIGfxBase;
    __asm volatile ("jsr a6@(-120:W)" : "+r"(a0) : "r"(a6)
        : "d0", "d1", "a1", "cc", "memory");
}
static ULONG GetImageAttr(APTR image, ULONG attr, APTR mri)
{
    register APTR a0 __asm("a0") = image;
    register ULONG d0 __asm("d0") = attr;
    register APTR a1 __asm("a1") = mri;
    register struct Library *a6 __asm("a6") = MUIGfxBase;
    __asm volatile ("jsr a6@(-126:W)" : "+r"(d0), "+r"(a0), "+r"(a1)
        : "r"(a6) : "d1", "cc", "memory");
    return d0;
}
static void GfxDrawImage(APTR mri, APTR image, LONG l, LONG t, LONG w, LONG h,
    ULONG flags, ULONG flags2)
{
    register APTR a0 __asm("a0") = mri;
    register APTR a1 __asm("a1") = image;
    register LONG d0 __asm("d0") = l;
    register LONG d1 __asm("d1") = t;
    register LONG d2 __asm("d2") = w;
    register LONG d3 __asm("d3") = h;
    register ULONG d4 __asm("d4") = flags;
    register ULONG d5 __asm("d5") = flags2;
    register struct Library *a6 __asm("a6") = MUIGfxBase;
    __asm volatile ("jsr a6@(-132:W)" : "+r"(a0), "+r"(a1), "+r"(d0), "+r"(d1)
        : "r"(d2), "r"(d3), "r"(d4), "r"(d5), "r"(a6) : "cc", "memory");
}
static UBYTE *GetFrame(LONG nr, LONG thin)
{
    register LONG d0 __asm("d0") = nr;
    register LONG d1 __asm("d1") = thin;
    register struct Library *a6 __asm("a6") = MUIGfxBase;
    __asm volatile ("jsr a6@(-180:W)" : "+r"(d0), "+r"(d1) : "r"(a6)
        : "a0", "a1", "cc", "memory");
    return (UBYTE *)d0;
}
static void DrawFrame(APTR mri, APTR frame, LONG l, LONG t, LONG w, LONG h,
    LONG state)
{
    register APTR a0 __asm("a0") = mri;
    register APTR a1 __asm("a1") = frame;
    register LONG d0 __asm("d0") = l;
    register LONG d1 __asm("d1") = t;
    register LONG d2 __asm("d2") = w;
    register LONG d3 __asm("d3") = h;
    register LONG d4 __asm("d4") = state;
    register struct Library *a6 __asm("a6") = MUIGfxBase;
    __asm volatile ("jsr a6@(-186:W)" : "+r"(a0), "+r"(a1), "+r"(d0), "+r"(d1)
        : "r"(d2), "r"(d3), "r"(d4), "r"(a6) : "cc", "memory");
}

int main(int argc, char **argv)
{
    static const char *specs[] = {
        "3:SU_TearOff/openlook_pin_mark.mb0",
        "3:SU_TearOff/tearoff_zigzag2.image",
        "6:1", "1:7", "2:r00000000,00000000,00000000", "0:1", NULL
    };
    Object *app, *win, *rect;
    ULONG open = 0;
    APTR mri;
    int i, j, thin;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    if (!(MUIGfxBase = OpenLibrary("muigfx.library", 0)))
    {
        printf("no muigfx.library\n");
        return 20;
    }
    app = ApplicationObject, MUIA_Application_Title, "muigfxprobe",
        SubWindow, win = WindowObject, MUIA_Window_Title, "muigfxprobe",
            MUIA_Window_Width, 300, MUIA_Window_Height, 160,
            WindowContents, rect = RectangleObject, End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open) return 20;
    mri = muiRenderInfo(rect);
    for (thin = 0; thin < 2; thin++)
    {
        printf("frames thin=%d:", thin);
        for (i = 0; i < 12; i++)
        {
            UBYTE *f = GetFrame(i, thin);
            printf(" %d=", i);
            if (!f)
                printf("NULL");
            else
                for (j = 0; j < 4; j++) printf("%d", f[j]);
        }
        printf("\n");
    }
    for (i = 0; specs[i]; i++)
    {
        APTR image = GetImage(mri, specs[i], 0);
        printf("%-36s %s", specs[i], image ? "image" : "NULL");
        if (image)
        {
            for (j = 1; j <= 6; j++)
                printf(" %lu", GetImageAttr(image, j, mri));
            ReleaseImage(image);
        }
        printf("\n");
    }
    fflush(stdout);
    for (i = 0; i < 12; i++)
        DrawFrame(mri, GetFrame(i, 0), _mleft(rect) + 4 + i * 24,
            _mtop(rect) + 4, 20, 30, 0);
    for (i = 0; i < 12; i++)
        DrawFrame(mri, GetFrame(i, 0), _mleft(rect) + 4 + i * 24,
            _mtop(rect) + 40, 20, 30, 1);
    /* Each spec drawn 10x30 and 20x30, normal and with the 0x20000000
     * flag TearOffPanel passes. */
    for (i = 0; specs[i]; i++)
    {
        APTR image = GetImage(mri, specs[i], 0);
        if (!image) continue;
        GfxDrawImage(mri, image, _mleft(rect) + 4 + i * 40, _mtop(rect) + 76, 10, 30, 0, 0);
        GfxDrawImage(mri, image, _mleft(rect) + 18 + i * 40, _mtop(rect) + 76, 20, 30, 0x20000000, 0);
        ReleaseImage(image);
    }
    if (argc > 1 && !strcmp(argv[1], "hold")) Delay(400);
    MUI_DisposeObject(app);
    CloseLibrary(MUIGfxBase);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}

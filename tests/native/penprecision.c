/* MUI pen allocation probe: prints a window's MUI pens with their RGB
 * values and which pen MUI_ObtainPen returns for RGB specs at growing
 * distance from the background colour, for comparing native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit penprecision.c -o penprecision -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void rgb(struct ColorMap *cm, LONG pen, ULONG *c)
{
    GetRGB32(cm, pen, 1, c);
}

/* Number of pens still free for exclusive allocation. */
static int free_pens(struct ColorMap *cm)
{
    LONG pens[256];
    int n = 0, i;

    while (n < 256 && (pens[n] = ObtainPen(cm, -1, 0, 0, 0,
        PEN_EXCLUSIVE | PEN_NO_SETCOLOR)) != -1)
        n++;
    for (i = 0; i < n; i++)
        ReleasePen(cm, pens[i]);
    return n;
}

int main(int argc, char **argv)
{
    static const ULONG tests[] = {
        0xb0b0b0, 0xb4b4b4, 0xb8b8b8, 0xbfbfbf, 0xc8c8c8, 0xd4d4d4,
        0x8c8c8c, 0x999999, 0xffa997, 0xff0000
    };
    Object *app, *win, *rect;
    ULONG open = 0, c[3];
    struct MUI_RenderInfo *mri;
    struct ColorMap *cm;
    struct Screen *wb = NULL, *own = NULL;
    ULONG old[3], oldshadow[3];
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    /* "bg": run with a different Workbench background colour (pen 0). */
    if (argc > 1 && strcmp(argv[1], "screen") && (wb = LockPubScreen(NULL)))
    {
        GetRGB32(wb->ViewPort.ColorMap, 0, 1, old);
        SetRGB32(&wb->ViewPort, 0, 0x80808080, 0x90909090, 0x60606060);
        GetRGB32(wb->ViewPort.ColorMap, 1, 1, oldshadow);
        /* "bg shadow": also a non-black shadow pen (pen 1). */
        if (argc > 2)
            SetRGB32(&wb->ViewPort, 1, 0x30303030, 0x60606060, 0x18181818);
    }
    /* "screen": a private 32 colour screen with free shared pens. */
    if (argc > 1 && !strcmp(argv[1], "screen"))
    {
        own = OpenScreenTags(NULL, SA_Depth, 5, SA_SharePens, TRUE,
            SA_LikeWorkbench, TRUE, SA_Title, (ULONG)"penprecision",
            TAG_DONE);
        if (!own) return 20;
    }
    app = ApplicationObject, MUIA_Application_Title, "penprecision",
        SubWindow, win = WindowObject, MUIA_Window_Title, "penprecision",
            MUIA_Window_Screen, own,
            WindowContents, rect = RectangleObject, End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
    {
        MUI_DisposeObject(app);
        if (own) CloseScreen(own);
        return 20;
    }
    mri = muiRenderInfo(rect);
    cm = mri->mri_Screen->ViewPort.ColorMap;
    printf("free pens with window open: %d\n", free_pens(cm));
    for (i = 0; i < 8; i++)
    {
        rgb(cm, mri->mri_Pens[i], c);
        printf("mpen %d = %u rgb %02lx%02lx%02lx\n", i, mri->mri_Pens[i],
            c[0] >> 24, c[1] >> 24, c[2] >> 24);
    }
    for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++)
    {
        struct MUI_PenSpec spec;
        LONG pen;

        memset(&spec, 0, sizeof(spec));
        sprintf(spec.buf, "r%08lx,%08lx,%08lx",
            ((tests[i] >> 16) & 0xff) * 0x01010101UL,
            ((tests[i] >> 8) & 0xff) * 0x01010101UL,
            (tests[i] & 0xff) * 0x01010101UL);
        pen = MUI_ObtainPen(mri, &spec, 0);
        rgb(cm, MUIPEN(pen), c);
        printf("obtain %06lx -> %08lx rgb %02lx%02lx%02lx\n", tests[i],
            (ULONG)pen, c[0] >> 24, c[1] >> 24, c[2] >> 24);
        MUI_ReleasePen(mri, pen);
    }
    MUI_DisposeObject(app);
    if (own)
        CloseScreen(own);
    if (wb)
    {
        SetRGB32(&wb->ViewPort, 0, old[0], old[1], old[2]);
        SetRGB32(&wb->ViewPort, 1, oldshadow[0], oldshadow[1], oldshadow[2]);
        UnlockPubScreen(NULL, wb);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}

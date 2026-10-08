/* Read-only public-screen palette and pixel snapshot. No MUI or pen
 * allocation. Usage: palettesnapshot [stage [x y [x y ...]]]
 * Redirect stdout to retain snapshots around another application's reload.
 */
#include <stdio.h>
#include <stdlib.h>
#include <exec/types.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

ULONG __stack = 16384;
#define MAX_PENS 256
#define MAX_SAMPLES 32

struct Sample {
    LONG x, y;
    const char *label;
};

static struct Sample samples[MAX_SAMPLES] = {
    {12, 5, "screen-title"},
    {250, 15, "window-title"},
    {24, 54, "toolbar-left"},
    {100, 60, "toolbar-middle"},
    {180, 60, "toolbar-right"},
    {32, 110, "document-left"},
    {160, 110, "document-image"},
    {320, 110, "document-middle"},
    {620, 390, "workbench-lower-right"}
};

int main(int argc, char **argv)
{
    struct Screen *screen;
    struct ColorMap *cm;
    struct DrawInfo *di;
    ULONG rgb[MAX_PENS * 3], depth, count, i;
    UWORD pens[16], numpens = 0;
    LONG nfree = -1, nshared = -1, sharable = -1;
    int nsamples = 9;
    const char *stage = argc > 1 ? argv[1] : "snapshot";

    if (argc > 2) {
        if ((argc - 2) % 2 || (argc - 2) / 2 > MAX_SAMPLES) {
            puts("Usage: palettesnapshot [stage [x y [x y ...]]]");
            return 20;
        }
        nsamples = (argc - 2) / 2;
        for (int s = 0; s < nsamples; s++) {
            char *endx, *endy;
            samples[s].x = strtol(argv[2 + 2 * s], &endx, 10);
            samples[s].y = strtol(argv[3 + 2 * s], &endy, 10);
            if (*endx || *endy) {
                puts("Pixel coordinates must be decimal integers.");
                return 20;
            }
            samples[s].label = "requested";
        }
    }

    screen = LockPubScreen(NULL);
    if (!screen) {
        puts("Cannot lock the default public screen.");
        return 20;
    }
    cm = screen->ViewPort.ColorMap;
    if (!cm) {
        UnlockPubScreen(NULL, screen);
        puts("Public screen has no ColorMap.");
        return 20;
    }
    depth = GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH);
    count = depth < 8 ? 1UL << depth : MAX_PENS;
    if (count > cm->Count) count = cm->Count;

    if (cm->PalExtra)
        ObtainSemaphoreShared(&cm->PalExtra->pe_Semaphore);
    GetRGB32(cm, 0, count, rgb);
    if (cm->PalExtra) {
        nfree = cm->PalExtra->pe_NFree;
        nshared = cm->PalExtra->pe_NShared;
        sharable = cm->PalExtra->pe_SharableColors;
        ReleaseSemaphore(&cm->PalExtra->pe_Semaphore);
    }
    di = GetScreenDrawInfo(screen);
    if (di) {
        numpens = di->dri_NumPens < 16 ? di->dri_NumPens : 16;
        for (i = 0; i < numpens; i++) pens[i] = di->dri_Pens[i];
        FreeScreenDrawInfo(screen, di);
    }

    printf("stage=%s screen=%08lx size=%ux%u depth=%lu cmap=%u "
        "palette=%lu free=%ld shared=%ld sharable=%ld\n",
        stage, (ULONG)screen, screen->Width, screen->Height, depth,
        cm->Count, count, nfree, nshared, sharable);
    printf("DrawInfo:");
    for (i = 0; i < numpens; i++) printf(" %lu=%u", i, pens[i]);
    printf("\n");
    for (i = 0; i < count; i++)
        printf("pen %lu rgb=%08lx,%08lx,%08lx\n", i,
            rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);

    for (int s = 0; s < nsamples; s++) {
        LONG pen = -1;
        if (samples[s].x >= 0 && samples[s].x < screen->Width &&
            samples[s].y >= 0 && samples[s].y < screen->Height)
            pen = ReadPixel(&screen->RastPort, samples[s].x, samples[s].y);
        printf("pixel %s %ld,%ld pen=%ld", samples[s].label,
            samples[s].x, samples[s].y, pen);
        if (pen >= 0 && (ULONG)pen < count)
            printf(" rgb=%08lx,%08lx,%08lx", rgb[pen * 3],
                rgb[pen * 3 + 1], rgb[pen * 3 + 2]);
        printf("\n");
    }
    UnlockPubScreen(NULL, screen);
    return 0;
}

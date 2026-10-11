/* TextLength and TextExtent of styled strings in the screen font, to
 * model how MUI 3.9 measures bold and italic text chunks.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit styleextent.c -o styleextent -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

ULONG __stack = 32768;

static void show(struct RastPort *rp, const char *s, ULONG style,
    const char *name)
{
    struct TextExtent te;

    SetSoftStyle(rp, style, AskSoftStyle(rp));
    TextExtent(rp, (STRPTR)s, strlen(s), &te);
    printf("%-6s %-24s len %3ld width %3d extent %d..%d\n", name, s,
        (long)TextLength(rp, (STRPTR)s, strlen(s)), te.te_Width,
        te.te_Extent.MinX, te.te_Extent.MaxX);
}

int main(void)
{
    static const char *strings[] = { "Bold text width check", "bold", "b",
        "Italic text", "M", "MUI", "a" };
    struct Screen *scr = LockPubScreen(NULL);
    struct RastPort rp;
    int i;

    if (!scr)
        return 20;
    InitRastPort(&rp);
    SetFont(&rp, scr->RastPort.Font);
    printf("font %s/%d baseline %d smear %d\n",
        scr->RastPort.Font->tf_Message.mn_Node.ln_Name,
        scr->RastPort.Font->tf_YSize, scr->RastPort.Font->tf_Baseline,
        scr->RastPort.Font->tf_BoldSmear);
    for (i = 0; i < (int)(sizeof(strings) / sizeof(strings[0])); i++)
    {
        show(&rp, strings[i], FS_NORMAL, "plain");
        show(&rp, strings[i], FSF_BOLD, "bold");
        show(&rp, strings[i], FSF_ITALIC, "italic");
    }
    UnlockPubScreen(NULL, scr);
    printf("rc=0\n");
    return 0;
}

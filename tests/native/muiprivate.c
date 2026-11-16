/* Call MUI's private library functions -132 to -306 directly and print
 * what they return, to compare MUI 3.8, MUI 5 and Zune68. MUI's own
 * classes and preferences program call them.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os muiprivate.c \
 *        -o muiprivate -lmui -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;

/* Call LVO lvo of muimaster with d0, a0 and a1; return d0. */
static ULONG lvo(LONG lvo, ULONG d0, APTR a0, APTR a1)
{
    register ULONG r_d0 __asm("d0") = d0;
    register APTR r_a0 __asm("a0") = a0;
    register APTR r_a1 __asm("a1") = a1;
    register APTR r_fn __asm("a2") = (UBYTE *)MUIMasterBase + lvo;
    register struct Library *r_a6 __asm("a6") = MUIMasterBase;

    __asm volatile ("jsr %%a2@"
        : "+r" (r_d0), "+r" (r_a0), "+r" (r_a1)
        : "r" (r_fn), "r" (r_a6)
        : "d1", "cc", "memory");
    return r_d0;
}

int main(void)
{
    Object *app, *win, *box;
    ULONG mem, ok, level, image;
    LONG rect[4], far[4] = { 30000, 30000, 30001, 30001 };
    UBYTE item[8] = { 1, 1, 4, 0, 0, 0, 0, 4 };

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    printf("muimaster %ld.%ld\n", (long)MUIMasterBase->lib_Version,
        (long)MUIMasterBase->lib_Revision);

    mem = lvo(-132, 100, NULL, NULL);
    ok = mem && !*(UBYTE *)mem;
    if (mem)
    {
        memset((APTR)mem, 0x55, 100);
        lvo(-138, 100, NULL, (APTR)mem);
    }
    printf("-132/-138 alloc and free: %s\n", ok ? "ok" : "failed");
    mem = lvo(-144, 100, NULL, NULL);
    ok = mem != 0;
    if (mem)
    {
        memset((APTR)mem, 0x55, 100);
        lvo(-150, 0, NULL, (APTR)mem);
    }
    printf("-144/-150 alloc and free: %s\n", ok ? "ok" : "failed");

    /* MUI 3.8 checks the level in the item's fourth byte */
    printf("-270 item allowed: %lu\n", lvo(-270, 0, item, NULL));
    level = lvo(-282, 0, NULL, NULL);
    printf("-282 level: %lu\n", level);

    app = ApplicationObject,
        MUIA_Application_Title, "muiprivate",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "muiprivate",
            WindowContents, VGroup,
                Child, box = TextObject, MUIA_Text_Contents, "box", End,
                End,
            End,
        End;
    if (!app)
    {
        printf("no application\n");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    set(win, MUIA_Window_Open, TRUE);
    lvo(-240, 0, box, rect);
    printf("-240 virtual rect: %ld %ld %ld %ld\n", rect[0], rect[1], rect[2],
        rect[3]);
    printf("-246 in its rect: %lu, far away: %lu\n", lvo(-246, 0, box, rect),
        lvo(-246, 0, box, far));
    image = lvo(-204, 0, muiRenderInfo(box), "2:00000000,00000000,00000000");
    printf("-204 image: %s\n", image ? "ok" : "none");
    if (image)
        lvo(-210, 0, NULL, (APTR)image);
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}

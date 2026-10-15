/* Does a program setting PGA_Top/PGA_Visible/PGA_Total on an Intuition
 * propgclass gadget with SetGadgetAttrs send an IDCMP_IDCMPUPDATE to its
 * ICTARGET_IDCMP? Prop.mui brackets every such update with two SetAttrs
 * that clear and restore ICA_TARGET. Counts the IDCMPUPDATE messages
 * queued after 20 updates with the target set, and after 20 with it
 * cleared, and after 20 OM_UPDATEs as from another gadget, in a window
 * on the Workbench screen. On Kickstart 3.2.3 none of them notifies; only
 * user input does, so this shows no notification for program updates but
 * has no positive case of its own.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit propnotify.c -o propnotify -lamiga
 */
#include <stdio.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>

ULONG __stack = 32768;

static int drain(struct Window *w)
{
    struct IntuiMessage *im;
    int n = 0;

    while ((im = (struct IntuiMessage *)GetMsg(w->UserPort)))
    {
        if (im->Class == IDCMP_IDCMPUPDATE)
            n++;
        ReplyMsg((struct Message *)im);
    }
    return n;
}

int main(void)
{
    struct Window *w;
    struct Gadget *g;
    int i, with, without, control;
    struct TagItem update[] = { { PGA_Top, 0 }, { TAG_DONE, 0 } };

    w = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 40, WA_Width, 200,
        WA_Height, 150, WA_IDCMP, IDCMP_IDCMPUPDATE, WA_SimpleRefresh, TRUE,
        TAG_DONE);
    if (!w)
        return 20;
    g = (struct Gadget *)NewObject(NULL, "propgclass", GA_Left, 20,
        GA_Top, 20, GA_Width, 12, GA_Height, 100, PGA_Freedom, FREEVERT,
        PGA_Total, 200, PGA_Visible, 20, PGA_Top, 0, ICA_TARGET,
        ICTARGET_IDCMP, GA_ID, 1, TAG_DONE);
    if (!g)
    {
        CloseWindow(w);
        return 20;
    }
    AddGadget(w, g, -1);
    RefreshGList(g, w, NULL, 1);
    Delay(5);
    drain(w);
    for (i = 0; i < 20; i++)
        if (SetGadgetAttrs(g, w, NULL, PGA_Top, (i * 7) % 180,
                PGA_Visible, 20, PGA_Total, 200, TAG_DONE))
            RefreshGList(g, w, NULL, 1);
    Delay(5);
    with = drain(w);
    SetAttrs(g, ICA_TARGET, NULL, TAG_DONE);
    for (i = 0; i < 20; i++)
        if (SetGadgetAttrs(g, w, NULL, PGA_Top, (i * 11) % 180,
                PGA_Visible, 20, PGA_Total, 200, TAG_DONE))
            RefreshGList(g, w, NULL, 1);
    Delay(5);
    without = drain(w);
    /* OM_UPDATE, as from another gadget */
    SetAttrs(g, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
    for (i = 0; i < 20; i++)
    {
        update[0].ti_Data = (i * 13) % 180;
        DoGadgetMethod(g, w, NULL, OM_UPDATE, (ULONG) update, NULL, 0UL);
    }
    Delay(5);
    control = drain(w);
    printf("idcmpupdate with-target=%d without-target=%d om-update-control=%d"
        " (20 updates each)\n", with, without, control);
    RemoveGadget(w, g);
    DisposeObject(g);
    CloseWindow(w);
    printf("PASS\n");
    return 0;
}

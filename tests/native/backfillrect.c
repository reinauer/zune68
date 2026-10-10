/* Rectangles MUIM_CustomBackfill receives for a framed object (String
 * frame, so frame inner spacing exists), relative to the object's box:
 * on the first window draw, MUI_Redraw(obj, MADF_DRAWOBJECT), and inside
 * a virtual group. Compare native and MUI 3.9.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit backfillrect.c -o backfillrect -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define MUIM_CustomBackfill 0x80428d73  /* undocumented MUI 3.9 */
#define MUIA_CustomBackfill 0x80420a63
struct MUIP_CustomBackfill
{
    ULONG MethodID;
    LONG left, top, right, bottom, xoffset, yoffset;
};

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static const char *phase = "open";

static ULONG dispatcher(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_AskMinMax)
    {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;

        DoSuperMethodA(cl, obj, msg);
        m->MinMaxInfo->MinWidth += 30;
        m->MinMaxInfo->DefWidth += 30;
        m->MinMaxInfo->MaxWidth += 30;
        m->MinMaxInfo->MinHeight += 8;
        m->MinMaxInfo->DefHeight += 8;
        m->MinMaxInfo->MaxHeight += 8;
        return 0;
    }
    if (msg->MethodID == MUIM_CustomBackfill)
    {
        struct MUIP_CustomBackfill *m = (struct MUIP_CustomBackfill *)msg;

        printf("%-8s %s box %d,%d %dx%d inner %d,%d-%d,%d backfill"
            " %ld,%ld-%ld,%ld off %ld,%ld\n", phase,
            (char *)muiNotifyData(obj)->mnd_UserData,
            _left(obj), _top(obj), _width(obj), _height(obj),
            _mleft(obj), _mtop(obj), _mright(obj), _mbottom(obj),
            m->left, m->top, m->right, m->bottom, m->xoffset, m->yoffset);
        SetAPen(_rp(obj), 3);
        RectFill(_rp(obj), m->left, m->top, m->right, m->bottom);
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(void)
{
    struct MUI_CustomClass *mcc;
    Object *app, *win, *plain, *virt;
    ULONG open = FALSE;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 4, (APTR)dispatcher);
    if (!mcc)
        goto done;
    app = ApplicationObject,
        MUIA_Application_Title, "backfillrect",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "backfillrect",
            WindowContents, VGroup,
                Child, plain = NewObject(mcc->mcc_Class, NULL,
                    MUIA_Frame, MUIV_Frame_String, MUIA_FillArea, TRUE,
                    MUIA_CustomBackfill, TRUE, MUIA_UserData, "plain",
                    TAG_DONE),
                Child, ScrollgroupObject, MUIA_Scrollgroup_Contents,
                    VirtgroupObject,
                        Child, virt = NewObject(mcc->mcc_Class, NULL,
                            MUIA_Frame, MUIV_Frame_String,
                            MUIA_FillArea, TRUE, MUIA_CustomBackfill, TRUE,
                            MUIA_UserData, "virtual", TAG_DONE),
                        End,
                    End,
                End,
            End,
        End;
    if (!app)
        goto done;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open)
        goto done;
    DoMethod(app, MUIM_Application_InputBuffered);
    phase = "redraw";
    MUI_Redraw(plain, MADF_DRAWOBJECT);
    MUI_Redraw(virt, MADF_DRAWOBJECT);
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    if (mcc)
        MUI_DeleteCustomClass(mcc);
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}

/* Custom paint after Super must remain under the subclass's control. */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG call_super, paint_pen, before_flags, after_flags, failures;

static ULONG dispatch(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_AskMinMax) {
        struct MUIP_AskMinMax *m = (APTR)msg;
        DoSuperMethodA(cl, obj, msg);
        m->MinMaxInfo->MinWidth += 32;
        m->MinMaxInfo->MinHeight += 32;
        m->MinMaxInfo->DefWidth += 64;
        m->MinMaxInfo->DefHeight += 64;
        m->MinMaxInfo->MaxWidth = MUI_MAXMAX;
        m->MinMaxInfo->MaxHeight = MUI_MAXMAX;
        return 0;
    }
    if (msg->MethodID == MUIM_Draw) {
        before_flags = ((struct MUIP_Draw *)msg)->flags;
        if (call_super) DoSuperMethodA(cl, obj, msg);
        after_flags = ((struct MUIP_Draw *)msg)->flags;
        SetDrMd(_rp(obj), JAM1);
        _rp(obj)->AreaPtrn = NULL;
        _rp(obj)->AreaPtSz = 0;
        SetAPen(_rp(obj), paint_pen);
        RectFill(_rp(obj), _mleft(obj), _mtop(obj),
            _mright(obj), _mbottom(obj));
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void sample(Object *area, ULONG disabled)
{
    ULONG same = 0, shadow = 0, other = 0, row[8];
    ULONG actual = 99, shadow_pen = _pens(area)[MPEN_SHADOW];
    SetAttrs(area, MUIA_Disabled, disabled, TAG_DONE);
    GetAttr(MUIA_Disabled, area, &actual);
    MUI_Redraw(area, MADF_DRAWOBJECT);
    WaitBlit();
    for (int y = 0; y < 8; y++) {
        row[y] = 0;
        for (int x = 0; x < 8; x++) {
            LONG p = ReadPixel(_rp(area), _mleft(area) + 2 + x,
                _mtop(area) + 2 + y);
            if (p == (LONG)paint_pen) same++;
            else {
                row[y] |= 1UL << (7-x);
                if (p == (LONG)shadow_pen) shadow++;
                else other++;
            }
        }
    }
    if (same != 64 || shadow || other) failures++;
    printf("super=%lu disabled=%lu get=%lu flags=%08lx->%08lx "
        "paintpen=%lu shadowpen=%lu same=%lu shadow=%lu other=%lu rows=",
        call_super, disabled, actual, before_flags, after_flags,
        paint_pen, shadow_pen, same, shadow, other);
    for (int y = 0; y < 8; y++) printf("%02lx%s", row[y], y == 7 ? "\n" : ",");
    fflush(stdout);
}

int main(void)
{
    Object *area, *win, *app;
    struct MUI_CustomClass *cc;
    ULONG sig = 0, opened = 0;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) return 20;
    printf("library %u.%u\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);
    cc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 0, (APTR)dispatch);
    if (!cc) { CloseLibrary(MUIMasterBase); return 20; }
    area = NewObject(cc->mcc_Class, NULL, MUIA_Background,
        MUII_BACKGROUND, TAG_DONE);
    win = MUI_NewObject(MUIC_Window, MUIA_Window_Title,
        (ULONG)"Disabled paint probe", MUIA_Window_Width, 100,
        MUIA_Window_Height, 80, MUIA_Window_RootObject, (ULONG)area,
        TAG_DONE);
    app = MUI_NewObject(MUIC_Application, MUIA_Application_Title,
        (ULONG)"DisablePaintProbe", MUIA_Application_Base,
        (ULONG)"DISABLEPAINTPROBE", MUIA_Application_Window,
        (ULONG)win, TAG_DONE);
    if (!app) { MUI_DeleteCustomClass(cc); CloseLibrary(MUIMasterBase); return 20; }
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, win, &opened);
    if (opened) {
        for (int i = 0; i < 5; i++) {
            DoMethod(app, MUIM_Application_NewInput, &sig); Delay(1);
        }
        paint_pen = _pens(area)[MPEN_SHINE];
        for (call_super = 0; call_super <= 1; call_super++) {
            sample(area, FALSE);
            sample(area, TRUE);
            sample(area, FALSE);
        }
        SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    }
    MUI_DisposeObject(app);
    MUI_DeleteCustomClass(cc);
    CloseLibrary(MUIMasterBase);
    return opened && !failures ? 0 : 20;
}

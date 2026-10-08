/* Custom virtual layout grow/shrink regression.
 * Reference MUI keeps virtual extents at least as large as the viewport.
 * ExitChange can relayout the virtual group without its parent hook.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit relayout.c -o relayout -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static Object *virt;
static LONG extent = 60;
static ULONG parentCalls, virtualCalls;
static ULONG parentLayout(struct Hook *hook __asm("a0"),
    Object *obj __asm("a2"), struct MUI_LayoutMsg *lm __asm("a1"))
{
    (void)hook; (void)obj;
    if (lm->lm_Type == MUILM_MINMAX) {
        lm->lm_MinMax.MinWidth=100; lm->lm_MinMax.MinHeight=50;
        lm->lm_MinMax.DefWidth=250; lm->lm_MinMax.DefHeight=120;
        lm->lm_MinMax.MaxWidth=MUI_MAXMAX;
        lm->lm_MinMax.MaxHeight=MUI_MAXMAX;
        return 0;
    }
    if (lm->lm_Type == MUILM_LAYOUT) {
        parentCalls++;
        return MUI_Layout(virt,0,0,lm->lm_Layout.Width,
            lm->lm_Layout.Height,0);
    }
    return 0;
}
static ULONG virtualLayout(struct Hook *hook __asm("a0"),
    Object *obj __asm("a2"), struct MUI_LayoutMsg *lm __asm("a1"))
{
    Object *child; APTR state=lm->lm_Children->mlh_Head;
    (void)hook; (void)obj;
    if (lm->lm_Type == MUILM_MINMAX) {
        lm->lm_MinMax.MinWidth=60; lm->lm_MinMax.MinHeight=extent;
        lm->lm_MinMax.DefWidth=200; lm->lm_MinMax.DefHeight=extent;
        lm->lm_MinMax.MaxWidth=MUI_MAXMAX;
        lm->lm_MinMax.MaxHeight=MUI_MAXMAX;
        return 0;
    }
    if (lm->lm_Type == MUILM_LAYOUT) {
        virtualCalls++;
        child=NextObject(&state);
        if (child) MUI_Layout(child,0,0,lm->lm_Layout.Width,extent,0);
        lm->lm_Layout.Width=extent;
        lm->lm_Layout.Height=extent;
        return 1;
    }
    return 0;
}
static void pump(Object *app) {
    ULONG sig=0; int i;
    for(i=0;i<15;i++) {DoMethod(app,MUIM_Application_NewInput,&sig);Delay(1);}
}
static int report(const char *phase) {
    ULONG width=0,height=0;
    ULONG expectedWidth=extent > _mwidth(virt) ? extent : _mwidth(virt);
    ULONG expectedHeight=extent > _mheight(virt) ? extent : _mheight(virt);
    GetAttr(MUIA_Virtgroup_Width,virt,&width);
    GetAttr(MUIA_Virtgroup_Height,virt,&height);
    printf("%s parent=%lu virtual=%lu extent=%ld virtual=%lu,%lu box=%d,%d\n",
        phase,parentCalls,virtualCalls,extent,width,height,_width(virt),_height(virt));
    fflush(stdout);
    return width != expectedWidth || height != expectedHeight;
}
int main(void) {
    struct Hook ph={0},vh={0};
    Object *app,*win,*parent,*leaf;
    int failed=0;
    MUIMasterBase=OpenLibrary("muimaster.library",19);
    if(!MUIMasterBase)return 20;
    ph.h_Entry=(HOOKFUNC)parentLayout;vh.h_Entry=(HOOKFUNC)virtualLayout;
    leaf=MUI_NewObject(MUIC_Rectangle,TAG_DONE);
    virt=MUI_NewObject(MUIC_Virtgroup,MUIA_Group_LayoutHook,(ULONG)&vh,
        MUIA_Group_Child,(ULONG)leaf,TAG_DONE);
    parent=MUI_NewObject(MUIC_Group,MUIA_Group_LayoutHook,(ULONG)&ph,
        MUIA_Group_Child,(ULONG)virt,TAG_DONE);
    win=MUI_NewObject(MUIC_Window,MUIA_Window_Title,(ULONG)"RelayoutProbe",
        MUIA_Window_RootObject,(ULONG)parent,TAG_DONE);
    app=MUI_NewObject(MUIC_Application,MUIA_Application_Title,(ULONG)"RelayoutProbe",
        MUIA_Application_Base,(ULONG)"RELAYOUTPROBE",MUIA_Application_Window,(ULONG)win,TAG_DONE);
    if(!app)return 20;
    printf("library=%u.%u\n",MUIMasterBase->lib_Version,MUIMasterBase->lib_Revision);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);
    failed |= report("initial");
    parentCalls=virtualCalls=0;
    DoMethod(virt,MUIM_Group_InitChange);extent=1000;
    DoMethod(virt,MUIM_Group_ExitChange);pump(app);
    failed |= report("grown");
    failed |= parentCalls != 0 || virtualCalls != 1;
    parentCalls=virtualCalls=0;
    DoMethod(virt,MUIM_Group_InitChange);extent=60;
    DoMethod(virt,MUIM_Group_ExitChange);pump(app);
    failed |= report("shrunk");
    failed |= parentCalls != 0 || virtualCalls != 1;
    SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
    MUI_DisposeObject(app);CloseLibrary(MUIMasterBase);
    return failed ? 20 : 0;
}

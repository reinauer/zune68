/* Real Configdata construction, saved overrides and mutable defaults.
 * Uses the private prefs structure to check what widgets actually consume. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include "mui.h"
#include "classes/configdata.h"
#include "prefs.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack=131072;
static int failures;
static void check(BOOL ok,const char *label)
{ if(!ok){printf("FAIL %s\n",label);failures++;} }
static ULONG attr(Object *obj,ULONG id)
{ ULONG v=0;GetAttr(id,obj,&v);return v; }
static ULONG shared_locks, releases;
static ULONG dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if(msg->MethodID==MUIM_Semaphore_ObtainShared)shared_locks++;
    if(msg->MethodID==MUIM_Semaphore_Release)releases++;
    return DoSuperMethodA(cl,obj,msg);
}
static Object *application(void)
{
    ULONG tags[]={MUIA_Application_SingleTask,TRUE,MUIA_Application_Base,(ULONG)"Zune68ConfigInitProbe",TAG_DONE};
    return MUI_NewObjectA(MUIC_Application,(APTR)tags);
}
int main(void)
{
    const char *file="ENV:zune/Zune68ConfigInitProbe.prefs";
    setvbuf(stdout,NULL,_IONBF,0);
    MUIMasterBase=OpenLibrary("muimaster.library",19);if(!MUIMasterBase)return 20;
    DeleteFile(file);
    BPTR dir=CreateDir("ENV:zune"); if(dir)UnLock(dir);
    BPTR initial=Open(file,MODE_NEWFILE); if(initial)Close(initial);
    Object *app=application();if(!app)return 20;
    Object *cfg=muiGlobalInfo(app)->mgi_Configdata;
    struct ZunePrefsNew *prefs=(APTR)attr(cfg,MUIA_Configdata_ZunePrefs);
    if (!cfg || !prefs) { puts("FAIL missing Configdata"); return 20; }
    check(prefs->window_inner_left==4 && prefs->window_inner_right==4,
        "default margins");
    check(prefs->group_hspacing==4 && prefs->group_vspacing==4,"default spacing");
    check(prefs->screenmodeid==0xffffffff && prefs->screen_width==0,"default screen");
    check(prefs->drag_autostart && prefs->drag_autostart_length==3,"default drag");
    check(prefs->muikeys[MUIKEY_PRESS].ix_well,"default key parsed");
    DoMethod(cfg,MUIM_Configdata_SetULong,MUICFG_Window_Spacing_Left,13);
    DoMethod(cfg,MUIM_Configdata_SetULong,MUICFG_Group_HSpacing,17);
    DoMethod(cfg,MUIM_Configdata_SetULong,MUICFG_Screen_Mode_ID,0x12345678);
    DoMethod(cfg,MUIM_Configdata_SetULong,MUICFG_Screen_Width,641);
    DoMethod(cfg,MUIM_Configdata_SetULong,MUICFG_Drag_Autostart,FALSE);
    DoMethod(cfg,MUIM_Configdata_SetString,MUICFG_Keyboard_Press,"-upstroke space");
    DoMethod(cfg,MUIM_Configdata_SetString,MUICFG_CustomFrame_1,"test-frame.iff");
    DoMethod(cfg,MUIM_Configdata_Save,file);
    for(int i=0;i<30;i++) {
        DoMethod(app,MUIM_Application_InputBuffered); Delay(2);
        cfg=muiGlobalInfo(app)->mgi_Configdata;
        prefs=(APTR)attr(cfg,MUIA_Configdata_ZunePrefs);
        if(prefs->window_inner_left==13)break;
    }
    check(prefs->window_inner_left==13,"filesystem notification reload");
    MUI_DisposeObject(app);
    app=application();if(!app)return 20;
    cfg=muiGlobalInfo(app)->mgi_Configdata;
    prefs=(APTR)attr(cfg,MUIA_Configdata_ZunePrefs);
    check(prefs->window_inner_left==13 && prefs->window_inner_right==4,"saved margin and default neighbor");
    check(prefs->group_hspacing==17 && prefs->group_vspacing==4,"saved word and default neighbor");
    check(prefs->screenmodeid==0x12345678 && prefs->screen_width==641 && !prefs->screen_height,
        "saved long and word fields");
    check(!prefs->drag_autostart && prefs->drag_autostart_length==3,"saved false boolean");
    check(!strcmp(prefs->customframe_config_1,"test-frame.iff"),"saved custom frame");
    check(prefs->muikeys[MUIKEY_PRESS].ix_well &&
        !strcmp(prefs->muikeys[MUIKEY_PRESS].readable_hotkey,"-upstroke space"),"saved key parsed");
    DoMethod(cfg,MUIM_Configdata_SetString,MUICFG_Keyboard_Press,"-upstroke return");
    check(prefs->muikeys[MUIKEY_PRESS].ix_well &&
        !strcmp(prefs->muikeys[MUIKEY_PRESS].readable_hotkey,"-upstroke return"),"key reset to default");
    DoMethod(cfg,MUIM_Configdata_SetULong,MUICFG_Group_HSpacing,4);
    check(DoMethod(cfg,MUIM_Configdata_GetULong,MUICFG_Group_HSpacing)==4,"runtime default lookup");
    MUI_DisposeObject(app);DeleteFile(file);
    app=application();cfg=muiGlobalInfo(app)->mgi_Configdata;
    prefs=(APTR)attr(cfg,MUIA_Configdata_ZunePrefs);
    check(prefs->window_inner_left==4 && prefs->group_hspacing==4,"removed file returns defaults");
    struct MUI_CustomClass *cc=MUI_CreateCustomClass(NULL,MUIC_Configdata,NULL,32,(APTR)dispatch);
    ULONG tags[]={MUIA_Configdata_Application,(ULONG)app,TAG_DONE};
    Object *derived=cc?NewObjectA(cc->mcc_Class,NULL,(APTR)tags):NULL;
    check(derived!=NULL,"Configdata subclass creation");
    if(derived) {
        memset(INST_DATA(cc->mcc_Class,derived),0xa5,32);
        prefs=(APTR)attr(derived,MUIA_Configdata_ZunePrefs);
        check(prefs->window_inner_left==4 && prefs->muikeys[MUIKEY_PRESS].ix_well,
            "subclass inherited defaults");
        check(shared_locks>100 && shared_locks==releases,"subclass retains semaphore dispatch");
        DoMethod(derived,MUIM_Configdata_SetString,MUICFG_Keyboard_Press,"-upstroke space");
        check(prefs->muikeys[MUIKEY_PRESS].ix_well &&
            !strcmp(prefs->muikeys[MUIKEY_PRESS].readable_hotkey,"-upstroke space"),
            "subclass string synchronization uses base instance");
        UBYTE *guard=INST_DATA(cc->mcc_Class,derived);
        for(int i=0;i<32;i++)check(guard[i]==0xa5,"subclass instance intact");
        MUI_DisposeObject(derived);
    }
    if(cc)MUI_DeleteCustomClass(cc);
    MUI_DisposeObject(app);CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);
    return failures?20:0;
}

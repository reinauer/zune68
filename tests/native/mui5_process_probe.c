/* Process lifetime, copied names, signalling and restart on AmigaOS. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack=65536;
static volatile ULONG started, signalled, finished, natural;
static int failures;
static ULONG worker(struct IClass *cl __asm("a0"),Object *obj __asm("a2"),Msg msg __asm("a1"))
{
    if(msg->MethodID==MUIM_Process_Process)
    {
        struct MUIP_Process_Process *m=(APTR)msg;
        started++;
        if(!natural)
            while(!*(volatile ULONG*)m->kill)
                if(Wait(SIGBREAKF_CTRL_C|SIGBREAKF_CTRL_D)&SIGBREAKF_CTRL_D)signalled++;
        finished++;
        return 0;
    }
    return DoSuperMethodA(cl,obj,msg);
}
static void check(BOOL ok,const char *s){if(!ok){printf("FAIL %s\n",s);failures++;}}
static ULONG attr(Object *o,ULONG id){ULONG v=0;GetAttr(id,o,&v);return v;}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);
    MUIMasterBase=OpenLibrary("muimaster.library",0);if(!MUIMasterBase)return 20;
    struct MUI_CustomClass *cl=MUI_CreateCustomClass(NULL,MUIC_Notify,NULL,0,(APTR)worker);
    struct TagItem empty[]={{TAG_DONE,0}};
    Object *source=cl?NewObjectA(cl->mcc_Class,NULL,empty):NULL;
    char name[]="Zune process probe";
    struct TagItem tags[]={{MUIA_Process_AutoLaunch,FALSE},{MUIA_Process_Name,(ULONG)name},
        {MUIA_Process_SourceObject,(ULONG)source},{MUIA_Process_SourceClass,(ULONG)(cl?cl->mcc_Class:NULL)},
        {TAG_DONE,0}};
    Object *p=source?MUI_NewObjectA(MUIC_Process,tags):NULL;
    check(p!=NULL,"construction");
    if(p)
    {
        name[0]='X';check(!attr(p,MUIA_Process_Task),"auto launch disabled");
        DoMethod(p,MUIM_Process_Launch);
        for(int i=0;i<100&&!started;i++)Delay(1);
        check(started==1,"worker started");
        struct Task *task=(APTR)attr(p,MUIA_Process_Task);
        check(task&&task!=FindTask(NULL),"separate task");
        check(task&&!strcmp(task->tc_Node.ln_Name,"Zune process probe"),"name copied");
        DoMethod(p,MUIM_Process_Launch);Delay(2);check(started==1,"launch idempotent");
        DoMethod(p,MUIM_Process_Signal,SIGBREAKF_CTRL_D);
        for(int i=0;i<100&&!signalled;i++)Delay(1);
        check(signalled==1,"signal delivered");
        check(DoMethod(p,MUIM_Process_Kill,2),"kill joins");
        check(finished==1&&!attr(p,MUIA_Process_Task),"task cleared after exit");
        natural=1;DoMethod(p,MUIM_Process_Launch);
        for(int i=0;i<100&&attr(p,MUIA_Process_Task);i++)Delay(1);
        check(finished==2&&!attr(p,MUIA_Process_Task),"natural exit and restart");
        natural=0;DoMethod(p,MUIM_Process_Launch);
        MUI_DisposeObject(p);check(finished==3,"dispose joins child");
    }
    if(source)MUI_DisposeObject(source);
    if(cl)MUI_DeleteCustomClass(cl);
    CloseLibrary(MUIMasterBase);printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);
    return failures?20:0;
}

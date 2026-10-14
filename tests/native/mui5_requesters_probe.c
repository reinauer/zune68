/* Zune requester worker, ASL result ownership and repeated invocation. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <dos/dostags.h>
#include <dos/dosasl.h>
#include <workbench/startup.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
extern struct IntuitionBase *IntuitionBase;
ULONG __stack = 131072;
static int failures;
static volatile ULONG done, cancel, multi, worker_seen;
static struct Task *caller;
static struct FileRequester *expected_request;
static volatile ULONG wrong_request;
static ULONG attr(Object *o, ULONG a) { ULONG v=0; GetAttr(a,o,&v); return v; }
static void check(BOOL ok, const char *name)
{ if (!ok) { printf("FAIL %s\n",name); failures++; } }
static Object *find(Object *o, const char *name, BOOL label)
{
    for (struct IClass *c=OCLASS(o); c; c=c->cl_Super)
        if (c->cl_ID && !strcmp((char *)c->cl_ID,label?MUIC_Text:name))
        {
            if (!label) return o;
            const char *text=(APTR)attr(o,MUIA_Text_Contents);
            if (text && !strcmp(text,name)) return o;
            break;
        }
    struct List *children=(APTR)attr(o,MUIA_Group_ChildList);
    if (children)
    {
        APTR state=children->lh_Head; Object *child;
        while ((child=NextObject(&state)))
        { Object *found=find(child,name,label); if(found)return found; }
    }
    return NULL;
}
static void driver(void)
{
    Object *win=NULL;
    for (int i=0;i<500&&!win;i++)
    {
        ULONG lock=LockIBase(0);
        for (struct Screen *s=IntuitionBase->FirstScreen;s&&!win;s=s->NextScreen)
            for (struct Window *w=s->FirstWindow;w;w=w->NextWindow)
                if (w->Title&&!strcmp((char *)w->Title,"Zune requester probe"))
                { win=(APTR)w->UserData; break; }
        UnlockIBase(lock);
        if (!win) Delay(1);
    }
    if (win)
    {
        Object *root=(APTR)attr(win,MUIA_Window_RootObject);
        Object *button=find(root,cancel?"Cancel":"OK",TRUE);
        Object *app=(APTR)attr(win,MUIA_ApplicationObject);
        if (multi)
        {
            Object *list=find(root,MUIC_Dirlist,FALSE);
            if (list) DoMethod(app,MUIM_Application_PushMethod,list,4,
                MUIM_List_Select,MUIV_List_Select_All,MUIV_List_Select_On,NULL);
        }
        if (button)
        {
            DoMethod(app,MUIM_Application_PushMethod,button,3,MUIM_Set,MUIA_Pressed,TRUE);
            DoMethod(app,MUIM_Application_PushMethod,button,3,MUIM_Set,MUIA_Pressed,FALSE);
        }
        else DoMethod(app,MUIM_Application_PushMethod,win,3,MUIM_Set,MUIA_Window_CloseRequest,TRUE);
    }
    Forbid(); done=1;
}
static BOOL run(APTR request, struct TagItem *tags)
{
    done=0;
    struct TagItem proc[]={{NP_Entry,(ULONG)driver},{NP_Name,(ULONG)"Requester probe driver"},
        {NP_StackSize,16384},{TAG_DONE,0}};
    if (!CreateNewProc(proc)) return FALSE;
    BOOL result=MUI_AslRequest(request,tags);
    while(!done)Delay(1);
    return result;
}
static ULONG filter(struct Hook *h __asm("a0"), struct FileRequester *r __asm("a2"),
    struct AnchorPath *a __asm("a1"))
{
    if(FindTask(NULL)!=caller)worker_seen++;
    if(expected_request && r!=expected_request)wrong_request++;
    return TRUE;
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0); caller=FindTask(NULL);
    MUIMasterBase=OpenLibrary("muimaster.library",0);if(!MUIMasterBase)return 20;
    BPTR lock=CreateDir("RAM:zune-requester-test");if(lock)UnLock(lock);
    const char *paths[]={"RAM:zune-requester-test/one","RAM:zune-requester-test/two"};
    for(int i=0;i<2;i++){BPTR f=Open(paths[i],MODE_NEWFILE);if(f){Write(f,"x",1);Close(f);}}
    struct Hook hook={0};hook.h_Entry=(HOOKFUNC)filter;
    char title[]="Zune requester probe";
    struct TagItem tags[]={{ASLFR_TitleText,(ULONG)title},
        {ASLFR_InitialDrawer,(ULONG)"RAM:zune-requester-test"},
        {ASLFR_InitialFile,(ULONG)"one"},{ASLFR_FilterFunc,(ULONG)&hook},
        {ASLFR_DoMultiSelect,TRUE},{TAG_DONE,0}};
    struct FileRequester *file=MUI_AllocAslRequest(ASL_FileRequest,tags);
    check(file!=NULL,"file allocation");title[0]='X';
    if(file)
    {
        expected_request=file;
        multi=TRUE;cancel=FALSE;
        check(run(file,NULL),"multiple-file acceptance");
        printf("files=%ld worker callbacks=%lu drawer=%s\n",file->fr_NumArgs,worker_seen,file->fr_Drawer);
        check(worker_seen>0,"filter executes in requester process");
        check(!wrong_request,"filter receives allocated requester identity");
        check(file->fr_NumArgs==2,"two selected arguments");
        for(LONG i=0;i<file->fr_NumArgs;i++)
        {
            char path[256];
            check(file->fr_ArgList[i].wa_Lock && NameFromLock(file->fr_ArgList[i].wa_Lock,path,sizeof(path)),"returned directory lock");
            check(file->fr_ArgList[i].wa_Name && (!strcmp((char*)file->fr_ArgList[i].wa_Name,"one") || !strcmp((char*)file->fr_ArgList[i].wa_Name,"two")),"owned argument name");
        }
        multi=FALSE;cancel=TRUE;check(!run(file,NULL),"cancel repeated requester");
        cancel=FALSE;check(run(file,NULL),"reopen after cancellation");
        MUI_FreeAslRequest(file);
        expected_request=NULL;
    }
    struct TagItem ft[]={{ASLFO_TitleText,(ULONG)"Zune requester probe"},
        {ASLFO_InitialName,(ULONG)"topaz.font"},{ASLFO_InitialSize,9},
        {ASLFO_InitialStyle,FSF_BOLD},{ASLFO_DoStyle,TRUE},
        {ASLFO_DoSpecialMode,TRUE},{ASLFO_InitialSpecMode,FO_SPECIALMODE_SHADOW},
        {TAG_DONE,0}};
    struct FontRequester *font=MUI_AllocAslRequest(ASL_FontRequest,ft);
    check(font!=NULL,"font allocation");
    if(font){check(run(font,NULL),"font acceptance");check(font->fo_Attr.ta_Name&&!strcmp((char*)font->fo_Attr.ta_Name,"topaz.font"),"owned ASL font name");check(font->fo_Attr.ta_YSize==9&&font->fo_Attr.ta_Style==FSF_BOLD,"font attributes");check(font->fo_SpecialDrawMode==FO_SPECIALMODE_SHADOW,"extended font style");MUI_FreeAslRequest(font);}
    CloseLibrary(MUIMasterBase);printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

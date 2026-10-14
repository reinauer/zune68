/* Selector construction, filtering, completion and repeated opening. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <dos/dosasl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
struct Library *MUIMasterBase;
ULONG __stack=131072;
static int failures,filtered;
static ULONG attr(Object *obj,ULONG id) {ULONG v=0;GetAttr(id,obj,&v);return v;}
static void check(BOOL ok,const char *label)
{if(!ok){printf("FAIL %s\n",label);failures++;}}
static Object *find(Object *obj,const char *name,BOOL label)
{
    for(struct IClass *cl=OCLASS(obj);cl;cl=cl->cl_Super)
        if(cl->cl_ID && !strcmp((char*)cl->cl_ID,label?MUIC_Text:name))
        {
            if(!label)return obj;
            ULONG text=attr(obj,MUIA_Text_Contents);
            if(text&&!strcmp((char*)text,name))return obj;
            break;
        }
    struct List *list=(APTR)attr(obj,MUIA_Group_ChildList);
    if(list){APTR state=list->lh_Head;Object *child;while((child=NextObject(&state)))
        {Object *found=find(child,name,label);if(found)return found;}}
    return NULL;
}
static ULONG filter(struct Hook *h __asm("a0"),struct FileRequester *r __asm("a2"),
    struct AnchorPath *a __asm("a1"))
{filtered++;check(r->fr_Drawer&&!strcmp((char*)r->fr_Drawer,"RAM:z68-panel"),"filter requester drawer");return !strcmp(a->ap_Info.fib_FileName,"keep.txt");}
static void createfile(const char *path)
{BPTR f=Open(path,MODE_NEWFILE);check(f!=0,"fixture file");if(f){Write(f,"x",1);Close(f);}}
static void runpanel(Object *panel,struct Screen *screen)
{
    Object *win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_RootObject,panel,TAG_DONE);
    Object *app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    Object *accept=find(panel,"OK",TRUE),*cancel=find(panel,"Cancel",TRUE);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
    check(accept&&cancel,"accept/cancel controls");
    if(accept&&cancel)
    {
        for(int i=0;i<3;i++)
        {
            Object *button=i==1?cancel:accept;
            DoMethod(app,MUIM_Application_PushMethod,button,3,MUIM_Set,MUIA_Pressed,TRUE);
            DoMethod(app,MUIM_Application_PushMethod,button,3,MUIM_Set,MUIA_Pressed,FALSE);
            ULONG result=DoMethod(panel,MUIM_Panel_Run,app,win);
            printf("run %d result=%lu open=%lu\n",i,result,attr(win,MUIA_Window_Open));
            check(result==(ULONG)(i!=1),"completion result");
            check(attr(win,MUIA_Window_Open),"window stays open");
            SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
        }
        DoMethod(app,MUIM_Application_PushMethod,win,3,MUIM_Set,MUIA_Window_CloseRequest,TRUE);
        check(!DoMethod(panel,MUIM_Panel_Run,app,win),"close requester cancels");
    }
    MUI_DisposeObject(app);
}
int main(int argc,char **argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    MUIMasterBase=OpenLibrary("muimaster.library",0);if(!MUIMasterBase)return 20;
    struct Screen *screen=NULL;if(argc>1)screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    BPTR lock=CreateDir("RAM:z68-panel");if(lock)UnLock(lock);
    createfile("RAM:z68-panel/keep.txt");createfile("RAM:z68-panel/drop.bin");createfile("RAM:z68-panel/keep.txt.info");
    Object *p=MO(MUIC_Filepanel,MUIA_Filepanel_Drawer,"RAM:z68-panel",MUIA_Filepanel_File,"initial",
        MUIA_Filepanel_DoPatterns,TRUE,MUIA_Filepanel_Pattern,"#?.txt",MUIA_Filepanel_RejectIcons,TRUE,TAG_DONE);
    check(p!=NULL,"file panel creation");
    if(p)
    {
        Object *list=find(p,"Dirlist.mui",FALSE);
        check(list!=NULL,"directory list child");
        printf("pattern entries=%lu\n",attr(list,MUIA_List_Entries));
        check(attr(list,MUIA_List_Entries)==1,"pattern and icon filtering");
        ULONG drawer=attr(p,MUIA_Filepanel_Drawer);
        check(drawer&&!strcmp((char*)drawer,"RAM:z68-panel"),"drawer getter");
        puts("changing pattern");
        SetAttrs(p,MUIA_Filepanel_Pattern,"#?",TAG_DONE);
        puts("pattern changed");
        check(attr(list,MUIA_List_Entries)==2,"changed pattern rereads");
        puts("running file panel"); runpanel(p,screen);
    }
    struct Hook h={0};h.h_Entry=(HOOKFUNC)filter;
    p=MO(MUIC_Filepanel,ASLFR_InitialDrawer,"RAM:z68-panel",ASLFR_FilterFunc,&h,
        MUIA_Filepanel_DrawersOnly,TRUE,TAG_DONE);
    check(p!=NULL,"filtered panel creation");
    if(p){Object *list=find(p,"Dirlist.mui",FALSE);check(filtered==3,"filter receives each entry");
        check(attr(list,MUIA_List_Entries)==1,"filter overrides other filters");MUI_DisposeObject(p);}
    p=MO(MUIC_Fontpanel,ASLFO_InitialName,"topaz.font",ASLFO_InitialSize,9,
        ASLFO_DoStyle,TRUE,ASLFO_InitialStyle,FSF_BOLD,TAG_DONE);
    check(p!=NULL,"font panel creation");
    if(p){ULONG name=attr(p,ASLFO_InitialName);check(name&&!strcmp((char*)name,"topaz"),"font name");
        check(attr(p,ASLFO_InitialSize)==9,"font size");check(attr(p,ASLFO_InitialStyle)==FSF_BOLD,"font style");
        SetAttrs(p,ASLFO_InitialName,"topaz.font",ASLFO_InitialSize,11,
            ASLFO_InitialStyle,FSF_ITALIC,ASLFO_InitialFrontPen,2,
            ASLFO_InitialBackPen,1,ASLFO_InitialDrawMode,INVERSVID,TAG_DONE);
        check(attr(p,ASLFO_InitialSize)==11,"set font size");
        check(attr(p,ASLFO_InitialStyle)==FSF_ITALIC,"set font style");
        check(attr(p,ASLFO_InitialFrontPen)==2,"set front pen");
        check(attr(p,ASLFO_InitialBackPen)==1,"set back pen");
        check(attr(p,ASLFO_InitialDrawMode)==INVERSVID,"inverse draw mode value");
        Object *list=find(p,"List.mui",FALSE);check(attr(list,MUIA_List_Entries)>0,"installed font inventory");runpanel(p,screen);}
    if(screen)CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

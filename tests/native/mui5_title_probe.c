/* Title's public page-group contract, compared with the OS3 SDK runtime. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static ULONG attr(Object *o, ULONG a) { ULONG v=0xdeadbeef; GetAttr(a,o,&v); return v; }
static struct Window *native;
static void input(Object *app, ULONG type, ULONG code, ULONG qual, LONG x, LONG y)
{
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(APTR)CreateIORequest(port,sizeof(*io)):NULL;
    if(io && !OpenDevice("input.device",0,(APTR)io,0)) {
        struct InputEvent event={0}; struct IEPointerPixel pixel={0};
        event.ie_Class=type;event.ie_Code=code;event.ie_Qualifier=qual;
        if(type==IECLASS_NEWPOINTERPOS){pixel.iepp_Screen=native->WScreen;
            pixel.iepp_Position.X=native->LeftEdge+x;pixel.iepp_Position.Y=native->TopEdge+y;
            event.ie_SubClass=IESUBCLASS_PIXEL;event.ie_EventAddress=&pixel;}
        io->io_Command=IND_WRITEEVENT;io->io_Data=&event;io->io_Length=sizeof(event);
        DoIO((APTR)io);CloseDevice((APTR)io);
    }
    if(io)DeleteIORequest((APTR)io);
    if(port)DeleteMsgPort(port);
    for(int j=0;j<5;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
}
static int failures;
static void check(BOOL ok,const char *label){if(!ok){printf("FAIL %s\n",label);failures++;}}
static Object *root;
static ULONG new_calls, close_calls;
static ULONG dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"),
    Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_Title_New)
    {
        Object *tab=MO(MUIC_Text,MUIA_Text_Contents,"New page",TAG_DONE);
        Object *page=MO(MUIC_Text,MUIA_Text_Contents,"New contents",TAG_DONE);
        new_calls++;
        DoMethod(root,MUIM_Group_InitChange);
        DoMethod(obj,MUIM_Group_InitChange);
        DoMethod(obj,OM_ADDMEMBER,tab);
        DoMethod(root,OM_ADDMEMBER,page);
        DoMethod(obj,MUIM_Group_ExitChange);
        DoMethod(root,MUIM_Group_ExitChange);
        return 1;
    }
    if (msg->MethodID == MUIM_Title_Close) close_calls++;
    return DoSuperMethodA(cl,obj,msg);
}
static Object *child(Object *obj, LONG index)
{
    struct List *list=(APTR)attr(obj,MUIA_Group_ChildList);
    APTR state=list->lh_Head;
    Object *result;
    while((result=NextObject(&state))) if(!index--) return result;
    return NULL;
}
static void click(Object *app,LONG x,LONG y)
{
    input(app,IECLASS_NEWPOINTERPOS,0,0,x,y);
    input(app,IECLASS_RAWMOUSE,IECODE_LBUTTON,0,0,0);
    input(app,IECLASS_RAWMOUSE,IECODE_LBUTTON|IECODE_UP_PREFIX,0,0,0);
}
int main(int argc, char **argv)
{
    Object *title,*tabs[3],*pages[3],*group,*win,*app;
    struct Screen *screen=NULL;
    MUIMasterBase=OpenLibrary("muimaster.library",0);
    if (!MUIMasterBase) return 20;
    printf("library=%u.%u\n",MUIMasterBase->lib_Version,MUIMasterBase->lib_Revision);
    if(argc>1 && !strcmp(argv[1],"screen")) screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    for(int i=0;i<3;i++) { tabs[i]=MO(MUIC_Text,MUIA_Text_Contents,i==0?"First":i==1?"Second":"Third",TAG_DONE); pages[i]=MO(MUIC_Text,MUIA_Text_Contents,"Page contents",MUIA_FixWidth,180,MUIA_FixHeight,70,TAG_DONE); }
    struct MUI_CustomClass *cl=MUI_CreateCustomClass(NULL,MUIC_Title,NULL,0,dispatch);
    if(!cl)return 20;
    struct TagItem tags[]={{MUIA_CycleChain,TRUE},{MUIA_Group_Child,(ULONG)tabs[0]},
        {MUIA_Group_Child,(ULONG)tabs[1]},{MUIA_Group_Child,(ULONG)tabs[2]}, {TAG_DONE,0}};
    title=NewObjectA(cl->mcc_Class,NULL,tags);
    if(!title) { puts("FAIL Title creation"); return 20; }
    printf("defaults position=%ld close=%ld new=%ld sort=%ld\n",attr(title,MUIA_Title_Position),attr(title,MUIA_Title_Closable),attr(title,MUIA_Title_Newable),attr(title,MUIA_Title_Sortable));
    group=MO(MUIC_Group,MUIA_Group_Child,title,MUIA_Group_Child,pages[0],MUIA_Group_Child,pages[1],MUIA_Group_Child,pages[2],TAG_DONE);
    root=group;
    win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_Title,"Title probe",MUIA_Window_Width,300,MUIA_Window_RootObject,group,TAG_DONE);
    app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    if(!app) return 20;
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
    for(int j=0;j<4;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
    printf("open=%lu page-mode=%lu active=%ld title=%ld\n",attr(win,MUIA_Window_Open),attr(group,MUIA_Group_PageMode),attr(group,MUIA_Group_ActivePage),attr(title,MUIA_Group_ActivePage));
    for(int i=0;i<3;i++){
        SetAttrs(group,MUIA_Group_ActivePage,i,TAG_DONE);
        printf("page %d active=%ld title=%ld boxes title=%d,%d %dx%d page=%d,%d %dx%d\n",i,attr(group,MUIA_Group_ActivePage),attr(title,MUIA_Group_ActivePage),_left(title),_top(title),_width(title),_height(title),_left(pages[i]),_top(pages[i]),_width(pages[i]),_height(pages[i]));
    }
    native=(struct Window *)attr(win,MUIA_Window_Window);
    check(attr(win,MUIA_Window_Open),"window opens");
    SetAttrs(group,MUIA_Group_ActivePage,0,TAG_DONE);
    input(app,IECLASS_NEWPOINTERPOS,0,0,_left(tabs[1])+3,_top(tabs[1])+3);
    input(app,IECLASS_RAWMOUSE,IECODE_LBUTTON,0,0,0);
    input(app,IECLASS_RAWMOUSE,IECODE_LBUTTON|IECODE_UP_PREFIX,0,0,0);
    printf("click second parent=%ld\n",attr(group,MUIA_Group_ActivePage));
    check(attr(group,MUIA_Group_ActivePage)==1,"click selects corresponding page");
    SetAttrs(title,MUIA_Group_ActivePage,0,TAG_DONE);
    printf("title-set parent=%ld title=%ld\n",attr(group,MUIA_Group_ActivePage),attr(title,MUIA_Group_ActivePage));
    SetAttrs(title,MUIA_Title_Position,MUIV_Title_Position_Bottom,MUIA_Title_Closable,TRUE,MUIA_Title_Newable,TRUE,TAG_DONE);
    for(int j=0;j<4;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
    printf("bottom=%ld title-y=%d page-y=%d close=%ld new=%ld\n",attr(title,MUIA_Title_Position),_top(title),_top(pages[0]),attr(title,MUIA_Title_Closable),attr(title,MUIA_Title_Newable));
    check(DoMethod(title,MUIM_Title_FindPage,tabs[1])==(ULONG)pages[1],"find corresponding page");
    check(!DoMethod(title,MUIM_Title_FindPage,win),"reject unrelated tab");
    check(DoMethod(title,MUIM_Title_Close,tabs[1]),"default close");
    check(child(group,1)==pages[0] && child(group,2)==pages[2] && !child(group,3),"close removes page pair");
    check(DoMethod(title,MUIM_Title_FindPage,tabs[2])==(ULONG)pages[2],"find page after removal");
    struct List *children=NULL; GetAttr(MUIA_Group_ChildList,title,(ULONG*)&children);
    APTR state=children->lh_Head; int count=0; while(NextObject(&state)) count++;
    printf("children after close=%d\n",count);
    if(MUIMasterBase->lib_Version==19)
    {
        check(_top(title)>_top(pages[0]),"bottom placement");
        click(app,_mright(title)-8,_mtop(title)+8);
        check(new_calls==1 && child(group,3)!=NULL,"new button calls subclass");
        SetAttrs(title,MUIA_Title_Sortable,TRUE,TAG_DONE);
        LONG target=_left(tabs[2])+3;
        input(app,IECLASS_NEWPOINTERPOS,0,0,_left(tabs[0])+3,_top(tabs[0])+3);
        input(app,IECLASS_RAWMOUSE,IECODE_LBUTTON,0,0,0);
        input(app,IECLASS_NEWPOINTERPOS,0,IEQUALIFIER_LEFTBUTTON,target,_top(tabs[2])+3);
        input(app,IECLASS_RAWMOUSE,IECODE_LBUTTON|IECODE_UP_PREFIX,0,0,0);
        check(child(title,1)==tabs[0] && child(group,2)==pages[0],"drag reorders tab and page together");
        click(app,_right(tabs[0])+7,_top(tabs[0])+3);
        check(close_calls==2,"close button calls subclass");
        while(child(title,1)) DoMethod(title,MUIM_Title_Close,child(title,1));
        SetAttrs(title,MUIA_Title_OnLastClose,MUIV_Title_OnLastClose_WindowAction,TAG_DONE);
        ULONG close_requested=0;
        DoMethod(win,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,win,3,
            MUIM_WriteLong,1,&close_requested);
        Object *last=child(title,0);
        click(app,_right(last)+7,_top(last)+3);
        check(close_requested,"last close requests window close");
        check(child(title,0)==last,"window close keeps last page");
        DoMethod(title,MUIM_Title_Close,last);
        check(!child(title,0) && !child(group,1),"empty page group");
        for(int i=0;i<12;i++)DoMethod(title,MUIM_Title_New);
        SetAttrs(group,MUIA_Group_ActivePage,11,TAG_DONE);
        SetAttrs(win,MUIA_Window_ActiveObject,title,TAG_DONE);
        input(app,IECLASS_RAWKEY,0x4f,0,0,0);
        input(app,IECLASS_RAWKEY,0x4f|IECODE_UP_PREFIX,0,0,0);
        check(attr(group,MUIA_Group_ActivePage)==10,"keyboard selects previous tab");
        SetAttrs(group,MUIA_Group_ActivePage,11,TAG_DONE);
        check(_width(child(title,11))>0 && attr(child(title,11),MUIA_ShowMe),"select reveals overflow tab");
        Object *menu=(APTR)DoMethod(title,MUIM_ContextMenuBuild,_mright(title)-24,_mtop(title)+8);
        check((ULONG)menu>1,"overflow chooser");
        if((ULONG)menu>1) {
            Object *item=(APTR)DoMethod(menu,MUIM_FindUData,child(title,0));
            check(item!=NULL,"chooser contains hidden tab");
            if(item)DoMethod(title,MUIM_ContextMenuChoice,item);
            check(attr(group,MUIA_Group_ActivePage)==0,"chooser selects hidden page");
        }
        SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
        SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
        check(attr(win,MUIA_Window_Open),"reopen overflow window");
    }
    MUI_DisposeObject(app); MUI_DeleteCustomClass(cl); if(screen) CloseScreen(screen); CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures); return failures?20:0;
}

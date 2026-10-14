/* Keyboard capture, popup interaction and mouse-handler options. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <libraries/commodities.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <dos/dostags.h>
#include <proto/commodities.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n,...) ({ULONG tags_[]={__VA_ARGS__};MUI_NewObjectA(n,(APTR)tags_);})
struct Library *MUIMasterBase;
ULONG __stack=65536;
static int failures;
static ULONG attr(Object *o,ULONG a){ULONG v=0;GetAttr(a,o,&v);return v;}
static void check(BOOL ok,const char *s){if(!ok){printf("FAIL %s\n",s);failures++;}}
static Object *findstring(Object *o)
{
    for(struct IClass *c=OCLASS(o);c;c=c->cl_Super)
        if(c->cl_ID&&!strcmp(c->cl_ID,MUIC_String))return o;
    struct List *list=(APTR)attr(o,MUIA_Group_ChildList);
    if(list){APTR state=list->lh_Head;Object *child;while((child=NextObject(&state)))
        {Object *s=findstring(child);if(s)return s;}}
    return NULL;
}
static struct Screen *popup_screen;
static struct Window *main_window;
static volatile BOOL popup_done;
static BOOL popup_last;
static void inject(struct IOStdReq *io, ULONG type, ULONG code, LONG x, LONG y)
{
    struct InputEvent event={0}; struct IEPointerPixel point={0};
    event.ie_Class=type; event.ie_Code=code;
    if(type==IECLASS_NEWPOINTERPOS)
    {
        point.iepp_Screen=popup_screen;
        point.iepp_Position.X=x; point.iepp_Position.Y=y;
        event.ie_SubClass=IESUBCLASS_PIXEL; event.ie_EventAddress=&point;
    }
    io->io_Command=IND_WRITEEVENT;io->io_Data=&event;io->io_Length=sizeof(event);
    DoIO((APTR)io);Delay(5);
}
static void popup_driver(void)
{
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(APTR)CreateIORequest(port,sizeof(*io)):NULL;
    if(io&&!OpenDevice("input.device",0,(APTR)io,0))
    {
        LONG x=-1,y=0;
        for(int i=0;i<100&&x<0;i++)
        {
            Delay(2);Forbid();
            for(struct Window *w=popup_screen->FirstWindow;w;w=w->NextWindow)
                if(w!=main_window&&(w->Flags&WFLG_BORDERLESS))
                {x=w->LeftEdge+20;y=w->TopEdge+(popup_last?w->Height-8:8);break;}
            Permit();
        }
        if(x>=0)
        {
            inject(io,IECLASS_NEWPOINTERPOS,0,x,y);
            Delay(20);
            /* Hovering the menu title opens its item window. */
            Forbid();
            for(struct Window *w=popup_screen->FirstWindow;w;w=w->NextWindow)
                if(w!=main_window&&(w->Flags&WFLG_BORDERLESS))
                {x=w->LeftEdge+20;y=w->TopEdge+(popup_last?w->Height-8:8);break;}
            Permit();
            inject(io,IECLASS_NEWPOINTERPOS,0,x,y);
            inject(io,IECLASS_RAWMOUSE,IECODE_LBUTTON,0,0);
            inject(io,IECLASS_RAWMOUSE,IECODE_LBUTTON|IECODE_UP_PREFIX,0,0);
        }
        BOOL still_open=FALSE;
        Forbid();
        for(struct Window *w=popup_screen->FirstWindow;w;w=w->NextWindow)
            if(w!=main_window&&(w->Flags&WFLG_BORDERLESS))still_open=TRUE;
        Permit();
        if(still_open)
        {
            inject(io,IECLASS_RAWKEY,0x45,0,0);
            inject(io,IECLASS_RAWKEY,0xc5,0,0);
        }
        CloseDevice((APTR)io);
    }
    if(io)DeleteIORequest((APTR)io);
    if(port)DeleteMsgPort(port);
    Forbid();popup_done=TRUE;
}
static void popup(Object *button,BOOL last)
{
    popup_last=last;popup_done=FALSE;
    struct Process *p=CreateNewProcTags(NP_Entry,popup_driver,
        NP_Name,"Keyadjust popup input",NP_StackSize,32768,TAG_DONE);
    check(p!=NULL,"popup input process");if(!p)return;
    SetAttrs(button,MUIA_Pressed,TRUE,TAG_DONE);
    SetAttrs(button,MUIA_Pressed,FALSE,TAG_DONE);
    while(!popup_done)Delay(1);
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);MUIMasterBase=OpenLibrary("muimaster.library",0);if(!MUIMasterBase)return 20;
    struct Screen *screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    char original[]="control a";
    Object *o=MO(MUIC_Keyadjust,MUIA_Keyadjust_Key,original,TAG_DONE);
    check(o!=NULL,"construction");if(!o){if(screen)CloseScreen(screen);CloseLibrary(MUIMasterBase);return 20;}
    original[0]='X';check(!strcmp((char*)attr(o,MUIA_Keyadjust_Key),"control a"),"copied shortcut");
    SetAttrs(o,MUIA_Keyadjust_Key,"control a, lalt b",TAG_DONE);
    struct InputXpression ix;
    check(strchr((char*)attr(o,MUIA_Keyadjust_Key),',')!=NULL,"multiple shortcuts");
    SetAttrs(o,MUIA_Keyadjust_AllowMultipleKeys,FALSE,MUIA_Keyadjust_Key,"rcommand c",TAG_DONE);
    check(!attr(o,MUIA_Keyadjust_AllowMultipleKeys),"multiple flag");
    if(strstr((char*)MUIMasterBase->lib_IdString,"Zune68"))
    {
        SetAttrs(o,MUIA_Keyadjust_Key,"not a valid key expression",TAG_DONE);
        check(!strcmp((char*)attr(o,MUIA_Keyadjust_Key),"rcommand c"),"invalid expression rejected");
        char longkeys[1102];
        for(int i=0;i<1101;i++)longkeys[i]=(i&1)?',':'a';
        longkeys[1101]=0;
        SetAttrs(o,MUIA_Keyadjust_AllowMultipleKeys,TRUE,MUIA_Keyadjust_Key,longkeys,TAG_DONE);
        check(!strcmp((char*)attr(o,MUIA_Keyadjust_Key),"rcommand c"),"oversized shortcuts rejected");
    }
    Object *w=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_RootObject,o,TAG_DONE);
    Object *app=MO(MUIC_Application,MUIA_Application_Window,w,TAG_DONE);
    SetAttrs(w,MUIA_Window_Open,TRUE,TAG_DONE);check(attr(w,MUIA_Window_Open),"window opens");
    Object *s=findstring(o);check(s!=NULL,"shortcut input");
    struct MsgPort *port=CreateMsgPort();struct IOStdReq *io=port?(APTR)CreateIORequest(port,sizeof(*io)):NULL;
    if(s&&io&&!OpenDevice("input.device",0,(APTR)io,0))
    {
        SetAttrs(w,MUIA_Window_ActiveObject,s,TAG_DONE);
        for(int i=0;i<4;i++){Delay(2);DoMethod(app,MUIM_Application_InputBuffered);}
        struct InputEvent event={0};event.ie_Class=IECLASS_RAWKEY;event.ie_Code=0x20;event.ie_Qualifier=IEQUALIFIER_CONTROL;
        io->io_Command=IND_WRITEEVENT;io->io_Data=&event;io->io_Length=sizeof(event);DoIO((APTR)io);
        for(int i=0;i<4;i++){Delay(2);DoMethod(app,MUIM_Application_InputBuffered);}
        event.ie_Code|=IECODE_UP_PREFIX;DoIO((APTR)io);
        for(int i=0;i<4;i++){Delay(2);DoMethod(app,MUIM_Application_InputBuffered);}
        char *key=(APTR)attr(o,MUIA_Keyadjust_Key);printf("captured=%s\n",key);
        if(strstr((char*)MUIMasterBase->lib_IdString,"Zune68"))
            check(!ParseIX(key,&ix)&&ix.ix_Code==0x20&&(ix.ix_Qualifier&IEQUALIFIER_CONTROL),"captured system expression");
        else puts("REFERENCE DIFFERENCE: shortcut capture uses a different input path");
        if(strstr((char*)MUIMasterBase->lib_IdString,"Zune68"))
        {
            struct IntuiMessage mouse={0};mouse.Class=IDCMP_MOUSEBUTTONS;
            mouse.Code=SELECTDOWN;mouse.MouseX=attr(s,MUIA_LeftEdge)+2;
            mouse.MouseY=attr(s,MUIA_TopEdge)+2;mouse.Seconds=10;
            SetAttrs(o,MUIA_Keyadjust_AllowMouseEvents,FALSE,TAG_DONE);
            DoMethod(o,MUIM_HandleEvent,&mouse,0);
            check(!strcmp((char*)attr(o,MUIA_Keyadjust_Key),"control a"),"mouse disabled");
            SetAttrs(o,MUIA_Keyadjust_AllowMouseEvents,TRUE,TAG_DONE);
            mouse.Seconds=20;DoMethod(o,MUIM_HandleEvent,&mouse,0);
            key=(APTR)attr(o,MUIA_Keyadjust_Key);
            check(!ParseIX(key,&ix)&&ix.ix_Class==IECLASS_RAWMOUSE,"mouse expression");
            SetAttrs(o,MUIA_Keyadjust_AllowDoubleClick,FALSE,MUIA_Keyadjust_Key,"a",TAG_DONE);
            mouse.Micros=100000;DoMethod(o,MUIM_HandleEvent,&mouse,0);
            check(!strcmp((char*)attr(o,MUIA_Keyadjust_Key),"a"),"double click suppressed");
            SetAttrs(o,MUIA_Keyadjust_AllowTripleClick,FALSE,TAG_DONE);
            mouse.Micros=200000;DoMethod(o,MUIM_HandleEvent,&mouse,0);
            check(!strcmp((char*)attr(o,MUIA_Keyadjust_Key),"a"),"triple click suppressed");
            SetAttrs(o,MUIA_Keyadjust_AllowMultipleKeys,TRUE,MUIA_Keyadjust_Key,"a,b",TAG_DONE);
            struct List *children=(APTR)attr(o,MUIA_Group_ChildList);
            APTR state=children->lh_TailPred; Object *button=NextObject(&state);
            popup_screen=screen;main_window=(APTR)attr(w,MUIA_Window_Window);
            popup(button,FALSE);
            printf("popup remove=%s\n",(char*)attr(o,MUIA_Keyadjust_Key));
            check(!strcmp((char*)attr(o,MUIA_Keyadjust_Key),"b"),"popup removes selected shortcut");
            popup(button,TRUE);
            inject(io,IECLASS_RAWKEY,0x20,0,0);
            DoMethod(app,MUIM_Application_InputBuffered);
            inject(io,IECLASS_RAWKEY,0xa0,0,0);
            DoMethod(app,MUIM_Application_InputBuffered);
            printf("popup append=%s\n",(char*)attr(o,MUIA_Keyadjust_Key));
            check(!strcmp((char*)attr(o,MUIA_Keyadjust_Key),"b,a"),"popup appends captured shortcut");
        }
        CloseDevice((APTR)io);
    }
    else check(FALSE,"input device");
    if(io)DeleteIORequest((APTR)io);
    if(port)DeleteMsgPort(port);
    MUI_DisposeObject(app);if(screen)CloseScreen(screen);CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

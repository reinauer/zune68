/* Keyboard shortcut validation and real input capture. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <libraries/commodities.h>
#include <devices/input.h>
#include <devices/inputevent.h>
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
        CloseDevice((APTR)io);
    }
    else check(FALSE,"input device");
    if(io)DeleteIORequest((APTR)io);
    if(port)DeleteMsgPort(port);
    MUI_DisposeObject(app);if(screen)CloseScreen(screen);CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

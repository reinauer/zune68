/* Native title gadget actions, saved positions, and modal popup lifetime.
 * Run only in a disposable OS installation: writes ZuneTitleProbe prefs. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <intuition/intuitionbase.h>
#include <graphics/modeid.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(name, ...) ({ ULONG t[]={__VA_ARGS__}; MUI_NewObjectA(name,(APTR)t); })
struct Library *MUIMasterBase;
ULONG __stack=65536;
static Object *app,*win;
static struct Window *native;
static struct Screen *screen;
static volatile ULONG done, popup_seen;
static LONG selection;
static int failures;
static const char *paths[]={"ENV:zune/ZuneTitleProbe.prefs","ENVARC:zune/ZuneTitleProbe.prefs"};
static void check(int ok,const char *label) { printf("%s %s\n",ok?"PASS":"FAIL",label); if(!ok) failures++; }
static Object *config(void) { return MO(MUIC_Configdata,TAG_DONE); }
static void prefs(ULONG mask)
{
 Object *c=config();
 for(int i=0;i<2;i++) {
  DoMethod(c,MUIM_Dataspace_Clear);
  DoMethod(c,MUIM_Configdata_SetULong,MUICFG_Window_Buttons,mask);
  DoMethod(c,MUIM_Configdata_SetULong,MUICFG_Window_Positions,0);
  DoMethod(c,MUIM_Configdata_SetULong,MUICFG_Window_Spacing_Left,i?9:5);
  DoMethod(c,MUIM_Configdata_Save,paths[i]);
 }
 MUI_DisposeObject(c);
}
static int openapp(ULONG id)
{
 win=MO(MUIC_Window,MUIA_Window_Title,"Title actions",MUIA_Window_ID,id,
  MUIA_Window_Screen,screen,MUIA_Window_LeftEdge,40,MUIA_Window_TopEdge,40,
  MUIA_Window_Width,400,MUIA_Window_Height,130,
  MUIA_Window_RootObject,MO(MUIC_Rectangle,TAG_DONE),TAG_DONE);
 app=MO(MUIC_Application,MUIA_Application_SingleTask,TRUE,MUIA_Application_Base,"ZuneTitleProbe",
  MUIA_Application_Title,"Title actions",MUIA_Application_Window,win,TAG_DONE);
 if(!app)return 0;
 SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
 GetAttr(MUIA_Window_Window,win,(ULONG *)&native);
 return native!=NULL;
}
static int gadgets(struct Gadget **out)
{
 int n=0;
 GetAttr(MUIA_Window_Window,win,(ULONG *)&native);
 for(struct Gadget *g=native->FirstGadget;g;g=g->NextGadget)
  if(!(g->GadgetType&GTYP_SYSGADGET) && g->TopEdge<native->BorderTop && (g->Flags&GFLG_RELRIGHT)) {
   if(out && n<4)out[n]=g;
   n++;
  }
 return n;
}
static void click(struct Gadget *g,ULONG seconds,ULONG micros)
{
 struct MsgPort *reply=CreateMsgPort();
 struct IntuiMessage msg={0};
 msg.ExecMessage.mn_ReplyPort=reply; msg.ExecMessage.mn_Length=sizeof(msg);
 msg.Class=IDCMP_GADGETUP;msg.IDCMPWindow=native;msg.IAddress=g;
 msg.Seconds=seconds;msg.Micros=micros;
 PutMsg(native->UserPort,(APTR)&msg);
 DoMethod(app,MUIM_Application_InputBuffered);
 check(GetMsg(reply)==(APTR)&msg,"gadget message replied");
 DeleteMsgPort(reply);
}
static int saved(ULONG id,int file)
{
 Object *c=config();ULONG *raw;int found=0;
 DoMethod(c,MUIM_Dataspace_Clear);DoMethod(c,MUIM_Configdata_Load,paths[file]);
 raw=(APTR)DoMethod(c,MUIM_Dataspace_Find,MUICFG_WindowPos);
 if(raw) { struct windowpos *p=(APTR)(raw+1);ULONG n=(*raw-4)/sizeof(*p);
  for(ULONG i=0;i<n;i++)if(p[i].id==id)found=1; }
 check(DoMethod(c,MUIM_Configdata_GetULong,MUICFG_Window_Spacing_Left)==(file?9:5),"preserved independent appearance prefs");
 MUI_DisposeObject(c);return found;
}
static void inject(struct IOStdReq *io,ULONG type,ULONG code,LONG x,LONG y)
{
 struct InputEvent ev={0};struct IEPointerPixel pos={0};
 ev.ie_Class=type;ev.ie_Code=code;
 if(type==IECLASS_NEWPOINTERPOS) { pos.iepp_Screen=screen;pos.iepp_Position.X=x;pos.iepp_Position.Y=y;
  ev.ie_SubClass=IESUBCLASS_PIXEL;ev.ie_EventAddress=&pos; }
 io->io_Command=IND_WRITEEVENT;io->io_Data=&ev;io->io_Length=sizeof(ev);
 DoIO((APTR)io);Delay(5);
}
static void driver(void)
{
 struct MsgPort *port=CreateMsgPort();struct IOStdReq *io=port?(APTR)CreateIORequest(port,sizeof(*io)):NULL;
 if(io && !OpenDevice("input.device",0,(APTR)io,0)) {
  LONG x=0,y=0,h=0;Delay(25);
  Forbid();
  for(struct Window *w=screen->FirstWindow;w;w=w->NextWindow)
   if(w!=native && (w->Flags&WFLG_BORDERLESS)) { x=w->LeftEdge;y=w->TopEdge;h=w->Height;popup_seen=1;break; }
  Permit();
  if(selection>=0 && h) {
   /* Open the Window submenu, then select one of its five text rows. */
   inject(io,IECLASS_NEWPOINTERPOS,0,x+12,y+8);
   Delay(10);
   Forbid();
   for(struct Window *w=screen->FirstWindow;w;w=w->NextWindow)
    if(w!=native && (w->Flags&WFLG_BORDERLESS) && w->Height>h) {
     x=w->LeftEdge;y=w->TopEdge;h=w->Height;break;
    }
   Permit();
   inject(io,IECLASS_NEWPOINTERPOS,0,x+12,y+2+(h-4)*(2*selection+1)/10);
   inject(io,IECLASS_RAWMOUSE,IECODE_LBUTTON,0,0);
   inject(io,IECLASS_RAWMOUSE,IECODE_LBUTTON|IECODE_UP_PREFIX,0,0);
  }
  inject(io,IECLASS_RAWKEY,0x45,0,0);inject(io,IECLASS_RAWKEY,0xc5,0,0);
  CloseDevice((APTR)io);
 }
 if(io)DeleteIORequest((APTR)io);if(port)DeleteMsgPort(port);done=1;
}
int main(void)
{
 struct Gadget *g[4];ULONG got;
 setvbuf(stdout,NULL,_IONBF,0);
 puts("starting title actions");
 MUIMasterBase=OpenLibrary("muimaster.library",19);if(!MUIMasterBase)return 20;
 puts("library opened");
 screen=OpenScreenTags(NULL,SA_DisplayID,HIRESLACE_KEY,SA_Width,640,SA_Height,480,SA_Depth,2,TAG_DONE);
 if(!screen)return 20;
 puts("screen opened");
 {
  Object *detached=MO(MUIC_Window,MUIA_Window_ID,0x44455441,
   MUIA_Window_RootObject,MO(MUIC_Rectangle,TAG_DONE),TAG_DONE);
  check(detached && !DoMethod(detached,MUIM_Window_Snapshot,1),"detached window snapshot rejected");
  if(detached)MUI_DisposeObject(detached);
 }
 for(ULONG mask=0;mask<16;mask++) {
  int n=0;for(int i=0;i<4;i++)n+=!!(mask&(1<<i));printf("preparing mask %lu\n",mask);prefs(mask);puts("prefs saved");
  if(!openapp(0x5449544c))return 20;
  printf("mask=%lu count=%d\n",mask,gadgets(NULL));check(gadgets(NULL)==n,"title gadget mask");
  MUI_DisposeObject(app);
 }
 prefs(15);if(!openapp(0))return 20;gadgets(g);
 check(g[2]->Flags&GFLG_DISABLED,"snapshot disabled without window ID");
 check(!DoMethod(win,MUIM_Window_Snapshot,1),"snapshot rejects missing ID");MUI_DisposeObject(app);
 if(!openapp(0x5449544c))return 20;check(gadgets(g)==4,"all four gadgets");
 {
  Object *other=MO(MUIC_Window,MUIA_Window_ID,0x4f544852,
   MUIA_Window_Screen,screen,MUIA_Window_Title,"Other position",
   MUIA_Window_RootObject,MO(MUIC_Rectangle,TAG_DONE),TAG_DONE);
  DoMethod(app,OM_ADDMEMBER,other);
  SetAttrs(other,MUIA_Window_Open,TRUE,TAG_DONE);
  DoMethod(other,MUIM_Window_Snapshot,1);
  SetAttrs(other,MUIA_Window_Open,FALSE,TAG_DONE);
  check(saved(0x4f544852,0)&&saved(0x4f544852,1),"other window saved");
 }
 gadgets(g);click(g[2],100,0);
 check(saved(0x5449544c,0)&&saved(0x5449544c,1),"snapshot persisted to ENV and ENVARC");
 gadgets(g);click(g[2],100,100000);
 check(!saved(0x5449544c,0)&&!saved(0x5449544c,1),"double click unsnapshots both files");
 got=1;SetAttrs(app,MUIA_Application_SearchWinId,0x5449544c,TAG_DONE);GetAttr(MUIA_Application_GetWinPos,app,&got);
 check(!got,"unsnapshot removed live position");
 check(saved(0x4f544852,0)&&saved(0x4f544852,1),"unsnapshot preserves other window");
 ActivateWindow(native);
 for(selection=-1;selection<=2;selection++) {
  if(selection==0)continue;done=popup_seen=0;
  if(!CreateNewProcTags(NP_Entry,driver,NP_Name,"Title menu input",NP_StackSize,16384,TAG_DONE))return 20;
  gadgets(g);click(g[3],110+selection,0);while(!done)Delay(1);
  check(popup_seen,"popup window opened");
  if(selection==1)check(saved(0x5449544c,0),"popup snapshot action");
  if(selection==2)check(!saved(0x5449544c,0),"popup unsnapshot action");
 }
 /* Closing during the nested popup input loop must unwind safely. */
 DoMethod(app,MUIM_Application_PushMethod,win,3,MUIM_Set,MUIA_Window_Open,FALSE);
 gadgets(g);click(g[3],120,0);got=1;GetAttr(MUIA_Window_Open,win,&got);check(!got,"popup parent close");
 SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
 GetAttr(MUIA_Window_Window,win,(ULONG *)&native);
 MoveWindow(native,30,20);Delay(5);
 LONG x=native->LeftEdge,y=native->TopEdge;
 DoMethod(win,MUIM_Window_Snapshot,1);
 MUI_DisposeObject(app);
 if(!openapp(0x5449544c))return 20;
 check(native->LeftEdge==x && native->TopEdge==y,"snapshot restored by new application");
 DoMethod(win,MUIM_Window_Snapshot,0);
 MUI_DisposeObject(app);
 if(!openapp(0x5449544c))return 20;
 check(native->LeftEdge==40 && native->TopEdge==40,"unsnapshot restores application defaults");
 {
  Object *c=config();ULONG bad=6000,bytes=0,*raw;
  DoMethod(c,MUIM_Dataspace_Clear);
  DoMethod(c,MUIM_Configdata_Load,paths[1]);
  DoMethod(c,MUIM_Dataspace_Add,&bad,sizeof(bad),MUICFG_WindowPos);
  DoMethod(c,MUIM_Configdata_Save,paths[1]);
  DoMethod(win,MUIM_Window_Snapshot,1);
  DoMethod(c,MUIM_Dataspace_Clear);
  DoMethod(c,MUIM_Configdata_Load,paths[1]);
  raw=(APTR)DoMethod(c,MUIM_Dataspace_Get,MUICFG_WindowPos,&bytes);
  check(raw && bytes==sizeof(bad) && *raw==bad,"malformed position table preserved");
  MUI_DisposeObject(c);
 }
 MUI_DisposeObject(app);CloseScreen(screen);CloseLibrary(MUIMasterBase);
 printf("failures=%d rc=%d\n",failures,failures?20:0);return failures?20:0;
}

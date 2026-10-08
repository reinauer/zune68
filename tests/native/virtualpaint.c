/* Virtual-group painting lifecycle regression.
 * Run on MUI 3.9 and Zune68; a refused begin must suppress contents/end.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os \
 *        -Wl,-u,___stkinit virtualpaint.c -o virtualpaint -lmui
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <graphics/clip.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#ifndef MUIA_CustomBackfill
#define MUIA_CustomBackfill 0x80420a63UL
#define MUIM_CustomBackfill 0x80428d73UL
#endif

struct Library *MUIMasterBase;
ULONG __stack=65536;
static int phase, count, begins, ends, failures;
static APTR outer_clip, client_clip;
static ULONG dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
 if(msg->MethodID==0x80425b5cUL || msg->MethodID==0x8042da52UL) {
  ULONG result;
  if(msg->MethodID==0x80425b5cUL) begins++; else ends++;
  printf("paint method %08lx before phase%d\n",msg->MethodID,phase);
  if (phase==2 && msg->MethodID==0x80425b5cUL) return 0;
  if(msg->MethodID==0x80425b5cUL) outer_clip=_rp(obj)->Layer->ClipRegion;
  else if(client_clip) MUI_RemoveClipping(muiRenderInfo(obj),client_clip);
  result=DoSuperMethodA(cl,obj,msg);
  if(msg->MethodID==0x80425b5cUL) {
   if(!_rp(obj)->Layer->ClipRegion) failures++;
   client_clip=MUI_AddClipping(muiRenderInfo(obj),_mleft(obj),_mtop(obj),_mwidth(obj),_mheight(obj));
   if(!client_clip) failures++;
  } else if(_rp(obj)->Layer->ClipRegion!=outer_clip) failures++;
  printf("paint method %08lx result %08lx\n",msg->MethodID,result);return result;
 }
 if(msg->MethodID==MUIM_Draw) {
  ULONG result;printf("draw before phase%d flags%lx\n",phase,((struct MUIP_Draw *)msg)->flags);
  result=DoSuperMethodA(cl,obj,msg);printf("draw after phase%d flags%lx\n",phase,((struct MUIP_Draw *)msg)->flags);return result;
 }
 if(msg->MethodID==MUIM_AskMinMax) {
  struct MUIP_AskMinMax *m=(APTR)msg;DoSuperMethodA(cl,obj,msg);
  m->MinMaxInfo->MinWidth+=20;m->MinMaxInfo->MinHeight+=20;
  m->MinMaxInfo->DefWidth+=200;m->MinMaxInfo->DefHeight+=120;
  m->MinMaxInfo->MaxWidth=MUI_MAXMAX;m->MinMaxInfo->MaxHeight=MUI_MAXMAX;return 0;
 }
 if(msg->MethodID==MUIM_CustomBackfill) {
  LONG *v=(LONG *)msg;
  printf("backfill phase%d box %d %d %d %d args %ld %ld %ld %ld %ld %ld\n",phase,_mleft(obj),_mtop(obj),_mwidth(obj),_mheight(obj),v[1],v[2],v[3],v[4],v[5],v[6]);fflush(stdout);count++;
  SetAPen(_rp(obj),1);RectFill(_rp(obj),v[1],v[2],v[3],v[4]);return 1;
 }
 return DoSuperMethodA(cl,obj,msg);
}
int main(void)
{
 Object *area,*win,*app;struct MUI_CustomClass *cc;ULONG sig=0,opened=0;
 MUIMasterBase=OpenLibrary("muimaster.library",19);if(!MUIMasterBase)return 20;
 printf("library %u.%u\n",MUIMasterBase->lib_Version,MUIMasterBase->lib_Revision);
 cc=MUI_CreateCustomClass(0,MUIC_Virtgroup,0,0,(APTR)dispatch);if(!cc)return 20;
 area=NewObject(cc->mcc_Class,0,MUIA_CustomBackfill,TRUE,MUIA_Background,MUII_BACKGROUND,MUIA_InnerLeft,3,MUIA_InnerTop,4,MUIA_InnerRight,5,MUIA_InnerBottom,6,TAG_DONE);
 win=MUI_NewObject(MUIC_Window,MUIA_Window_Title,"Backfill probe",MUIA_Window_Width,200,MUIA_Window_Height,120,MUIA_Window_RootObject,area,TAG_DONE);
 app=MUI_NewObject(MUIC_Application,MUIA_Application_Title,"Backfillprobe",MUIA_Application_Base,"BACKFILLPROBE",MUIA_Application_Window,win,TAG_DONE);
 {ULONG value=0xdeadbeef,ok;ok=GetAttr(MUIA_CustomBackfill,area,&value);printf("get custom %lu value%lu\n",ok,value);}
 SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);GetAttr(MUIA_Window_Open,win,&opened);
 for(int i=0;i<5;i++){DoMethod(app,MUIM_Application_NewInput,&sig);Delay(1);}
 printf("open %lu calls%d\n",opened,count);
 if(!opened || !begins || begins!=ends || !count) failures++;
 phase=1;count=0;begins=ends=0;
 DoMethod(area,MUIM_DrawBackground,_mleft(area)+1,_mtop(area)+2,13,11,100,200,0);
 printf("explicit calls%d\n",count);
 if(count!=1 || begins || ends) failures++;
 phase=2;count=0;begins=ends=0;
 MUI_Redraw(area,MADF_DRAWOBJECT);for(int i=0;i<10;i++){DoMethod(app,MUIM_Application_NewInput,&sig);Delay(1);}
 printf("redraw calls%d\n",count);
 if(count || begins!=1 || ends) failures++;
 phase=3;count=0;SetAttrs(area,MUIA_CustomBackfill,FALSE,TAG_DONE);DoMethod(area,MUIM_DrawBackground,_mleft(area),_mtop(area),13,11,100,200,0);MUI_Redraw(area,MADF_DRAWOBJECT);printf("disable calls%d\n",count);
 phase=4;count=0;SetAttrs(area,MUIA_CustomBackfill,TRUE,MUIA_FillArea,FALSE,TAG_DONE);MUI_Redraw(area,MADF_DRAWOBJECT);printf("no-fill calls%d\n",count);
 SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);MUI_DisposeObject(app);MUI_DeleteCustomClass(cc);CloseLibrary(MUIMasterBase);printf("failures %d\n",failures);return failures ? 20 : 0;
}

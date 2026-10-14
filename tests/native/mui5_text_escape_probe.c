/* Escape state, invisible images and pen rendering on a planar screen. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n,...) ({ULONG tags_[]={__VA_ARGS__};MUI_NewObjectA(n,(APTR)tags_);})
struct Library *MUIMasterBase; ULONG __stack=65536;
static ULONG pixels(Object *o){ULONG h=2166136261UL;for(LONG y=_mtop(o);y<=_mbottom(o);y++)for(LONG x=_mleft(o);x<=_mright(o);x++)h=(h^ReadPixel(_rp(o),x,y))*16777619UL;return h;}
int main(void)
{
    int failures=0;setvbuf(stdout,NULL,_IONBF,0);
    MUIMasterBase=OpenLibrary("muimaster.library",0);if(!MUIMasterBase)return 20;
    struct Screen *screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    if(!screen)return 20;
    const char *samples[]={"plain", "\33t\33Tplain", "\33s\33Splain", "\33g\33Gplain",
        "\33B\33Fplain", "\33V\33vplain", "\33h\33I[6:14]\33Hplain",
        "\33P[1]plain", "\33p[1]\33B      ", "\33P[ffffff]plain",
        "\33tplain", "\33splain", "\33gplain"};
    Object *objects[13];Object *group=MO(MUIC_Group,MUIA_Group_Columns,2,TAG_DONE);
    for(int i=0;i<13;i++)
    {
        objects[i]=MO(MUIC_Text,MUIA_Text_Contents,samples[i],MUIA_Font,MUIV_Font_Fixed,
            MUIA_FixWidth,80,MUIA_FixHeight,10,MUIA_Background,0,
            MUIA_Text_SetMin,FALSE,TAG_DONE);
        DoMethod(group,OM_ADDMEMBER,objects[i]);
    }
    DoMethod(group,OM_ADDMEMBER,MO(MUIC_Rectangle,TAG_DONE));
    Object *win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_RootObject,group,TAG_DONE);
    Object *app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
    ULONG opened=0;GetAttr(MUIA_Window_Open,win,&opened);
    if(!opened){puts("FAIL window open");MUI_DisposeObject(app);CloseScreen(screen);CloseLibrary(MUIMasterBase);return 20;}
    for(int i=0;i<4;i++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
    for(int i=0;i<13;i++)MUI_Redraw(objects[i],MADF_DRAWOBJECT);
    WaitBlit();
    for(int i=0;i<13;i++){printf("escape=%d rect=%d,%d %dx%d\n",i,_mleft(objects[i]),_mtop(objects[i]),_mwidth(objects[i]),_mheight(objects[i]));printf("pixels=%08lx\n",pixels(objects[i]));}
    for(int i=1;i<=5;i++)if(pixels(objects[i])!=pixels(objects[0])){printf("FAIL reset %d\n",i);failures++;}
    if(ReadPixel(_rp(objects[8]),_mleft(objects[8])+1,_mtop(objects[8])+2)!=1){puts("FAIL background pen");failures++;}
    for(int i=10;i<13;i++)if(pixels(objects[i])==pixels(objects[0])){printf("FAIL effect %d\n",i);failures++;}
    MUI_DisposeObject(app);CloseScreen(screen);CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

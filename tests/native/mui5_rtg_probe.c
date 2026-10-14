/* Exact RGB and alpha text/image pixels on a CyberGraphX/Picasso96 screen. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n,...) ({ULONG tags_[]={__VA_ARGS__};MUI_NewObjectA(n,(APTR)tags_);})
struct Library *MUIMasterBase, *CyberGfxBase;
ULONG __stack=65536;
int main(void)
{
    int failures=0;setvbuf(stdout,NULL,_IONBF,0);
    CyberGfxBase=OpenLibrary("cybergraphics.library",41);
    MUIMasterBase=OpenLibrary("muimaster.library",0);
    if(!CyberGfxBase||!MUIMasterBase){puts("FAIL missing graphics/MUI library");return 20;}
    ULONG mode=BestCModeIDTags(CYBRBIDTG_NominalWidth,640,CYBRBIDTG_NominalHeight,480,CYBRBIDTG_Depth,24,TAG_DONE);
    printf("RTG mode=%08lx\n",mode);
    struct Screen *screen=OpenScreenTags(NULL,SA_DisplayID,mode,SA_Width,640,SA_Height,480,SA_Depth,24,TAG_DONE);
    if(!screen){puts("FAIL RTG screen");return 20;}
    struct {LONG width,height,dummy[2];ULONG pixels[4];} image={2,2,{0,0},{0x8000ff00,0x8000ff00,0x8000ff00,0x8000ff00}};
    char raw[40];sprintf(raw,"\33A[$%lx]",(ULONG)&image);
    const char *samples[]={"\33P[ff0000]MMMM", "\33P[80ff0000]MMMM",
        "\33P[ff0000]\33P[00------]MMMM", "\33P[ff0000]\33P[80------]MMMM", raw};
    Object *objects[5],*group=MO(MUIC_Group,TAG_DONE);
    for(int i=0;i<5;i++)
    {
        objects[i]=MO(MUIC_Text,MUIA_Text_Contents,samples[i],MUIA_Font,MUIV_Font_Fixed,
            MUIA_FixWidth,80,MUIA_FixHeight,8,MUIA_Background,"2:00000000,00000000,00000000",TAG_DONE);
        DoMethod(group,OM_ADDMEMBER,objects[i]);
    }
    Object *win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_RootObject,group,TAG_DONE);
    Object *app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
    for(int i=0;i<5;i++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
    ULONG expected[]={0xff0000,0x800000,0,0x800000,0x008000};
    for(int i=0;i<5;i++)
    {
        ULONG brightest=0;
        for(LONG y=_mtop(objects[i]);y<=_mbottom(objects[i]);y++)
            for(LONG x=_mleft(objects[i]);x<=_mright(objects[i]);x++)
            {ULONG rgb=ReadRGBPixel(_rp(objects[i]),x,y)&0xffffff;if(rgb>brightest)brightest=rgb;}
        printf("text=%d brightest=%06lx expected=%06lx\n",i,brightest,expected[i]);
        if(brightest!=expected[i])failures++;
    }
    MUI_DisposeObject(app);CloseScreen(screen);CloseLibrary(MUIMasterBase);CloseLibrary(CyberGfxBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

/* Mouse marking, formatted text and clipboard interoperability. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static int failures;
static BOOL multiline;
static void check(BOOL ok, const char *label)
{ if (!ok) { printf("FAIL %s\n", label); failures++; } }
static void pump(Object *app)
{ for (int i=0;i<3;i++) { DoMethod(app,MUIM_Application_InputBuffered); Delay(2); } }
static void event(Object *obj, ULONG cls, ULONG code, LONG x, LONG y)
{
    struct IntuiMessage im={0};
    im.Class=cls; im.Code=code; im.MouseX=x; im.MouseY=y;
    im.Qualifier=cls==IDCMP_RAWKEY?IEQUALIFIER_RCOMMAND:IEQUALIFIER_LEFTBUTTON;
    DoMethod(obj,MUIM_HandleEvent,&im,0);
}
static ULONG pixels(Object *obj)
{
    ULONG hash=2166136261UL;
    for(LONG y=_mtop(obj);y<=_mbottom(obj);y++)
        for(LONG x=_mleft(obj);x<=_mright(obj);x++)
            hash=(hash^ReadPixel(_rp(obj),x,y))*16777619UL;
    return hash;
}
static void copy_range(Object *app,Object *text,Object *str,
    LONG start,LONG end,const char *expected)
{
    LONG x=_mleft(text),y=_mtop(text)+(multiline ? _font(text)->tf_YSize / 2 : _mheight(text)/2);
    LONG ey=y+(multiline ? _font(text)->tf_YSize : 0);
    LONG w=_font(text)->tf_XSize;
    ULONG before=pixels(text);
    event(text,IDCMP_MOUSEBUTTONS,SELECTDOWN,x+start*w,y);
    event(text,IDCMP_MOUSEMOVE,0,x+end*w,ey);
    event(text,IDCMP_MOUSEBUTTONS,SELECTUP,x+end*w,ey);
    pump(app);
    check(before!=pixels(text),"mark is visible");
    event(text,IDCMP_RAWKEY,0x33,0,0);
    SetAttrs(str,MUIA_String_Contents,"",TAG_DONE);
    SetAttrs(_win(str),MUIA_Window_ActiveObject,str,TAG_DONE);
    event(str,IDCMP_RAWKEY,0x34,0,0);
    pump(app);
    ULONG value=0; GetAttr(MUIA_String_Contents,str,&value);
    printf("copy expected=[%s] actual=[%s]\n",expected,(char*)value);
    check(value && !strcmp((char*)value,expected),"clipboard selection");
}
int main(int argc,char **argv)
{
    struct Screen *screen=NULL;
    setvbuf(stdout,NULL,_IONBF,0);
    MUIMasterBase=OpenLibrary("muimaster.library",0);
    if(!MUIMasterBase)return 20;
    if(argc>1)screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    Object *text=MO(MUIC_Text,MUIA_Text_Marking,TRUE,MUIA_Font,MUIV_Font_Fixed,
        MUIA_Text_Contents,"ab\033ucd\033Uefghijklmnop",MUIA_FixWidth,128,
        MUIA_Text_SetMin,FALSE,TAG_DONE);
    Object *str=MO(MUIC_String,TAG_DONE);
    Object *group=MO(MUIC_Group,MUIA_Group_Child,text,MUIA_Group_Child,str,TAG_DONE);
    Object *win=MO(MUIC_Window,MUIA_Window_Screen,screen,
        MUIA_Window_RootObject,group,TAG_DONE);
    Object *app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    if(!app)return 20;
    ULONG marking=0;GetAttr(MUIA_Text_Marking,text,&marking);
    check(marking==TRUE,"marking getter");
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);
    copy_range(app,text,str,1,5,"bcde");
    SetAttrs(text,MUIA_Text_Contents,"abcdefghijklmnop",TAG_DONE);pump(app);
    copy_range(app,text,str,7,2,"cdefg");
    SetAttrs(text,MUIA_Text_Contents,"abcdefghijklmnopqrstuv",
        MUIA_Text_Shorten,MUIV_Text_Shorten_ElideLeft,TAG_DONE);pump(app);
    LONG chars=_mwidth(text)/_font(text)->tf_XSize;
    copy_range(app,text,str,chars-3,chars,"tuv");
    SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);
    SetAttrs(text,MUIA_Text_Shorten,0,MUIA_Text_Contents,"reopened",TAG_DONE);pump(app);
    copy_range(app,text,str,0,8,"reopened");
    SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
    SetAttrs(text,MUIA_Text_Contents,"abc\ndefgh",TAG_DONE);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);
    multiline=TRUE;
    copy_range(app,text,str,1,3,"bc\ndef");
    MUI_DisposeObject(app);if(screen)CloseScreen(screen);CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

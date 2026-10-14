/* Column widths, reorder/preparse and borrowed object-cell lifetimes. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n,...) ({ULONG t[]={__VA_ARGS__};MUI_NewObjectA(n,(APTR)t);})
struct Library *MUIMasterBase;
ULONG __stack=131072;
static int failures;
static ULONG setups,cleanups,draws,disposes;
static void check(BOOL ok,const char *label)
{if(!ok){printf("FAIL %s\n",label);failures++;}}
static ULONG cell_dispatch(struct IClass *cl __asm("a0"),Object *obj __asm("a2"),Msg msg __asm("a1"))
{
    if(msg->MethodID==MUIM_Setup)setups++;
    if(msg->MethodID==MUIM_Cleanup)cleanups++;
    if(msg->MethodID==MUIM_Draw)draws++;
    if(msg->MethodID==OM_DISPOSE)disposes++;
    return DoSuperMethodA(cl,obj,msg);
}
static ULONG attr(Object *o,ULONG id){ULONG v=0;GetAttr(id,o,&v);return v;}
static void pump(Object *app){for(int i=0;i<3;i++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}}
static void boundaries(Object *list,LONG *first)
{
    for(int i=0;i<3;i++)first[i]=-1;
    for(LONG x=_mleft(list);x<=_mright(list);x++)
    {
        struct MUI_List_TestPos_Result p={0};
        DoMethod(list,MUIM_List_TestPos,x,_mtop(list)+25,&p);
        if(p.column>=0&&p.column<3&&first[p.column]<0)first[p.column]=x-p.xoffset;
    }
    printf("columns %ld %ld %ld\n",first[0],first[1],first[2]);
}
int main(int argc,char **argv)
{
    struct Screen *screen=NULL;
    setvbuf(stdout,NULL,_IONBF,0);
    MUIMasterBase=OpenLibrary("muimaster.library",0);if(!MUIMasterBase)return 20;
    if(argc>1)screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    char *row[]={"abcdefghijklmnop","second long column","last"};
    char *titles[]={"First","Second","Third"};
    Object *list=MO(MUIC_List,MUIA_List_MaxColumns,3,MUIA_List_TitleArray,titles,
        MUIA_List_Format,"MINWIDTH=40px MAXWIDTH=40px ELIDE=right,FIXWIDTH=60,MINWIDTH=30px MAXWIDTH=30px",
        MUIA_List_DisplayHook,MUIV_List_DisplayHook_StringArray,MUIA_FixWidth,240,TAG_DONE);
    DoMethod(list,MUIM_List_InsertSingle,row,MUIV_List_Insert_Bottom);
    Object *win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_RootObject,list,TAG_DONE);
    Object *app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);
    LONG first[3];boundaries(list,first);
    check(first[0]>=0&&first[1]-first[0]==50,"pixel width with spacing");
    check(first[2]-first[1]==60,"legacy fixed pixel width");
    UBYTE order[]={2,0,1};SetAttrs(list,MUIA_List_ColumnOrder,order,TAG_DONE);pump(app);
    SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);
    boundaries(list,first);
    check(first[1]-first[0]==50&&first[2]-first[1]==60,"physical widths survive column reorder");
    SetAttrs(list,MUIA_List_HideColumn,1,TAG_DONE);pump(app);
    boundaries(list,first);check(first[2]<0,"hidden logical column not hit");
    SetAttrs(list,MUIA_List_ShowColumn,1,TAG_DONE);pump(app);
    boundaries(list,first);check(first[2]>=0,"show column restores hits");
    MUI_DisposeObject(app);

    struct MUI_CustomClass *cc=MUI_CreateCustomClass(NULL,MUIC_Rectangle,NULL,0,(APTR)cell_dispatch);
    ULONG tags[]={MUIA_FixWidth,24,MUIA_FixHeight,12,MUIA_Background,MUII_ButtonBack,TAG_DONE};
    Object *cell=NewObjectA(cc->mcc_Class,NULL,(APTR)tags);
    APTR cells[]={cell,"text"};
    list=MO(MUIC_List,MUIA_List_MaxColumns,2,MUIA_List_Format,"ISOBJECT,",
        MUIA_List_DisplayHook,MUIV_List_DisplayHook_StringArray,TAG_DONE);
    DoMethod(list,MUIM_List_InsertSingle,cells,MUIV_List_Insert_Bottom);
    win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_RootObject,list,TAG_DONE);
    app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    for(int i=0;i<2;i++)
    {
        SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);
        check(attr(win,MUIA_Window_Open),"object-cell window opens");
        SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
    }
    printf("object setups=%lu cleanups=%lu draws=%lu entries=%lu\n",setups,cleanups,draws,attr(list,MUIA_List_Entries));
    if(strstr((char *)MUIMasterBase->lib_IdString,"Zune68"))
        check(draws>0,"object cell drawn");
    else if(!draws) puts("REFERENCE DIFFERENCE: 21.227 does not draw ISOBJECT cells");
    check(setups==cleanups,"object setup and cleanup balanced");
    DoMethod(list,MUIM_List_Clear);MUI_DisposeObject(app);
    check(disposes==0,"list does not dispose borrowed cell");
    MUI_DisposeObject(cell);check(disposes==1,"caller owns object cell");
    MUI_DeleteCustomClass(cc);
    if(screen)CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

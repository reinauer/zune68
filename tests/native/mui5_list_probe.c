/* Compare modern List callbacks and attributes with the MUI OS3 runtime. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
struct Library *MUIMasterBase;
ULONG __stack=65536;
static LONG compared=-99;
static ULONG edits, commits, aborts;
static Object *editor;
static BOOL veto;
static char row0[32]="gamma",row1[32]="alpha",row2[32]="beta";
static ULONG dispatch(struct IClass *cl __asm("a0"),Object *obj __asm("a2"),Msg msg __asm("a1"))
{
    switch(msg->MethodID) {
    case MUIM_List_Compare: {
        struct MUIP_List_Compare *m=(APTR)msg;
        compared=m->column;
        return strcmp(m->entry1,m->entry2);
    }
    case MUIM_List_Display: {
        struct MUIP_List_Display *m=(APTR)msg;
        m->array[0]=m->entry?m->entry:"Name";
        m->array[1]=m->entry?"value":"Detail";
        return 1;
    }
    case MUIM_List_CreateEditObject: {
        struct MUIP_List_CreateEditObject *m=(APTR)msg;
        edits++;
        editor=MO(MUIC_String,MUIA_String_Contents,m->entry,MUIA_String_MaxLen,32,
            MUIA_CycleChain,1,TAG_DONE);
        printf("create row=%ld col=%ld entry=%s\n",m->row,m->column,(char*)m->entry);
        return (ULONG)editor;
    }
    case MUIM_List_EditDone: {
        struct MUIP_List_EditDone *m=(APTR)msg;
        printf("done row=%ld col=%ld abort=%d veto=%d\n",m->row,m->column,!m->entry,veto);
        if(veto && m->entry) return 0;
        if(m->entry){ STRPTR text=NULL; GetAttr(MUIA_String_Contents,m->editobj,(ULONG*)&text); if(text) strcpy(m->entry,text); commits++; }
        else aborts++;
        return 1;
    }
    }
    return DoSuperMethodA(cl,obj,msg);
}
static ULONG attr(Object *o,ULONG a){ULONG v=0xdeadbeef;GetAttr(a,o,&v);return v;}
static void pump(Object *app){for(int j=0;j<4;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}}
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
int main(int argc,char **argv)
{
    struct Screen *screen=NULL;
    Object *list,*win,*app;
    MUIMasterBase=OpenLibrary("muimaster.library",0); if(!MUIMasterBase)return 20;
    printf("library=%u.%u\n",MUIMasterBase->lib_Version,MUIMasterBase->lib_Revision);
    if(argc>1&&!strcmp(argv[1],"screen"))screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    struct MUI_CustomClass *cl=MUI_CreateCustomClass(NULL,MUIC_List,NULL,0,dispatch);
    if(!cl)return 20;
    struct TagItem tags[]={{MUIA_List_Format,(ULONG)"BAR SORTABLE,SORTABLE"},{MUIA_List_Title,TRUE},
        {MUIA_List_Editable,TRUE},{MUIA_List_Stripes,TRUE},{MUIA_List_SortColumn,1},
        {MUIA_List_HScrollerVisibility,MUIV_List_HScrollerVisibility_Always},{TAG_DONE,0}};
    list=NewObjectA(cl->mcc_Class,NULL,tags);
    if(!list)return 20;
    APTR rows[]={row0,row1,row2,NULL};
    DoMethod(list,MUIM_List_Insert,rows,3,MUIV_List_Insert_Bottom);
    DoMethod(list,MUIM_List_Sort);
    printf("sort col=%ld get=%ld stripes=%lu edit=%lu titleclick=%ld\n",compared,attr(list,MUIA_List_SortColumn),attr(list,MUIA_List_Stripes),attr(list,MUIA_List_Editable),attr(list,MUIA_List_TitleClick));
    check(compared==1,"sort column reaches Compare");
    check(attr(list,MUIA_List_SortColumn)==1,"sort column getter");
    check(attr(list,MUIA_List_Editable)==1,"editable getter");
    check(DoMethod(list,MUIM_List_SortEntries,rows),"sort external array");
    check(rows[0]==row1 && rows[1]==row2 && rows[2]==row0,"external sort order");
    printf("sort-array ");
    printf("%s,%s,%s\n",(char*)rows[0],(char*)rows[1],(char*)rows[2]);
    win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_Title,"List probe",
        MUIA_Window_Width,280,MUIA_Window_Height,130,MUIA_Window_RootObject,list,TAG_DONE);
    app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);if(!app)return 20;
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);
    check(attr(win,MUIA_Window_Open),"window opens");
    native=(struct Window *)attr(win,MUIA_Window_Window);
    check(DoMethod(list,MUIM_List_Edit,0,0),"start editor");
    if(editor)SetAttrs(editor,MUIA_String_Contents,"edited",TAG_DONE);
    veto=TRUE; check(!DoMethod(list,MUIM_List_EndEdit,MUIV_List_EndEdit_Done),"veto keeps editor");
    veto=FALSE; check(DoMethod(list,MUIM_List_EndEdit,MUIV_List_EndEdit_Done),"commit editor");
    check(!strcmp(row1,"edited"),"application receives edited contents");
    editor=NULL;
    check(DoMethod(list,MUIM_List_Edit,1,0),"start second editor");
    check(DoMethod(list,MUIM_List_EndEdit,MUIV_List_EndEdit_Abort),"abort editor");
    SetAttrs(list,MUIA_List_HideColumn,0,TAG_DONE);pump(app);
    check(DoMethod(list,MUIM_List_Edit,0,0),"edit hidden column");
    SetAttrs(list,MUIA_List_ShowColumn,0,TAG_DONE);pump(app);
    input(app,IECLASS_RAWKEY,0x44,0,0,0);
    input(app,IECLASS_RAWKEY,0x44|IECODE_UP_PREFIX,0,0,0);
    printf("return commits=%lu aborts=%lu\n",commits,aborts);
    check(commits==2,"Return commits editor");
    check(DoMethod(list,MUIM_List_Edit,1,0),"start Escape editor");
    input(app,IECLASS_RAWKEY,0x45,0,0,0);
    input(app,IECLASS_RAWKEY,0x45|IECODE_UP_PREFIX,0,0,0);
    printf("escape commits=%lu aborts=%lu\n",commits,aborts);
    check(aborts==2,"Escape cancels editor");
    LONG title_x=-1;
    for(LONG x=_mleft(list);x<=_mright(list);x++) {
        struct MUI_List_TestPos_Result pos;
        DoMethod(list,MUIM_List_TestPos,x,_mtop(list)+2,&pos);
        if(pos.column==1) {title_x=x;break;}
    }
    check(title_x>=0,"find second title column");
    if(title_x>=0) {
        LONG title_y=_top(list)+5;
        ULONG clicked=0xdeadbeef;
        DoMethod(list,MUIM_Notify,MUIA_List_TitleClick,MUIV_EveryTime,list,3,
            MUIM_WriteLong,MUIV_TriggerValue,&clicked);
        printf("title hit %ld,%ld top=%d inner=%d\n",title_x,title_y,_top(list),_mtop(list));
        input(app,IECLASS_NEWPOINTERPOS,0,0,title_x,title_y);
        input(app,IECLASS_RAWMOUSE,IECODE_LBUTTON,0,0,0);
        input(app,IECLASS_RAWMOUSE,IECODE_LBUTTON|IECODE_UP_PREFIX,0,0,0);
        printf("title click get=%lu notify=%lu\n",attr(list,MUIA_List_TitleClick),clicked);
        if(strstr((char *)MUIMasterBase->lib_IdString, "Zune68") != NULL)
            check(clicked==1,"title click notifies column");
        else if(clicked!=1)
            puts("REFERENCE LIMIT: no TitleClick notification in this fixture");
        DoMethod(list,MUIM_KillNotify,MUIA_List_TitleClick);
    }
    check(DoMethod(list,MUIM_List_Edit,1,0),"start disposal editor");
    MUI_DisposeObject(app);MUI_DeleteCustomClass(cl);if(screen)CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    check(edits==5 && commits==2 && aborts==3,"editor lifecycle counts");
    printf("%s failures=%d edits=%lu commits=%lu aborts=%lu\n",failures?"FAIL":"PASS",failures,edits,commits,aborts);
    return failures?20:0;
}

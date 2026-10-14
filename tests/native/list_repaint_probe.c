/* Compare incremental List painting against a complete repaint of the same
 * state. Run in an owned guest on planar and RTG screens. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n, ...) ({ ULONG t[] = {__VA_ARGS__}; MUI_NewObjectA(n, (APTR)t); })
struct Library *MUIMasterBase;
ULONG __stack = 131072;
static int failures, comparisons;
static ULONG active_notifications;
static char events[32];
static unsigned int nevents;
static char rows[40][32];
static ULONG attr(Object *obj, ULONG id)
{ ULONG v = 0; GetAttr(id, obj, &v); return v; }
static void pump(Object *app)
{ for (int i = 0; i < 3; i++) { DoMethod(app, MUIM_Application_InputBuffered); Delay(1); } }
static void check(BOOL ok, const char *label)
{ if (!ok) { printf("FAIL %s\n", label); failures++; } }
static ULONG dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_CustomBackfill) {
        struct MUIP_CustomBackfill *m = (APTR)msg;
        for (LONG y=m->top; y<=m->bottom; y++) {
            SetAPen(_rp(obj), _pens(obj)[(y % 3) ? MPEN_BACKGROUND : MPEN_SHADOW]);
            RectFill(_rp(obj),m->left,y,m->right,y);
        }
        return TRUE;
    }
    if (msg->MethodID == 0xad000001 || msg->MethodID == 0xad000002) {
        if (msg->MethodID == 0xad000001) active_notifications++;
        if (nevents < sizeof(events)) events[nevents++] =
            msg->MethodID == 0xad000001 ? 'A' : 'F';
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}
static void compare(Object *app, Object *list, const char *name)
{
    LONG w = _mwidth(list), h = _mheight(list), different = 0, firstx = -1, firsty = -1;
    ULONG depth=GetBitMapAttr(_rp(list)->BitMap,BMA_DEPTH);
    struct BitMap maps[2];
    struct RastPort ports[2];
    if(!depth || depth>8) { check(FALSE,"indexed screen required"); return; }
    memset(maps,0,sizeof(maps));
    for(int i=0;i<2;i++) {
        InitBitMap(&maps[i],depth,w,h);
        InitRastPort(&ports[i]); ports[i].BitMap=&maps[i];
        for(ULONG plane=0;plane<depth;plane++) {
            maps[i].Planes[plane]=AllocRaster(w,h);
            if(!maps[i].Planes[plane]) { check(FALSE,"capture bitmap allocation"); goto cleanup; }
            memset(maps[i].Planes[plane],0,RASSIZE(w,h));
        }
    }
    pump(app);
    /* One layer-aware blit per snapshot, rather than one blit per line. */
    ClipBlit(_rp(list),_mleft(list),_mtop(list),&ports[0],0,0,w,h,0xc0);
    WaitBlit();
    MUI_Redraw(list, MADF_DRAWOBJECT);
    ClipBlit(_rp(list),_mleft(list),_mtop(list),&ports[1],0,0,w,h,0xc0);
    WaitBlit();
    for(ULONG plane=0;plane<depth;plane++)
        for(ULONG i=0;i<RASSIZE(w,h);i++)
            if(maps[0].Planes[plane][i]!=maps[1].Planes[plane][i]) {
                different++;
                if(firstx<0) { firstx=(i%maps[0].BytesPerRow)*8; firsty=i/maps[0].BytesPerRow; }
            }
cleanup:
    for(int i=0;i<2;i++) for(ULONG plane=0;plane<depth;plane++)
        if(maps[i].Planes[plane])FreeRaster(maps[i].Planes[plane],w,h);
    comparisons++;
    if (different) {
        printf("FAIL %s plane-bytes=%ld first-byte=%ld,%ld active=%ld top=%lu size=%ld,%ld\n",
            name, different, firstx, firsty, attr(list,MUIA_List_Active),
            attr(list,MUIA_List_First), w, h);
        failures++;
    }
}
static void scenario(struct Screen *screen, struct MUI_CustomClass *cc, int mode)
{
    Object *list, *win, *app;
    ULONG tags[] = {MUIA_Frame,MUIV_Frame_InputList, MUIA_FixWidth,150,
        MUIA_FixHeight,73, MUIA_List_ConstructHook,MUIV_List_ConstructHook_String,
        MUIA_List_DestructHook,MUIV_List_DestructHook_String,
        MUIA_List_Title,mode==1?(ULONG)"Title":0,
        MUIA_List_Stripes,mode==2,
        MUIA_CustomBackfill,mode==5,
        MUIA_Background,mode==4?(ULONG)"5:RAM:zune68-list-tile.iff":
            mode==3?(ULONG)"0:132":(ULONG)"0:128",TAG_DONE};
    list = NewObjectA(cc->mcc_Class,NULL,(APTR)tags);
    Object *root=list;
    if(mode==6) {
        Object *virt=MO(MUIC_Virtgroup,MUIA_Group_Child,(ULONG)list,TAG_DONE);
        root=MO(MUIC_Scrollgroup,MUIA_Scrollgroup_Contents,(ULONG)virt,TAG_DONE);
    }
    win = MO(MUIC_Window,MUIA_Window_Screen,(ULONG)screen,MUIA_Window_RootObject,(ULONG)root,TAG_DONE);
    app = MO(MUIC_Application,MUIA_Application_Window,(ULONG)win,TAG_DONE);
    if (!app) { check(FALSE,"create application"); return; }
    for (int i=0;i<40;i++) DoMethod(list,MUIM_List_InsertSingle,rows[i],MUIV_List_Insert_Bottom);
    DoMethod(list,MUIM_Notify,MUIA_List_Active,MUIV_EveryTime,list,1,0xad000001);
    DoMethod(list,MUIM_Notify,MUIA_List_First,MUIV_EveryTime,list,1,0xad000002);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
    check(attr(win,MUIA_Window_Open),"window opens");
    struct Window *opened=(APTR)attr(win,MUIA_Window_Window);
    printf("scenario=%d visible=%lu depth=%lu mode=%08lx\n",mode,attr(list,MUIA_List_Visible),
        GetBitMapAttr(_rp(list)->BitMap,BMA_DEPTH),GetVPModeID(&opened->WScreen->ViewPort));
    const LONG positions[]={0,1,2,7,8,9,10,8,5,3,0,39,38,35,-1};
    for (unsigned int i=0;i<sizeof(positions)/sizeof(*positions);i++) {
        ULONG before=active_notifications;
        nevents=0;
        SetAttrs(list,MUIA_List_Active,positions[i],TAG_DONE);
        check(attr(list,MUIA_List_Active)==(ULONG)positions[i],"active state");
        check(active_notifications==before+1,"one active notification");
        check(nevents && events[nevents-1]=='A',"First precedes Active notification");
        compare(app,list,"active");
    }
    SetAttrs(list,MUIA_List_Active,2,TAG_DONE);
    DoMethod(list,MUIM_List_Select,2,MUIV_List_Select_On,NULL);
    DoMethod(list,MUIM_List_Select,4,MUIV_List_Select_On,NULL);
    for (int i=0;i<4;i++) {
        SetAttrs(list,MUIA_List_First,i,TAG_DONE);
        compare(app,list,"direct first with selection");
    }
    SetAttrs(list,MUIA_List_Quiet,TRUE,TAG_DONE);
    SetAttrs(list,MUIA_List_Active,4,TAG_DONE);
    SetAttrs(list,MUIA_List_Active,7,TAG_DONE);
    SetAttrs(list,MUIA_List_Quiet,FALSE,TAG_DONE);
    compare(app,list,"quiet selection");
    SetAttrs(list,MUIA_Disabled,TRUE,TAG_DONE);
    SetAttrs(list,MUIA_List_Active,20,TAG_DONE);
    compare(app,list,"disabled autoscroll");
    SetAttrs(list,MUIA_Disabled,FALSE,MUIA_List_Active,3,TAG_DONE);
    compare(app,list,"mixed inherited tags");
    SetAttrs(list,MUIA_List_Active,25,TAG_DONE);
    struct Window *native=(APTR)attr(win,MUIA_Window_Window);
    struct Window *cover=OpenWindowTags(NULL,WA_CustomScreen,native->WScreen,
        WA_Left,native->LeftEdge+30,WA_Top,native->TopEdge+20,
        WA_Width,70,WA_Height,40,WA_Borderless,TRUE,TAG_DONE);
    if(cover) {
        SetAttrs(list,MUIA_List_Active,27,TAG_DONE);
        CloseWindow(cover);pump(app);
        compare(app,list,"covered autoscroll exposed");
    } else check(FALSE,"cover window opens");
    SetAttrs(list,MUIA_Listview_Input,FALSE,TAG_DONE);
    SetAttrs(list,MUIA_List_Active,39,TAG_DONE);
    compare(app,list,"read-only autoscroll");
    SetAttrs(list,MUIA_Listview_Input,TRUE,TAG_DONE);
    MUI_Redraw(list,MADF_DRAWOBJECT);
    ULONG before=active_notifications;
    SetAttrs(list,MUIA_NoNotify,TRUE,MUIA_List_Active,1,TAG_DONE);
    check(active_notifications==before,"NoNotify retained");
    compare(app,list,"NoNotify active");
    SetAttrs(list,MUIA_Group_Forward,TRUE,MUIA_List_Active,3,TAG_DONE);
    check(active_notifications==before+1,"explicit forwarding notification");
    compare(app,list,"explicit forwarding");
    SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
    SetAttrs(list,MUIA_List_Active,1,TAG_DONE);
    check(attr(list,MUIA_List_Active)==1,"closed selection");
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
    compare(app,list,"reopened");
    MUI_DisposeObject(app);
}
/* Original 16x7 test tile, uncompressed two-plane ILBM. */
static const UBYTE tile[]={
    0x46,0x4f,0x52,0x4d,0x00,0x00,0x00,0x58,0x49,0x4c,0x42,0x4d,0x42,0x4d,0x48,0x44,
    0x00,0x00,0x00,0x14,0x00,0x10,0x00,0x07,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
    0x00,0x00,0x0a,0x0a,0x00,0x10,0x00,0x07,0x43,0x4d,0x41,0x50,0x00,0x00,0x00,0x0c,
    0xaa,0xaa,0xaa,0x00,0x00,0x00,0xff,0xff,0xff,0x66,0x88,0xbb,0x42,0x4f,0x44,0x59,
    0x00,0x00,0x00,0x1c,0x55,0x55,0x00,0x00,0xaa,0xaa,0xff,0xff,0x55,0x55,0xff,0xff,
    0xaa,0xaa,0x00,0x00,0x55,0x55,0xff,0xff,0xaa,0xaa,0xff,0xff,0x55,0x55,0x00,0x00,
};
int main(int argc,char **argv)
{
    struct Screen *screen=NULL;
    setvbuf(stdout,NULL,_IONBF,0);
    MUIMasterBase=OpenLibrary("muimaster.library",19); if(!MUIMasterBase)return 20;
    if(argc>1 && !strcmp(argv[1],"rtg"))
        screen=OpenScreenTags(NULL,SA_DisplayID,0x50031000,SA_Width,640,
            SA_Height,480,SA_Depth,8,TAG_DONE);
    else if(argc>1)
        screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    if(argc>1 && !screen) { puts("FAIL requested screen"); return 20; }
    if(argc>1 && !strcmp(argv[1],"rtg") && GetVPModeID(&screen->ViewPort)!=0x50031000) {
        puts("FAIL requested RTG mode unavailable"); return 20;
    }
    struct MUI_CustomClass *cc=MUI_CreateCustomClass(NULL,MUIC_List,NULL,0,(APTR)dispatch);
    if(!cc)return 20;
    for(int i=0;i<40;i++)sprintf(rows[i],i%3==0?"\33bRow %02d bold":"Row %02d plain",i);
    BPTR file=Open("RAM:zune68-list-tile.iff",MODE_NEWFILE);
    if(file) { check(Write(file,(APTR)tile,sizeof(tile))==sizeof(tile),"write tile"); Close(file); }
    else check(FALSE,"create tile");
    for(int i=0;i<7;i++)scenario(screen,cc,i);
    DeleteFile("RAM:zune68-list-tile.iff");
    MUI_DeleteCustomClass(cc);
    if(screen)CloseScreen(screen);
    CloseLibrary(MUIMasterBase);
    printf("%s comparisons=%d failures=%d\n",failures?"FAIL":"PASS",comparisons,failures);
    return failures?20:0;
}

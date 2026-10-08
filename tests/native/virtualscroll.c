/* Virtual scrolling must preserve layout-hook state and paint only the
 * exposed pixels after moving the existing contents. This standalone
 * regression also runs with the original MUI SDK and library.
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <graphics/layers.h>
#include <graphics/regions.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define CUSTOM_BACKFILL 0x80420a63UL
#define BACKFILL 0x80428d73UL
#define BEGIN 0x80425b5cUL
#define END 0x8042da52UL
struct Library *MUIMasterBase;
ULONG __stack=65536;
static Object *virt;
static int failures, expect_blit, sampled_blit;
static LONG marker_x, marker_y;
static void require(int okay,const char *what) {
    if(!okay){printf("FAIL: %s\n",what);failures++;}
}
struct Event {
    const char *name;
    ULONG flags,left,top,clip;
    LONG args[6];
    WORD bounds[4],region[4];
    ULONG regions;
};
static struct Event events[128];
static unsigned used;
static void record(const char *name,Object *obj,ULONG flags,LONG *args) {
    struct Event *e;
    struct Layer *layer=_rp(obj) ? _rp(obj)->Layer : NULL;
    struct Region *clip=layer ? layer->ClipRegion : NULL;
    struct RegionRectangle *r;
    if(used>=128)return;
    e=&events[used++];e->name=name;e->flags=flags;
    GetAttr(MUIA_Virtgroup_Left,virt,&e->left);
    GetAttr(MUIA_Virtgroup_Top,virt,&e->top);
    e->clip=(ULONG)clip;
    if(args)for(int i=0;i<6;i++)e->args[i]=args[i];
    if(clip) {
        e->bounds[0]=clip->bounds.MinX;e->bounds[1]=clip->bounds.MinY;
        e->bounds[2]=clip->bounds.MaxX;e->bounds[3]=clip->bounds.MaxY;
        r=clip->RegionRectangle;
        if(r) {
            e->region[0]=r->bounds.MinX;e->region[1]=r->bounds.MinY;
            e->region[2]=r->bounds.MaxX;e->region[3]=r->bounds.MaxY;
        }
        for(;r;r=r->Next)e->regions++;
    }
}
static void geometry(const char *name,Object *obj,ULONG flags) {
    LONG box[6]={_left(obj),_top(obj),_width(obj),_height(obj),0,0};
    record(name,obj,flags,box);
}
static ULONG groupDispatch(struct IClass *cl __asm("a0"),Object *obj __asm("a2"),Msg msg __asm("a1")) {
    ULONG result;
    if(msg->MethodID==0x8042845bUL || msg->MethodID==0x90420200UL)
        geometry("nested-layout",obj,0);
    if(msg->MethodID==MUIM_Show || msg->MethodID==MUIM_Hide)
        geometry(msg->MethodID==MUIM_Show?"nested-show":"nested-hide",obj,0);
    result=DoSuperMethodA(cl,obj,msg);
    return result;
}
static ULONG textDispatch(struct IClass *cl __asm("a0"),Object *obj __asm("a2"),Msg msg __asm("a1")) {
    if(msg->MethodID==0x8042845bUL || msg->MethodID==0x90420200UL)
        geometry("child-layout",obj,0);
    if(msg->MethodID==MUIM_Show || msg->MethodID==MUIM_Hide)
        geometry(msg->MethodID==MUIM_Show?"child-show":"child-hide",obj,0);
    if(msg->MethodID==MUIM_Draw) {
        LONG box[6]={_left(obj),_top(obj),_width(obj),_height(obj),0,0};
        record("child-draw",obj,((struct MUIP_Draw *)msg)->flags,box);
    }
    return DoSuperMethodA(cl,obj,msg);
}
static ULONG dispatch(struct IClass *cl __asm("a0"),Object *obj __asm("a2"),Msg msg __asm("a1")) {
    if(msg->MethodID==BEGIN || msg->MethodID==END) {
        ULONG result;
        if(msg->MethodID==BEGIN && expect_blit && !sampled_blit) {
            require(ReadPixel(_rp(obj),marker_x,marker_y)==3,
                "pixels must move before BeginPaint");
            sampled_blit=1;
        }
        record(msg->MethodID==BEGIN?"begin-before":"end-before",obj,0,NULL);
        result=DoSuperMethodA(cl,obj,msg);
        record(msg->MethodID==BEGIN?"begin-after":"end-after",obj,result,NULL);
        return result;
    }
    if(msg->MethodID==MUIM_Draw) {
        ULONG result;
        record("draw-before",obj,((struct MUIP_Draw *)msg)->flags,NULL);
        result=DoSuperMethodA(cl,obj,msg);
        record("draw-after",obj,((struct MUIP_Draw *)msg)->flags,NULL);
        return result;
    }
    if(msg->MethodID==BACKFILL) {
        LONG *v=(LONG *)msg, top=0;
        record("backfill",obj,0,v+1);
        GetAttr(MUIA_Virtgroup_Top,obj,(ULONG *)&top);
        for(LONG y=v[2];y<=v[4];y++) {
            LONG row=y-_mtop(obj)+top;
            SetAPen(_rp(obj),((row/20)&1)?1:2);
            RectFill(_rp(obj),v[1],y,v[3],y);
        }
        return 1;
    }
    return DoSuperMethodA(cl,obj,msg);
}
static ULONG layout(struct Hook *hook __asm("a0"),Object *obj __asm("a2"),struct MUI_LayoutMsg *lm __asm("a1")) {
    (void)hook;(void)obj;
    if(lm->lm_Type==MUILM_MINMAX) {
        lm->lm_MinMax.MinWidth=100;lm->lm_MinMax.MinHeight=600;
        lm->lm_MinMax.DefWidth=220;lm->lm_MinMax.DefHeight=140;
        lm->lm_MinMax.MaxWidth=lm->lm_MinMax.MaxHeight=MUI_MAXMAX;return 0;
    }
    if(lm->lm_Type==MUILM_LAYOUT) {
        APTR state=lm->lm_Children->mlh_Head;Object *child;LONG y=60;
        record("layout-hook",obj,0,NULL);
        while((child=NextObject(&state))) {MUI_Layout(child,8,y,lm->lm_Layout.Width-16,20,0);y+=300;}
        lm->lm_Layout.Height=600;return 1;
    }
    return 0;
}
static void pump(Object *app) {
    ULONG sig=0;for(int i=0;i<15;i++){DoMethod(app,MUIM_Application_NewInput,&sig);Delay(1);}
}
static void validate(int scrolling,LONG delta) {
    unsigned begins=0,ends=0,full_background=0;
    int in_paint=0;
    ULONG outer=0;
    WORD left=_mleft(virt),top=_mtop(virt),right=_mright(virt),bottom=_mbottom(virt);
    for(unsigned i=0;i<used;i++) {
        struct Event *e=&events[i];
        if(scrolling)
            require(strcmp(e->name,"layout-hook") && strcmp(e->name,"nested-layout") &&
                strcmp(e->name,"child-layout"),"scroll must not invoke layout");
        if(!strcmp(e->name,"begin-before")){outer=e->clip;in_paint=1;}
        if(!strcmp(e->name,"begin-after")) {
            if(!begins) {
                WORD exposed_top=top;
                if(scrolling && delta>0 && delta<_mheight(virt))exposed_top=bottom-delta+1;
                require(e->flags && e->clip && e->bounds[0]==left &&
                    e->bounds[1]==exposed_top && e->bounds[2]==right &&
                    e->bounds[3]==bottom,"BeginPaint exposure clip");
            }
            begins++;
        }
        if(in_paint && !strcmp(e->name,"backfill") && e->args[0]==left &&
            e->args[1]==top && e->args[2]==right && e->args[3]==bottom)full_background++;
        if(!strcmp(e->name,"end-after")) {
            require(e->clip==outer,"EndPaint restores prior clip");
            ends++;in_paint=0;
        }
    }
    require(begins>0 && ends==begins,"balanced BeginPaint/EndPaint");
    require(full_background>0,"custom backfill receives full viewport");
    if(expect_blit)require(sampled_blit,"scroll BeginPaint reached pixel check");
}
static void shifted(Object **objects,WORD *before,LONG delta) {
    for(int i=0;i<3;i++) {
        require(_top(objects[i])==before[i*2+1]-delta,"descendant Y shifts without layout");
        require(_left(objects[i])==before[i*2],"vertical scroll preserves descendant X");
        before[i*2]=_left(objects[i]);before[i*2+1]=_top(objects[i]);
    }
}
static void dump(const char *phase) {
    ULONG top=0;GetAttr(MUIA_Virtgroup_Top,virt,&top);
    printf("PHASE %s top=%lu inner=%d,%d,%d,%d events=%u\n",phase,top,_mleft(virt),_mtop(virt),_mwidth(virt),_mheight(virt),used);
    for(unsigned i=0;i<used;i++) {
        struct Event *e=&events[i];
        printf("%s flags=%08lx scroll=%lu,%lu args=%ld,%ld,%ld,%ld,%ld,%ld clip=%08lx bounds=%d,%d,%d,%d rects=%lu first=%d,%d,%d,%d\n",
            e->name,e->flags,e->left,e->top,e->args[0],e->args[1],e->args[2],e->args[3],e->args[4],e->args[5],e->clip,
            e->bounds[0],e->bounds[1],e->bounds[2],e->bounds[3],e->regions,
            e->region[0],e->region[1],e->region[2],e->region[3]);
    }
    used=0;for(unsigned i=0;i<128;i++)events[i]=(struct Event){0};
    fflush(stdout);
}
int main(void) {
    struct MUI_CustomClass *cc,*tc,*gc;struct Hook hook={0};Object *a,*b,*nested,*win,*app;
    MUIMasterBase=OpenLibrary("muimaster.library",19);if(!MUIMasterBase)return 20;
    printf("library=%u.%u\n",MUIMasterBase->lib_Version,MUIMasterBase->lib_Revision);
    cc=MUI_CreateCustomClass(NULL,MUIC_Virtgroup,NULL,0,(APTR)dispatch);
    tc=MUI_CreateCustomClass(NULL,MUIC_Text,NULL,0,(APTR)textDispatch);
    gc=MUI_CreateCustomClass(NULL,MUIC_Group,NULL,0,(APTR)groupDispatch);
    if(!cc||!tc||!gc)return 20;
    a=NewObject(tc->mcc_Class,NULL,MUIA_Text_Contents,(ULONG)"row60: scroll text A",TAG_DONE);
    b=NewObject(tc->mcc_Class,NULL,MUIA_Text_Contents,(ULONG)"row360: scroll text B",TAG_DONE);
    nested=NewObject(gc->mcc_Class,NULL,MUIA_Group_Child,(ULONG)a,TAG_DONE);
    hook.h_Entry=(HOOKFUNC)layout;
    virt=NewObject(cc->mcc_Class,NULL,CUSTOM_BACKFILL,TRUE,
        MUIA_InnerLeft,3,MUIA_InnerTop,4,MUIA_InnerRight,5,MUIA_InnerBottom,6,
        MUIA_Group_LayoutHook,(ULONG)&hook,MUIA_Group_Child,(ULONG)nested,MUIA_Group_Child,(ULONG)b,TAG_DONE);
    win=MUI_NewObject(MUIC_Window,MUIA_Window_Title,(ULONG)"Virtual scroll paint",MUIA_Window_Width,220,MUIA_Window_Height,140,MUIA_Window_RootObject,(ULONG)virt,TAG_DONE);
    app=MUI_NewObject(MUIC_Application,MUIA_Application_Title,(ULONG)"VirtualScrollPaint",MUIA_Application_Base,(ULONG)"VIRTUALSCROLLPAINT",MUIA_Application_Window,(ULONG)win,TAG_DONE);
    if(!app)return 20;
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);pump(app);validate(0,0);dump("initial");
    {
        Object *objects[3]={nested,a,b};WORD before[6];ULONG oldtop=0,newtop=0;
        LONG small=37;
        if(small>=_mheight(virt))small=(_mheight(virt)/3)|1;
        require(small>0 && small<_mheight(virt),"viewport permits small scroll");
        for(int i=0;i<3;i++){before[i*2]=_left(objects[i]);before[i*2+1]=_top(objects[i]);}
        marker_x=_mleft(virt)+1;marker_y=_mtop(virt)+1;
        SetAPen(_rp(virt),0);WritePixel(_rp(virt),marker_x,marker_y);
        SetAPen(_rp(virt),3);WritePixel(_rp(virt),marker_x,marker_y+small);
        expect_blit=1;
        SetAttrs(virt,MUIA_Virtgroup_Top,small,TAG_DONE);pump(app);
        GetAttr(MUIA_Virtgroup_Top,virt,&newtop);
        shifted(objects,before,newtop-oldtop);validate(1,newtop-oldtop);dump("small-scroll");
        expect_blit=0;oldtop=newtop;
        SetAttrs(virt,MUIA_Virtgroup_Top,350,TAG_DONE);pump(app);
        GetAttr(MUIA_Virtgroup_Top,virt,&newtop);
        shifted(objects,before,newtop-oldtop);validate(1,newtop-oldtop);dump("large-scroll");
    }
    MUI_Redraw(virt,MADF_DRAWOBJECT);pump(app);validate(0,0);dump("explicit-redraw");
    SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);MUI_DisposeObject(app);
    MUI_DeleteCustomClass(gc);MUI_DeleteCustomClass(tc);MUI_DeleteCustomClass(cc);CloseLibrary(MUIMasterBase);printf("failures=%d\n",failures);return failures?20:0;
}

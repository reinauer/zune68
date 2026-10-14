/* Runtime allocation deltas after warming classes, with disposal checks.
 * This measures Exec free-memory changes, including allocator rounding;
 * it is not a private allocator estimate or an application's peak usage. */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#define MO(n,...) ({ULONG tags_[]={__VA_ARGS__};MUI_NewObjectA(n,(APTR)tags_);})
struct Library *MUIMasterBase;
ULONG __stack=131072;
static Object *objects[128];
static ULONG available(void)
{
    ULONG bytes;
    Forbid(); bytes=AvailMem(MEMF_ANY); Permit();
    return bytes;
}
static Object *create(const char *name)
{
    return MO(name,MUIA_Text_Contents,"Memory sample",MUIA_String_Contents,"Memory sample",MUIA_Process_AutoLaunch,FALSE,TAG_DONE);
}
int main(void)
{
    int failures=0;setvbuf(stdout,NULL,_IONBF,0);
    Forbid();
    BOOL loaded=FindName(&SysBase->LibList,"muimaster.library")!=NULL;
    Permit();
    ULONG before_open=available();
    MUIMasterBase=OpenLibrary("muimaster.library",0);
    ULONG after_open=available();
    if(!MUIMasterBase)return 20;
    printf("library=%u.%u already-loaded=%d open-held=%ld\n",
        MUIMasterBase->lib_Version,MUIMasterBase->lib_Revision,loaded,
        (LONG)(before_open-after_open));
    const char *classes[]={MUIC_Notify,MUIC_Rectangle,MUIC_Text,MUIC_String,
        MUIC_Group,MUIC_List,"Title.mui","Process.mui","Keyadjust.mui",
        "Filepanel.mui","Fontpanel.mui",NULL};
    for(int c=0;classes[c];c++)
    {
        struct IClass *cl=MUI_GetClass((STRPTR)classes[c]);
        if(!cl){printf("SKIP class=%s unavailable\n",classes[c]);continue;}
        int count=c>=9?4:128;
        for(int round=0;round<4;round++)
        {
            ULONG before=available();int n=0;
            for(;n<count;n++)if(!(objects[n]=create(classes[c])))break;
            ULONG held=available();
            for(int i=0;i<n;i++)MUI_DisposeObject(objects[i]);
            ULONG after=available();
            if(round)printf("class=%s round=%d n=%d own=%lu inherited=%u held=%ld retained=%ld\n",
                classes[c],round,n,(ULONG)cl->cl_InstSize,cl->cl_InstOffset,(LONG)(before-held),(LONG)(before-after));
            if(n!=count){puts("FAIL allocation");failures++;break;}
        }
        MUI_FreeClass(cl);
    }
    struct Screen *screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    Object *group=MO(MUIC_Group,MUIA_Group_PageMode,TRUE,TAG_DONE);
    for(int i=0;i<128;i++)DoMethod(group,OM_ADDMEMBER,create(MUIC_Text));
    Object *win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_RootObject,group,TAG_DONE);
    Object *app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    for(int round=0;round<4;round++)
    {
        ULONG before=available(),opened=0;
        SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
        GetAttr(MUIA_Window_Open,win,&opened);
        ULONG held=available();
        SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);
        ULONG after=available();
        if(round)printf("setup=text128 round=%d open=%lu held=%ld retained=%ld\n",
            round,opened,(LONG)(before-held),(LONG)(before-after));
        if(!opened)failures++;
    }
    MUI_DisposeObject(app);if(screen)CloseScreen(screen);CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures);return failures?20:0;
}

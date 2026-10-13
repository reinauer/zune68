/* Text ownership and shortening, built against the public OS3 SDK. */
#define NO_INLINE_STDARG
#include <stdio.h>
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
ULONG __stack = 65536;
static ULONG attr(Object *o, ULONG a) { ULONG v=0xdeadbeef; GetAttr(a,o,&v); return v; }
static int failures;
static void check(BOOL ok, const char *label)
{
    if (!ok) { printf("FAIL %s\n", label); failures++; }
}
static ULONG pixels(Object *obj)
{
    ULONG hash = 2166136261UL;
    for (LONG y = _mtop(obj); y <= _mbottom(obj); y++)
        for (LONG x = _mleft(obj); x <= _mright(obj); x++)
            hash = (hash ^ ReadPixel(_rp(obj), x, y)) * 16777619UL;
    return hash;
}
int main(int argc, char **argv)
{
    Object *texts[6], *expected[6], *group, *win, *app;
    char copied[] = "changed";
    char value[] = "abcdefghijklmnopqrstuv";
    struct Screen *screen=NULL;
    setvbuf(stdout,NULL,_IONBF,0);
    MUIMasterBase=OpenLibrary("muimaster.library",0);
    if (!MUIMasterBase) return 20;
    printf("library=%u.%u\n",MUIMasterBase->lib_Version,MUIMasterBase->lib_Revision);
    if(argc>1 && !strcmp(argv[1],"screen")) screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,2,TAG_DONE);
    static const char *samples[] = {"abcdefghij", "abcdefghij", "",
        "...pqrstuv", "abc...stuv", "abcdefg..."};
    group=MO(MUIC_Group, MUIA_Group_Columns, 2, TAG_DONE);
    for(int i=0;i<6;i++) {
        texts[i]=MO(MUIC_Text,MUIA_Text_Contents,value,MUIA_Text_Shorten,i,
            MUIA_Font,MUIV_Font_Fixed,MUIA_Text_SetMin,FALSE,MUIA_FixWidth,80,TAG_DONE);
        DoMethod(group,OM_ADDMEMBER,texts[i]);
        expected[i]=MO(MUIC_Text, MUIA_Text_Contents,samples[i],
            MUIA_Font,MUIV_Font_Fixed,MUIA_Text_SetMin,FALSE,
            MUIA_FixWidth,80,TAG_DONE);
        DoMethod(group,OM_ADDMEMBER,expected[i]);
    }
    Object *borrowed=MO(MUIC_Text,MUIA_Text_Copy,FALSE,MUIA_Text_Contents,value,TAG_DONE);
    printf("borrow=%d copy=%lu\n",attr(borrowed,MUIA_Text_Contents)==(ULONG)value,attr(borrowed,MUIA_Text_Copy));
    check(attr(borrowed,MUIA_Text_Contents)==(ULONG)value, "borrow contents");
    check(attr(borrowed,MUIA_Text_Copy)==FALSE, "copy getter");
    MUI_DisposeObject(borrowed);
    borrowed=MO(MUIC_Text,MUIA_Text_Copy,TRUE,MUIA_Text_Contents,copied,TAG_DONE);
    check(attr(borrowed,MUIA_Text_Contents)!=(ULONG)copied, "copy on creation");
    check(!strcmp((char *)attr(borrowed,MUIA_Text_Contents),"changed"), "copied contents");
    MUI_DisposeObject(borrowed);
    borrowed=MO(MUIC_Text,MUIA_Text_HiChar,'a',MUIA_Text_Contents,value,TAG_DONE);
    printf("hichar copy=%lu borrowed=%d\n",attr(borrowed,MUIA_Text_Copy),attr(borrowed,MUIA_Text_Contents)==(ULONG)value);
    MUI_DisposeObject(borrowed);
    win=MO(MUIC_Window,MUIA_Window_Screen,screen,MUIA_Window_Title,"Text probe",MUIA_Window_RootObject,group,TAG_DONE);
    app=MO(MUIC_Application,MUIA_Application_Window,win,TAG_DONE);
    SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
    for(int j=0;j<4;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
    /* Both fixtures use a fixed font, but their configured sizes differ.
     * Build the expected visible strings from the actual character width. */
    LONG capacity = 80 / _font(texts[0])->tf_XSize;
    for (int i=0;i<6;i++) {
        char sample[64] = {0};
        LONG prefix=0, suffix=0;
        if(i<2) prefix=capacity;
        else if(i==3) suffix=capacity-3;
        else if(i==4) {prefix=(capacity-3)/2;suffix=capacity-3-prefix;}
        else if(i==5) prefix=capacity-3;
        if(prefix) memcpy(sample,value,prefix);
        if(i>=3)strcpy(sample+prefix,"...");
        if(suffix)strcat(sample,value+strlen(value)-suffix);
        SetAttrs(expected[i],MUIA_Text_Contents,sample,TAG_DONE);
    }
    for(int j=0;j<4;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
    for(int i=0;i<6;i++) printf("mode=%d get=%lu short=%lu width=%d contents=%s\n",i,attr(texts[i],MUIA_Text_Shorten),attr(texts[i],MUIA_Text_Shortened),_mwidth(texts[i]),(char*)attr(texts[i],MUIA_Text_Contents));
    check(attr(win,MUIA_Window_Open), "window opens");
    for(int i=0;i<6;i++) {
        check(attr(texts[i],MUIA_Text_Shorten)==(ULONG)i,"shorten getter");
        check(attr(texts[i],MUIA_Text_Shortened)==(ULONG)(i!=0),"shortened getter");
        check(!strcmp((char *)attr(texts[i],MUIA_Text_Contents),value),"source unchanged");
        check(pixels(texts[i])==pixels(expected[i]),"shortened rendering");
        printf("pixels mode=%d equal=%d width=%d height=%d/%d hash=%08lx/%08lx\n",i,
            pixels(texts[i])==pixels(expected[i]),_mwidth(texts[i]),
            _mheight(texts[i]),_mheight(expected[i]),pixels(texts[i]),pixels(expected[i]));
    }
    /* Preserve parsed styles on both sides of the cut, without treating
     * escape bytes as printable characters. */
    SetAttrs(texts[5],MUIA_Text_Contents,"\033uabc\033Udefghijklmnopqrstuv",TAG_DONE);
    char styled[64]="\033uabc\033U";
    strncat(styled,value+3,capacity-6);
    strcat(styled,"...");
    SetAttrs(expected[5],MUIA_Text_Contents,styled,TAG_DONE);
    for(int j=0;j<4;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
    if(strstr((char *)MUIMasterBase->lib_IdString, "Zune68") != NULL)
        check(pixels(texts[5])==pixels(expected[5]),"elision preserves styled chunks");
    else if(pixels(texts[5])!=pixels(expected[5]))
        puts("REFERENCE DIFFERENCE: formatted elision differs from explicit text");
    DoMethod(group,MUIM_Group_InitChange);
    SetAttrs(texts[2],MUIA_Text_Contents,"abcdefghijklmnopqrstuv\nOK",TAG_DONE);
    SetAttrs(expected[2],MUIA_Text_Contents,"\n",TAG_DONE);
    DoMethod(group,MUIM_Group_ExitChange);
    for(int j=0;j<4;j++){DoMethod(app,MUIM_Application_InputBuffered);Delay(2);}
    check(pixels(texts[2])==pixels(expected[2]),"hide removes all lines");
    SetAttrs(texts[0],MUIA_Text_ControlChar,'a',TAG_DONE);
    check(attr(texts[0],MUIA_Text_ControlChar)=='a',"text control character");
    check(attr(texts[0],MUIA_ControlChar)==0,"underline does not change activation key");
    MUI_DisposeObject(app); if(screen) CloseScreen(screen); CloseLibrary(MUIMasterBase);
    printf("%s failures=%d\n",failures?"FAIL":"PASS",failures); return failures?20:0;
}

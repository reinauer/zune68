/* ccprio: which superclass MUI_CreateCustomClass() takes when a program
 * passes both a supername and a supermcc (a black-box probe; LumiZip 1.0
 * passes "Area.mui" with garbage in A2).
 *
 *   ccprio
 *
 * Makes a private Group subclass P, then a class C with supername
 * "Area.mui" and supermcc P, and prints which one C's mcc_Super is: "name"
 * (Area's class), "mcc" (P's class) or "other". Then deletes C and P (in
 * that order) and prints the results. Built by hand:
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -s -o ccprio tests/native/ccprio.c
 */
#include <exec/execbase.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <stdio.h>

struct Library *MUIMasterBase;

static ULONG disp(void)
{
    return 0;   /* never called: no objects are made */
}

int main(void)
{
    struct MUI_CustomClass *p, *c;
    struct IClass *area;
    int dp, dc;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
    {
        printf("no muimaster.library\n");
        return 20;
    }
    area = MUI_GetClass(MUIC_Area);
    p = MUI_CreateCustomClass(NULL, MUIC_Group, NULL, 4, (APTR)disp);
    c = p ? MUI_CreateCustomClass(NULL, MUIC_Area, p, 4, (APTR)disp) : NULL;
    printf("muimaster %d.%d p %s c %s\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision, p ? "made" : "failed", c ? "made" : "failed");
    if (c)
        printf("super: %s\n", c->mcc_Super == area ? "name"
            : c->mcc_Super == p->mcc_Class ? "mcc" : "other");
    dc = c ? MUI_DeleteCustomClass(c) : -1;
    dp = p ? MUI_DeleteCustomClass(p) : -1;
    printf("delete c %d p %d\n", dc, dp);
    MUI_FreeClass(area);
    CloseLibrary(MUIMasterBase);
    return 0;
}

/* Measure how much stack MUI uses for the MUI 3.8 Pages demo window: run
 * it on a pattern-filled 64 KB stack and report the high-water mark after
 * creation, opening, input, closing and disposal. The MUI 3.8 demos ask
 * for an 8 KB stack (__stack = 8192). Compare native and MUI 3.8.
 * With a file argument, also writes the stack left by opening the window,
 * whose return addresses show the deepest call chain. With a second file
 * argument, writes the stack's address there and waits 10 s for a
 * debugger.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os stackdepth.c \
 *        -o stackdepth -lmui -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define STK 65536
#define FILL 0xa5

struct Library *MUIMasterBase;
static UBYTE *stk;
static struct StackSwapStruct sss;
static Object *app, *win, *reg;
static ULONG marks[5];
static UBYTE residue[STK];
static ULONG residue_used;

static char *Sex[] = { "male", "female", NULL };
static char *Pages[] = { "Race", "Class", "Armor", "Level", NULL };
static char *Races[] = { "Human", "Elf", "Dwarf", "Hobbit", "Gnome", NULL };
static char *Classes[] = { "Warrior", "Rogue", "Bard", "Monk", "Magician",
    "Archmage", NULL };

static ULONG used(void)
{
    ULONG i;

    for (i = 0; i < STK && stk[i] == FILL; i++)
        ;
    return STK - i;
}

static Object *create(void)
{
    return ApplicationObject,
        MUIA_Application_Title, "Pages-Demo",
        MUIA_Application_Base, "STACKDEPTH",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "Character Definition",
            WindowContents, VGroup,
                Child, ColGroup(2),
                    Child, Label2("Name:"), Child, String("Frodo", 32),
                    Child, Label1("Sex:"), Child, Cycle(Sex),
                    End,
                Child, VSpace(2),
                Child, reg = RegisterGroup(Pages),
                    MUIA_Register_Frame, TRUE,
                    Child, HCenter(Radio(NULL, Races)),
                    Child, HCenter(Radio(NULL, Classes)),
                    Child, HGroup,
                        Child, HSpace(0),
                        Child, ColGroup(2),
                            Child, Label1("Cloak:"), Child, CheckMark(TRUE),
                            Child, Label1("Shield:"), Child, CheckMark(TRUE),
                            Child, Label1("Gloves:"), Child, CheckMark(TRUE),
                            Child, Label1("Helmet:"), Child, CheckMark(TRUE),
                            End,
                        Child, HSpace(0),
                        End,
                    Child, ColGroup(2),
                        Child, Label("Experience:"), Child, Slider(0, 100, 3),
                        Child, Label("Strength:"), Child, Slider(0, 100, 42),
                        Child, Label("Dexterity:"), Child, Slider(0, 100, 24),
                        Child, Label("Condition:"), Child, Slider(0, 100, 39),
                        Child, Label("Intelligence:"), Child, Slider(0, 100, 74),
                        End,
                    End,
                End,
            End,
        End;
}

static void __attribute__((noinline)) run(void)
{
    int i;

    app = create();
    marks[0] = used();
    if (!app)
        return;
    set(win, MUIA_Window_Open, TRUE);
    marks[1] = used();
    /* The deepest call chain of the open leaves its return addresses here. */
    residue_used = marks[1];
    CopyMem(stk, residue, STK);
    for (i = 0; i < 20; i++)
    {
        set(reg, MUIA_Group_ActivePage, i & 3);
        DoMethod(app, MUIM_Application_InputBuffered);
        Delay(1);
    }
    marks[2] = used();
    set(win, MUIA_Window_Open, FALSE);
    marks[3] = used();
    MUI_DisposeObject(app);
    marks[4] = used();
}

int main(int argc, char **argv)
{
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 11)))
        return 20;
    /* Chip memory keeps this stack apart from the system's stacks in fast
     * memory, so a debugger can stop when A7 falls below a depth here. */
    if (!(stk = AllocMem(STK, MEMF_CHIP)))
        return 20;
    memset(stk, FILL, STK);
    if (argc > 2)
    {
        /* argv[2]: a file for the stack's address; wait for a debugger */
        FILE *f = fopen(argv[2], "w");

        if (f)
        {
            fprintf(f, "stack %08lx %lu lvo30 %08lx\n", (ULONG)stk, (ULONG)STK,
                *(ULONG *)((UBYTE *)MUIMasterBase - 28));
            fclose(f);
        }
        Delay(10 * 50);
    }
    sss.stk_Lower = stk;
    sss.stk_Upper = (ULONG)(stk + STK);
    sss.stk_Pointer = stk + STK;
    StackSwap(&sss);
    run();
    StackSwap(&sss);
    printf("muimaster %ld.%ld app=%s\n", (long)MUIMasterBase->lib_Version,
        (long)MUIMasterBase->lib_Revision, app ? "ok" : "failed");
    printf("stack used: create %lu open %lu input %lu close %lu dispose %lu\n",
        marks[0], marks[1], marks[2], marks[3], marks[4]);
    if (argc > 1)
    {
        /* argv[1]: write the vector -30 target, then the used stack residue
         * (deepest first) as hex longwords, for offline symbolization. */
        FILE *f = fopen(argv[1], "w");
        ULONG i;

        if (f)
        {
            fprintf(f, "lvo30 %08lx\n", *(ULONG *)((UBYTE *)MUIMasterBase - 28));
            for (i = STK - residue_used; i + 4 <= STK; i += 2)
                fprintf(f, "%05lx %08lx\n", STK - i, *(ULONG *)(residue + i));
            fclose(f);
        }
    }
    FreeMem(stk, STK);
    CloseLibrary(MUIMasterBase);
    return 0;
}

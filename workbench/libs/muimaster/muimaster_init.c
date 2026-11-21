/* Copyright (C) 2002, The AROS Development Team. All rights reserved.
 * Native resource ownership: unwind partial initialization in reverse order.
 */
#include <proto/exec.h>
#include <proto/graphics.h>
#include <clib/alib_protos.h>
#include <graphics/text.h>
#include "muimaster_intern.h"
#include "penspec.h"

struct DosLibrary *DOSBase;
struct Library *UtilityBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *AslBase;
struct Library *LayersBase;
struct Library *CxBase;
struct RxsLib *RexxSysBase;
struct Library *GadToolsBase;
struct Library *KeymapBase;
struct Library *LocaleBase;
struct Library *DataTypesBase;
struct Library *IFFParseBase;
struct Library *DiskfontBase;
struct Library *IconBase;
struct Library *WorkbenchBase;
struct Library *MUIMasterBase, *MUIScreenBase, *CyberGfxBase;
extern struct Library *MathIeeeDoubBasBase, *MathIeeeDoubTransBase;
BOOL Locale_Initialize(void);
void MUIGfx_Install(struct Library *owner);
void MUIGfx_Remove(void);
void Locale_Deinitialize(void);
struct dependency {
    struct Library **base;
    CONST_STRPTR name;
    ULONG version;
};
static const struct dependency dependencies[] = {
    { (struct Library **)&DOSBase, "dos.library", 37 },
    { (struct Library **)&UtilityBase, "utility.library", 37 },
    { (struct Library **)&GfxBase, "graphics.library", 39 },
    { (struct Library **)&IntuitionBase, "intuition.library", 39 },
    { (struct Library **)&LayersBase, "layers.library", 37 },
    { (struct Library **)&GadToolsBase, "gadtools.library", 37 },
    { (struct Library **)&KeymapBase, "keymap.library", 37 },
    { (struct Library **)&LocaleBase, "locale.library", 38 },
};
/* Opened on first use (lazylib.h), in ZUNE_LIB_* order. On OS 3.1 all
 * but icon and workbench are loaded from disk. AmigaOS 2.04 to 3.1 ship
 * rexxsyslib.library 36. */
static const struct dependency lazy_dependencies[ZUNE_LIB_COUNT] = {
    { (struct Library **)&AslBase, "asl.library", 37 },
    { (struct Library **)&CxBase, "commodities.library", 37 },
    { (struct Library **)&RexxSysBase, "rexxsyslib.library", 36 },
    { (struct Library **)&DataTypesBase, "datatypes.library", 39 },
    { (struct Library **)&IFFParseBase, "iffparse.library", 37 },
    { (struct Library **)&DiskfontBase, "diskfont.library", 37 },
    { (struct Library **)&IconBase, "icon.library", 37 },
    { (struct Library **)&WorkbenchBase, "workbench.library", 37 },
    { &MathIeeeDoubBasBase, "mathieeedoubbas.library", 0 },
    { &MathIeeeDoubTransBase, "mathieeedoubtrans.library", 0 },
};

/* A library that failed once is not searched for again until the next
 * expunge: every caller would otherwise wait for ramlib to scan LIBS:. */
struct Library *zune_lazylib_open(unsigned which)
{
    const struct dependency *dep = &lazy_dependencies[which];
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct Library *lib = *dep->base;

    if (lib)
        return lib;
    ObtainSemaphore(&base->LazyLibSemaphore);
    if (!(lib = *dep->base) && !(base->LazyLibFailed & (1UL << which))) {
        if (which == ZUNE_LIB_MATHTRANS && !zune_lazylib_open(ZUNE_LIB_MATHBAS))
            lib = NULL;
        else
            lib = OpenLibrary(dep->name, dep->version);
        if (lib)
            *dep->base = lib;
        else
            base->LazyLibFailed |= 1UL << which;
    }
    ReleaseSemaphore(&base->LazyLibSemaphore);
    return lib;
}

void L_ExpungeLib(struct Library *library)
{
    struct MUIMasterBase_intern *base = MUIMB(library);
    unsigned i;
    MUIGfx_Remove();
    zune_identity_exit(base);
    zune_vector_free_templates();
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    CyberGfxBase = NULL;
    Locale_Deinitialize();
    if (base->topaz8font) CloseFont(base->topaz8font);
    if (base->defaultPens)
        FreeMem(base->defaultPens, MPEN_COUNT * sizeof(*base->defaultPens));
    if (base->SpecialMemory) FreeMem(base->SpecialMemory, 4);
    for (i = ZUNE_LIB_COUNT; i--; ) {
        if (*lazy_dependencies[i].base)
            CloseLibrary(*lazy_dependencies[i].base);
        *lazy_dependencies[i].base = NULL;
    }
    base->LazyLibFailed = 0;
    i = sizeof(dependencies) / sizeof(dependencies[0]);
    while (i--) {
        if (*dependencies[i].base) CloseLibrary(*dependencies[i].base);
        *dependencies[i].base = NULL;
    }
    MUIMasterBase = NULL;
}

ULONG L_InitLib(struct Library *library)
{
    struct MUIMasterBase_intern *base = MUIMB(library);
    struct TextAttr font = { "topaz.font", 8, FS_NORMAL, FPF_ROMFONT };
    /* Half pens are derived from the screen by Window setup; the mark
     * pen defaults to MUI 3.9's built-in colour. */
    static const signed char system_pens[MPEN_COUNT] = {
        SHINEPEN, -1, BACKGROUNDPEN, -1, SHADOWPEN,
        TEXTPEN, FILLPEN, -1
    };
    static const char mark_pen[] = "rffa997";
    unsigned i;
    MUIMasterBase = library;
    InitSemaphore(&base->ZuneSemaphore);
    InitSemaphore(&base->LazyLibSemaphore);
    NewList((struct List *)&base->BuiltinClasses);
    NewList((struct List *)&base->Applications);
    for (i = 0; i < sizeof(dependencies) / sizeof(dependencies[0]); ++i) {
        *dependencies[i].base = OpenLibrary(dependencies[i].name,
            dependencies[i].version);
        if (!*dependencies[i].base) goto fail;
    }
    base->dosbase = DOSBase;
    base->utilitybase = (struct UtilityBase *)UtilityBase;
    base->gfxbase = GfxBase;
    base->intuibase = IntuitionBase;
    /* Do not load a screen helper during LibInit: it can reopen this
       library before Exec has published it. Planar graphics need no RTG. */
    base->topaz8font = OpenFont(&font);
    if (!base->topaz8font) goto fail;
    base->SpecialMemory = AllocAbs(4, (APTR)MUIV_TriggerValue);
    base->defaultPens = AllocMem(MPEN_COUNT * sizeof(*base->defaultPens),
        MEMF_PUBLIC | MEMF_CLEAR);
    if (!base->defaultPens) goto fail;
    for (i = 0; i < MPEN_COUNT; ++i) {
        if (system_pens[i] >= 0) {
            base->defaultPens[i].buf[0] = PST_SYS;
            base->defaultPens[i].buf[1] = '0' + system_pens[i];
        }
    }
    for (i = 0; i < sizeof(mark_pen); ++i)
        base->defaultPens[MPEN_MARK].buf[i] = mark_pen[i];
    zune_identity_init(base);
    Locale_Initialize();
    MUIGfx_Install(library);
    return TRUE;
fail:
    L_ExpungeLib(library);
    return FALSE;
}

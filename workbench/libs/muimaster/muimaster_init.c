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
BOOL ZuneGccInit(void);
void ZuneGccCleanup(void);
BOOL Locale_Initialize(void);
void Locale_Deinitialize(void);
static const struct {
    struct Library **base;
    CONST_STRPTR name;
    ULONG version;
} dependencies[] = {
    { (struct Library **)&DOSBase, "dos.library", 37 },
    { (struct Library **)&UtilityBase, "utility.library", 37 },
    { (struct Library **)&GfxBase, "graphics.library", 39 },
    { (struct Library **)&IntuitionBase, "intuition.library", 39 },
    { (struct Library **)&AslBase, "asl.library", 37 },
    { (struct Library **)&LayersBase, "layers.library", 37 },
    { (struct Library **)&CxBase, "commodities.library", 37 },
    { (struct Library **)&RexxSysBase, "rexxsyslib.library", 37 },
    { (struct Library **)&GadToolsBase, "gadtools.library", 37 },
    { (struct Library **)&KeymapBase, "keymap.library", 37 },
    { (struct Library **)&LocaleBase, "locale.library", 38 },
    { (struct Library **)&DataTypesBase, "datatypes.library", 39 },
    { (struct Library **)&IFFParseBase, "iffparse.library", 37 },
    { (struct Library **)&DiskfontBase, "diskfont.library", 37 },
    { (struct Library **)&IconBase, "icon.library", 37 },
    { (struct Library **)&WorkbenchBase, "workbench.library", 37 },
};

void L_ExpungeLib(struct Library *library)
{
    struct MUIMasterBase_intern *base = MUIMB(library);
    unsigned i = sizeof(dependencies) / sizeof(dependencies[0]);
    Locale_Deinitialize();
    ZuneGccCleanup();
    if (base->topaz8font) CloseFont(base->topaz8font);
    if (base->defaultPens)
        FreeMem(base->defaultPens, MPEN_COUNT * sizeof(*base->defaultPens));
    if (base->SpecialMemory) FreeMem(base->SpecialMemory, 4);
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
    /* Half pens are derived from the screen by Window setup. */
    static const signed char system_pens[MPEN_COUNT] = {
        SHINEPEN, -1, BACKGROUNDPEN, -1, SHADOWPEN,
        TEXTPEN, FILLPEN, HIGHLIGHTTEXTPEN
    };
    unsigned i;
    MUIMasterBase = library;
    InitSemaphore(&base->ZuneSemaphore);
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
    if (!ZuneGccInit()) goto fail;
    Locale_Initialize();
    return TRUE;
fail:
    L_ExpungeLib(library);
    return FALSE;
}

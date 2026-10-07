/* Copyright (C) 2003, The AROS Development Team. All rights reserved.
 * GCC m68k definitions for the AROS source interfaces.
 */
#ifndef _MUIMASTER_SUPPORT_AMIGAOS_H_
#define _MUIMASTER_SUPPORT_AMIGAOS_H_
#include <stddef.h>
#include <exec/types.h>
#include <intuition/classes.h>
#include <proto/utility.h>

/* Library gate bodies use C arguments. Hooks and dispatchers use registers. */
#define ASM
#define REG(reg, arg) arg __asm(#reg)
#define SAVEDS
#define STDARGS __stdargs
#define VARARGS68K
#define MUI_LIB_ENTRY
#define MUI_LIB_ARG(reg, arg) arg
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#define __stackparm
#define STACKED
#define AROS_STACKSIZE 65536
#define AROS_BIG_ENDIAN 1
#define AROS_LONG2BE(x) (x)
#define AROS_BE2LONG(x) (x)
#define BNULL 0
#define RAWARG APTR
#define IMSPEC_EXTERNAL_PREFIX "MUI:Images/"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef ULONG IPTR;
typedef LONG SIPTR;
typedef ULONG STACKULONG;
typedef LONG STACKLONG;
typedef unsigned long long UQUAD;
typedef void (*VOID_FUNC)(void);

#define ZUNE_BUILTIN_ABOUTMUI 1
#define ZUNE_BUILTIN_BALANCE 1
#define ZUNE_BUILTIN_BOOPSI 1
#define ZUNE_BUILTIN_COLORADJUST 1
#define ZUNE_BUILTIN_COLORFIELD 1
#define ZUNE_BUILTIN_CRAWLING 1
#define ZUNE_BUILTIN_FLOATTEXT 1
#define ZUNE_BUILTIN_DIRLIST 1
#define ZUNE_BUILTIN_DTPIC 1
#define ZUNE_BUILTIN_FRAMEADJUST 1
#define ZUNE_BUILTIN_FRAMEDISPLAY 1
#define ZUNE_BUILTIN_GAUGE 1
#define ZUNE_BUILTIN_ICONLISTVIEW 0
#define ZUNE_BUILTIN_IMAGEADJUST 1
#define ZUNE_BUILTIN_IMAGEDISPLAY 1
#define ZUNE_BUILTIN_KNOB 1
#define ZUNE_BUILTIN_LEVELMETER 1
#define ZUNE_BUILTIN_NUMERICBUTTON 1
#define ZUNE_BUILTIN_PALETTE 1
#define ZUNE_BUILTIN_PENADJUST 1
#define ZUNE_BUILTIN_PENDISPLAY 1
#define ZUNE_BUILTIN_POPASL 1
#define ZUNE_BUILTIN_POPFRAME 1
#define ZUNE_BUILTIN_POPIMAGE 1
#define ZUNE_BUILTIN_POPLIST 1
#define ZUNE_BUILTIN_POPPEN 1
#define ZUNE_BUILTIN_POPSCREEN 1
#define ZUNE_BUILTIN_RADIO 1
#define ZUNE_BUILTIN_SCALE 1
#define ZUNE_BUILTIN_SCROLLGROUP 1
#define ZUNE_BUILTIN_SETTINGS 1
#define ZUNE_BUILTIN_SETTINGSGROUP 1
#define ZUNE_BUILTIN_VIRTGROUP 1
#define ZUNE_BUILTIN_VOLUMELIST 1
#define ZUNE_BUILTIN_PANEL 0
#define ZUNE_BUILTIN_PANELGROUP 0
#define ZUNE_BUILTIN_DRAGHANDLE 0
#define ZUNE_BUILTIN_PANELTITLE 0

LONG HexToIPTR(CONST_STRPTR str, IPTR *value);
LONG HexToLong(CONST_STRPTR str, ULONG *value);
char *StrDup(const char *str);
int stricmp(const char *left, const char *right);
size_t strlcat(char *dst, const char *src, size_t size);
Object *DoSuperNewTagList(Class *cl, Object *obj, void *unused,
    struct TagItem *tags);
Object *DoSuperNewTags(Class *cl, Object *obj, void *unused, ...);
APTR AllocVecPooled(APTR pool, ULONG size);
void FreeVecPooled(APTR pool, APTR memory);
struct RastPort *ZuneCloneRastPort(const struct RastPort *rp);
#define DeinitRastPort(rp) ((void)0)
#define CloneRastPort(rp) ZuneCloneRastPort(rp)
#define FreeRastPort(rp) FreeVec(rp)
#define EXEC_INTERFACE_DECLARE(x)
#define EXEC_INTERFACE_GET_MAIN(interface, base) 1
#define EXEC_INTERFACE_DROP(interface)
#define EXEC_INTERFACE_ASSIGN(a,b)

#define NEWLIST(list) NewList((struct List *)(list))
#undef IsListEmpty
#define IsListEmpty(list) (!((struct List *)(list))->lh_Head->ln_Succ)
#define AROS_BSTR_ADDR(value) ((STRPTR)BADDR(value) + 1)
#define TAGLIST(...) ((struct TagItem *)(IPTR[]){ __VA_ARGS__, TAG_DONE })
#define ForeachNode(list, node) \
    for ((node) = (void *)((struct List *)(list))->lh_Head; \
        ((struct Node *)(node))->ln_Succ; \
        (node) = (void *)((struct Node *)(node))->ln_Succ)
#define ForeachNodeSafe(list, node, next) \
    for ((node) = (void *)((struct List *)(list))->lh_Head; \
        ((struct Node *)(node))->ln_Succ && \
        ((next) = (void *)((struct Node *)(node))->ln_Succ, 1); \
        (node) = (next))

#define AROS_LIBFUNC_INIT
#define AROS_LIBFUNC_EXIT
#define AROS_USERFUNC_INIT
#define AROS_USERFUNC_EXIT
#define AROS_ASMSYMNAME(name) name
#define AROS_LIBBASE_EXT_DECL(type, name) extern type name;
#define AROS_LHA(type, name, reg) type name
#define AROS_UFHA(type, name, reg) type name __asm(ZUNE_REG_##reg)
#define AROS_UFPA AROS_UFHA
#define ZUNE_REG_A0 "a0"
#define ZUNE_REG_A1 "a1"
#define ZUNE_REG_A2 "a2"
#define ZUNE_REG_A3 "a3"
#define ZUNE_REG_A4 "a4"
#define ZUNE_REG_A5 "a5"
#define ZUNE_REG_A6 "a6"
#define ZUNE_REG_A7 "a7"
#define ZUNE_REG_D0 "d0"
#define ZUNE_REG_D1 "d1"
#define ZUNE_REG_D2 "d2"
#define ZUNE_REG_D3 "d3"
#define ZUNE_REG_D4 "d4"
#define ZUNE_REG_D5 "d5"
#define ZUNE_REG_D6 "d6"
#define ZUNE_REG_D7 "d7"
#define AROS_LH0(ret, name, base_type, base_name, lvo, lib) ret name(void)
#define AROS_UFH0(ret, name) ret name(void)
#define AROS_UFH0S(ret, name) static ret name(void)
#define AROS_UFP0 AROS_UFH0
#define AROS_LH1(ret, name, a1, base_type, base_name, lvo, lib) ret name(a1)
#define AROS_UFH1(ret, name, a1) ret name(a1)
#define AROS_UFH1S(ret, name, a1) static ret name(a1)
#define AROS_UFP1 AROS_UFH1
#define AROS_LH2(ret, name, a1, a2, base_type, base_name, lvo, lib) ret name(a1, a2)
#define AROS_UFH2(ret, name, a1, a2) ret name(a1, a2)
#define AROS_UFH2S(ret, name, a1, a2) static ret name(a1, a2)
#define AROS_UFP2 AROS_UFH2
#define AROS_LH3(ret, name, a1, a2, a3, base_type, base_name, lvo, lib) ret name(a1, a2, a3)
#define AROS_UFH3(ret, name, a1, a2, a3) ret name(a1, a2, a3)
#define AROS_UFH3S(ret, name, a1, a2, a3) static ret name(a1, a2, a3)
#define AROS_UFP3 AROS_UFH3
#define AROS_LH4(ret, name, a1, a2, a3, a4, base_type, base_name, lvo, lib) ret name(a1, a2, a3, a4)
#define AROS_UFH4(ret, name, a1, a2, a3, a4) ret name(a1, a2, a3, a4)
#define AROS_UFH4S(ret, name, a1, a2, a3, a4) static ret name(a1, a2, a3, a4)
#define AROS_UFP4 AROS_UFH4
#define AROS_LH5(ret, name, a1, a2, a3, a4, a5, base_type, base_name, lvo, lib) ret name(a1, a2, a3, a4, a5)
#define AROS_UFH5(ret, name, a1, a2, a3, a4, a5) ret name(a1, a2, a3, a4, a5)
#define AROS_UFH5S(ret, name, a1, a2, a3, a4, a5) static ret name(a1, a2, a3, a4, a5)
#define AROS_UFP5 AROS_UFH5
#define AROS_LH6(ret, name, a1, a2, a3, a4, a5, a6, base_type, base_name, lvo, lib) ret name(a1, a2, a3, a4, a5, a6)
#define AROS_UFH6(ret, name, a1, a2, a3, a4, a5, a6) ret name(a1, a2, a3, a4, a5, a6)
#define AROS_UFH6S(ret, name, a1, a2, a3, a4, a5, a6) static ret name(a1, a2, a3, a4, a5, a6)
#define AROS_UFP6 AROS_UFH6
#define AROS_LH7(ret, name, a1, a2, a3, a4, a5, a6, a7, base_type, base_name, lvo, lib) ret name(a1, a2, a3, a4, a5, a6, a7)
#define AROS_UFH7(ret, name, a1, a2, a3, a4, a5, a6, a7) ret name(a1, a2, a3, a4, a5, a6, a7)
#define AROS_UFH7S(ret, name, a1, a2, a3, a4, a5, a6, a7) static ret name(a1, a2, a3, a4, a5, a6, a7)
#define AROS_UFP7 AROS_UFH7
#define AROS_LH8(ret, name, a1, a2, a3, a4, a5, a6, a7, a8, base_type, base_name, lvo, lib) ret name(a1, a2, a3, a4, a5, a6, a7, a8)
#define AROS_UFH8(ret, name, a1, a2, a3, a4, a5, a6, a7, a8) ret name(a1, a2, a3, a4, a5, a6, a7, a8)
#define AROS_UFH8S(ret, name, a1, a2, a3, a4, a5, a6, a7, a8) static ret name(a1, a2, a3, a4, a5, a6, a7, a8)
#define AROS_UFP8 AROS_UFH8

#define BOOPSI_DISPATCHER_PROTO(ret, name, cl, obj, msg) \
    ret name(Class *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
#define BOOPSI_DISPATCHER(ret, name, cl, obj, msg) \
    BOOPSI_DISPATCHER_PROTO(ret, name, cl, obj, msg) {
#define BOOPSI_DISPATCHER_END }
#endif

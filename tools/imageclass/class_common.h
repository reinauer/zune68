/* Native environment of the AROS classes built from their .conf files. */
#ifndef ZUNE68_CLASS_COMMON_H
#define ZUNE68_CLASS_COMMON_H
#include <libraries/mui.h>
#include <mcc_common.h>
#include <SDI_hook.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <stdio.h>
#include "../plugin/runtime.h"
#define DEBUG 0
#define D(...)
#define E(...) ((void)0)
#define LC_LIBDEFS_FILE "class_libdefs.h"
#define __sprintf sprintf
#define CALLHOOKPKT(hook, object, message) CallHookPkt(hook, object, message)
Object *DoSuperNewTags(struct IClass *cl, Object *obj, void *unused, ...);
ULONG __stdargs SetSuperAttrs(struct IClass *cl, Object *obj, ULONG tag1, ...);
#include "shellstart.c"
#endif

/* Native build: the version of the system stands in for ArosInquire. */
#ifndef ZUNE68_PROTO_AROS_H
#define ZUNE68_PROTO_AROS_H
#include <exec/execbase.h>
#include <proto/exec.h>
#define AI_ArosVersion 0
#define ArosInquire(tag, store, ...) (*(int *)(store) = SysBase->LibNode.lib_Version)
#endif

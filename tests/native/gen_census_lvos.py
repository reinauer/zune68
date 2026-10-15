#!/usr/bin/env python3
"""Generate perfcensus_lvos.h from the NDK inline headers of m68k-amigaos-gcc.

usage: gen_census_lvos.py /opt/amiga-13.4/m68k-amigaos/ndk-include/inline > perfcensus_lvos.h

Interrupt-level, scheduler, stack-switching and self-used exec functions
are left out because the census wraps the listed functions with SetFunction.
"""
import re
import sys

LIBS = ["graphics", "layers", "intuition", "exec", "utility", "dos",
        "diskfont", "keymap", "commodities"]
EXCLUDE = {
    "exec": set("""Supervisor ExitIntr Schedule Reschedule Switch Dispatch Exception
        SetSR SuperState UserState SetIntVector AddIntServer RemIntServer Cause Alert
        Debug Disable Enable Forbid Permit Signal PutMsg ReplyMsg GetMsg AddHead AddTail
        Remove RemHead RemTail Insert Enqueue SetFunction CacheClearU CacheClearE
        CacheControl StackSwap NewStackSwap RemTask AddTask InitCode InitStruct
        MakeLibrary MakeFunctions SumKickData SumLibrary ColdReboot RawIOInit
        RawMayGetChar RawPutChar Vacate Procure SetExcept SetSignal GetCC AllocAbs
        Allocate Deallocate AddMemList ObtainQuickVector CachePreDMA CachePostDMA
        AddResource RemResource AddDevice RemDevice AddLibrary RemLibrary SetTaskPri
        NewMinList NewGetTaskAttrsA NewSetTaskAttrsA ExecReserved""".split()),
    "dos": set("Exit Execute RunCommand StackSwap".split()),
}


def main():
    inline = sys.argv[1]
    print("/* Generated from the NDK inline headers of m68k-amigaos-gcc 13.4 by\n"
          " * tests/native/gen_census_lvos.py; interrupt-level, scheduler and\n"
          " * stack-switching exec functions are left out. */")
    print("static struct CensusFn census_fns[] = {")
    for lib in LIBS:
        text = open(f"{inline}/{lib}.h").read()
        seen = set()
        for m in re.finditer(r"#define __(\w+)_base\(.*?jsr %/a6@\((-?\d+):W\)", text, re.S):
            name, lvo = m.group(1), int(m.group(2))
            if name in EXCLUDE.get(lib, set()) or name in seen:
                continue
            seen.add(name)
            print(f'    {{ "{lib}.library", "{name}", {lvo} }},')
    print("};")


if __name__ == "__main__":
    main()

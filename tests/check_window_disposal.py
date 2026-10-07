#!/usr/bin/env python3
"""Run linked Window disposal/close code on a 68000 with modelled OS calls.

This checks teardown ordering, not an Intuition GUI session.
"""
import sys
from pathlib import Path
from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from amitools.vamos.machine.machine import Machine

BASE, STACK = 0x10000, 0xf0000
CLASS, OBJECT, ROOT, MESSAGE = 0x70000, 0x71000, 0x72000, 0x73000
EXEC, INTUITION, WINDOW = 0x74000, 0x75000, 0x76000
PORT, FIRST, SECOND = 0x77000, 0x78000, 0x78100
# Classic m68k layout, checked with offsetof against window.c and the NDK.
INSTANCE = OBJECT + 64
FLAGS, CHILD, IWINDOW = INSTANCE + 526, INSTANCE + 522, INSTANCE + 16
ROOT_FLAGS, USERPORT = ROOT + 64, WINDOW + 86
SETUP, DRAWABLE = 1 << 28, 1 << 14
CLEANUP, WINDOW_CLEANUP, SNAPSHOT = 0x8042d985, 0x8042ab26, 0x8042945e


def check(path):
    image = BinFmt().load_image(str(path))
    reloc = Relocate(image)
    data = bytes(reloc.relocate_one_block(BASE))
    symbols = {s.name.decode(): address + s.offset
               for seg, address in zip(image.get_segments(), reloc.get_seq_addrs(BASE))
               if seg.symtab for s in seg.symtab.symbols}
    for mode in ['open', 'shared port', 'closed', 'partial setup', 'no root']:
        machine = Machine()
        mem, cpu = machine.mem, machine.cpu
        events = []

        def argument(n):
            return mem.r32(cpu.r_sp() + 4 + n * 4)

        def trap(address, callback):
            def invoke(opcode, pc):
                callback()
            number = machine.traps.alloc(invoke)
            mem.w16(address, 0xa000 | number)
            mem.w16(address + 2, 0x4e75)

        def hide():
            assert argument(0) == ROOT
            events.append('hide')
            mem.w32(ROOT_FLAGS, mem.r32(ROOT_FLAGS) & ~DRAWABLE)

        def method(obj, method_id):
            if obj == ROOT and method_id == CLEANUP:
                events.append('cleanup root')
                mem.w32(ROOT_FLAGS, mem.r32(ROOT_FLAGS) & ~SETUP)
            elif obj == OBJECT and method_id == WINDOW_CLEANUP:
                events.append('cleanup render')
            else:
                assert obj == OBJECT and method_id == SNAPSHOT, (obj, method_id)
            cpu.w_reg(0, 1)

        def dispose():
            assert argument(0) == ROOT
            assert mem.r32(IWINDOW) == 0, 'Intuition window outlives its objects'
            assert mem.r32(ROOT_FLAGS) & (SETUP | DRAWABLE) == 0
            events.append('dispose root')

        def close():
            assert cpu.r_reg(8) == WINDOW
            assert mem.r32(USERPORT) == 0, 'detach shared port before CloseWindow'
            assert 'dispose root' not in events
            events.append('close window')

        def remove():
            node = cpu.r_reg(9)
            assert node == FIRST
            succ, pred = mem.r32(node), mem.r32(node + 4)
            mem.w32(pred, succ)
            mem.w32(succ + 4, pred)

        def reply():
            assert cpu.r_reg(9) == FIRST
            events.append('reply window message')

        def free_vec():
            assert cpu.r_reg(9) == 0, 'no explicit cycle chain in this window'

        def superclass():
            assert [argument(i) for i in range(3)] == [CLASS, OBJECT, MESSAGE]
            events.append('super dispose')
            cpu.w_reg(0, 42)

        try:
            mem.w_block(BASE, data)
            mem.w32(symbols['_SysBase'], EXEC)
            mem.w32(symbols['_IntuitionBase'], INTUITION)
            mem.w16(CLASS + 32, 64)
            mem.w32(CHILD, 0 if mode == 'no root' else ROOT)
            opened = mode in ['open', 'shared port']
            mem.w32(FLAGS, int(opened))
            mem.w32(IWINDOW, WINDOW if opened else 0)
            mem.w32(ROOT_FLAGS, SETUP | DRAWABLE if opened or mode == 'partial setup' else 0)
            if mode == 'shared port':
                mem.w32(USERPORT, PORT)
                # Leave another window's queued message on the shared port.
                mem.w32(PORT + 20, FIRST)
                mem.w32(PORT + 24, 0)
                mem.w32(PORT + 28, SECOND)
                for node, succ, pred, win in [(FIRST, SECOND, PORT + 20, WINDOW),
                                              (SECOND, PORT + 24, FIRST, WINDOW + 512)]:
                    mem.w32(node, succ)
                    mem.w32(node + 4, pred)
                    mem.w32(node + 44, win)
            trap(symbols['_DoHideMethod'], hide)
            trap(symbols['_zune_imspec_hide'], lambda: None)
            trap(symbols['_DoMethod'], lambda: method(argument(0), argument(1)))
            trap(symbols['_DoMethodA'], lambda: method(argument(0), mem.r32(argument(1))))
            trap(symbols['_MUI_DisposeObject'], dispose)
            trap(symbols['_DoSuperMethodA'], superclass)
            trap(INTUITION - 54, lambda: None)  # ClearMenuStrip
            trap(INTUITION - 72, close)
            trap(INTUITION - 150, lambda: None)  # ModifyIDCMP
            trap(EXEC - 132, lambda: None)  # Forbid
            trap(EXEC - 138, lambda: None)  # Permit
            trap(EXEC - 252, remove)
            trap(EXEC - 378, reply)
            trap(EXEC - 690, free_vec)
            if '_ZuneTraceOutput' in symbols:
                trap(symbols['_ZuneTraceOutput'], lambda: None)
            for i, value in enumerate([CLASS, OBJECT, MESSAGE]):
                mem.w32(STACK + i * 4, value)
            machine.prepare(symbols['_Window__OM_DISPOSE'], STACK)
            result = machine.execute(100000)
            assert machine.was_exit(result), (mode, 'disposal did not return')
            assert cpu.r_sp() == STACK and cpu.r_reg(0) == 42
            expected = []
            if opened:
                expected = ['hide']
                if mode == 'shared port':
                    expected += ['reply window message']
                    assert mem.r32(PORT + 20) == SECOND
                    assert mem.r32(SECOND + 4) == PORT + 20
                expected += ['close window', 'cleanup root', 'cleanup render']
            elif mode == 'partial setup':
                expected = ['hide', 'cleanup root']
            if mode != 'no root':
                expected += ['dispose root']
            assert events == expected + ['super dispose'], (mode, events)
            assert mem.r32(CHILD) == 0 and mem.r32(FLAGS) & 1 == 0
        finally:
            machine.cleanup()
    print(f'{path.name}: window disposal closes once, drains own messages, '
          'preserves other messages and cleans up before freeing objects OK')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

#!/usr/bin/env python3
"""Check GCC Hunk metadata and execute register gates on an emulated 68000.

Requires amitools and machine68k. This does not run AmigaOS or the GUI.
"""
import re
import struct
import sys
from pathlib import Path

from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from amitools.vamos.machine.machine import Machine

ROOT = Path(__file__).resolve().parents[1]
FD = ROOT / 'workbench/libs/muimaster/include/fd/muimaster_lib.fd'
ENTRIES = re.findall(r'^(MUI_\w+)\([^\n]*\)\(([^)]*)\)$',
                     FD.read_text(), re.MULTILINE)
assert len(ENTRIES) == 29, 'expected 25 public and four private FD entries'
BASE, STACK, STUB, RESULT = 0x10000, 0xf0000, 0x70000, 0x71000


def check(path):
    image = BinFmt().load_image(str(path))
    reloc = Relocate(image)
    addresses = reloc.get_seq_addrs(BASE)
    data = bytes(reloc.relocate_one_block(BASE))
    symbols = {}
    for segment, address in zip(image.get_segments(), addresses):
        if segment.symtab:
            symbols.update((s.name.decode(), address + s.offset)
                           for s in segment.symtab.symbols)

    def long(address):
        return struct.unpack_from('>I', data, address - BASE)[0]

    resident = symbols['_RomTag']
    assert data[:4] == bytes.fromhex('70ff4e75'), 'CLI entry must return -1'
    assert data[resident - BASE:resident - BASE + 2] == b'\x4a\xfc'
    assert long(resident + 2) == resident
    assert long(resident + 6) >= resident + 26, 'Resident scan must advance'
    assert data[resident - BASE + 10] & 0x80, 'RTF_AUTOINIT'
    name = data[long(resident + 14) - BASE:].split(b'\0', 1)[0]
    assert name.decode() == path.name
    table = long(resident + 22)
    assert table == symbols['_LibInitTable']
    assert long(table + 12) == symbols['_LibInit']
    vectors = long(table + 4)
    expected = ['_LibOpen', '_LibClose', '_LibExpunge', '_LibReserved']
    expected += [('_' if name.startswith('MUI_Priv') else '_Gate_') + name
                 for name, _ in ENTRIES]
    for i, name in enumerate(expected):
        assert long(vectors + i * 4) == symbols[name], name
    assert long(vectors + len(expected) * 4) == 0xffffffff

    machine = Machine()
    mem, cpu = machine.mem, machine.cpu

    def run(address):
        machine.prepare(address, STACK)
        result = machine.execute(10000)
        assert machine.was_exit(result), '68000 execution did not return'
        assert cpu.r_sp() == STACK, 'unbalanced stack'

    try:
        mem.w_block(BASE, data)
        run(BASE)
        assert cpu.r_reg(0) == 0xffffffff
        for name, regs in ENTRIES:
            if name.startswith('MUI_Priv'):
                continue
            mem.w_block(BASE, data)
            values = [0x1122ffe0 + i for i in range(15)]
            for i, value in enumerate(values):
                cpu.w_reg(i, value)
            regs = regs.split(',') if regs else []
            want = [values[int(r[1:]) + (8 if r[0] == 'a' else 0)]
                    for r in regs]
            if name == 'MUI_AddClipping':
                want[1:] = [v | 0xffff0000 for v in want[1:]]
            # Replace only the implementation body with a stack-argument
            # recorder. Execute the real, linked register-to-stack gate.
            stub = b''.join(struct.pack('>HHI', 0x23ef, 4 + i * 4,
                                        RESULT + i * 4)
                            for i in range(len(regs)))
            mem.w_block(STUB, stub + bytes.fromhex('702a4e75'))
            mem.w_block(symbols['_' + name], struct.pack('>HI', 0x4ef9, STUB))
            run(symbols['_Gate_' + name])
            assert [mem.r32(RESULT + i * 4) for i in range(len(regs))] == want, name
            assert cpu.r_reg(0) == 42, name
            for i in [*range(2, 8), *range(10, 15)]:
                assert cpu.r_reg(i) == values[i], (name, 'preserved register', i)

        # External class dispatch must supply the class base in A6 and
        # restore the caller's A6 after the custom dispatcher returns.
        mem.w_block(BASE, data)
        cl, obj, msg, base = 0x72000, 0x73000, 0x74000, 0x75000
        mem.w32(cl + 12, STUB)  # Hook.h_SubEntry
        mem.w32(cl + 16, base)  # Hook.h_Data
        stub = b''.join(struct.pack('>HI', 0x23c0 + reg, RESULT + i * 4)
                        for i, reg in enumerate([8, 10, 9, 14]))
        mem.w_block(STUB, stub + bytes.fromhex('702a4e75'))
        for reg, value in [(8, cl), (10, obj), (9, msg), (14, 0x76000)]:
            cpu.w_reg(reg, value)
        run(symbols['_metaDispatcher'])
        assert [mem.r32(RESULT + i * 4) for i in range(4)] == [cl, obj, msg, base]
        assert cpu.r_reg(14) == 0x76000
        assert cpu.r_reg(0) == 42

        # Exercise the real iterator body, including Intuition's tail sentinel.
        # Each list node is the 12-byte _Object header preceding an Object.
        mem.w_block(BASE, data)
        state, first, second, tail = 0x72000, 0x73000, 0x74000, 0x75000
        mem.w32(first, second)
        mem.w32(second, tail)
        mem.w32(tail, 0)
        mem.w32(state, first)
        mem.w32(STACK, state)  # First argument of the stack-ABI helper.
        for obj, successor in [(first + 12, second), (second + 12, tail),
                               (0, tail), (0, tail)]:
            run(symbols['_ZuneNextObject'])
            assert cpu.r_reg(0) == obj, 'NextObject result'
            assert mem.r32(state) == successor, 'NextObject iterator'
        mem.w32(state, 0)
        run(symbols['_ZuneNextObject'])
        assert cpu.r_reg(0) == 0, 'NextObject null iterator'
        mem.w32(STACK, 0)
        run(symbols['_ZuneNextObject'])
        assert cpu.r_reg(0) == 0, 'NextObject null storage'

        # SetSuperAttrs must dispatch OM_SET to cl->cl_Super, not to obj.
        # The toolchain's alib helper used sp@(24) instead of a0@(24).
        mem.w_block(BASE, data)
        cl, supercl, obj = 0x72000, 0x72100, 0x73000
        mem.w32(cl + 24, supercl)
        mem.w32(supercl + 8, STUB)
        # Keep the broken object-data dispatch deterministic for this check.
        mem.w32(obj + 8, STUB)
        stub = b''.join(struct.pack('>HI', 0x23c0 + reg, RESULT + i * 4)
                        for i, reg in enumerate([8, 10, 9]))
        mem.w_block(STUB, stub + bytes.fromhex('702a4e75'))
        args = [cl, obj, 0x8042d0cd, 7, 0x80421654, 3, 0]
        for i, value in enumerate(args):
            mem.w32(STACK + i * 4, value)
        run(symbols['_SetSuperAttrs'])
        assert mem.r32(RESULT) == supercl, 'SetSuperAttrs superclass'
        assert mem.r32(RESULT + 4) == obj, 'SetSuperAttrs object'
        msg = mem.r32(RESULT + 8)
        assert mem.r32(msg) == 0x103, 'SetSuperAttrs OM_SET'
        assert mem.r32(msg + 4) == STACK + 8, 'SetSuperAttrs tag list'
        assert mem.r32(msg + 8) == 0, 'SetSuperAttrs GadgetInfo'
        assert cpu.r_reg(0) == 42, 'SetSuperAttrs return value'
    finally:
        machine.cleanup()
    print(f'{path.name}: Resident, 33 vectors, 25 gates, CLI, MCC ABI and '
          f'object iterator and superclass attributes OK; '
          f'{path.stat().st_size} file bytes, {len(data)} segment bytes')


if __name__ == '__main__':
    paths = sys.argv[1:] or [ROOT / 'build/muimaster' / name for name in
                            ['zunemaster.library', 'muimaster.library']]
    for path in paths:
        check(Path(path))

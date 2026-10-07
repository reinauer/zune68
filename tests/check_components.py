#!/usr/bin/env python3
"""Check native component metadata and execute small ABI/runtime probes.

No AmigaOS GUI or full MCC initialization is executed.
"""
import struct
import sys
from pathlib import Path
from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from amitools.vamos.machine.machine import Machine

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from components import PLUGINS, EXAMPLES, catalog_paths
from check_opentest_stack import check as check_stack

BASE, STACK = 0x10000, 0xf0000
BUFFER, FORMAT, STRING = 0x70000, 0x71000, 0x72000


def plugin(path):
    image = BinFmt().load_image(str(path))
    reloc = Relocate(image)
    data = bytes(reloc.relocate_one_block(BASE))
    symbols = {s.name.decode(): address + s.offset
               for seg, address in zip(image.get_segments(), reloc.get_seq_addrs(BASE))
               if seg.symtab for s in seg.symtab.symbols}
    # Executable stdio and exit machinery must not enter shared libraries.
    assert not {'_exit', '___initstdio', '___INIT_LIST__'} & symbols.keys(), path
    machine = Machine()
    mem, cpu = machine.mem, machine.cpu

    def run(address, *args):
        for i, value in enumerate(args):
            mem.w32(STACK + i * 4, value & 0xffffffff)
        machine.prepare(address, STACK)
        result = machine.execute(100000)
        assert machine.was_exit(result), path
        assert cpu.r_sp() == STACK
        return cpu.r_reg(0)

    try:
        mem.w_block(BASE, data)
        # libnix's 68000 division helpers use the library's utility base.
        if '___UtilityBase' in symbols:
            utility = 0x78000
            mem.w32(symbols['___UtilityBase'], utility)
            mem.w32(symbols['_UtilityBase'], utility)
            def divide(opcode, pc):
                dividend, divisor = cpu.r_reg(0), cpu.r_reg(1)
                cpu.w_reg(0, dividend // divisor)
                cpu.w_reg(1, dividend % divisor)
            trap = machine.traps.alloc(divide)
            mem.w16(utility - 156, 0xa000 | trap)
            mem.w16(utility - 154, 0x4e75)
        if '_SysBase' in symbols:
            execbase = 0x79000
            mem.w32(symbols['_SysBase'], execbase)
            def copy_memory(opcode, pc):
                mem.w_block(cpu.r_reg(9), mem.r_block(cpu.r_reg(8), cpu.r_reg(0)))
            trap = machine.traps.alloc(copy_memory)
            for offset in [624, 630]:
                mem.w16(execbase - offset, 0xa000 | trap)
                mem.w16(execbase - offset + 2, 0x4e75)
        assert run(BASE) == 20, f'{path}: safe Shell entry'
        residents = [i for i in range(0, len(data) - 26, 2)
                     if data[i:i+2] == b'\x4a\xfc' and mem.r32(BASE+i+2) == BASE+i]
        assert len(residents) == 1, path
        resident = BASE + residents[0]
        assert mem.r_cstr(mem.r32(resident + 14)) == path.name
        assert mem.r8(resident + 10) & 0x80
        init = mem.r32(resident + 22)
        vectors = mem.r32(init + 4)
        query = mem.r32(vectors + 4 * 4)  # MCC_Query, LVO -30, D0 argument.
        cpu.w_reg(0, 999)
        assert run(query) == 0, f'{path}: unknown MCC query'
        if '_snprintf' in symbols:
            cases = [('%s:%ld/%d', [STRING, -2147483648, 42], 'hello:-2147483648/42'),
                     ('%08lx/%p', [0x1234, 0xabcdef], '00001234/0xabcdef'),
                     ('%-6.3s %+05d', [STRING, 7], 'hel    +0007'),
                     ('%#x/%#o/%%', [42, 9], '0x2a/011/%')]
            mem.w_block(STRING, b'hello\0')
            for fmt, args, expected in cases:
                mem.w_block(FORMAT, fmt.encode() + b'\0')
                for size in [0, 1, 5, 100]:
                    mem.w_block(BUFFER, b'!' * 128)
                    result = run(symbols['_snprintf'], BUFFER, size, FORMAT, *args)
                    assert result == len(expected), (path, fmt, size, result)
                    if size:
                        assert mem.r_cstr(BUFFER) == expected[:size - 1]
                    assert mem.r8(BUFFER + size) == ord('!'), 'buffer overrun'
            for fmt, args in [('%f', [0, 0]), ('%2147483648d', [1]),
                              ('%.2147483648d', [1]),
                              ('%*s', [-2147483648, STRING])]:
                mem.w_block(FORMAT, fmt.encode() + b'\0')
                assert run(symbols['_snprintf'], BUFFER, 8, FORMAT, *args) == 0xffffffff
        if '_zune68_strlcpy' in symbols:
            mem.w_block(STRING, b'hello\0')
            assert run(symbols['_zune68_strlcpy'], BUFFER, STRING, 4) == 5
            assert mem.r_cstr(BUFFER) == 'hel'
            assert run(symbols['_zune68_strlcat'], BUFFER, STRING, 6) == 8
            assert mem.r_cstr(BUFFER) == 'helhe'
    finally:
        machine.cleanup()


def check(build):
    for name in PLUGINS.values():
        plugin(build / 'Libs/MUI' / name)
    check_stack(build / 'Prefs/Zune')
    for name in EXAMPLES:
        check_stack(build / 'Examples' / name)
    for name in catalog_paths(build):
        data = (build / name).read_bytes()
        assert data[:4] == b'FORM' and data[8:12] == b'CTLG', name
        assert struct.unpack_from('>I', data, 4)[0] + 8 == len(data), name
    print(f'Components: {len(PLUGINS)} MCC/MCP headers, CLI/query ABI and runtime helpers; '
          f'Prefs and {len(EXAMPLES)} example stacks; {len(catalog_paths(build))} catalogs OK')


if __name__ == '__main__':
    check(Path(sys.argv[1]))

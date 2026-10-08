#!/usr/bin/env python3
"""Exercise linked RGB pen parsing with inputs at the end of emulated RAM.

Compact RGB formats are confirmed by the original MUI reference probe.
Malformed inputs need safe rejection, not the reference's permissive output.
"""
import sys
from pathlib import Path
from check_class_lifetime import Harness

# Native PenSpec intern union starts at 14 (BOOL is a 16-bit WORD).
OUTPUT = 0x70000
RAM_END = 0x100000
RGB = ord('r')


def check(path):
    h = Harness(path)
    cases = 0
    # Older parsing used strtoul/libgcc, whose multiplication uses Utility.
    for name in ('_UtilityBase', '___UtilityBase'):
        if name in h.symbols:
            h.mem.w32(h.symbols[name], 0x78000)
    h.trap(0x78000 - 138, lambda: h.cpu.w_reg(0,
        (h.cpu.r_reg(0) * h.cpu.r_reg(1)) & 0xffffffff))
    def divide():
        q, r = divmod(h.cpu.r_reg(0), h.cpu.r_reg(1))
        h.cpu.w_reg(0, q)
        h.cpu.w_reg(1, r)
    h.trap(0x78000 - 156, divide)
    try:
        valid = {
            'rffa997': (0xff000000, 0xa9000000, 0x97000000),
            'ffa997': (0xff000000, 0xa9000000, 0x97000000),
            'rFFA997': (0xff000000, 0xa9000000, 0x97000000),
            'r000000': (0, 0, 0),
            'rffffff': (0xff000000,) * 3,
            'rffffffff,a9a9a9a9,97979797': (0xffffffff, 0xa9a9a9a9, 0x97979797),
            'ffffffff,a9a9a9a9,97979797': (0xffffffff, 0xa9a9a9a9, 0x97979797),
            'r1,2,33': (1, 2, 0x33),
            'r0,0,0': (0, 0, 0),
            'rff,aa,99': (0xff, 0xaa, 0x99),
        }
        invalid = ['', 'r', 'r123', 'r12345678', 'r123456,',
                   'r123456;123456;123456', 'r0,,0', 'r,0,0',
                   'r0,0,', 'r0,0,0,0', 'r0,0,0junk', 'rgg1122',
                   'r100000000,0,0', 'r0,100000000,0', 'r0,0,100000000']
        for source, expected in [(s, rgb) for s, rgb in valid.items()] + [
                (s, None) for s in invalid]:
            data = source.encode() + b'\0'
            address = RAM_END - len(data)
            h.mem.w_block(address, data)
            h.mem.w_block(OUTPUT, b'!' * 44)
            result = h.call('zune_pen_spec_to_intern', address, OUTPUT + 8) & 0xffff
            assert bool(result) == (expected is not None), (source, result)
            if expected is not None:
                assert h.mem.r32(OUTPUT + 8) == RGB
                assert tuple(h.mem.r32(OUTPUT + 22 + 4 * i)
                             for i in range(3)) == expected, source
            assert bytes(h.mem.r_block(address, len(data))) == data
            assert bytes(h.mem.r_block(OUTPUT, 8)) == b'!' * 8
            assert bytes(h.mem.r_block(OUTPUT + 34, 8)) == b'!' * 8
            cases += 1
        # Fixed-size PenSpec buffers must terminate inside their 32 bytes.
        for byte in [b'0', b'r', b'm', b's', b'p']:
            h.mem.w_block(RAM_END - 32, byte * 32)
            assert not (h.call('zune_pen_spec_to_intern', RAM_END - 32, OUTPUT + 8) & 0xffff)
            cases += 1
        assert not (h.call('zune_pen_spec_to_intern', 0, OUTPUT + 8) & 0xffff)
        assert not (h.call('zune_pen_spec_to_intern', RAM_END - 32, 0) & 0xffff)
        assert not h.allocations
    finally:
        h.machine.cleanup()
    print(f'{path.name}: {cases} bounded compact/expanded RGB parser cases passed')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

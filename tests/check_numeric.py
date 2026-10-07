#!/usr/bin/env python3
"""Compare linked 68000 Numeric methods with original MUI observations."""
import re
import sys
from pathlib import Path
from check_notifications import NotifyHarness, UTILITY

CLASS, OBJECT, DATA = 0x70000, 0x71000, 0x71100


def check(path):
    h = NotifyHarness(path)
    if '___UtilityBase' in h.symbols:
        h.mem.w32(h.symbols['___UtilityBase'], UTILITY)
    h.trap(UTILITY - 138, lambda: h.cpu.w_reg(0,
        (h.cpu.r_reg(0) * h.cpu.r_reg(1)) & 0xffffffff))

    def divide():
        quotient, remainder = divmod(h.cpu.r_reg(0), h.cpu.r_reg(1))
        h.cpu.w_reg(0, quotient)
        h.cpu.w_reg(1, remainder)

    h.trap(UTILITY - 156, divide)
    h.mem.w16(CLASS + 32, DATA - OBJECT)
    fixture = Path(__file__).parent / 'native/numeric_cases.h'
    cases = [list(map(int, re.findall(r'-?\d+', line)))
             for line in fixture.read_text().splitlines() if line.startswith('{')]
    # Wide ranges exercise arithmetic safety, not undocumented MUI overflow.
    cases += [
        [1, -2147483648, 2147483647, 0, 100, 0, 0, 50],
        [1, 0, 100, -2147483648, 2147483647, 0, 50, 0],
        [0, -2147483648, 2147483647, 0, 100, 0, 100, 2104959219],
        [0, 0, 100, -2147483648, 2147483647, 0, 2147483647, 100],
    ]
    for kind, low, high, smin, smax, reverse, value, expected in cases:
        for offset, number in [(8, high), (12, low), (16, value), (20, reverse)]:
            h.mem.w32(DATA + offset, number & 0xffffffff)
        msg = h.words(0, smin & 0xffffffff, smax & 0xffffffff,
                      value & 0xffffffff)
        method = 'Numeric__MUIM_' + ('ValueToScale' if kind else 'ScaleToValue')
        result = h.call(method, CLASS, OBJECT, msg)
        assert result == expected & 0xffffffff, (method, low, high, smin,
                                                smax, reverse, value, result, expected)
        if kind:
            extended = h.words(0, value & 0xffffffff, smin & 0xffffffff, smax & 0xffffffff)
            result = h.call('Numeric__MUIM_ValueToScaleExt', CLASS, OBJECT, extended)
            assert result == expected & 0xffffffff
    assert not h.allocations
    h.machine.cleanup()
    print(f'{path.name}: Numeric reference cases and wide ranges passed')


if __name__ == '__main__':
    for arg in sys.argv[1:]:
        check(Path(arg))

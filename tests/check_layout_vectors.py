#!/usr/bin/env python3
"""Check absolute/relative layout geometry on linked 68000 code.

Object dispatch and GetAttr are modelled; native/mui5_layout_probe.c
provides the corresponding AmigaOS comparison with the MUI 5 runtime.
"""
import sys
from pathlib import Path
from check_class_lifetime import Harness, INTUITION

OBJECT, PARENT = 0x71000, 0x72000
BOX, INNER, FLAGS, VTOP = 52, 60, 64, 100


def check(path):
    h = Harness(path)
    try:
        h.mem.w32(OBJECT + 16, PARENT)
        h.mem.w16(OBJECT + 44, 1000)  # Area maximum width/height
        h.mem.w16(OBJECT + 46, 1000)
        h.mem.w16(PARENT + BOX, 13)
        h.mem.w16(PARENT + BOX + 2, 19)
        h.mem.w8(PARENT + INNER, 5)
        h.mem.w8(PARENT + INNER + 1, 7)
        calls = []
        method_result = [1]

        def dispatch():
            obj = h.mem.r32(h.cpu.r_sp() + 4)
            assert obj == OBJECT
            calls.append(tuple(h.mem.r16(OBJECT + BOX + i * 2)
                               for i in range(4)))
            h.cpu.w_reg(0, method_result[0])

        h.trap(h.symbols['_DoMethodA'], dispatch)
        for result in (1, 0):
            method_result[0] = result
            assert h.call('MUI_LayoutObj', OBJECT, 31, 47, 100, 70, 0) == result
            assert calls[-1] == (31, 47, 100, 70)
            assert h.mem.r32(OBJECT + VTOP) == 47
            assert h.call('MUI_Layout', OBJECT, 31, 47, 100, 70, 0) == result
            assert calls[-1] == (49, 73, 100, 70)
            assert h.mem.r32(OBJECT + VTOP) == 73
        assert len(calls) == 4, 'one layout dispatch per library call'

        # A virtual parent's scroll affects relative placement only.
        h.mem.w32(PARENT + FLAGS, 1 << 30)
        scroll = iter((11, 17))

        def get_scroll():
            assert h.cpu.r_reg(8) == PARENT
            h.mem.w32(h.cpu.r_reg(9), next(scroll))
            h.cpu.w_reg(0, 1)

        h.trap(INTUITION - 654, get_scroll)
        method_result[0] = 1
        assert h.call('MUI_Layout', OBJECT, 31, 47, 100, 70, 0) == 1
        assert calls[-1] == (38, 56, 100, 70)
        assert h.call('MUI_LayoutObj', OBJECT, 31, 47, 100, 70, 0) == 1
        assert calls[-1] == (31, 47, 100, 70)
        # Rejected sizes update the box but must not dispatch layout.
        before = len(calls)
        assert h.call('MUI_LayoutObj', OBJECT, 31, 47, 1001, 70, 0) == 0
        assert h.mem.r16(OBJECT + BOX + 4) == 1001
        assert len(calls) == before
        h.mem.w16(OBJECT + 40, 10)  # Area minimum width
        assert h.call('MUI_LayoutObj', OBJECT, 31, 47, 0, 70, 0) == 0
        assert h.mem.r16(OBJECT + BOX + 4) == 0
        assert len(calls) == before
        # Absolute placement must also work without any parent.
        h.mem.w32(OBJECT + 16, 0)
        assert h.call('MUI_LayoutObj', OBJECT, 31, 47, 100, 70, 0) == 1
        assert calls[-1] == (31, 47, 100, 70)
    finally:
        h.machine.cleanup()
    print(f'{path.name}: absolute/relative layout, scroll and failure propagation OK')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

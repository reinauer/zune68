#!/usr/bin/env python3
"""Check absolute/relative layout geometry on linked 68000 code.

Object dispatch and GetAttr are modelled; native/mui5_layout_probe.c
provides the corresponding AmigaOS comparison with the MUI 5 runtime.
"""
import sys
import struct
from pathlib import Path
from check_class_lifetime import Harness, INTUITION, STACK

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
            msg = h.mem.r32(h.cpu.r_sp() + 8)
            assert h.mem.r32(msg) == 0x90420200, 'preserve old Zune callback ID'
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

        # Enter the actual Area dispatcher with the public 24-byte message.
        # The implementation must issue the distinct, old one-word callback.
        message, klass = 0x74000, 0x75000

        def area_method(method, width=100):
            h.mem.w_block(message, struct.pack('>6I', method, 53, 67,
                                               width, 70, 0))
            h.cpu.w_reg(8, klass)
            h.cpu.w_reg(10, OBJECT)
            h.cpu.w_reg(9, message)
            h.machine.prepare(h.symbols['_Area_Dispatcher'], STACK)
            result = h.machine.execute(100000)
            assert h.machine.was_exit(result), 'Area layout dispatcher'
            assert h.cpu.r_sp() == STACK
            return h.cpu.r_reg(0)

        for result in (1, 0):
            method_result[0] = result
            before = len(calls)
            assert area_method(0x8042845b) == result
            assert len(calls) == before + 1, 'one internal callback, no recursion'
            assert calls[-1] == (53, 67, 100, 70)
            assert h.mem.r32(OBJECT + VTOP) == 67
        before = len(calls)
        assert area_method(0x8042845b, 1001) == 0
        assert len(calls) == before
        assert h.mem.r16(OBJECT + BOX + 4) == 1001
        # A legacy client still sends only its method ID; Area returns success
        # without interpreting the following memory as coordinates.
        assert area_method(0x90420200) == 1
        assert len(calls) == before
        assert h.mem.r16(OBJECT + BOX + 4) == 1001
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
        method_result[0] = 1
        h.mem.w32(OBJECT + 16, 0)
        assert h.call('MUI_LayoutObj', OBJECT, 31, 47, 100, 70, 0) == 1
        assert calls[-1] == (31, 47, 100, 70)

        # Group library calls dispatch the public method before touching
        # geometry, even for a rejected size. Direct method tests above
        # cover the superclass bridge to the legacy callback.
        h.mem.w32(OBJECT + 104, 1 << 2)  # MADF2_ISGROUP
        public_calls = []

        def public_dispatch():
            assert h.mem.r32(h.cpu.r_sp() + 4) == OBJECT
            msg = h.mem.r32(h.cpu.r_sp() + 8)
            public_calls.append(struct.unpack('>6I', h.mem.r_block(msg, 24)))
            assert calls[-1] == (31, 47, 100, 70)
            assert h.mem.r16(OBJECT + BOX + 4) == 100
            h.cpu.w_reg(0, method_result[0])

        h.trap(h.symbols['_DoMethodA'], public_dispatch)
        for result in (0, 0x12345678):
            method_result[0] = result
            # Amiga BOOL returns a 16-bit value; upper D0 is unspecified.
            rc = h.call('MUI_LayoutObj', OBJECT, 53, 67, 1001, 70, 0)
            assert rc & 0xffff == bool(result)
            assert public_calls[-1] == (0x8042845b, 53, 67, 1001, 70, 0)
        assert h.call('DoLayoutMethod', OBJECT) == 1
        assert public_calls[-1] == (0x8042845b, 31, 47, 100, 70, 0)
    finally:
        h.machine.cleanup()
    print(f'{path.name}: absolute/relative layout, scroll and failure propagation OK')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

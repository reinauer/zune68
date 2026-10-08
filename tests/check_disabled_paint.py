#!/usr/bin/env python3
"""Exercise linked built-in/custom Draw boundaries on a 68000.

Synthetic Draw bodies replace pixel rendering; the actual class dispatcher,
superclass calls and MUI_Redraw determine when the disabled painter runs.
The companion native/disablepaint.c checks pixels against original MUI.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness, INTUITION

OBJECT, MRI, RP = 0x71000, 0x72000, 0x73000
DRAW = 0x80426f3f


class PaintHarness(Harness):
    def __init__(self, path):
        super().__init__(path)
        self.events = []
        self.getters = 0
        self.trap(INTUITION - 654, self.get_attr)
        self.pixel = 0
        self.area = self.get('Area.mui')
        self.text = self.get('Text.mui')
        self.mem.w32(OBJECT + 28, MRI)
        self.mem.w32(MRI + 20, RP)
        self.mem.w32(OBJECT + 64, 1 << 14)
        self.mem.w_block(OBJECT + 52, struct.pack('>4h', 0, 0, 20, 10))
        self.trap(self.symbols['_ZuneDrawDisabled'], self.ghost)
        self.mem.w32(self.area + 12, self.body('area'))
        self.mem.w32(self.text + 12, self.body('text', inherit=True))
        mcc = self.create('Text.mui')
        self.custom = self.mem.r32(mcc + 24)

    def get_attr(self):
        self.getters += 1
        self.cpu.w_reg(0, 0)

    def ghost(self):
        assert self.mem.r32(self.cpu.r_sp() + 4) == OBJECT
        self.events.append('ghost')
        self.pixel = 2

    def body(self, name, inherit=False):
        address = self.alloc(64)
        offset = 0
        if inherit:
            # Save a0-a2; pass class/object/message to the real alib helper.
            code = bytes.fromhex('48e700e0 2f2f0004 2f2f000c 2f2f0008 4eb9')
            code += struct.pack('>I', self.symbols['_DoSuperMethodA'])
            code += bytes.fromhex('4fef000c 4cdf0700')
            self.mem.w_block(address, code)
            offset = len(code)
        def paint():
            self.events.append(name)
            self.pixel = 3 if name == 'custom' else 1
            self.cpu.w_reg(0, 0x1234)
        self.trap(address + offset, paint)
        return address

    def redraw(self, cl, disabled):
        self.events.clear()
        self.mem.w32(OBJECT - 4, cl)
        # Native Area ends at object+246; the disable count is its last LONG.
        self.mem.w32(OBJECT + 242, 0x80000000 if disabled else 0)
        self.call('MUI_Redraw', OBJECT, 1)


def check(path):
    h = PaintHarness(path)
    h.redraw(h.text, False)
    assert h.events == ['area', 'text'], h.events
    assert h.getters == 0, 'enabled redraw should not query disabled attributes'
    h.redraw(h.text, True)
    assert h.events == ['area', 'text', 'ghost'], h.events
    assert h.pixel == 2
    h.mem.w32(h.custom + 12, h.body('custom'))
    h.redraw(h.custom, True)
    assert h.events == ['custom'] and h.pixel == 3, h.events
    h.mem.w32(h.custom + 12, h.body('custom', inherit=True))
    h.redraw(h.custom, True)
    assert h.events == ['area', 'text', 'ghost', 'custom'], h.events
    assert h.pixel == 3, 'library painted over custom content'
    h.redraw(h.custom, False)
    assert h.events == ['area', 'text', 'custom'], h.events
    # Direct Area subclasses get the same opportunity to replace Super paint.
    h.mem.w32(h.custom + 24, h.area)
    h.redraw(h.custom, True)
    assert h.events == ['area', 'ghost', 'custom'], h.events
    h.machine.cleanup()
    print(f'{path}: disabled paint stays at the built-in Draw boundary')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

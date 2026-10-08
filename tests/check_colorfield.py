#!/usr/bin/env python3
"""Exercise linked Colorfield pen ownership on a 68000.

OS palette services are modeled. Live automatic and explicit pen behavior
matches the accompanying original-SDK guest probe against MUI 3.9.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness

CLASS, OBJECT, MESSAGE, TAGS = 0x70000, 0x71000, 0x72000, 0x73000
UTILITY, GFX, MRI, SCREEN = 0x74000, 0x75000, 0x76000, 0x77000
COLORMAP, RP, STORE = 0x78000, 0x79000, 0x7a000
PEN, RED = 0x8042713a, 0x804279f6
NONE = 0xffffffff


class ColorfieldHarness(Harness):
    def __init__(self, path, supplied=None, allocated=11):
        super().__init__(path)
        self.allocated = allocated
        self.obtained, self.released, self.colors = [], [], []
        self.mem.w16(CLASS + 32, 256)
        self.mem.w32(OBJECT + 28, MRI)
        self.mem.w32(MRI + 4, SCREEN)
        self.mem.w32(MRI + 20, RP)
        self.mem.w32(SCREEN + 48, COLORMAP)
        self.mem.w32(self.symbols['_GfxBase'], GFX)
        for name in ('_UtilityBase', '___UtilityBase'):
            if name in self.symbols:
                self.mem.w32(self.symbols[name], UTILITY)
        self.trap(UTILITY - 48, self.next_tag)
        self.trap(self.symbols['_DoSuperMethodA'], self.super_method)
        self.trap(GFX - 954, self.obtain_pen)
        self.trap(GFX - 948, self.release_pen)
        self.trap(GFX - 852, self.set_rgb)
        self.trap(GFX - 960, lambda: self.cpu.w_reg(0, 4))
        self.tags([] if supplied is None else [(PEN, supplied)])
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x101, TAGS, 0))
        assert self.call('Colorfield__OM_NEW', CLASS, OBJECT, MESSAGE) == OBJECT

    def tags(self, items):
        values = [value & NONE for item in items for value in item] + [0, 0]
        self.mem.w_block(TAGS, struct.pack('>' + 'I' * len(values), *values))

    def next_tag(self):
        storage = self.cpu.r_reg(8)
        tag = self.mem.r32(storage)
        self.cpu.w_reg(0, tag if self.mem.r32(tag) else 0)
        self.mem.w32(storage, tag + 8)

    def super_method(self):
        msg = self.mem.r32(self.cpu.r_sp() + 12)
        self.cpu.w_reg(0, OBJECT if self.mem.r32(msg) == 0x101 else 1)

    def obtain_pen(self):
        assert self.cpu.r_reg(8) == COLORMAP
        assert self.cpu.r_reg(0) == NONE
        self.obtained.append(self.allocated)
        self.cpu.w_reg(0, self.allocated & NONE)

    def release_pen(self):
        assert self.cpu.r_reg(8) == COLORMAP
        pen = self.cpu.r_reg(0)
        assert pen != NONE
        self.released.append(pen)

    def set_rgb(self):
        assert self.cpu.r_reg(8) == SCREEN + 44
        pen = self.cpu.r_reg(0)
        assert pen != NONE, 'invalid pen must never reach SetRGB32'
        self.colors.append((pen, self.cpu.r_reg(1)))

    def get_pen(self):
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x104, PEN, STORE))
        assert self.call('Colorfield__OM_GET', CLASS, OBJECT, MESSAGE)
        return self.mem.r32(STORE)

    def set(self, attr, value):
        self.tags([(attr, value)])
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x103, TAGS, 0))
        self.call('Colorfield__OM_SET', CLASS, OBJECT, MESSAGE)

    def setup(self):
        self.mem.w32(MESSAGE, 0)
        assert self.call('Colorfield__MUIM_Setup', CLASS, OBJECT, MESSAGE)
        self.mem.w32(OBJECT + 64, 1 << 28)

    def cleanup(self):
        self.mem.w32(MESSAGE, 0)
        self.call('Colorfield__MUIM_Cleanup', CLASS, OBJECT, MESSAGE)
        self.mem.w32(OBJECT + 64, 0)


def check(path):
    # An owned pen survives live Pen changes, including the reset sentinel.
    for allocated in (0, 11, 255):
        h = ColorfieldHarness(path, allocated=allocated)
        try:
            assert h.get_pen() == NONE
            for cycle in range(2):
                h.setup()
                assert h.get_pen() == allocated
                for value in (15, -1):
                    h.set(PEN, value)
                    assert h.get_pen() == allocated and not h.released[cycle:]
                    h.set(RED, 0x99999999)
                    assert h.colors[-1] == (allocated, 0x99999999)
                h.cleanup()
                assert h.get_pen() == NONE
                assert h.released == [allocated] * (cycle + 1)
                h.cleanup()
                assert h.released == [allocated] * (cycle + 1)
            assert h.obtained == [allocated, allocated]
        finally:
            h.machine.cleanup()

    # A supplied pen is borrowed. Resetting it does not allocate until reopen.
    for supplied in (0, 15, 255):
        h = ColorfieldHarness(path, supplied=supplied)
        try:
            assert h.get_pen() == supplied
            h.setup()
            assert not h.obtained and h.colors[-1][0] == supplied
            h.set(RED, 0x99999999)
            assert h.colors[-1] == (supplied, 0x99999999)
            h.set(PEN, -1)
            assert h.get_pen() == NONE
            count = len(h.colors)
            h.set(RED, 0x22222222)
            assert len(h.colors) == count and not h.obtained
            h.cleanup()
            assert not h.released
            h.setup()
            assert h.get_pen() == 11 and h.obtained == [11]
            h.cleanup()
            assert h.released == [11]
        finally:
            h.machine.cleanup()

    # Failed allocation preserves the sentinel and never changes/releases a pen.
    h = ColorfieldHarness(path, allocated=-1)
    try:
        h.setup()
        assert h.get_pen() == NONE
        h.set(RED, 0x99999999)
        h.cleanup()
        assert not h.colors and not h.released
        h.allocated = 11
        h.setup()
        h.set(RED, 0x22222222)
        assert h.colors[-1] == (11, 0x22222222)
        h.cleanup()
        assert h.released == [11]
    finally:
        h.machine.cleanup()

    # Re-enter OM_SET from ReleasePen while Cleanup still has MADF_SETUP.
    # The owned pen was already cleared, so changing RGB must not use -1.
    h = ColorfieldHarness(path)
    try:
        h.setup()
        h.set(RED, 0x11111111)
        count = len(h.colors)
        callback, message, tags = 0x7b000, MESSAGE + 128, TAGS + 128
        h.mem.w_block(tags, struct.pack('>4I', RED, 0xeeeeeeee, 0, 0))
        h.mem.w_block(message, struct.pack('>3I', 0x103, tags, 0))
        # Preserve the OS-call registers and run a genuine nested C call.
        code = bytes.fromhex('48e7fffe')  # movem.l d0-d7/a0-a6,-(sp)
        for argument in (message, OBJECT, CLASS):
            code += bytes.fromhex('2f3c') + struct.pack('>I', argument)
        code += bytes.fromhex('4eb9') + struct.pack('>I',
            h.symbols['_Colorfield__OM_SET'])
        code += bytes.fromhex('4fef000c4cdf7fff4e75')
        h.mem.w_block(callback, code)
        h.mem.w_block(GFX - 948 + 2,
                      bytes.fromhex('4ef9') + struct.pack('>I', callback))
        h.cleanup()
        assert h.released == [11] and len(h.colors) == count
        assert h.mem.r32(OBJECT + 256 + 4) == 0xeeeeeeee
        assert h.get_pen() == NONE
    finally:
        h.machine.cleanup()
    print(f'{path.name}: Colorfield automatic/borrowed/failed pens and '
          'live resets preserve ownership; -1 remains distinct from pen 255')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

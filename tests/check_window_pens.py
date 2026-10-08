#!/usr/bin/env python3
"""Run linked Window pen setup/cleanup on a 68000 with modeled OS services.

MUI_ObtainPen responses model owned, shared, borrowed and failed pens;
MUI_ReleasePen runs unmodified, so lost ownership bits cause a leak here.
This checks ownership and the public UWORD pen array, not color quality.
"""
import sys
from collections import Counter
from pathlib import Path
from check_class_lifetime import Harness, INTUITION, MASTER

CLASS, OBJECT, GLOBAL, PREFS = 0x70000, 0x71000, 0x72000, 0x73000
SCREEN, DRAWINFO, DRAWPENS, COLORMAP = 0x75000, 0x76000, 0x76100, 0x77000
GFX, SPECS, MESSAGE = 0x78000, 0x79000, 0x7a000
# Native offsets checked with offsetof against window.c and the classic SDK.
DATA = OBJECT + 64
USER_SCREEN, CREATE_FLAGS = DATA + 618, DATA + 462
PUBLIC_PENS = DATA + 36


class PenHarness(Harness):
    def __init__(self, path, handles, derived):
        super().__init__(path)
        self.handles = handles
        self.derived = derived
        self.requests = []
        self.released = []
        self.mem.w16(CLASS + 32, 64)
        self.mem.w32(OBJECT, GLOBAL)
        self.mem.w32(GLOBAL + 20, PREFS)
        self.mem.w32(USER_SCREEN, SCREEN)
        self.mem.w32(CREATE_FLAGS, 0x800)  # Borderless avoids unrelated sysiclass.
        self.mem.w16(SCREEN + 12, 640)
        self.mem.w16(SCREEN + 14, 256)
        self.mem.w32(SCREEN + 48, COLORMAP)
        self.mem.w32(DRAWINFO + 4, DRAWPENS)
        for i in range(12):
            self.mem.w16(DRAWPENS + 2 * i, i)
        size = self.mem.r32(self.symbols['_LibInitTable'])
        self.mem.w32(MASTER + size - 4, SPECS)
        for i in range(8):
            text = '' if derived and i in (1, 3) else f'r{i:08x},00000000,00000000'
            self.mem.w_block(SPECS + 32 * i, text.encode() + b'\0')
        self.mem.w32(self.symbols['_GfxBase'], GFX)
        self.trap(INTUITION - 690, lambda: self.cpu.w_reg(0, DRAWINFO))
        self.trap(INTUITION - 696, lambda: None)
        self.trap(GFX - 960, lambda: self.cpu.w_reg(0, 4))
        self.trap(GFX - 900, self.rgb)
        self.trap(GFX - 948, self.release_pen)
        # libgcc's 68000 division helper delegates to utility.library.
        self.trap(self.symbols['___divsi3'], lambda: self.cpu.w_reg(0,
            self.mem.r32(self.cpu.r_sp() + 4) //
            self.mem.r32(self.cpu.r_sp() + 8)))
        for name in ('_UtilityBase', '___UtilityBase'):
            if name in self.symbols:
                self.mem.w32(self.symbols[name], 0x7b000)
        self.trap(0x7b000 - 156, self.udiv)
        self.trap(self.symbols['_load_custom_frame'], lambda: self.cpu.w_reg(0, 0))
        self.trap(self.symbols['_dispose_custom_frame'], lambda: None)
        self.trap(self.symbols['_zune_imspec_setup'], lambda: self.cpu.w_reg(0, 0))
        self.trap(self.symbols['_zune_imspec_cleanup'], lambda: None)
        self.trap(self.symbols['_MUI_ObtainPen'], self.obtain_pen)

    def udiv(self):
        quotient, remainder = divmod(self.cpu.r_reg(0), self.cpu.r_reg(1))
        self.cpu.w_reg(0, quotient)
        self.cpu.w_reg(1, remainder)

    def rgb(self):
        assert self.cpu.r_reg(8) == COLORMAP
        for i in range(3):
            self.mem.w32(self.cpu.r_reg(9) + 4 * i, (self.cpu.r_reg(0) * 0x20) * 0x01010101)

    def obtain_pen(self):
        sp = self.cpu.r_sp()
        assert self.mem.r32(sp + 4) == DATA
        assert self.mem.r32(sp + 12) == 0
        spec = self.mem.r_cstr(self.mem.r32(sp + 8))
        if self.derived and spec == 'rc0c0c0c0,c0c0c0c0,c0c0c0c0':
            index = 1
        elif self.derived and spec == 'rd0d0d0d0,d0d0d0d0,d0d0d0d0':
            index = 3
        else:
            index = int(spec[1:9], 16)
            assert spec == f'r{index:08x},00000000,00000000', spec
        assert index not in self.requests and 0 <= index < 8
        self.requests.append(index)
        self.cpu.w_reg(0, self.handles[index] & 0xffffffff)

    def release_pen(self):
        assert self.cpu.r_reg(8) == COLORMAP
        self.released.append(self.cpu.r_reg(0))


def check(path):
    for derived in (False, True):
        for handles in ([0x10000 | i for i in range(8)],
                        [2, 0x10009, -1, 0x10009, 4, 0x10000, 6, 0x1000b]):
            h = PenHarness(path, handles, derived)
            try:
                for cycle in range(3):
                    h.requests.clear()
                    h.released.clear()
                    assert h.call('Window__MUIM_Setup', CLASS, OBJECT, MESSAGE)
                    assert len(h.requests) == 8, 'MARK must also be initialized'
                    pens = h.mem.r32(DATA + 12)
                    assert pens == PUBLIC_PENS
                    for i, handle in enumerate(handles):
                        # Failed BACKGROUND allocation borrows BACKGROUNDPEN=7.
                        expected = 7 if handle == -1 else handle & 0xffff
                        assert h.mem.r16(pens + 2 * i) == expected, (i, handle)
                    assert h.call('Window__MUIM_Cleanup', CLASS, OBJECT, MESSAGE)
                    expected = [p & 0xffff for p in handles if p >= 0x10000]
                    assert Counter(h.released) == Counter(expected), (cycle, h.released, expected)
                    # Cleanup consumes ownership; stale handles cannot release twice.
                    h.call('Window__MUIM_Cleanup', CLASS, OBJECT, MESSAGE)
                    assert Counter(h.released) == Counter(expected)
                    assert not h.allocations, 'derived shades need no heap storage'
            finally:
                h.machine.cleanup()
    print(f'{path.name}: window owned/borrowed/failed/shared pens balance '
          'across repeated setup and cleanup; public pen ABI stays UWORD')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

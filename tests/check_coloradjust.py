#!/usr/bin/env python3
"""Run Coloradjust pen lifetime and callbacks from linked 68000 code.

Child gadgets and OS palette services are modeled. The real constructor,
setup, cleanup and installed slider/wheel callbacks execute on the CPU.
"""
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import EXEC, INTUITION

CLASS, OBJECT, DATA = 0x70000, 0x71000, 0x71100
GFX, WHEEL, MRI, SCREEN, BITMAP = 0x79000, 0x7a000, 0x7b000, 0x7c000, 0x7d000
SETUP, CLEANUP, SETUP_FLAG = 0x80428354, 0x8042d985, 1 << 28
PEN = DATA + 110


class ColorHarness(NotifyHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 256)
        self.mem.w32(OBJECT - 4, CLASS)
        self.mem.w32(OBJECT + 28, MRI)
        self.mem.w32(MRI + 4, SCREEN)
        self.mem.w16(MRI + 12, 2)  # Borrowed shine and shadow pens.
        self.mem.w16(MRI + 20, 1)
        self.mem.w32(SCREEN + 48, 123)
        self.mem.w32(SCREEN + 88, BITMAP)
        self.mem.w32(self.symbols['_GfxBase'], GFX)
        self.setup_ok = True
        self.pen_result = 0
        self.obtained, self.released, self.changed = [], [], []
        self.trap(self.symbols['_DoSuperNewTags'],
                  lambda: self.cpu.w_reg(0, OBJECT))
        self.trap(self.symbols['_DoSuperMethodA'], self.super_method)
        for name in ('MUI_NewObjectA', 'MUI_NewObject', 'MUI_MakeObject'):
            self.trap(self.symbols['_' + name],
                      lambda: self.cpu.w_reg(0, 0x78000))
        for name in ('DoMethod', 'CoerceMethod', 'CoerceMethodA'):
            self.trap(self.symbols['_' + name], lambda: self.cpu.w_reg(0, 1))
        label = self.blob(b'color\0')
        self.trap(self.symbols['__'], lambda: self.cpu.w_reg(0, label))
        self.trap(EXEC - 552, lambda: self.cpu.w_reg(0, WHEEL))
        self.trap(EXEC - 414, lambda: None)
        self.trap(EXEC - 624, lambda: self.mem.w_block(self.cpu.r_reg(9),
                  self.mem.r_block(self.cpu.r_reg(8), self.cpu.r_reg(0))))
        self.trap(INTUITION - 648, lambda: self.cpu.w_reg(0, 1))
        self.trap(INTUITION - 654, self.get_attr)
        for vector in (30, 36):
            self.trap(WHEEL - vector, lambda: self.mem.w_block(
                self.cpu.r_reg(9), self.mem.r_block(self.cpu.r_reg(8), 12)))
        self.trap(GFX - 954, self.obtain_pen)
        self.trap(GFX - 948, self.release_pen)
        self.trap(GFX - 852, self.set_rgb)
        self.trap(GFX - 960, lambda: self.cpu.w_reg(0, 4))
        self.mem.w32(PEN, 0)  # Real zero-initialized object starts with pen zero.
        assert self.call('Coloradjust__OM_NEW', CLASS, 0,
                         self.words(0x101, self.words(0, 0), 0)) == OBJECT
        assert self.mem.r32(PEN) == 0xffffffff
        assert not self.obtained and not self.changed and not self.released
        for name, offset in (('Slider', 8), ('Wheel', 28)):
            self.symbols['_Test' + name] = self.mem.r32(DATA + offset + 12)

    def super_method(self):
        method = self.mem.r32(self.argument(2))
        if method == SETUP and self.setup_ok:
            self.mem.w32(OBJECT + 64, SETUP_FLAG)
        if method == CLEANUP:
            self.mem.w32(OBJECT + 64, 0)
        self.cpu.w_reg(0, int(self.setup_ok) if method == SETUP else 1)

    def get_attr(self):
        self.mem.w32(self.cpu.r_reg(9), 127)
        self.cpu.w_reg(0, 1)

    def obtain_pen(self):
        assert self.cpu.r_reg(8) == 123
        assert self.cpu.r_reg(0) == 0xffffffff
        assert self.cpu.r_reg(4) == 1  # PENF_EXCLUSIVE before changing RGB.
        self.obtained.append(self.pen_result)
        self.cpu.w_reg(0, self.pen_result)

    def release_pen(self):
        assert self.cpu.r_reg(8) == 123
        assert self.mem.r32(PEN) == 0xffffffff, 'ownership live during ReleasePen'
        self.released.append(self.cpu.r_reg(0))

    def set_rgb(self):
        assert self.cpu.r_reg(8) == SCREEN + 44
        assert self.mem.r32(OBJECT + 64) & SETUP_FLAG
        pen = self.cpu.r_reg(0)
        assert pen == self.pen_result and pen != 0xffffffff
        self.changed.append(pen)

    def method(self, name, method):
        return self.call('Coloradjust__' + name, CLASS, OBJECT, self.words(method))

    def callbacks(self):
        self.call('TestSlider', DATA + 8, OBJECT, self.words(DATA, 0))
        self.call('TestWheel', DATA + 28, OBJECT, self.words(DATA))

    def finish(self):
        assert not self.allocations
        self.machine.cleanup()


def check(path):
    for wheel, setup_ok, pen in ((False, True, 0), (True, False, 0),
                                (True, True, 0xffffffff),
                                (True, True, 0), (True, True, 7)):
        h = ColorHarness(path)
        h.setup_ok, h.pen_result = setup_ok, pen
        # Notifications before Setup must never mutate a screen palette.
        h.callbacks()
        assert not h.changed
        # Also exercise the setup guard independently of the sentinel.
        h.mem.w32(PEN, 0)
        h.callbacks()
        assert not h.changed
        h.mem.w32(PEN, 0xffffffff)
        if not wheel:
            h.mem.w32(DATA + 84, 0)
        assert h.method('MUIM_Setup', SETUP) == int(setup_ok)
        owned = wheel and setup_ok and pen != 0xffffffff
        assert h.obtained == ([pen] if wheel and setup_ok else [])
        assert h.changed == ([pen] if owned else [])
        h.callbacks()
        assert len(h.changed) == (3 if owned else 0)
        h.method('MUIM_Cleanup', CLEANUP)
        h.callbacks()
        h.method('MUIM_Cleanup', CLEANUP)
        assert h.released == ([pen] if owned else [])
        assert len(h.changed) == (3 if owned else 0)
        assert h.mem.r32(PEN) == 0xffffffff
        h.finish()
    print(f'{path}: Coloradjust pen ownership and callback checks passed')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

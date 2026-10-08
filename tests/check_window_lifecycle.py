#!/usr/bin/env python3
"""Run real Window open/close paths with modeled GUI and custom hooks.

The custom hook trampoline calls the linked superclass, checking resource
availability both before Setup and after Cleanup, including open failures.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import INTUITION
from check_window_pens import (PenHarness, CLASS, OBJECT, GLOBAL, DATA, SCREEN,
                               DRAWINFO, GFX, MESSAGE)

ROOT, WINDOW, RP, FONT = 0x7c000, 0x7d000, 0x7d200, 0x7d400
TAGS, UTILITY, GADTOOLS = 0x7e000, 0x7b000, 0x7f000
ROOT_SLOT, FLAGS = DATA + 522, DATA + 526
WINDOW_SETUP, WINDOW_CLEANUP = 0x8042c34c, 0x8042ab26
ROOT_CLEANUP = 0x8042d985


class WindowHarness(PenHarness):
    def __init__(self, path, mode='success', handles=None, derived=False):
        super().__init__(path, handles or [0x10000 | i for i in range(8)], derived)
        self.mode = mode
        self.events = []
        self.mem.w32(ROOT_SLOT, ROOT)
        self.mem.w32(DRAWINFO + 8, FONT)
        self.mem.w16(FONT + 20, 8)
        self.mem.w32(WINDOW + 50, RP)
        self.mem.w16(WINDOW + 8, 100)
        self.mem.w16(WINDOW + 10, 60)
        self.mem.w16(WINDOW + 112, 100)
        self.mem.w16(WINDOW + 114, 60)
        self.mem.w32(self.symbols['_GadToolsBase'], GADTOOLS)
        for name in ('_UtilityBase', '___UtilityBase'):
            if name in self.symbols:
                self.mem.w32(self.symbols[name], UTILITY)
        self.trap(UTILITY - 48, self.next_tag)
        self.trap(self.symbols['_DoSuperMethodA'], lambda: self.cpu.w_reg(0, 1))
        self.trap(self.symbols['_DoMethod'], self.method)
        self.trap(self.symbols['_DoSetupMethod'], self.root_setup)
        self.trap(self.symbols['_DoShowMethod'], lambda: self.events.append('show'))
        self.trap(self.symbols['_DoHideMethod'], lambda: self.events.append('hide'))
        self.trap(self.symbols['___area_finish_minmax'], lambda: None)
        for name in ('_zune_imspec_show', '_zune_imspec_hide', '_zune_imspec_draw',
                     '_MUI_Redraw'):
            self.trap(self.symbols[name], lambda: None)
        self.trap(INTUITION - 606, self.open_window)
        self.trap(INTUITION - 654, lambda: self.cpu.w_reg(0, 0))
        self.trap(INTUITION - 648, lambda: None)
        self.trap(INTUITION - 318, lambda: self.cpu.w_reg(0, 1))
        self.trap(INTUITION - 150, lambda: self.cpu.w_reg(0, 1))
        self.trap(INTUITION - 54, lambda: None)
        self.trap(INTUITION - 72, lambda: self.events.append('close'))
        self.trap(INTUITION - 690, self.drawinfo)
        self.trap(INTUITION - 696, lambda: self.events.append('free drawinfo'))
        self.trap(GFX - 354, lambda: None)  # SetDrMd
        self.trap(GADTOOLS - 126, lambda: self.cpu.w_reg(0, 0))
        self.make_hook(WINDOW_SETUP, 'Setup', 0x80000)
        self.make_hook(WINDOW_CLEANUP, 'Cleanup', 0x80100)

    def drawinfo(self):
        self.cpu.w_reg(0, 0 if self.mode == 'drawinfo failure' else DRAWINFO)

    def next_tag(self):
        where = self.cpu.r_reg(8)
        tag = self.mem.r32(where)
        self.cpu.w_reg(0, tag if self.mem.r32(tag) else 0)
        self.mem.w32(where, tag + 8)

    def resources_live(self):
        assert self.mem.r32(DATA + 4) == SCREEN, 'screen unavailable inside hook'
        assert self.mem.r32(DATA + 8) == DRAWINFO, 'DrawInfo unavailable inside hook'
        assert self.mem.r32(DATA + 12), 'pens unavailable inside hook'
        assert not self.released, 'pens released before custom cleanup returned'

    def make_hook(self, method, name, address):
        before, after, message = address + 128, address + 136, address + 144
        self.mem.w32(message, method)
        def enter():
            self.resources_live()
            self.events.append(name + ' before super')
        def leave():
            self.resources_live()
            self.events.append(name + ' after super')
            if name == 'Setup' and self.mode == 'custom setup failure':
                self.cpu.w_reg(0, 0)
        self.trap(before, enter)
        self.trap(after, leave)
        code = bytes.fromhex('4eb9') + struct.pack('>I', before)
        for value in (message, OBJECT, CLASS):
            code += bytes.fromhex('2f3c') + struct.pack('>I', value)
        code += bytes.fromhex('4eb9') + struct.pack('>I',
            self.symbols['_Window__MUIM_' + name])
        code += bytes.fromhex('4fef000c4eb9') + struct.pack('>I', after)
        code += bytes.fromhex('4e75')
        self.mem.w_block(address, code)

    def method(self):
        sp = self.cpu.r_sp()
        obj, method = self.mem.r32(sp + 4), self.mem.r32(sp + 8)
        address = self.symbols['_DoMethod'] + 2
        self.mem.w16(address, 0x4e75)
        if obj == OBJECT and method in (WINDOW_SETUP, WINDOW_CLEANUP):
            target = 0x80000 if method == WINDOW_SETUP else 0x80100
            self.mem.w_block(address, bytes.fromhex('4ef9') + struct.pack('>I', target))
        elif obj == ROOT and method == ROOT_CLEANUP:
            self.resources_live()
            self.events.append('root cleanup')
        self.cpu.w_reg(0, 1)

    def root_setup(self):
        self.resources_live()
        self.events.append('root setup')
        self.cpu.w_reg(0, int(self.mode != 'root setup failure'))

    def open_window(self):
        self.resources_live()
        self.events.append('open')
        self.cpu.w_reg(0, 0 if self.mode == 'display failure' else WINDOW)

    def opened(self, value):
        self.mem.w_block(TAGS, struct.pack('>4I', 0x80428aa0, value, 0, 0))
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x103, TAGS, 0))
        self.call('Window__OM_SET', CLASS, OBJECT, MESSAGE)


def check(path):
    for mode in ('success', 'drawinfo failure', 'custom setup failure',
                 'root setup failure', 'display failure', 'no root'):
        h = WindowHarness(path, mode)
        try:
            if mode == 'no root':
                h.mem.w32(ROOT_SLOT, 0)
            h.opened(1)
            if mode == 'success':
                assert h.mem.r32(FLAGS) & 1
                h.resources_live()
                h.opened(0)
            assert h.mem.r32(DATA + 4) == 0, (mode, 'stale screen')
            assert not h.mem.r32(FLAGS) & 1
            if mode in ('no root', 'drawinfo failure'):
                assert not h.requests and not h.released and not h.events
            else:
                assert h.requests == list(range(8))
                assert sorted(h.released) == list(range(8))
                expected = ['Setup before super', 'Setup after super']
                if mode != 'custom setup failure':
                    expected += ['root setup']
                    if mode != 'root setup failure':
                        expected += ['open']
                        if mode == 'success':
                            expected += ['show', 'hide', 'close']
                        expected += ['root cleanup']
                    expected += ['Cleanup before super', 'Cleanup after super']
                expected += ['free drawinfo']
                assert h.events == expected, (mode, h.events, expected)
            before = h.released[:]
            h.opened(0)
            assert h.released == before, 'second close released resources twice'
        finally:
            h.machine.cleanup()
    print(f'{path.name}: Window screen/pens span custom Setup/Cleanup; '
          'open failures and repeated close balance resources')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

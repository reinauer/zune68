#!/usr/bin/env python3
"""Exercise native Floattext lookup, text ownership and layout on a 68000.

List operations and fixed-width font metrics are modelled; no GUI is opened.
"""
import sys
from pathlib import Path
from check_class_lifetime import Harness, EXEC, INTUITION, MASTER, STACK, REFS

CLASS, OBJECT, MESSAGE, TAGS = 0x70000, 0x71000, 0x72000, 0x73000
WINDOW, UTILITY, GRAPHICS, TEXT = 0x74000, 0x75000, 0x76000, 0x77000
DATA = OBJECT + 256
TEXT_TAG, SKIP_TAG, TAB_TAG = 0x8042d16a, 0x80425c7d, 0x80427d17


class FloattextHarness(Harness):
    def __init__(self, path):
        self.guard_sizes = {}
        super().__init__(path)
        self.lines = []
        self.width = 80
        self.window = WINDOW
        self.fail_after = None
        self.mem.w32(self.symbols['_UtilityBase'], UTILITY)
        if '___UtilityBase' in self.symbols:
            self.mem.w32(self.symbols['___UtilityBase'], UTILITY)
        self.mem.w32(self.symbols['_GfxBase'], GRAPHICS)
        self.mem.w16(CLASS + 32, 256)
        self.mem.w32(WINDOW + 50, WINDOW + 512)  # Window.RPort
        self.trap(UTILITY - 48, self.next_tag)
        self.trap(UTILITY - 138, lambda: self.cpu.w_reg(0,
                  (self.cpu.r_reg(0) * self.cpu.r_reg(1)) & 0xffffffff))
        self.trap(UTILITY - 156, self.divide)
        self.trap(INTUITION - 654, self.get_attr)
        self.trap(INTUITION - 648, lambda: None)  # SetAttrsA: List string hooks
        self.trap(EXEC - 624, self.copy)
        self.trap(EXEC - 630, self.copy)
        self.trap(GRAPHICS - 54, lambda: self.cpu.w_reg(0, self.cpu.r_reg(0) * 8))
        self.trap(GRAPHICS - 696, self.fit)
        self.trap(self.symbols['_DoMethod'], self.method)
        self.trap(self.symbols['_DoSuperMethodA'], self.super_method)

    def alloc(self, size):
        address = super().alloc(size + 16)
        self.guard_sizes[address] = size
        self.mem.w_block(address + size, b'?' * 16)
        return address

    def check_bounds(self):
        for address in self.allocations:
            assert bytes(self.mem.r_block(address + self.guard_sizes[address], 16)) == b'?' * 16

    def argument(self, n):
        return self.mem.r32(self.cpu.r_sp() + 4 + 4 * n)

    def alloc_vec(self):
        if self.fail_after is not None:
            if self.fail_after == 0:
                self.cpu.w_reg(0, 0)
                return
            self.fail_after -= 1
        super().alloc_vec()

    def free_vec(self):
        address = self.cpu.r_reg(9)
        if address:
            self.check_bounds()
            # Poison freed text so setting the current string catches UAF.
            self.mem.w_block(address, b'!' * self.allocations[address])
            super().free_vec()

    def copy(self):
        self.mem.w_block(self.cpu.r_reg(9), self.mem.r_block(self.cpu.r_reg(8), self.cpu.r_reg(0)))

    def divide(self):
        dividend, divisor = self.cpu.r_reg(0), self.cpu.r_reg(1)
        self.cpu.w_reg(0, dividend // divisor)
        self.cpu.w_reg(1, dividend % divisor)

    def next_tag(self):
        storage = self.cpu.r_reg(8)
        tag = self.mem.r32(storage)
        self.cpu.w_reg(0, tag if self.mem.r32(tag) else 0)
        self.mem.w32(storage, tag + 8)

    def get_attr(self):
        values = {0x80421591: self.window, 0x8042b59c: self.width + 8,
                  0x804228f8: 0, 0x804297ff: 0}
        self.mem.w32(self.cpu.r_reg(9), values[self.cpu.r_reg(0)] & 0xffffffff)
        self.cpu.w_reg(0, 1)

    def fit(self):
        count = self.cpu.r_reg(0)
        assert count <= 65535
        self.cpu.w_reg(0, min(count, self.cpu.r_reg(2) // 8))

    def method(self):
        assert self.argument(0) == OBJECT
        method = self.argument(1)
        if method == 0x8042ad89:  # List_Clear
            self.lines = []
        elif method == 0x804254d5:  # List_InsertSingle copies the string
            self.lines.append(self.mem.r_cstr(self.argument(2)))
        else:
            assert method == 0x80427993, hex(method)  # List_Redraw
        self.cpu.w_reg(0, 0)

    def super_method(self):
        self.cpu.w_reg(0, OBJECT if self.mem.r32(self.argument(2)) == 0x101 else 0)

    def set(self, text, width=80, skip=None, tab=8, justify=False):
        self.width = width
        self.mem.w_block(TEXT, text.encode() + b'\0')
        self.mem.w_block(TEXT + 4096, (skip or '').encode() + b'\0')
        tags = [(TEXT_TAG, TEXT), (TAB_TAG, tab),
                (SKIP_TAG, TEXT + 4096 if skip is not None else 0),
                (0x8042dc03, int(justify)), (0, 0)]
        for i, (key, value) in enumerate(tags):
            self.mem.w32(TAGS + i * 8, key)
            self.mem.w32(TAGS + i * 8 + 4, value)
        self.mem.w32(MESSAGE, 0x103)  # OM_SET
        self.mem.w32(MESSAGE + 4, TAGS)
        self.call('Floattext__OM_SET', CLASS, OBJECT, MESSAGE)
        self.check_bounds()
        assert self.mem.r16(DATA + 6) == 0, (text, 'typesetting left set', self.mem.r16(DATA + 6))
        assert len(self.allocations) == 1, 'layout scratch allocation leaked'
        assert self.mem.r_cstr(self.mem.r32(DATA)) == text
        return self.lines


def check(path):
    h = FloattextHarness(path)
    try:
        # Real class registry lookup, including the List superclass graph.
        h.call('LibOpen')
        cl = h.call('ZUNE_GetBuiltinClass', h.string('Floattext.mui'), MASTER)
        assert cl, 'Floattext is missing from the built-in class registry'
        assert h.mem.r32(cl + REFS) == 1
        h.call('MUI_FreeClass', cl)
        h.call('LibClose')
        h.assert_expunged()

        # Allocate a List superclass object through the real constructor.
        h.mem.w32(MESSAGE, 0x101)
        h.mem.w32(MESSAGE + 4, TAGS)
        assert h.call('Floattext__OM_NEW', CLASS, OBJECT, MESSAGE) == OBJECT
        cases = [('hello world', 80, None, ['hello', 'world']),
                 ('abc', 1, None, ['a', 'b', 'c']),
                 ('abc', 0, None, []), ('abc', -1, None, []),
                 ('xxx\nhello', 80, 'x', ['hello']),
                 ('\nhello', 80, None, ['', 'hello']),
                 ('\x1b', 80, None, []),
                 ('hello\x1b', 80, None, ['hello\x1b']),
                 ('a\tb', 16, None, ['a        ', 'b']),
                 ('\t\t', 1, None, ['        ']),
                 ('\x1bbhello world', 80, None, ['\x1bbhello', '\x1bbworld']),
                 ('xcax', 8, 'x', ['c', 'a']),
                 ('xxx', 80, 'x', []), ('', 80, None, [])]
        for text, width, skip, expected in cases:
            assert h.set(text, width, skip) == expected, (text, h.lines)
        assert h.set('ab cd ef', 40, justify=True)
        # Replacing with the current text must copy before releasing it.
        h.mem.w32(TAGS + 4, h.mem.r32(DATA))
        h.call('Floattext__OM_SET', CLASS, OBJECT, MESSAGE)
        assert h.mem.r_cstr(h.mem.r32(DATA)) == 'ab cd ef'
        # Force each temporary allocation to fail after copying new text.
        for successful in [1, 2]:
            h.fail_after = successful
            h.set('hello world', 80)
            h.fail_after = None
            assert h.set('retry', 80) == ['retry']
        h.mem.w32(MESSAGE, 0x102)
        h.call('Floattext__OM_DISPOSE', CLASS, OBJECT, MESSAGE)
        assert not h.allocations
    finally:
        h.machine.cleanup()
    print(f'{path.name}: Floattext registration, wrapping, narrow/empty text, '
          'tabs, ownership and allocation-failure cleanup OK')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

#!/usr/bin/env python3
"""Execute native String selection and focus forwarding on a 68000.

Contracts come from the bundled BetterString autodoc and implementation,
whose public extension IDs String exposes. This is not a MUI5 claim.
OS allocation, superclass handling and window focus dispatch are modeled.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness, EXEC, INTUITION, MASTER, STACK

CLASS, OBJECT, DATA = 0x70000, 0x71000, 0x71100
UTILITY, TAGS, MESSAGE, STORE = 0x72000, 0x73000, 0x73400, 0x73500
MRI, WINDOW, INPUT = 0x74000, 0x75000, 0xd0000
CONTENTS, MAXLEN, CURSOR = 0x80428ffd, 0x80424984, 0x80428b6c
SELECT, UP, DOWN, LIST = 0xad001001, 0xad001008, 0xad001009, 0x80420fd2


class StringHarness(Harness):
    def __init__(self, path, maxlen=81, **focus):
        self.guards = {}
        super().__init__(path)
        self.redraws = 0
        self.focus_events = []
        self.mem.w16(CLASS + 32, DATA - OBJECT)
        self.mem.w32(self.symbols['_UtilityBase'], UTILITY)
        if '___UtilityBase' in self.symbols:
            self.mem.w32(self.symbols['___UtilityBase'], UTILITY)
        self.trap(UTILITY - 48, self.next_tag)
        self.trap(self.symbols['_DoSuperNewTags'], lambda: self.cpu.w_reg(0, OBJECT))
        self.trap(self.symbols['_DoSuperMethodA'], lambda: self.cpu.w_reg(0, 1))
        self.trap(self.symbols['_MUI_Redraw'], self.redraw)
        self.trap(INTUITION - 84, lambda: None)  # CurrentTime
        self.trap(INTUITION - 648, self.set_attrs)
        self.trap(EXEC - 624, self.copy)
        self.trap(EXEC - 630, self.copy)
        tags = [(MAXLEN, maxlen)]
        tags += [(UP, focus['up'])] if 'up' in focus else []
        tags += [(DOWN, focus['down'])] if 'down' in focus else []
        self.message(tags, 0x101)
        assert self.call('String__OM_NEW', CLASS, OBJECT, MESSAGE) == OBJECT

    def call(self, name, *args):
        for i, value in enumerate(args):
            self.mem.w32(STACK + 4 * i, value)
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_' + name], STACK)
        assert self.machine.was_exit(self.machine.execute(10000000)), name
        assert self.cpu.r_sp() == STACK
        assert self.lock_depth == 0
        return self.cpu.r_reg(0)

    def alloc(self, size):
        block = super().alloc(size + 16)
        self.guards[block] = size
        self.mem.w_block(block + size, b'!' * 16)
        return block

    def bounds(self):
        for block, size in self.guards.items():
            assert bytes(self.mem.r_block(block + size, 16)) == b'!' * 16

    def free_vec(self):
        block = self.cpu.r_reg(9)
        if block:
            self.bounds()
            del self.guards[block]
            super().free_vec()

    def copy(self):
        self.mem.w_block(self.cpu.r_reg(9),
                         self.mem.r_block(self.cpu.r_reg(8), self.cpu.r_reg(0)))

    def next_tag(self):
        storage = self.cpu.r_reg(8)
        tag = self.mem.r32(storage)
        self.cpu.w_reg(0, tag if tag and self.mem.r32(tag) else 0)
        self.mem.w32(storage, tag + 8)

    def redraw(self):
        self.redraws += 1

    def set_attrs(self):
        obj, tags = self.cpu.r_reg(8), self.cpu.r_reg(9)
        self.focus_events.append((obj, self.mem.r32(tags), self.mem.r32(tags + 4)))
        self.cpu.w_reg(0, 1)

    def message(self, tags, method=0x103):
        flat = [word & 0xffffffff for pair in tags for word in pair] + [0, 0]
        self.mem.w_block(TAGS, struct.pack('>' + 'I' * len(flat), *flat))
        self.mem.w_block(MESSAGE, struct.pack('>3I', method, TAGS, 0))

    def set(self, *tags):
        self.message(tags)
        self.call('String__OM_SET', CLASS, OBJECT, MESSAGE)
        self.bounds()

    def contents(self, text):
        self.mem.w_block(INPUT, text.encode() + b'\0')
        self.set((CONTENTS, INPUT))

    def get(self, attr):
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x104, attr, STORE))
        assert self.call('String__OM_GET', CLASS, OBJECT, MESSAGE)
        return self.mem.r32(STORE)

    def key(self, key):
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0, 0, key))
        return self.call('String__MUIM_HandleEvent', CLASS, OBJECT, MESSAGE)

    def finish(self):
        self.call('String__OM_DISPOSE', CLASS, OBJECT, MESSAGE)
        assert not self.allocations and not self.guards
        self.machine.cleanup()


def check(path):
    h = StringHarness(path)
    # Bounds, signed length, -1 as one character, and cursor-end sentinel.
    for cursor in (0, 2, 6, 99, 0xffffffff):
        for length in (-2147483648, -7, -3, -1, 0, 1, 3, 7, 2147483647):
            h.contents('abcdef')
            # Simulate stale word/all-selection and keyboard-marking state;
            # a programmatic selection must always return to character mode.
            h.mem.w16(DATA + 84, 2)
            h.mem.w32(DATA, 12)
            old_redraws = h.redraws
            h.set((CURSOR, cursor), (SELECT, length))
            start = min(cursor, 6)
            end = min(max(start + length, 0), 6)
            assert h.get(SELECT) == (end - start) & 0xffffffff
            assert h.get(CURSOR) == start
            assert h.mem.r16(DATA + 84) == 0
            assert not h.mem.r32(DATA) & 8
            assert h.redraws > old_redraws
            assert h.mem.r_cstr(h.get(CONTENTS)) == 'abcdef'
            h.call('String__MUIM_ClearSelected', CLASS, OBJECT, MESSAGE)
            lo, hi = sorted((start, end))
            assert h.mem.r_cstr(h.get(CONTENTS)) == 'abcdef'[:lo] + 'abcdef'[hi:]
            assert h.get(SELECT) == 0
            assert h.get(CURSOR) == lo
    # Zero selection clears a preceding selection; repeated sizes remain relative
    # to the cursor rather than extending the previous marked endpoint.
    h.contents('abcdef')
    h.set((CURSOR, 2), (SELECT, 3))
    h.set((SELECT, -1))
    assert h.get(SELECT) == 0xffffffff and h.get(CURSOR) == 2
    h.set((SELECT, 0))
    assert h.get(SELECT) == 0
    h.finish()

    # 16-bit selection indices previously wrapped on long contents.
    h = StringHarness(path, maxlen=50001)
    h.contents('x' * 50000)
    h.set((CURSOR, 40000), (SELECT, 2147483647))
    assert h.get(SELECT) == 10000
    h.call('String__MUIM_ClearSelected', CLASS, OBJECT, MESSAGE)
    assert len(h.mem.r_cstr(h.get(CONTENTS))) == 40000
    h.set((SELECT, -2147483648))
    assert h.get(SELECT) == (-40000) & 0xffffffff
    h.call('String__MUIM_ClearSelected', CLASS, OBJECT, MESSAGE)
    assert h.mem.r_cstr(h.get(CONTENTS)) == ''
    h.finish()

    h = StringHarness(path, up=0x1111, down=0x2222)
    assert h.get(UP) == 0x1111 and h.get(DOWN) == 0x2222
    h.mem.w32(OBJECT + 28, MRI)
    h.mem.w32(MRI, WINDOW)
    h.mem.w16(DATA + 242, 1)  # active String
    h.key(2); h.key(3)
    assert h.focus_events == [(WINDOW, 0x80427925, 0x1111),
                              (WINDOW, 0x80427925, 0x2222)]
    h.set((UP, 0x3333), (DOWN, 0), (LIST, 0x4444))
    assert h.get(UP) == 0x3333 and h.get(DOWN) == 0
    h.focus_events.clear()
    h.key(2); h.key(3)
    assert h.focus_events == [(WINDOW, 0x80427925, 0x3333),
                              (0x4444, 0x8042391c, (-5) & 0xffffffff)]
    # An absent render context must not be dereferenced; list behavior remains.
    h.mem.w32(OBJECT + 28, 0)
    h.focus_events.clear()
    h.key(2)
    assert h.focus_events == [(0x4444, 0x8042391c, (-4) & 0xffffffff)]
    h.mem.w16(DATA + 242, 0)
    h.focus_events.clear()
    assert h.key(2) == 0 and not h.focus_events
    h.finish()
    print(f'{path.name}: signed String selection, long ranges and focus forwarding passed')


if __name__ == '__main__':
    for arg in sys.argv[1:]:
        check(Path(arg))

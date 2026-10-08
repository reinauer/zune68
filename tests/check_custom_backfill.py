#!/usr/bin/env python3
"""Run Area background routing on linked 68000 code.

MUI 3.9 guest probes establish inclusive bounds, zero reserved offsets,
explicit background requests and runtime disabling of custom backfill.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness, INTUITION

CLASS, OBJECT, MESSAGE, TAGS, UTILITY = 0x70000, 0x71000, 0x72000, 0x73000, 0x74000
CUSTOM = 0x80420a63
BACKGROUND, BACKFILL = 0x804238ca, 0x80428d73


class BackfillHarness(Harness):
    def __init__(self, path):
        super().__init__(path)
        self.events = []
        self.mem.w16(CLASS + 32, 28)
        self.mem.w32(CLASS + 8, self.symbols['_Area_Dispatcher'])
        self.mem.w32(OBJECT - 4, CLASS)
        self.mem.w32(OBJECT + 64, 1 << 14)  # MADF_CANDRAW.
        self.mem.w32(self.symbols['_UtilityBase'], UTILITY)
        if '___UtilityBase' in self.symbols:
            self.mem.w32(self.symbols['___UtilityBase'], UTILITY)
        self.trap(UTILITY - 48, self.next_tag)
        self.trap(self.symbols['_DoSuperMethodA'], lambda: self.cpu.w_reg(0, 1))
        self.trap(self.symbols['_DoMethod'], self.method)

    def next_tag(self):
        where = self.cpu.r_reg(8)
        tag = self.mem.r32(where)
        while tag and self.mem.r32(tag) == 1:
            tag += 8
        self.cpu.w_reg(0, tag if tag and self.mem.r32(tag) else 0)
        self.mem.w32(where, tag + 8)

    def method(self):
        args = [self.mem.r32(self.cpu.r_sp() + 4 + i * 4) for i in range(9)]
        self.events.append(args)
        if args[1] == BACKFILL:
            assert self.mem.r32(OBJECT + 68) & 2
        self.cpu.w_reg(0, 1)

    def enable(self, value):
        self.mem.w_block(TAGS, struct.pack('>4I', CUSTOM, value, 0, 0))
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x103, TAGS, 0))
        self.call('DoMethodA', OBJECT, MESSAGE)

    def enabled(self):
        storage = MESSAGE + 64
        self.mem.w32(storage, 0xdeadbeef)
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x104, CUSTOM, storage))
        assert self.call('DoMethodA', OBJECT, MESSAGE) == 1
        return self.mem.r32(storage)

    def background(self, left, top, width, height):
        self.events.clear()
        self.mem.w_block(MESSAGE, struct.pack('>8I', BACKGROUND,
            left & 0xffffffff, top & 0xffffffff, width, height, 100, 200, 0))
        return self.call('DoMethodA', OBJECT, MESSAGE)


def check_group(path):
    h = BackfillHarness(path)
    h.mem.w16(CLASS + 32, 246)
    childlist, node, child = 0x75000, 0x75100, 0x7510c
    h.mem.w32(childlist, node)
    h.mem.w32(node, childlist + 4)
    h.mem.w32(childlist + 4, 0)
    def get_children():
        h.mem.w32(h.cpu.r_reg(9), childlist)
        h.cpu.w_reg(0, 1)
    def tags_at(address):
        tags = []
        while h.mem.r32(address):
            if h.mem.r32(address) != 1:
                tags.append((h.mem.r32(address), h.mem.r32(address + 4)))
            address += 8
        return tags
    seen = []
    def super_set():
        msg = h.mem.r32(h.cpu.r_sp() + 12)
        seen.append(('parent', tags_at(h.mem.r32(msg + 4))))
        h.cpu.w_reg(0, 1)
    def child_set():
        assert h.mem.r32(h.cpu.r_sp() + 4) == child
        msg = h.mem.r32(h.cpu.r_sp() + 8)
        seen.append(('child', tags_at(h.mem.r32(msg + 4))))
        h.cpu.w_reg(0, 1)
    h.trap(INTUITION - 654, get_children)
    h.trap(h.symbols['_DoSuperMethodA'], super_set)
    h.trap(h.symbols['_DoMethodA'], child_set)
    unknown = 0xad123456
    h.mem.w_block(TAGS, struct.pack('>6I', CUSTOM, 0, unknown, 17, 0, 0))
    h.mem.w_block(MESSAGE, struct.pack('>3I', 0x103, TAGS, 0))
    h.call('Group__OM_SET', CLASS, OBJECT, MESSAGE)
    assert seen == [('parent', [(CUSTOM, 0), (unknown, 17)]),
                    ('child', [(unknown, 17)])], seen
    assert not h.allocations
    h.machine.cleanup()


def check(path):
    h = BackfillHarness(path)
    assert h.enabled() == 0
    h.enable(1)
    assert h.enabled() == 1
    assert h.mem.r32(OBJECT + 68) & 1
    for left, top, width, height in [(8, 17, 13, 11), (-9, -7, 4, 3), (0, 0, 1, 1)]:
        assert h.background(left, top, width, height) == 1
        assert len(h.events) == 1
        assert h.events[0][:8] == [OBJECT, BACKFILL, left & 0xffffffff,
            top & 0xffffffff, (left + width - 1) & 0xffffffff,
            (top + height - 1) & 0xffffffff, 0, 0]
    assert h.background(0, 0, 0, 1) == 0 and not h.events
    assert h.background(0, 0, 1, 0) == 0 and not h.events
    h.mem.w32(OBJECT + 64, 0)
    assert h.background(0, 0, 5, 5) == 0 and not h.events
    h.mem.w32(OBJECT + 64, 1 << 14)
    # A custom painter's superclass call must use ordinary background
    # painting instead of dispatching the callback recursively.
    assert h.mem.r32(OBJECT + 68) == 1
    h.mem.w32(OBJECT + 68, 3)
    h.background(0, 0, 5, 5)
    assert len(h.events) == 1 and h.events[0][1] != BACKFILL
    h.mem.w32(OBJECT + 68, 1)
    h.enable(0)
    assert h.enabled() == 0
    assert not h.mem.r32(OBJECT + 68) & 1
    h.background(0, 0, 5, 5)
    assert len(h.events) == 1 and h.events[0][1] != BACKFILL
    assert not h.allocations
    h.machine.cleanup()
    print(f'{path}: custom backfill bounds, routing and disabled state passed')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))
        check_group(Path(name))

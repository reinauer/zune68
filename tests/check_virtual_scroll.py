#!/usr/bin/env python3
"""Execute virtual offset setters on linked 68000 Group code.

Model visibility and drawing at their boundaries; child geometry and
class ancestry traversal run unchanged, including hidden descendants.
"""
import struct
import sys
from pathlib import Path
from check_custom_backfill import BackfillHarness, CLASS, OBJECT, MESSAGE, TAGS
from check_class_lifetime import INTUITION

GROUP_OFFSET = 246
DATA = OBJECT + GROUP_OFFSET
LEFT, TOP, FORWARD = 0x80429371, 0x80425200, 0x80421422
CANDRAW, SHOWME = 1 << 14, 1 << 9
GROUP_VIRTUAL = 1 << 6
OFFX, OFFY, OLDX, OLDY, EXTW, EXTH = 92, 96, 100, 104, 108, 112


class ScrollHarness(BackfillHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w16(CLASS + 32, GROUP_OFFSET)
        self.mem.w32(DATA + 8, GROUP_VIRTUAL)
        self.mem.w16(OBJECT + 56, 100)
        self.mem.w16(OBJECT + 58, 80)
        for offset, value in ((OFFX, 20), (OFFY, 30), (OLDX, 20),
                              (OLDY, 30), (EXTW, 300), (EXTH, 240)):
            self.mem.w32(DATA + offset, value)
        self.mem.w32(0x70100 + 24, CLASS)  # Custom Group subclass.
        self.mem.w32(0x70200 + 24, 0)  # An Area, not a Group.
        self.families = {}
        self.hidden_boxes = []
        self.children = [0x7500c, 0x7540c, 0x7580c, 0x75c0c]
        for i, child in enumerate(self.children):
            self.mem.w32(child - 4, 0x70100 if i == 1 else 0x70200)
            self.mem.w16(child + 52, 100 + i * 10)
            self.mem.w16(child + 54, 120 + i * 10)
            self.mem.w16(child + 56, 20)
            self.mem.w16(child + 58, 10)
            self.mem.w32(child + 64, (CANDRAW | SHOWME) if i < 2 else 0)
        self.family(OBJECT, 0x77000, 0x78000, self.children[:3])
        self.family(self.children[1], 0x77100, 0x78100, self.children[3:])
        self.trap(INTUITION - 654, self.get_children)
        self.trap(self.symbols['_DoShowMethod'], lambda: self.visibility(True))
        self.trap(self.symbols['_DoHideMethod'], lambda: self.visibility(False))
        self.trap(self.symbols['_IsObjectVisible'], lambda: self.cpu.w_reg(0, 1))
        self.trap(self.symbols['_MUI_Redraw'], self.redraw)
        self.trap(self.symbols['_DoMethod'], self.unexpected_method)

    def family(self, obj, family, head, children):
        self.families[family] = head
        self.mem.w32(obj + GROUP_OFFSET, family)
        self.mem.w32(head, children[0] - 12)
        for i, child in enumerate(children):
            self.mem.w32(child - 12,
                children[i + 1] - 12 if i + 1 < len(children) else head + 4)
        self.mem.w32(head + 4, 0)

    def get_children(self):
        assert self.cpu.r_reg(0) == 0x80424b9e  # Family_List only.
        self.mem.w32(self.cpu.r_reg(9), self.families[self.cpu.r_reg(8)])
        self.cpu.w_reg(0, 1)

    def visibility(self, show):
        obj = self.mem.r32(self.cpu.r_sp() + 4)
        flags = self.mem.r32(obj + 64)
        self.mem.w32(obj + 64, flags | CANDRAW if show else flags & ~CANDRAW)
        if not show:
            self.hidden_boxes.append(struct.unpack('>2h',
                bytes(self.mem.r_block(obj + 52, 4))))
        self.events.append(('show' if show else 'hide', obj))
        self.cpu.w_reg(0, 1)

    def redraw(self):
        # Internal calls use the stack entry, not the library register gate.
        obj = self.mem.r32(self.cpu.r_sp() + 4)
        flags = self.mem.r32(self.cpu.r_sp() + 8)
        self.events.append(('draw', obj, flags))

    def unexpected_method(self):
        raise AssertionError('scroll setter invoked a method/layout hook')

    def scroll(self, left, top):
        self.mem.w_block(TAGS, struct.pack('>8I', FORWARD, 0,
            LEFT, left & 0xffffffff, TOP, top & 0xffffffff, 0, 0))
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x103, TAGS, 0))
        self.call('Group__OM_SET', CLASS, OBJECT, MESSAGE)

    def boxes(self):
        return [struct.unpack('>4h', bytes(self.mem.r_block(obj + 52, 8)))
                for obj in self.children]


def check(path):
    h = ScrollHarness(path)
    initial = h.boxes()
    h.scroll(50, 70)
    assert h.boxes() == [(x - 30, y - 40, w, v) for x, y, w, v in initial]
    assert h.events == [('hide', h.children[0]), ('hide', h.children[1]),
                        ('show', h.children[0]), ('show', h.children[1]),
                        ('draw', OBJECT, 1)]
    assert h.hidden_boxes == [(70, 80), (80, 90)]
    assert h.mem.r32(DATA + 64) == 2
    assert h.mem.r32(DATA + OLDX) == 20 and h.mem.r32(DATA + OLDY) == 30
    h.events.clear()
    h.scroll(50, 70)
    assert not h.events  # No geometry or drawing work for unchanged offsets.

    h.mem.w32(OBJECT + 64, 0)
    h.mem.w32(DATA + 64, 0)
    h.scroll(10000, -5)
    assert h.mem.r32(DATA + OFFX) == 200 and h.mem.r32(DATA + OFFY) == 0
    assert h.boxes() == [(x - 180, y + 30, w, v) for x, y, w, v in initial]
    assert not h.events  # Hidden parent still moves every descendant.
    assert not h.mem.r32(DATA + 64)
    h.mem.w32(DATA + EXTW, 50)
    h.mem.w32(DATA + EXTH, 40)
    h.scroll(1, 1)
    assert h.mem.r32(DATA + OFFX) == 0 and h.mem.r32(DATA + OFFY) == 0
    assert h.boxes() == [(x + 20, y + 30, w, v) for x, y, w, v in initial]
    assert not h.events and not h.allocations
    h.machine.cleanup()
    print(f'{path}: virtual scrolling shifts descendants without layout passed')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

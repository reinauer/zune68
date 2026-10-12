#!/usr/bin/env python3
"""Exercise the linked 68000 pointer backend with modeled Intuition.

The native probe separately checks real input.device hover dispatch and
SetWindowPointerA calls against MUI 5. Images here are Zune's own artwork.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness, INTUITION
from check_custom_backfill import BackfillHarness, OBJECT, MESSAGE, TAGS

WINDOW, SLOT, BORROWED = 0x75000, 0x76000, 0x77000
POINTER, BUSY = 0x80000000 + 99 + 0x34, 0x80000000 + 99 + 0x35
# AmigaOS 3 intuition.h: WA_Dummy = TAG_USER + 99.


class PointerHarness(Harness):
    def __init__(self, path):
        super().__init__(path)
        self.images = set()
        self.active = 0
        self.events = []
        self.fail_image = False
        self.bits = []
        self.trap(INTUITION - 636, self.new_pointer)
        self.trap(INTUITION - 642, self.dispose_pointer)
        self.trap(INTUITION - 816, self.set_pointer)

    def tags(self, address):
        result = {}
        while address and self.mem.r32(address):
            result[self.mem.r32(address)] = self.mem.r32(address + 4)
            address += 8
        return result

    def new_pointer(self):
        assert not self.cpu.r_reg(8)
        assert self.mem.r_cstr(self.cpu.r_reg(9)) == 'pointerclass'
        tags = self.tags(self.cpu.r_reg(10))
        bitmap = tags[0x80039001]  # POINTERA_BitMap
        assert self.mem.r16(bitmap) == 2 and self.mem.r16(bitmap + 2) == 16
        assert self.mem.r8(bitmap + 5) == 2
        self.bits.append(tuple(self.mem.r_block(self.mem.r32(bitmap + 8 + i * 4), 32)
                               for i in range(2)))
        if self.fail_image:
            self.cpu.w_reg(0, 0)
            self.fail_image = False
        else:
            image = self.alloc(16)
            self.images.add(image)
            self.cpu.w_reg(0, image)

    def dispose_pointer(self):
        image = self.cpu.r_reg(8)
        assert image in self.images, 'must not dispose borrowed pointers'
        assert image != self.active, 'detach before freeing an active image'
        self.images.remove(image)
        del self.allocations[image]
        self.events.append(('dispose', image))

    def set_pointer(self):
        assert self.cpu.r_reg(8) == WINDOW
        tags = self.tags(self.cpu.r_reg(9))
        self.active = tags.get(POINTER, 0)
        assert self.active == 0 or self.active == BORROWED or self.active in self.images
        self.events.append(('set', tags))

    def set(self, kind):
        return self.call('ZuneSetPointer', WINDOW, kind & 0xffffffff, SLOT)

    def free(self):
        self.call('ZuneFreePointer', WINDOW, SLOT)
        assert not self.mem.r32(SLOT) and not self.images and not self.allocations


def check(path):
    h = BackfillHarness(path)
    try:
        for value in (0, 1, 13, 16, 30, 39, BORROWED, 0xffffffff, 0xfffffffe):
            h.mem.w_block(TAGS, struct.pack('>4I', 0x8042b467, value, 0, 0))
            h.mem.w_block(MESSAGE, struct.pack('>3I', 0x103, TAGS, 0))
            h.call('DoMethodA', OBJECT, MESSAGE)
            storage = MESSAGE + 64
            h.mem.w_block(MESSAGE, struct.pack('>3I', 0x104, 0x8042b467, storage))
            assert h.call('DoMethodA', OBJECT, MESSAGE) == 1
            assert h.mem.r32(storage) == value
            h.mem.w_block(MESSAGE, struct.pack('>3I', 0x8042e212, 17, 23))
            assert h.call('DoMethodA', OBJECT, MESSAGE) == value
    finally:
        h.machine.cleanup()
    h = PointerHarness(path)
    try:
        assert h.set(-1) == 0 and not h.allocations
        assert h.set(0) == 0 and h.events[-1] == ('set', {})
        assert h.set(1) == 1 and h.events[-1] == ('set', {BUSY: 1})
        for kind in range(2, 40):
            assert h.set(kind) == kind
            assert h.active in h.images and len(h.images) == 1
            plane0, plane1 = h.bits[-1]
            assert any(plane0 + plane1) == (kind != 16), (kind, 'empty glyph')
            assert all(not (a & b) for a, b in zip(plane0, plane1)), 'outline overlaps'
        assert h.set(BORROWED) == BORROWED and h.active == BORROWED
        h.free()
        # No allocation is needed to borrow an image or select a system pointer.
        assert h.set(BORROWED) == BORROWED and not h.allocations
        h.free()
        h.fail_alloc = True
        assert h.set(13) == 0 and not h.allocations
        h.fail_image = True
        assert h.set(13) == 0 and not h.images
        h.free()
        assert h.set(30) == 30
        h.fail_image = True
        assert h.set(13) == 0 and not h.images and not h.active
        h.free()
    finally:
        h.machine.cleanup()
    print(f'{path.name}: pointer attributes, images, ownership and failure cleanup OK')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

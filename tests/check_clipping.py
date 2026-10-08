#!/usr/bin/env python3
"""Check linked clip ownership and rectangular virtual clipping on a 68000."""
import struct
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import INTUITION

OBJECT, GFX, LAYERS = 0x71000, 0x79000, 0x79800
MRI, RP, LAYER = 0x7a000, 0x7b000, 0x7c000
WINDOW, SCREEN = 0x7d000, 0x7e000


class ClipHarness(NotifyHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w32(self.symbols['_GfxBase'], GFX)
        self.mem.w32(self.symbols['_LayersBase'], LAYERS)
        self.mem.w32(MRI + 20, RP)
        self.mem.w32(RP, LAYER)
        self.mem.w32(OBJECT + 28, MRI)
        self.mem.w32(OBJECT + 64, (1 << 14) | (1 << 29))
        self.mem.w_block(OBJECT + 52, struct.pack('>4h', 0, 0, 100, 100))
        self.parents = {}
        self.rectangles = []
        self.draws = 0
        self.protocol = []
        self.trap(LAYERS - 120, lambda: self.protocol.append('lock'))
        self.trap(LAYERS - 138, lambda: self.protocol.append('unlock'))
        self.trap(INTUITION - 354, lambda: self.protocol.append('begin'))
        self.trap(INTUITION - 366, self.end_refresh)
        self.fail_region = False
        self.fail_rect = False
        self.fail_intersect = False
        self.trap(GFX - 516, lambda: self.cpu.w_reg(0,
                  0 if self.fail_region else self.alloc(12)))
        self.trap(GFX - 534, self.dispose_region)
        self.trap(GFX - 510, self.rect)
        self.trap(GFX - 624, lambda: self.cpu.w_reg(0, not self.fail_intersect))
        self.trap(LAYERS - 174, self.install)
        self.trap(INTUITION - 654, self.get_attr)
        self.trap(self.symbols['_DoMethodA'], self.draw)

    def draw(self):
        self.draws += 1

    def dispose_region(self):
        self.cpu.w_reg(9, self.cpu.r_reg(8))
        self.free_vec()

    def end_refresh(self):
        assert self.cpu.r_reg(0) == 0, 'keep pending damage'
        self.protocol.append('end')

    def install(self):
        self.protocol.append('install')
        old = self.mem.r32(LAYER + 126)
        self.mem.w32(LAYER + 126, self.cpu.r_reg(9))
        self.cpu.w_reg(0, old)

    def rect(self):
        region, rectangle = self.cpu.r_reg(8), self.cpu.r_reg(9)
        bounds = bytes(self.mem.r_block(rectangle, 8))
        self.rectangles.append(struct.unpack('>4h', bounds))
        self.mem.w_block(region, bounds)
        self.cpu.w_reg(0, not self.fail_rect)

    def get_attr(self):
        obj, attr, storage = self.cpu.r_reg(8), self.cpu.r_reg(0), self.cpu.r_reg(9)
        self.mem.w32(storage, self.parents.get(obj, 0) if attr == 0x8042e35f else 0)
        self.cpu.w_reg(0, 1)

    def parent(self, child, x, y, w, h, inset=0):
        parent = self.blob(bytes(128))
        self.parents[child] = parent
        self.mem.w32(parent + 64, 1 << 30)
        self.mem.w_block(parent + 52, struct.pack('>4h', x, y, w, h))
        # Area margins are four UBYTE fields: left, top, subwidth, subheight.
        self.mem.w_block(parent + 60, bytes([inset, inset, inset * 2, inset * 2]))
        return parent

    def finish(self):
        assert not self.allocations
        assert self.mem.r32(MRI + 168) == 0
        self.check_freed()
        self.machine.cleanup()


def check_virtual_paint(path):
    for failure in ('none', 'region', 'rect', 'intersect'):
        h = ClipHarness(path)
        cl = 0x70000
        h.mem.w16(cl + 32, 246)
        outer = h.blob(bytes(12))
        h.mem.w32(LAYER + 126, outer)
        h.fail_region = failure == 'region'
        h.fail_rect = failure == 'rect'
        if failure == 'intersect':
            h.call('ZuneAddClipRegion', MRI, h.alloc(12))
            h.fail_intersect = True
        old_clip = h.mem.r32(LAYER + 126)
        count = h.mem.r32(MRI + 168)
        result = h.call('Group__MUIM_Virtgroup_BeginPaint', cl, OBJECT)
        assert bool(result) == (failure == 'none')
        if result:
            assert h.mem.r32(LAYER + 126) != old_clip
            assert h.mem.r32(MRI + 168) == count + 1
            assert not h.call('Group__MUIM_Virtgroup_BeginPaint', cl, OBJECT)
        h.call('Group__MUIM_Virtgroup_EndPaint', cl, OBJECT)
        assert h.mem.r32(LAYER + 126) == old_clip
        assert h.mem.r32(MRI + 168) == count
        if failure == 'intersect':
            h.call('ZuneRemoveClipRegion', MRI, outer)
        h.finish()


def check(path):
    # A damaged layer is not necessarily inside BeginRefresh/BeginUpdate.
    for simple in [False, True]:
        for flags, managed in [(0, False), (0x80, False), (0x90, False), (0x90, True)]:
            h = ClipHarness(path)
            h.mem.w32(MRI + 16, WINDOW)
            h.mem.w32(WINDOW + 24, 0x40 if simple else 0)
            h.mem.w32(WINDOW + 46, SCREEN)
            h.mem.w32(WINDOW + 124, LAYER)
            h.mem.w16(LAYER + 30, flags)
            h.mem.w32(MRI + 24, 8 if managed else 0)
            clip = h.call('ZuneAddClipRegion', MRI, h.alloc(12))
            if flags & 0x10 and not managed:
                assert clip == 0xffffffff and not h.protocol
            else:
                assert clip != 0xffffffff
                h.call('ZuneRemoveClipRegion', MRI, clip)
                expected = ['end', 'install', 'begin'] if managed else ['lock', 'install', 'unlock']
                assert h.protocol == expected * 2, h.protocol
            h.finish()
    h = ClipHarness(path)
    outer = h.blob(bytes(12))
    h.mem.w32(LAYER + 126, outer)
    first, second = h.alloc(12), h.alloc(12)
    a = h.call('ZuneAddClipRegion', MRI, first)
    b = h.call('ZuneAddClipRegion', MRI, second)
    assert (a, b) == (outer, first)
    h.call('ZuneRemoveClipRegion', MRI, b)
    assert h.mem.r32(LAYER + 126) == first
    h.call('ZuneRemoveClipRegion', MRI, a)
    assert h.mem.r32(LAYER + 126) == outer, 'lost external clipping region'
    h.call('ZuneRemoveClipRegion', MRI, a)  # empty stack cannot underflow
    h.finish()

    h = ClipHarness(path)
    clip = h.call('ZuneAddClipping', MRI, 10, 10, 0, 20)
    assert not h.rectangles, 'empty clipping rectangle sent to Layers'
    h.call('ZuneRemoveClipRegion', MRI, clip)
    h.finish()

    for failure in ['none', 'region', 'rect', 'empty']:
        h = ClipHarness(path)
        child = OBJECT
        for n in range(12):
            child = h.parent(child, n, n, 100 - 2 * n, 100 - 2 * n, inset=2)
        if failure == 'empty':
            h.parent(child, 200, 200, 10, 10)
        h.fail_region = failure == 'region'
        h.fail_rect = failure == 'rect'
        h.call('MUI_Redraw', OBJECT, 1)
        assert h.draws == (1 if failure == 'none' else 0)
        if failure == 'none':
            assert h.rectangles == [(13, 13, 86, 86)], h.rectangles
        h.finish()

    h = ClipHarness(path)
    a = h.call('ZuneAddClipRegion', MRI, h.alloc(12))
    h.fail_intersect = True
    assert h.call('ZuneAddClipRegion', MRI, h.alloc(12)) == 0xffffffff
    assert h.mem.r32(MRI + 168) == 1
    h.call('ZuneRemoveClipRegion', MRI, a)
    h.finish()
    print(f'{path.name}: nested clip ownership and 12 ancestors / one rectangle pass')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))
        check_virtual_paint(Path(name))

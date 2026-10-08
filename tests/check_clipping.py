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
        self.region_allocations = 0
        self.gets = 0
        self.protocol = []
        self.trap(LAYERS - 120, lambda: self.protocol.append('lock'))
        self.trap(LAYERS - 138, lambda: self.protocol.append('unlock'))
        self.trap(INTUITION - 354, lambda: self.protocol.append('begin'))
        self.trap(INTUITION - 366, self.end_refresh)
        self.fail_region = False
        self.fail_rect = False
        self.fail_intersect = False
        self.trap(GFX - 516, self.new_region)
        self.trap(GFX - 534, self.dispose_region)
        self.trap(GFX - 510, self.rect)
        self.trap(GFX - 624, lambda: self.cpu.w_reg(0, not self.fail_intersect))
        self.trap(LAYERS - 174, self.install)
        self.trap(INTUITION - 654, self.get_attr)
        self.trap(self.symbols['_DoMethodA'], self.draw)

    def draw(self):
        self.draws += 1

    def new_region(self):
        self.region_allocations += 1
        self.cpu.w_reg(0, 0 if self.fail_region else self.alloc(12))

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
        self.gets += 1
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

    # BeginPaint exposes only newly uncovered pixels after a scroll. The
    # subclass still receives a full-viewport background callback, so its
    # drawing must be bounded by this clip, including diagonal scrolls.
    viewport = (13, 24, 104, 93)
    cases = [
        (0, 0, 0, [viewport]),
        (2, 0, 7, [(13, 87, 104, 93)]),
        (2, 0, -7, [(13, 24, 104, 30)]),
        (2, 9, 0, [(96, 24, 104, 93)]),
        (2, -9, 0, [(13, 24, 21, 93)]),
        (2, 9, -7, [(96, 24, 104, 93), (13, 24, 104, 30)]),
        (2, 200, 0, [viewport]),
        (2, 0, -200, [viewport]),
        (2, 0, 0, []),
    ]
    for update, dx, dy, expected in cases:
        for fail_second in (False, True) if len(expected) == 2 else (False,):
            h = ClipHarness(path)
            cl, data = 0x70000, OBJECT + 246
            h.mem.w16(cl + 32, 246)
            h.mem.w_block(OBJECT + 52, struct.pack('>4h', 10, 20, 100, 80))
            h.mem.w_block(OBJECT + 60, bytes([3, 4, 8, 10]))
            h.mem.w32(data + 64, update)
            h.mem.w32(data + 92, (100 + dx) & 0xffffffff)
            h.mem.w32(data + 96, (100 + dy) & 0xffffffff)
            h.mem.w32(data + 100, 100)
            h.mem.w32(data + 104, 100)
            if fail_second:
                def rect():
                    h.rect()
                    if len(h.rectangles) == 2:
                        h.cpu.w_reg(0, 0)
                h.trap(GFX - 510, rect)
            result = h.call('Group__MUIM_Virtgroup_BeginPaint', cl, OBJECT)
            assert bool(result) != fail_second
            assert h.rectangles == expected, (h.rectangles, expected)
            h.call('Group__MUIM_Virtgroup_EndPaint', cl, OBJECT)
            h.finish()


def check_refresh_culling(path):
    # Guest probes establish window-relative damage with a NULL ClipRegion.
    # A nonzero screen-space Layer.bounds must not shift this damage again.
    def window(h):
        h.mem.w32(MRI + 16, WINDOW)
        h.mem.w32(WINDOW + 50, RP)
        h.mem.w32(WINDOW + 124, LAYER)

    sentinel = struct.pack('>4h', -90, -80, 190, 180)
    cases = [
        ((0, 0, 100, 100), True, True, True, (16, 22, 39, 37)),
        ((40, 22, 10, 16), True, True, False, None),
        ((39, 22, 1, 16), True, True, True, (39, 22, 39, 37)),
        ((16, 38, 24, 10), True, True, False, None),
        # Explicit redraws outside BeginRefresh ignore retained damage.
        ((100, 100, 20, 20), False, True, True, (100, 100, 119, 119)),
        # Empty damage is not evidence that a draw should be suppressed.
        ((100, 100, 20, 20), True, False, True, (100, 100, 119, 119)),
    ]
    for box, refreshing, nonempty, drawn, expected in cases:
        h = ClipHarness(path)
        window(h)
        h.mem.w32(OBJECT + 64, 1 << 14)
        h.mem.w_block(OBJECT + 52, struct.pack('>4h', *box))
        h.mem.w32(MRI + 16, WINDOW)
        h.mem.w32(WINDOW + 124, LAYER)
        h.mem.w32(MRI + 24, 8 if refreshing else 0)
        h.mem.w_block(LAYER + 16, struct.pack('>4h', 40, 50, 339, 249))
        damage = h.blob(struct.pack('>4hI', 16, 22, 39, 37, 1 if nonempty else 0))
        h.mem.w32(LAYER + 156, damage)
        h.mem.w_block(MRI + 172, sentinel)
        h.call('MUI_Redraw', OBJECT, 1)
        assert h.draws == int(drawn), (box, refreshing, nonempty)
        actual = bytes(h.mem.r_block(MRI + 172, 8))
        assert actual == (struct.pack('>4h', *expected) if drawn else sentinel)
        if not drawn:
            assert h.mem.r32(OBJECT + 64) == 1 << 14
        assert not h.region_allocations and not h.protocol
        assert h.mem.r32(LAYER + 126) == 0
        h.finish()

    # Reject before walking virtual parents or allocating their clip region.
    h = ClipHarness(path)
    window(h)
    h.parent(OBJECT, 0, 0, 100, 100)
    h.mem.w32(MRI + 24, 8)
    damage = h.blob(struct.pack('>4hI', 150, 150, 170, 170, 1))
    h.mem.w32(LAYER + 156, damage)
    h.mem.w_block(MRI + 172, sentinel)
    flags = h.mem.r32(OBJECT + 64)
    h.call('MUI_Redraw', OBJECT, 1)
    assert not h.draws and not h.gets and not h.region_allocations
    assert bytes(h.mem.r_block(MRI + 172, 8)) == sentinel
    assert h.mem.r32(OBJECT + 64) == flags
    h.finish()

    # An object can overlap damage but lie outside its ancestor/user clip.
    # The later rejection must release the installed region and leave the
    # previous shared clip rectangle and external region untouched.
    for virtual in (False, True):
        h = ClipHarness(path)
        window(h)
        h.mem.w32(MRI + 24, 8)
        damage = h.blob(struct.pack('>4hI', 16, 22, 39, 37, 1))
        h.mem.w32(LAYER + 156, damage)
        outer = h.blob(struct.pack('>4hI', 50, 50, 90, 90, 1))
        h.mem.w32(LAYER + 126, outer)
        if virtual:
            h.parent(OBJECT, 50, 50, 40, 40)
        else:
            h.mem.w32(OBJECT + 64, 1 << 14)
        h.mem.w_block(MRI + 172, sentinel)
        flags = h.mem.r32(OBJECT + 64)
        h.call('MUI_Redraw', OBJECT, 1)
        assert not h.draws
        assert h.region_allocations == int(virtual)
        assert h.mem.r32(LAYER + 126) == outer
        assert bytes(h.mem.r_block(MRI + 172, 8)) == sentinel
        assert h.mem.r32(OBJECT + 64) == flags
        h.finish()

    # Bounds-only culling intentionally retains an object in a region hole.
    h = ClipHarness(path)
    window(h)
    h.mem.w32(OBJECT + 64, 1 << 14)
    h.mem.w_block(OBJECT + 52, struct.pack('>4h', 40, 40, 10, 10))
    h.mem.w32(MRI + 24, 8)
    # RegionRectangle bounds are relative to the Region's bounding box.
    right = h.blob(struct.pack('>II4h', 0, 0, 90, 0, 99, 99))
    left = h.blob(struct.pack('>II4h', right, 0, 0, 0, 9, 99))
    damage = h.blob(struct.pack('>4hI', 0, 0, 99, 99, left))
    h.mem.w32(LAYER + 156, damage)
    h.call('MUI_Redraw', OBJECT, 1)
    assert h.draws == 1 and not h.region_allocations
    assert struct.unpack('>4h', h.mem.r_block(MRI + 172, 8)) == (40, 40, 49, 49)
    h.finish()
    # Only the observed ordinary window coordinate system is optimized.
    for mode in ('no window', 'scroll x', 'scroll y', 'other rastport', 'buffer'):
        h = ClipHarness(path)
        window(h)
        h.mem.w32(OBJECT + 64, 1 << 14)
        h.mem.w32(MRI + 24, 8)
        damage = h.blob(struct.pack('>4hI', 150, 150, 170, 170, 1))
        h.mem.w32(LAYER + 156, damage)
        if mode == 'no window':
            h.mem.w32(MRI + 16, 0)
        elif mode == 'scroll x':
            h.mem.w16(LAYER + 44, 1)
        elif mode == 'scroll y':
            h.mem.w16(LAYER + 46, 1)
        elif mode == 'other rastport':
            h.mem.w32(WINDOW + 50, RP + 256)
        else:
            h.mem.w32(MRI + 316, 1)
            h.trap(GFX - 552, lambda: None)  # Existing buffered ClipBlit.
        h.call('MUI_Redraw', OBJECT, 1)
        assert h.draws == 1 and not h.region_allocations, mode
        assert struct.unpack('>4h', h.mem.r_block(MRI + 172, 8)) == (0, 0, 99, 99)
        h.finish()
    print(f'{path.name}: refresh damage culls before allocation, bounds draw clips, '
          'preserves early-return state and leaves explicit redraws unchanged')


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
        check_refresh_culling(Path(name))

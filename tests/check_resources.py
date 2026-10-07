#!/usr/bin/env python3
"""Linked image allocation, pen ownership and planar fallback regressions."""
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import EXEC

CLASS, OBJECT, DATA = 0x70000, 0x71000, 0x71100
GFX, MRI, SCREEN, BM, RP = 0x79000, 0x7a000, 0x7b000, 0x7c000, 0x7d000


class ResourceHarness(NotifyHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 256)
        self.mem.w32(self.symbols['_GfxBase'], GFX)
        self.mem.w32(OBJECT + 28, MRI)
        self.mem.w32(MRI + 4, SCREEN)
        self.mem.w32(MRI + 20, RP)
        self.mem.w32(SCREEN + 48, 123)
        self.mem.w32(SCREEN + 84 + 4, BM)
        for offset, val in [(52, 0), (54, 0), (56, 8), (58, 2)]:
            self.mem.w16(OBJECT + offset, val)
        self.mem.w32(DATA, BM)
        self.mem.w32(DATA + 8, self.blob(bytes(256 * 12)))
        self.mem.w32(DATA + 12, 8)
        self.mem.w32(DATA + 16, 2)
        self.mem.w32(DATA + 24, 0xffffffff)
        self.bitmaps = {BM: (8, 2, 2)}
        self.obtained, self.released, self.blits = [], [], []
        self.waited = False
        self.fail_mask = False
        self.fail_pen = False
        self.fail_number = 0
        self.alloc_calls = 0
        self.trap(self.symbols['_DoSuperMethodA'], lambda: self.cpu.w_reg(0, 1))
        self.trap(self.symbols['_dt_load_picture'], lambda: self.cpu.w_reg(0, 0))
        self.trap(self.symbols['_dt_dispose_picture'], lambda: None)
        self.trap(EXEC - 624, lambda: self.mem.w_block(self.cpu.r_reg(9),
                  self.mem.r_block(self.cpu.r_reg(8), self.cpu.r_reg(0))))
        self.trap(GFX - 198, lambda: None)
        self.trap(GFX - 228, self.wait)
        self.trap(GFX - 918, self.alloc_bitmap)
        self.trap(GFX - 924, self.free_bitmap)
        self.trap(GFX - 960, self.bitmap_attr)
        self.trap(GFX - 492, self.alloc_mask)
        self.trap(GFX - 498, self.free_mask)
        self.trap(GFX - 768, self.read_pixels)
        self.trap(GFX - 774, lambda: None)
        self.trap(GFX - 840, self.obtain_pen)
        self.trap(GFX - 948, self.release_pen)
        self.trap(GFX - 1008, lambda: self.cpu.w_reg(0, 3))
        self.trap(GFX - 606, lambda: self.blits.append('opaque'))
        self.trap(GFX - 636, lambda: self.blits.append('masked'))

    def alloc_vec(self):
        self.alloc_calls += 1
        self.fail_alloc = self.alloc_calls == self.fail_number
        super().alloc_vec()

    def wait(self):
        self.waited = True

    def alloc_bitmap(self):
        addr = self.alloc(40)
        self.bitmaps[addr] = tuple(self.cpu.r_reg(i) for i in range(3))
        self.cpu.w_reg(0, addr)

    def free_bitmap(self):
        assert self.waited
        addr = self.cpu.r_reg(8)
        assert addr != BM, 'borrowed bitmap freed'
        del self.bitmaps[addr]
        self.cpu.w_reg(9, addr)
        self.free_vec()

    def bitmap_attr(self):
        width, height, depth = self.bitmaps[self.cpu.r_reg(8)]
        self.cpu.w_reg(0, {0: height, 4: depth, 8: width}[self.cpu.r_reg(1)])

    def alloc_mask(self):
        self.cpu.w_reg(0, 0 if self.fail_mask else self.alloc(4))

    def free_mask(self):
        assert self.waited, 'mask freed before blitter finished'
        self.cpu.w_reg(9, self.cpu.r_reg(8))
        self.free_vec()

    def read_pixels(self):
        self.mem.w_block(self.cpu.r_reg(10), bytes(self.cpu.r_reg(2)))

    def obtain_pen(self):
        if not self.fail_pen:
            self.obtained.append(0)
        self.cpu.w_reg(0, 0xffffffff if self.fail_pen else 0)

    def release_pen(self):
        assert self.cpu.r_reg(8) == 123
        self.released.append(self.cpu.r_reg(0))

    def bitmap(self, method):
        return self.call('Bitmap__' + method, CLASS, OBJECT, self.words(0))

    def finish(self):
        self.check_freed()
        assert not self.allocations, self.allocations
        assert self.bitmaps == {BM: (8, 2, 2)}
        assert self.obtained == self.released
        self.machine.cleanup()


def check(path):
    for cleanup in [False, True]:
        for failed_pen in [False, True]:
            h = ResourceHarness(path)
            h.fail_pen = failed_pen
            h.mem.w32(DATA + 24, 0)
            h.bitmap('MUIM_Setup')
            assert len(h.obtained) == (0 if failed_pen else 1), 'pen zero repeatedly obtained'
            h.mem.w32(DATA + 42, 0x80000000)
            h.mem.w16(DATA + 46, 1)
            h.bitmap('MUIM_Draw')
            assert h.blits == ['masked'], 'nonzero alpha must retain visible planar content'
            h.mem.w32(DATA + 42, 0)
            h.bitmap('MUIM_Draw')
            assert len(h.blits) == 1
            if cleanup:
                h.bitmap('MUIM_Cleanup')
                h.bitmap('MUIM_Cleanup')
            h.bitmap('OM_DISPOSE')
            h.finish()

    h = ResourceHarness(path)
    h.fail_mask = True
    h.mem.w32(DATA + 24, 0)
    h.bitmap('MUIM_Setup')
    h.bitmap('MUIM_Draw')
    assert h.blits == ['opaque']
    h.bitmap('OM_DISPOSE')
    h.finish()

    # Image-spec wrapper, first filename and optional second filename.
    for text in ['3:test.image', '4:test0', '4:', '5:test']:
        for failure in range(1, 5):
            h = ResourceHarness(path)
            h.fail_number = failure
            spec = h.call('zune_imspec_setup', h.blob(text.encode() + b'\0'), MRI)
            if spec:
                h.call('zune_imspec_cleanup', spec)
            h.finish()

    h = ResourceHarness(path)
    assert not h.call('NewImageContainer', 65535, 65535)
    assert not h.call('NewImageContainer', 0, 8)
    assert not h.call('zune_font_get', OBJECT, 0xfffffff7)
    image = h.call('NewImageContainer', 8, 8)
    assert image
    h.call('DisposeImageContainer', image)
    h.finish()
    # A failed custom frame must never dispose uninitialized image pointers.
    h = ResourceHarness(path)
    h.mem.w32(h.symbols['_DOSBase'], 0x7e000)
    h.trap(0x7e000 - 870, lambda: h.cpu.w_reg(0, h.cpu.r_reg(1)))
    h.trap(0x72000 - 162, lambda: h.cpu.w_reg(0, 0))
    h.trap(h.symbols['_ReadFrameConfig'], lambda: h.cpu.w_reg(0, 0))
    def allocate_dirty():
        flags = h.cpu.r_reg(1)
        h.alloc_vec()
        address = h.cpu.r_reg(0)
        if address and not (flags & 0x10000):
            h.mem.w_block(address, b'\xa5' * h.allocations[address])
    h.trap(EXEC - 684, allocate_dirty)
    assert not h.call('load_custom_frame', h.blob(b'frame.config\0'), SCREEN)
    h.finish()

    # Failed tile bitmap allocation must not cache an unusable backfill.
    h = ResourceHarness(path)
    h.mem.w32(h.symbols['_DataTypesBase'], 0x7e000)
    def dt_attributes():
        storage = h.mem.r32(h.cpu.r_reg(10) + 4)
        h.mem.w32(storage, BM)
        h.cpu.w_reg(0, 1)
    h.trap(0x7e000 - 66, dt_attributes)
    h.trap(GFX - 918, lambda: h.cpu.w_reg(0, 0))
    node = h.blob(bytes(192))
    for offset, value in [(12, 123), (16, 8), (20, 2), (24, SCREEN)]:
        h.mem.w32(node + offset, value)
    h.call('dt_put_on_rastport_tiled', node, RP, 0, 0, 31, 31, 0, 0)
    assert h.mem.r32(node + 32) == 0
    h.finish()

    h = ResourceHarness(path)
    h.mem.w32(MRI + 28, 123)
    pen = h.blob(bytes(32))
    h.call('zune_penspec_fill_rgb', pen, 0, 0, 0)
    assert h.call('zune_penspec_setup', pen, MRI)
    h.call('zune_penspec_cleanup', pen)
    h.call('zune_penspec_cleanup', pen)
    assert h.released == [0]
    h.finish()
    print(f'{path.name}: image failure cleanup, pen ownership and planar fallback pass')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

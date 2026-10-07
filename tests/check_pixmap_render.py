#!/usr/bin/env python3
"""Exercise linked native Pixmap rendering through modeled graphics vectors."""
import struct
import sys
from pathlib import Path
from check_pixmap import Pixmap, DATA, MESSAGE, OBJECT

CYBER, MRI, RP, CLUT = 0x7b000, 0x76000, 0x76100, 0xd0000
BACKGROUND = (17, 37, 97)


class RenderPixmap(Pixmap):
    def __init__(self, path, fail=0):
        super().__init__(path, fail)
        self.mem.w32(self.symbols['_CyberGfxBase'], CYBER)
        self.mem.w32(MRI + 20, RP)
        self.mem.w32(DATA + 40, 32)
        self.writes = []
        self.reads = 0
        self.pixels = {}
        self.trap(CYBER - 120, self.read_pixels)
        self.trap(CYBER - 126, self.write_pixels)
        self.trap(CYBER - 198, lambda: self.write_pixels(lut=True))

    def read_pixels(self):
        address = self.cpu.r_reg(8)
        sx, sy, stride, dx, dy, width, height, fmt = [self.cpu.r_reg(i) for i in range(8)]
        assert fmt == 2 and height == 1 and stride == width * 4
        self.mem.w_block(address + sy * stride + sx * 4,
                         bytes((255, *BACKGROUND)) * width)
        self.reads += 1
        self.cpu.w_reg(0, width)

    def write_pixels(self, lut=False):
        address = self.cpu.r_reg(8)
        sx, sy, stride, dx, dy, width, height, fmt = [self.cpu.r_reg(i) for i in range(8)]
        size = 1 if lut else 3 if fmt == 0 else 4
        assert lut or fmt in (0, 2)
        assert stride <= 65535
        self.writes.append((width, height, stride))
        for y in range(height):
            for x in range(width):
                pixel = address + (sy + y) * stride + (sx + x) * size
                if lut:
                    rgb = self.mem.r32(self.cpu.r_reg(10) + self.mem.r8(pixel) * 4)
                    color = ((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255)
                else:
                    color = tuple(self.mem.r_block(pixel + (size == 4), 3))
                self.pixels[dx + x, dy + y] = color
        self.cpu.w_reg(0, width * height)

    def draw(self, sx, sy, width, height, alpha=255):
        self.mem.w32(DATA + 28, alpha * 0x01010101)
        self.mem.w_block(MESSAGE, struct.pack('>8I', 0x8042ce0f,
            sx & 0xffffffff, sy & 0xffffffff, width, height, MRI, 3, 5))
        self.call('MUIM_Pixmap_DrawSection', MESSAGE)


class PlanarPixmap(Pixmap):
    def __init__(self, path, bitmap_fail=False, mask_fail=False, pen_fail=0):
        super().__init__(path, fail=1 if mask_fail else 0)
        self.bitmap_fail = bitmap_fail
        self.bitmap = False
        self.pen_fail = pen_fail
        self.pen_count = 0
        self.pens = set()
        self.waited = False
        self.points = []
        self.mem.w32(self.symbols['_GfxBase'], 0x7c000)
        self.mem.w32(OBJECT + 28, MRI)  # Area's public RenderInfo field
        self.mem.w32(MRI + 4, 0x76200)
        self.mem.w32(0x76200 + 48, 0x76500)  # Screen.ViewPort.ColorMap
        self.mem.w32(0x76200 + 88, 0x76600)  # Screen.RastPort.BitMap
        self.trap(self.symbols['_SetSuperAttrs'], lambda: self.cpu.w_reg(0, 1))
        self.trap(0x7c000 - 960, lambda: self.cpu.w_reg(0, 4))
        self.trap(0x7c000 - 918, self.alloc_bitmap)
        self.trap(0x7c000 - 924, self.free_bitmap)
        self.trap(0x7c000 - 228, self.wait_blit)
        self.trap(0x7c000 - 198, lambda: None)
        self.trap(0x7c000 - 840, self.obtain_pen)
        self.trap(0x7c000 - 948, self.release_pen)
        self.trap(0x7c000 - 342, lambda: None)
        self.trap(0x7c000 - 324, self.write_pixel)

    def alloc_bitmap(self):
        assert not self.bitmap
        self.bitmap = not self.bitmap_fail
        self.cpu.w_reg(0, 0x77000 if self.bitmap else 0)

    def wait_blit(self):
        self.waited = True

    def free_bitmap(self):
        assert self.bitmap and self.waited and self.cpu.r_reg(8) == 0x77000
        self.bitmap = self.waited = False

    def obtain_pen(self):
        self.pen_count += 1
        assert self.cpu.r_reg(8) == 0x76500
        if self.pen_count == self.pen_fail:
            self.cpu.w_reg(0, 0xffffffff)
        else:
            self.pens.add(self.pen_count)
            self.cpu.w_reg(0, self.pen_count)

    def release_pen(self):
        assert self.cpu.r_reg(8) == 0x76500
        self.pens.remove(self.cpu.r_reg(0))

    def write_pixel(self):
        self.points.append((self.cpu.r_reg(0), self.cpu.r_reg(1)))
        self.cpu.w_reg(0, 0)

    def clean(self):
        self.call('MUIM_Cleanup', MESSAGE)
        assert not self.bitmap and not self.pens and not self.allocated
        assert not self.mem.r32(DATA + 52)  # no retained ColorMap


def check_planar(path):
    argb = bytes((255, 255, 0, 0, 255, 0, 255, 0,
                  0, 0, 0, 255, 255, 255, 255, 255))
    for failure in ({}, {'bitmap_fail': True}, {'mask_fail': True},
                    {'pen_fail': 1}, {'pen_fail': 2}, {'pen_fail': 3}):
        h = PlanarPixmap(path, **failure)
        assert h.decode(argb, 0, 4, format=2)
        h.mem.w32(DATA + 28, 0xffffffff)
        h.call('MUIM_Setup', MESSAGE)
        if not failure:
            assert h.bitmap and len(h.pens) == 3
            assert h.points == [(0, 0), (1, 0), (3, 0)]
            assert h.mem.r8(h.mem.r32(DATA + 56)) == 0xd0
        else:
            assert not h.bitmap and not h.pens and not h.allocated
        h.clean()
        if not failure:
            h.call('MUIM_Setup', MESSAGE)
            assert h.bitmap and len(h.pens) == 3
            h.clean()
        h.finish()


def check(path):
    # Truecolor output, partial global and per-pixel alpha, chunk boundaries,
    # source sections, and strides beyond CyberGraphX's 16-bit interface.
    cases = [(fmt, alpha, width, sx, sy, sw, sh)
             for fmt in (0, 1, 2) for alpha in (0, 128, 255)
             for width, sx, sy, sw, sh in
             [(5, 1, 0, 3, 2), (520, 2, 0, 513, 2),
              (32767, 32760, 1, 7, 1)]]
    for fmt, alpha, width, sx, sy, sw, sh in cases:
        h = RenderPixmap(path)
        source = bytearray()
        palette = [((i * 23) & 255, (i * 71) & 255, (i * 113) & 255)
                   for i in range(256)]
        originals = []
        for i in range(width * 2):
            color = ((i * 29) & 255, (i * 67) & 255, (i * 137) & 255)
            own_alpha = (i * 31) & 255 if fmt == 2 else 255
            if fmt == 0:
                source.append(i & 255)
                color = palette[i & 255]
            else:
                source.extend(((own_alpha,) if fmt == 2 else ()) + color)
            originals.append((own_alpha, color))
        assert h.decode(bytes(source), 0, width, 2, fmt)
        h.mem.w_block(CLUT, b''.join(bytes((0, *color)) for color in palette))
        h.mem.w32(DATA + 24, CLUT)
        h.draw(sx, sy, sw, sh, alpha)
        if not alpha:
            assert not h.pixels and not h.count and not h.reads
        else:
            assert len(h.pixels) == sw * sh
            for y in range(sh):
                for x in range(sw):
                    own, rgb = originals[(sy + y) * width + sx + x]
                    a = (own * alpha + 127) // 255
                    expected = tuple((c * a + b * (255 - a) + 127) // 255
                                     for c, b in zip(rgb, BACKGROUND))
                    assert h.pixels[3 + x, 5 + y] == expected, (fmt, alpha, width, x, y)
            assert h.peak <= 1024
            if fmt != 2 and alpha == 255:
                assert h.count == 0 and h.reads == 0
        h.finish()
    # Failure to allocate blend scratch must neither draw nor leak.
    h = RenderPixmap(path, fail=1)
    assert h.decode(bytes((128, 255, 0, 0)), 0, 1, format=2)
    h.draw(0, 0, 1, 1)
    assert not h.pixels and not h.reads
    h.finish()
    # Source rectangles are clipped to the image, and negative origins reject.
    h = RenderPixmap(path)
    assert h.decode(bytes((255, 0, 0)) * 2, 0, 2)
    h.draw(1, 0, 20, 20)
    assert h.pixels == {(3, 5): (255, 0, 0)}
    h.pixels.clear()
    h.draw(-1, 0, 1, 1)
    assert not h.pixels
    h.finish()
    check_planar(path)
    print('Pixmap rendering: alpha, wide strides, clipping, bounded scratch and planar cleanup passed')


if __name__ == '__main__':
    check(Path(sys.argv[1]))

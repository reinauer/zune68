#!/usr/bin/env python3
"""Run native Pixmap decoding, replacement and failure cleanup on a 68000."""
import bz2
import struct
import sys
from pathlib import Path
from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from amitools.vamos.machine.machine import Machine
from amitools.vamos.machine.backend import Backend

BASE, STACK = 0x10000, 0xf0000
CLASS, OBJECT, DATA, MESSAGE, TAGS, STORE = 0x70000, 0x71000, 0x71100, 0x73000, 0x74000, 0x75000
EXEC, UTILITY = 0x78000, 0x79000


class Pixmap:
    def __init__(self, path, fail=0):
        image = BinFmt().load_image(str(path))
        reloc = Relocate(image)
        self.machine = Machine(Backend.get_default().create_machine(ram_size_kib=8192))
        self.mem, self.cpu = self.machine.mem, self.machine.cpu
        self.mem.w_block(BASE, bytes(reloc.relocate_one_block(BASE)))
        self.symbols = {s.name.decode(): address + s.offset
                        for seg, address in zip(image.get_segments(), reloc.get_seq_addrs(BASE))
                        if seg.symtab for s in seg.symtab.symbols}
        self.allocated = {}
        self.next = 0x100000
        self.count = 0
        self.fail = fail
        self.peak = 0
        self.mem.w16(CLASS + 32, DATA - OBJECT)
        self.mem.w_block(DATA + 60, b'\xff' * 1024)  # no acquired pens
        for name, value in [('_SysBase', EXEC), ('_UtilityBase', UTILITY),
                            ('___UtilityBase', UTILITY)]:
            if name in self.symbols:
                self.mem.w32(self.symbols[name], value)
        self.trap(EXEC - 684, self.alloc)
        self.trap(EXEC - 690, self.free)
        self.trap(EXEC - 624, self.copy)
        self.trap(EXEC - 630, self.copy)
        self.trap(UTILITY - 48, self.next_tag)
        self.trap(UTILITY - 138, lambda: self.cpu.w_reg(0,
            (self.cpu.r_reg(0) * self.cpu.r_reg(1)) & 0xffffffff))
        self.trap(UTILITY - 156, self.divide)
        self.trap(self.symbols['_DoSuperMethodA'], lambda: self.cpu.w_reg(0, 1))
        self.mem.w32(self.symbols['_MUIMasterBase'], 0x7a000)
        self.trap(0x7a000 - 102, lambda: None)

    def trap(self, address, callback):
        number = self.machine.traps.alloc(lambda op, pc: callback())
        self.mem.w16(address, 0xa000 | number)
        self.mem.w16(address + 2, 0x4e75)

    def alloc(self):
        self.count += 1
        size = self.cpu.r_reg(0)
        if self.count == self.fail:
            self.cpu.w_reg(0, 0)
            return
        address = self.next + 16
        self.next += (size + 47) & ~15
        assert self.next < 0x7f0000
        self.mem.w_block(address - 16, b'!' * (size + 32))
        if self.cpu.r_reg(1) & 0x10000:
            self.mem.w_block(address, bytes(size))
        self.allocated[address] = size
        self.peak = max(self.peak, sum(self.allocated.values()))
        self.cpu.w_reg(0, address)

    def free(self):
        address = self.cpu.r_reg(9)
        if not address: return
        size = self.allocated.pop(address)
        assert bytes(self.mem.r_block(address - 16, 16)) == b'!' * 16
        assert bytes(self.mem.r_block(address + size, 16)) == b'!' * 16
        self.mem.w_block(address, b'\xa5' * size)

    def copy(self):
        self.mem.w_block(self.cpu.r_reg(9), self.mem.r_block(self.cpu.r_reg(8), self.cpu.r_reg(0)))

    def divide(self):
        quotient, remainder = divmod(self.cpu.r_reg(0), self.cpu.r_reg(1))
        self.cpu.w_reg(0, quotient)
        self.cpu.w_reg(1, remainder)

    def next_tag(self):
        storage = self.cpu.r_reg(8)
        tag = self.mem.r32(storage)
        self.cpu.w_reg(0, tag if tag and self.mem.r32(tag) else 0)
        self.mem.w32(storage, tag + 8)

    def call(self, method, message):
        for i, value in enumerate([CLASS, OBJECT, message]):
            self.mem.w32(STACK + i * 4, value)
        self.machine.prepare(self.symbols['_Pixmap__' + method], STACK)
        assert self.machine.was_exit(self.machine.execute(100000000))
        assert self.cpu.r_sp() == STACK
        return self.cpu.r_reg(0)

    def decode(self, data, compression, width, height=1, format=1):
        self.mem.w_block(0x80000, data)
        for offset, value in [(8, 0x80000), (12, width), (16, height),
                              (20, format), (32, compression), (36, len(data))]:
            self.mem.w32(DATA + offset, value & 0xffffffff)
        self.mem.w_block(MESSAGE, struct.pack('>III', 0x103, 0x8042b085, STORE))
        self.call('OM_GET', MESSAGE)
        return self.mem.r32(STORE)

    def replace(self, data):
        self.mem.w_block(0x90000, data)
        self.mem.w_block(TAGS, struct.pack('>6I', 0x80429ea0, 0x90000,
                                         0x8042ce74, 0, 0, 0))
        self.mem.w_block(MESSAGE, struct.pack('>3I', 0x103, TAGS, 0))
        return self.call('OM_SET', MESSAGE)

    def finish(self):
        self.call('OM_DISPOSE', MESSAGE)
        assert not self.allocated, self.allocated
        self.machine.cleanup()


def check(path):
    rgb = bytes([255, 0, 0, 0, 255, 0])
    streams = [(0, rgb), (1, bytes([5]) + rgb), (2, bz2.compress(rgb))]
    for compression, stream in streams:
        h = Pixmap(path)
        result = h.decode(stream, compression, 2)
        assert result and bytes(h.mem.r_block(result, len(rgb))) == rgb
        count = h.count
        h.call('OM_GET', MESSAGE)
        assert h.count == count, 'decoded data should be reused'
        assert h.replace(bytes(reversed(rgb)))
        assert h.mem.r32(DATA + 44) == 0x90000
        h.finish()
        for failure in range(1, count + 1):
            h = Pixmap(path, failure)
            assert h.decode(stream, compression, 2) == 0
            h.finish()
    # RLE lengths, truncated streams, corrupt bzip CRC and oversized dimensions.
    bad = [(1, b'\x80', 2), (1, b'\xffX', 2), (1, b'\x05ab', 2),
           (1, b'\x04abcde', 2), (1, b'\x05abcdefX', 2),
           (2, bz2.compress(rgb)[:-2], 2), (2, b'BZh9broken', 2),
           (9, rgb, 2), (0, rgb, 0), (0, rgb, -1), (0, rgb, 65536)]
    corrupt = bytearray(bz2.compress(rgb)); corrupt[10] ^= 1
    bad.append((2, bytes(corrupt), 2))
    for compression, stream, width in bad:
        h = Pixmap(path)
        assert not h.decode(stream, compression, width)
        h.finish()
    print('Pixmap: raw/RLE/bzip2, replacement, allocation failures and malformed input passed')


if __name__ == '__main__':
    check(Path(sys.argv[1]))

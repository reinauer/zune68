#!/usr/bin/env python3
"""Exercise native initialization and every library-open failure on a 68000."""
import sys
from pathlib import Path
from check_class_lifetime import Harness, EXEC, MASTER, STACK

GRAPHICS = 0x79000


class InitHarness(Harness):
    def __init__(self, path, fail=0, allocation_failure=0):
        super().__init__(path)
        self.mem.w32(self.symbols["_IntuitionBase"], 0)
        self.fail = fail
        self.allocation_failure = allocation_failure
        self.allocation_number = 0
        self.opens = []
        self.live_libraries = set()
        self.font = False
        self.locale = False
        self.trap(EXEC - 198, self.alloc_mem)
        self.trap(EXEC - 204, self.alloc_abs)
        self.trap(EXEC - 210, self.free_mem)
        self.trap(EXEC - 558, lambda: None)
        self.trap(GRAPHICS - 72, self.open_font)
        self.trap(GRAPHICS - 78, self.close_font)
        self.trap(self.symbols['_Locale_Initialize'], self.locale_init)
        self.trap(self.symbols['_Locale_Deinitialize'], self.locale_exit)

    def open_library(self):
        name = self.mem.r_cstr(self.cpu.r_reg(9))
        self.opens.append(name)
        if len(self.opens) == self.fail:
            self.cpu.w_reg(0, 0)
            return
        address = GRAPHICS if name == 'graphics.library' else self.alloc(64)
        self.live_libraries.add(address)
        self.cpu.w_reg(0, address)

    def close_library(self):
        address = self.cpu.r_reg(9)
        if address == GRAPHICS:
            assert not self.font
        assert not self.locale
        self.live_libraries.remove(address)
        if address != GRAPHICS:
            del self.allocations[address]

    def alloc_mem(self):
        self.allocation_number += 1
        value = 0 if self.allocation_number == self.allocation_failure else self.alloc(self.cpu.r_reg(0))
        self.cpu.w_reg(0, value)

    def alloc_abs(self):
        # The trigger address may already be occupied; this is optional.
        self.cpu.w_reg(0, 0)

    def free_mem(self):
        address = self.cpu.r_reg(9)
        assert self.cpu.r_reg(0) == self.allocations.pop(address)

    def open_font(self):
        self.font = True
        self.cpu.w_reg(0, self.alloc(64))

    def close_font(self):
        assert self.font
        del self.allocations[self.cpu.r_reg(9)]
        self.font = False

    def locale_init(self):
        self.locale = True

    def locale_exit(self):
        self.locale = False

    def call(self, name):
        self.mem.w32(STACK, MASTER)
        self.machine.prepare(self.symbols['_' + name], STACK)
        assert self.machine.was_exit(self.machine.execute(200000))
        return self.cpu.r_reg(0)


def check(path):
    h = InitHarness(path)
    # Execute real teardown here, rather than the class harness's stub.
    # Restore the bytes overwritten by Harness.trap at L_ExpungeLib.
    from amitools.binfmt.BinFmt import BinFmt
    from amitools.binfmt.Relocate import Relocate
    image = BinFmt().load_image(str(path))
    data = bytes(Relocate(image).relocate_one_block(0x10000))

    def restore_cleanup(h):
        address = h.symbols['_L_ExpungeLib']
        h.mem.w_block(address, data[address - 0x10000:address - 0x10000 + 4])

    restore_cleanup(h)
    assert h.call('L_InitLib')
    assert 'muiscreen.library' not in h.opens
    assert 'cybergraphics.library' not in h.opens
    count = len(h.opens)
    size = h.mem.r32(h.symbols['_LibInitTable'])
    pens = h.mem.r32(MASTER + size - 4)
    for index, expected in enumerate([b's3', b'', b's7', b'', b's4', b's2', b's5', b's8']):
        assert h.mem.r_cstr(pens + 32 * index).encode() == expected, (index, h.mem.r_cstr(pens + 32 * index), expected)
    h.call('L_ExpungeLib')
    assert not h.live_libraries and not h.allocations
    h.machine.cleanup()
    for failed_open in range(1, count + 1):
        h = InitHarness(path, fail=failed_open)
        restore_cleanup(h)
        assert not h.call('L_InitLib')
        assert not h.live_libraries and not h.allocations
        h.machine.cleanup()
    h = InitHarness(path, allocation_failure=1)
    restore_cleanup(h)
    assert not h.call('L_InitLib')
    assert not h.live_libraries and not h.allocations
    h.machine.cleanup()
    print(f'{path.name}: native startup, pens and {count} failed opens unwind cleanly')


if __name__ == '__main__':
    for path in sys.argv[1:]:
        check(Path(path))

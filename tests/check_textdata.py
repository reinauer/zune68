#!/usr/bin/env python3
"""Run fallback Unicode conversions on a linked 68000 library.

Exercise malformed input, bounded buffers, supplementary characters,
conversion caching and disposal with codesets.library absent.
"""
import sys
from pathlib import Path
from check_notifications import NotifyHarness, CLASS, OBJECT, UTILITY
from check_class_lifetime import STACK

DATA = OBJECT + 256
CONVERT = 0x80425988


class TextdataHarness(NotifyHarness):
    def __init__(self, path, source, encoding):
        super().__init__(path)
        if '___UtilityBase' in self.symbols:
            self.mem.w32(self.symbols['___UtilityBase'], UTILITY)
        self.trap(UTILITY - 138, lambda: self.cpu.w_reg(0,
            (self.cpu.r_reg(0) * self.cpu.r_reg(1)) & 0xffffffff))
        self.trap(UTILITY - 156, self.divide)
        self.mem.w16(CLASS + 32, 256)
        pointer = self.blob(source)
        for offset, value in [(0, pointer), (4, encoding), (8, len(source))]:
            self.mem.w32(DATA + offset, value)

    def divide(self):
        quotient, remainder = divmod(self.cpu.r_reg(0), self.cpu.r_reg(1))
        self.cpu.w_reg(0, quotient)
        self.cpu.w_reg(1, remainder)

    def alloc_vec(self):
        clear = bool(self.cpu.r_reg(1) & 0x10000)
        super().alloc_vec()
        address = self.cpu.r_reg(0)
        if address and not clear:
            self.mem.w_block(address, b'\xa5' * self.allocations[address])

    def dispatch(self, method, encoding=0):
        message = self.words(method, encoding)
        self.cpu.w_reg(8, CLASS)
        self.cpu.w_reg(10, OBJECT)
        self.cpu.w_reg(9, message)
        self.machine.prepare(self.symbols['_Textdata_Dispatcher'], STACK)
        result = self.machine.execute(1000000)
        assert self.machine.was_exit(result), 'Textdata dispatcher'
        assert self.cpu.r_sp() == STACK
        return self.cpu.r_reg(0)


def check(path):
    cases = [
        (b'Hello', 4, 106, b'Hello\0'),
        (b'\xff\xe9', 4, 106, b'\xc3\xbf\xc3\xa9\0'),
        (b'\xc3\xa9\xf0\x9f\x98\x80', 106, 4, b'\xe9?\0'),
        (b'ab', 4, 1013, b'\0a\0b\0\0'),
        (b'\xf0\x9f\x98\x80', 106, 1013, b'\xd8=\xde\0\0\0'),
        (b'\xd8=\xde\0', 1013, 106, b'\xf0\x9f\x98\x80\0'),
        (b'\x08\0', 1013, 106, b'\xe0\xa0\x80\0'),
        (b'\0a\0b', 1013, 4, b'ab\0'),
        (b'', 4, 1013, b'\0\0'),
    ]
    for source, source_encoding, target, expected in cases:
        h = TextdataHarness(path, source, source_encoding)
        result = h.dispatch(CONVERT, target)
        assert result and h.mem.r_block(result, len(expected)) == expected
        assert h.dispatch(CONVERT, target) == result, 'conversion cache'
        h.dispatch(0x102)
        assert not h.allocations, 'conversion storage leaked'
        h.machine.cleanup()
    for source, source_encoding in [(b'\xc0\x80', 106), (b'\xe2\x82', 106),
                                     (b'\xed\xa0\x80', 106),
                                     (b'\xd8=', 1013), (b'\0', 1013)]:
        h = TextdataHarness(path, source, source_encoding)
        assert not h.dispatch(CONVERT, 4), 'malformed input accepted'
        assert not h.allocations, 'failed conversion leaked'
        h.machine.cleanup()
    h = TextdataHarness(path, b'x' * 1024, 106)
    result = h.dispatch(CONVERT, 4)
    assert result and h.allocations[result] <= 1025, 'narrow conversion overallocated'
    assert h.mem.r_block(result, 1025) == b'x' * 1024 + b'\0'
    h.dispatch(0x102)
    assert not h.allocations
    h.machine.cleanup()
    print(f'{path.name}: fallback encodings, malformed input and cleanup pass')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

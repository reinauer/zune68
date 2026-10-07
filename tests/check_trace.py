#!/usr/bin/env python3
"""Run the diagnostic backend with modelled DOS handles and real putchars."""
import struct
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import EXEC, STACK, MASTER

DOS = 0x79000


class TraceHarness(NotifyHarness):
    def trap(self, address, fn):
        if address == self.symbols.get('_ZuneTraceOutput'):
            return  # execute the real backend, unlike unrelated harnesses
        super().trap(address, fn)

    def __init__(self, path):
        super().__init__(path)
        self.mem.w32(self.symbols['_DOSBase'], DOS)
        self.console = False
        self.length = 0
        self.opens, self.closed, self.writes = [], [], []
        self.fail_open = False
        self.fail_seek = False
        self.trap(DOS - 60, lambda: self.cpu.w_reg(0, 8 if self.console else 0))
        self.trap(DOS - 30, self.open_file)
        self.trap(DOS - 36, lambda: self.closed.append(self.cpu.r_reg(1)))
        self.trap(DOS - 66, self.seek)
        self.trap(DOS - 48, self.write)
        self.trap(DOS - 354, lambda: self.writes.append('console'))  # VFPrintf
        self.trap(DOS - 360, lambda: None)  # Flush
        self.trap(EXEC - 522, self.raw_format)
        self.mem.w16(EXEC - 520, 0x4ef9)  # trap then jump through callback script

    def call(self, name, *args):
        for n, value in enumerate(args):
            self.mem.w32(STACK + n * 4, value)
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_' + name], STACK)
        assert self.machine.was_exit(self.machine.execute(2000000))
        assert self.lock_depth == 0
        return self.cpu.r_reg(0)

    def open_file(self):
        mode = self.cpu.r_reg(2)
        self.opens.append(mode)
        if mode == 1006:
            self.length = 0
        self.cpu.w_reg(0, 0 if self.fail_open else 4)

    def seek(self):
        self.cpu.w_reg(0, 0xffffffff if self.fail_seek else self.length)

    def write(self):
        count = self.cpu.r_reg(3)
        self.writes.append(bytes(self.mem.r_block(self.cpu.r_reg(2), count)))
        self.length += count
        assert self.length <= 65536
        self.cpu.w_reg(0, count)

    def raw_format(self):
        # DOS/Exec formatting is modelled; execute the library's actual
        # register callback for every emitted character, including NUL.
        value = self.mem.r_cstr(self.cpu.r_reg(8)).encode() + b'\0'
        buf, callback = self.cpu.r_reg(11), self.cpu.r_reg(10)
        code = bytearray()
        for char in value:
            code += b'\x20\x3c' + struct.pack('>I', char)
            code += b'\x26\x7c' + struct.pack('>I', buf)
            code += b'\x4e\xb9' + struct.pack('>I', callback)
        code += b'\x4e\x75'
        self.mem.w32(EXEC - 518, self.blob(code))


def check(path):
    h = TraceHarness(path)
    if '_ZuneTraceOutput' not in h.symbols:
        h.machine.cleanup()
        print(f'{path.name}: diagnostics absent from quiet library')
        return
    h.length = 65530
    h.call('ZuneTraceOutput', h.blob(b'0123456789\0'))
    assert h.opens == [1004, 1006] and len(h.closed) == 2
    assert h.writes == [b'0123456789'] and h.length == 10
    h.call('ZuneTraceOutput', h.blob(b'x' * 700 + b'\0'))
    assert h.writes[-1] == b'x' * 511
    h.fail_seek = True
    h.call('ZuneTraceOutput', h.blob(b'ignored\0'))
    assert len(h.writes) == 2 and len(h.closed) == 4
    h.fail_seek = False
    h.busy = True
    h.call('ZuneTraceOutput', h.blob(b'contended\0'))
    assert len(h.opens) == 4
    h.busy = False
    h.fail_open = True
    h.call('ZuneTraceOutput', h.blob(b'unavailable\0'))
    assert len(h.closed) == 4
    h.console = True
    h.call('ZuneTraceOutput', h.blob(b'console\0'))
    assert h.writes[-1] == 'console' and len(h.closed) == 4
    assert not h.allocations
    h.machine.cleanup()
    print(f'{path.name}: trace rotation, truncation, contention and handle cleanup pass')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

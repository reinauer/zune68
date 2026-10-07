#!/usr/bin/env python3
"""Exercise linked sorting and count comparison work; no OS timing claims."""
import random
import struct
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import STACK, MASTER, EXEC, INTUITION

CLASS, OBJECT, DATA = 0x70000, 0x71000, 0x71100


class SortHarness(NotifyHarness):
    def __init__(self, path, values):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 256)
        self.mem.w32(DATA, 32)  # LIST_QUIET
        self.comparisons = 0
        self.entries = []
        self.keys = {}
        for n, value in enumerate(values):
            entry = self.blob(bytes(20))
            self.mem.w32(entry, n + 1)
            self.entries.append(entry)
            self.keys[n + 1] = value
        self.array = self.words(*self.entries, 0)
        self.mem.w32(DATA + 60, len(values))
        self.mem.w32(DATA + 68, self.array)
        self.mem.w32(DATA + 80, 0 if values else 0xffffffff)
        self.trap(self.symbols['_DoMethodA'], self.compare)
        self.trap(self.symbols['_MUI_Redraw'], lambda: None)

    def compare(self):
        msg = self.argument(1)
        assert self.mem.r32(msg) == 0x80421b68  # MUIM_List_Compare
        a, b = self.keys[self.mem.r32(msg + 4)], self.keys[self.mem.r32(msg + 8)]
        self.cpu.w_reg(0, ((a > b) - (a < b)) & 0xffffffff)
        self.comparisons += 1

    def sort(self):
        for n, arg in enumerate([CLASS, OBJECT, self.words(0x80422275)]):
            self.mem.w32(STACK + 4 * n, arg)
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_List__MUIM_Sort'], STACK)
        assert self.machine.was_exit(self.machine.execute(100000000))
        assert not self.allocations, 'sort must not allocate'
        return [self.mem.r32(self.array + n * 4) for n in range(len(self.entries))]


class InsertHarness(SortHarness):
    def __init__(self, path, values):
        super().__init__(path, values)
        self.array = self.words(0, *self.entries, *([0] * 32)) + 4
        self.mem.w32(DATA + 68, self.array)
        self.mem.w32(DATA + 64, 32)
        self.mem.w32(DATA + 116, len(values))
        self.mem.w32(DATA + 16, 123)  # caller-owned entry pool
        self.fail_construct = 0
        self.constructs = 0
        self.trap(EXEC - 708, self.alloc_vec)
        self.trap(EXEC - 714, self.free_vec)
        self.trap(INTUITION - 648, self.set_attrs)
        self.trap(self.symbols['_DoSuperMethodA'], lambda: self.cpu.w_reg(0, 0))
        self.dispatch_stub = self.blob(bytes(8))
        self.trap(self.dispatch_stub, self.method)
        self.mem.w16(self.dispatch_stub + 2, 0x4ef9)
        self.mem.w16(self.symbols['_DoMethod'], 0x4ef9)
        self.mem.w32(self.symbols['_DoMethod'] + 2, self.dispatch_stub)
        self.sort_script = self.script(('List__MUIM_Sort', self.words(0x80422275)))

    def method(self):
        method = self.argument(1)
        target = self.ret
        if method == 0x8042d662:
            self.constructs += 1
            value = self.argument(2)
            self.cpu.w_reg(0, 0 if self.constructs == self.fail_construct else value)
        elif method == 0x80422275:
            target = self.sort_script
        else:
            raise AssertionError(hex(method))
        self.mem.w32(self.dispatch_stub + 4, target)

    def set_attrs(self):
        tags = self.cpu.r_reg(9)
        while self.mem.r32(tags):
            attr, value = self.mem.r32(tags), self.mem.r32(tags + 4)
            if attr == 0x80421654:
                self.mem.w32(DATA + 60, value)
            elif attr == 0x8042391c:  # List_Active
                self.mem.w32(DATA + 80, value)
            tags += 8
        self.cpu.w_reg(0, 0)

    def insert(self, values, position=-2):
        incoming = []
        for value in values:
            key = max(self.keys, default=0) + 1
            self.keys[key] = value
            incoming.append(key)
        message = self.words(0x80426c87, self.words(*incoming), len(values),
                             position & 0xffffffff)
        result = self.call('List__MUIM_Insert', CLASS, OBJECT, message)
        count = self.mem.r32(DATA + 60)
        entries = [self.mem.r32(self.array + n * 4) for n in range(count)]
        return result, entries

    def finish(self):
        for address in list(self.allocations):
            self.call('FreeVecPooled', 123, address + 4)
        self.check_freed()
        self.machine.cleanup()


def check(path, baseline=False):
    if not baseline:
        h = InsertHarness(path, [2, 4])
        result, entries = h.insert([1, 3])
        assert [h.keys[h.mem.r32(e)] for e in entries] == [1, 2, 3, 4]
        assert h.mem.r32(DATA + 84) == 0, 'first inserted entry position'
        assert entries[h.mem.r32(DATA + 80)] == h.entries[0]
        h.finish()
        h = InsertHarness(path, [2, 4])
        h.fail_alloc = True
        result, entries = h.insert([1], 0)
        assert result == 0xffffffff and entries == h.entries
        h.finish()
        for failure in [1, 2, 3]:
            h = InsertHarness(path, [2, 4])
            h.fail_construct = failure
            result, entries = h.insert([1, 3, 5], 0)
            expected = [1, 3, 5][:failure - 1] + [2, 4]
            assert result == 0xffffffff
            assert [h.keys[h.mem.r32(e)] for e in entries] == expected
            assert entries[h.mem.r32(DATA + 80)] == h.entries[0]
            h.finish()

    rng = random.Random(68)
    cases = [[], [1], [2, 1], [3, 1, 2], [1, 2, 2, 3, 2],
             list(range(128)), list(reversed(range(128))),
             [rng.randrange(16) for _ in range(128)]]
    for values in cases:
        h = SortHarness(path, values)
        active = h.entries[0] if values else None
        result = h.sort()
        assert sorted(result) == sorted(h.entries)
        assert [h.keys[h.mem.r32(e)] for e in result] == sorted(values)
        if active:
            assert result[h.mem.r32(DATA + 80)] == active
        if not baseline:
            assert h.comparisons <= max(1, len(values) * 20)
            if values == sorted(values):
                assert h.comparisons == max(0, len(values) - 1)
        h.machine.cleanup()

    h = SortHarness(path, list(range(128)))
    total = 0
    for count in range(1, 129):
        h.mem.w32(DATA + 60, count)
        h.sort()
    total = h.comparisons
    h.machine.cleanup()
    print(f'{path.name}: sorting, duplicate keys and active identity pass; '
          f'128 growing sorted prefixes: {total} comparisons')


if __name__ == '__main__':
    baseline = '--baseline' in sys.argv
    for name in sys.argv[1:]:
        if name != '--baseline':
            check(Path(name), baseline)

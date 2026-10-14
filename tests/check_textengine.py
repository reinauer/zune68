#!/usr/bin/env python3
"""Check linked ZText parsing allocations on a 68000.

Lines and chunks live in space inline after the ZText. Short texts take
one allocation; overflow nodes come from the heap and only those are freed.
"""
import sys
from pathlib import Path
from check_class_lifetime import Harness, EXEC

PREPARSE, CONTENT = 0x7e000, 0x7f000


class TextHarness(Harness):
    def __init__(self, path):
        super().__init__(path)
        # Twelve formatted lines exceed 100000 emulated cycles.
        self.cycle_budget = 200000
        self.allocs = 0
        self.trap(EXEC - 264, self.rem_tail)
        self.trap(EXEC - 684, self.counted_alloc)
        self.trap(EXEC - 624, self.copy_mem)

    def copy_mem(self):
        source, dest, size = (self.cpu.r_reg(8), self.cpu.r_reg(9),
                              self.cpu.r_reg(0))
        self.mem.w_block(dest, bytes(self.mem.r_block(source, size)))

    def counted_alloc(self):
        self.allocs += 1
        self.alloc_vec()

    def rem_tail(self):
        head = self.cpu.r_reg(8)
        node = self.mem.r32(head + 8)
        if node == head:
            self.cpu.w_reg(0, 0)
            return
        pred = self.mem.r32(node + 4)
        self.mem.w32(pred, head + 4)
        self.mem.w32(head + 8, pred)
        self.cpu.w_reg(0, node)

    def nodes(self, head):
        node = self.mem.r32(head)
        while self.mem.r32(node):
            yield node
            node = self.mem.r32(node)

    def cstring(self, address):
        out = bytearray()
        while self.mem.r8(address + len(out)):
            out.append(self.mem.r8(address + len(out)))
        return out.decode('latin-1')

    def parse(self, content, preparse=''):
        self.mem.w_block(PREPARSE, preparse.encode('latin-1') + b'\0')
        self.mem.w_block(CONTENT, content.encode('latin-1') + b'\0')
        self.allocs = 0
        return self.call('zune_text_new', PREPARSE, CONTENT, 0, 0)

    def chunks(self, text):
        # ZText.lines at 4; ZTextLine.chunklist at 10; ZTextChunk.str at 8.
        return [[self.cstring(self.mem.r32(chunk + 8))
                 for chunk in self.nodes(line + 10)]
                for line in self.nodes(text + 4)]


def check(path):
    h = TextHarness(path)
    text = h.parse('Status 42')
    assert text and h.allocs == 1, h.allocs
    assert h.chunks(text) == [['Status 42']]
    assert sum(h.allocations.values()) <= 128, 'short caption wastes arena space'
    h.call('zune_text_destroy', text)
    assert not h.allocations

    text = h.parse('x' * 220)
    assert h.allocs == 1 and h.chunks(text) == [['x' * 220]]
    h.call('zune_text_destroy', text)
    assert not h.allocations

    text = h.parse('two', '\33bone ')
    assert h.allocs == 1
    assert h.chunks(text) == [['one two']]
    h.call('zune_text_destroy', text)
    assert not h.allocations

    # Ending an underline must keep other styles, including bold.
    text = h.parse('\33b\33uA\33UB')
    line = next(h.nodes(text + 4))
    chunks = list(h.nodes(line + 10))
    assert h.chunks(text) == [['A', 'B']]
    assert [h.mem.r8(chunk + 36) for chunk in chunks] == [3, 2]
    h.call('zune_text_destroy', text)
    assert not h.allocations

    # Enough lines and style chunks to overflow the inline space.
    lines = ['\33bbold%d\33nplain%d' % (i, i) for i in range(12)]
    text = h.parse('\n'.join(lines))
    assert h.allocs > 1
    assert h.chunks(text) == [['bold%d' % i, 'plain%d' % i]
                              for i in range(12)]
    h.call('zune_text_destroy', text)
    assert not h.allocations, 'inline or heap nodes leaked'

    text = h.parse('a\tb')
    assert h.chunks(text) == [['a', '    ', 'b']]
    h.call('zune_text_destroy', text)
    assert not h.allocations

    h.fail_alloc = True
    assert h.parse('Status') == 0
    assert not h.allocations
    print(f'{path.name}: ZText lines and chunks use inline space, '
          'overflow nodes are freed and short texts take one allocation')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

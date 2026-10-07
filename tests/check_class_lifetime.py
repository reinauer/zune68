#!/usr/bin/env python3
"""Execute native class ownership and expunge code on a 68000.

Exec and Intuition allocation, class and object services are modelled here;
this checks Zune68's linked code, not AmigaOS or GUI dispatchers.
"""
import argparse
from pathlib import Path

from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from amitools.vamos.machine.machine import Machine

BASE, STACK = 0x10000, 0xf0000
EXEC, INTUITION, MASTER, EXTERNAL = 0x68000, 0x69000, 0x6a000, 0x6b000
ROOTCLASS, EXTCLASS, TEXT = 0x6c000, 0x6c100, 0x6d000
# Classic m68k IClass and Library ABI offsets.
SUPER, ID, REFS, SUBS, OBJECTS, FLAGS = 24, 28, 36, 40, 44, 48
OPEN_COUNT, LIB_FLAGS = 32, 14


class Harness:
    def __init__(self, path):
        image = BinFmt().load_image(str(path))
        reloc = Relocate(image)
        data = bytes(reloc.relocate_one_block(BASE))
        self.symbols = {s.name.decode(): address + s.offset
                        for seg, address in zip(image.get_segments(),
                                                reloc.get_seq_addrs(BASE))
                        if seg.symtab for s in seg.symtab.symbols}
        self.machine = Machine()
        self.mem, self.cpu = self.machine.mem, self.machine.cpu
        self.mem.w_block(BASE, data)
        self.allocations = {}
        self.classes = set()
        self.objects = {}
        self.next_address = 0x80000
        self.fail_alloc = False
        self.fail_class = None
        self.refuse_free = None
        self.cleanup_calls = 0
        self.library_freed = False
        self.lock_depth = 0
        self.busy = False
        self.mem.w32(self.symbols['_SysBase'], EXEC)
        self.mem.w32(self.symbols['_IntuitionBase'], INTUITION)
        self.mem.w32(self.symbols['_MUIMasterBase'], MASTER)
        self.mem.w8(MASTER + 8, 9)  # NT_LIBRARY
        self.mem.w8(EXTERNAL + 8, 9)
        self.mem.w32(EXTCLASS + 16, EXTERNAL)  # Hook.h_Data
        self.mem.w32(EXTCLASS + SUPER, ROOTCLASS)
        # BuiltinClasses, Applications and defaultPens end the internal base.
        size = self.mem.r32(self.symbols['_LibInitTable'])
        self.class_list = MASTER + size - 28
        self.mem.w32(self.class_list, self.class_list + 4)
        self.mem.w32(self.class_list + 8, self.class_list)
        self.mem.w32(MASTER + 34, EXEC)
        self.mem.w32(MASTER + 38, 0x1234)  # Segment BPTR returned by expunge.
        self.mem.w16(MASTER + 18, size)
        self.mem.w_block(TEXT, b'zunemaster.library\0')
        self.mem.w32(MASTER + 10, TEXT)
        for offset, fn in [(234, self.insert), (246, self.add_tail),
                           (252, self.remove), (414, self.close_library),
                           (552, self.open_library), (564, self.obtain),
                           (570, self.release), (576, self.attempt),
                           (684, self.alloc_vec),
                           (690, self.free_vec), (210, self.free_mem)]:
            self.trap(EXEC - offset, fn)
        for offset, fn in [(678, self.make_class), (714, self.free_class),
                           (636, self.new_object), (642, self.dispose_object)]:
            self.trap(INTUITION - offset, fn)
        # Keep the real lifecycle gate, but isolate unrelated OS teardown and
        # filesystem-based MCC discovery. External releases still run real code.
        self.trap(self.symbols['_L_ExpungeLib'], self.cleanup)
        self.trap(self.symbols['_ZUNE_GetExternalClass'], self.external_class)
        if '_ZuneTraceOutput' in self.symbols:
            self.trap(self.symbols['_ZuneTraceOutput'], lambda: None)

    def trap(self, address, fn):
        def callback(opcode, pc):
            fn()
        number = self.machine.traps.alloc(callback)
        self.mem.w16(address, 0xa000 | number)
        self.mem.w16(address + 2, 0x4e75)

    def alloc(self, size):
        address = self.next_address
        self.next_address += (size + 15) & ~15
        assert self.next_address < 0xd0000
        self.mem.w_block(address, bytes(size))
        self.allocations[address] = size
        return address

    def alloc_vec(self):
        result = 0 if self.fail_alloc else self.alloc(self.cpu.r_reg(0))
        self.fail_alloc = False
        self.cpu.w_reg(0, result)

    def free_vec(self):
        del self.allocations[self.cpu.r_reg(9)]

    def obtain(self):
        self.lock_depth += 1

    def attempt(self):
        if not self.busy:
            self.obtain()
        self.cpu.w_reg(0, 0 if self.busy else 1)

    def release(self):
        self.lock_depth -= 1
        assert self.lock_depth >= 0

    def link(self, node, pred, succ):
        self.mem.w32(node, succ)
        self.mem.w32(node + 4, pred)
        self.mem.w32(pred, node)
        self.mem.w32(succ + 4, node)

    def insert(self):
        pred = self.cpu.r_reg(10)
        self.link(self.cpu.r_reg(9), pred, self.mem.r32(pred))

    def add_tail(self):
        head, node = self.cpu.r_reg(8), self.cpu.r_reg(9)
        self.link(node, self.mem.r32(head + 8), head + 4)

    def remove(self):
        node = self.cpu.r_reg(9)
        if node != MASTER:
            succ, pred = self.mem.r32(node), self.mem.r32(node + 4)
            self.mem.w32(pred, succ)
            self.mem.w32(succ + 4, pred)

    def open_library(self):
        self.mem.w16(MASTER + OPEN_COUNT,
                     self.mem.r16(MASTER + OPEN_COUNT) + 1)
        self.cpu.w_reg(0, MASTER)

    def close_library(self):
        base = self.cpu.r_reg(9)
        count = self.mem.r16(base + OPEN_COUNT)
        assert count > 0, 'unbalanced library close'
        self.mem.w16(base + OPEN_COUNT, count - 1)

    def make_class(self):
        name = self.cpu.r_reg(8)
        name_text = self.mem.r_cstr(name) if name else '<private>'
        if self.fail_class in (name_text, '*'):
            self.fail_class = None
            self.cpu.w_reg(0, 0)
            return
        superclass = self.cpu.r_reg(10) or ROOTCLASS
        cl = self.alloc(52)
        self.classes.add(cl)
        self.mem.w32(cl + SUPER, superclass)
        self.mem.w32(cl + ID, name)
        self.mem.w32(superclass + SUBS, self.mem.r32(superclass + SUBS) + 1)
        self.cpu.w_reg(0, cl)

    def free_class(self):
        cl = self.cpu.r_reg(8)
        assert cl in self.classes, 'class freed twice'
        if (cl == self.refuse_free or self.mem.r32(cl + SUBS)
                or self.mem.r32(cl + OBJECTS)):
            self.cpu.w_reg(0, 0)
            return
        assert not self.mem.r32(cl + FLAGS) & 1, 'unlink before FreeClass'
        superclass = self.mem.r32(cl + SUPER)
        self.mem.w32(superclass + SUBS, self.mem.r32(superclass + SUBS) - 1)
        self.classes.remove(cl)
        del self.allocations[cl]
        self.cpu.w_reg(0, 1)

    def new_object(self):
        cl = self.cpu.r_reg(8)
        address = self.alloc(16)
        obj = address + 12
        self.mem.w32(obj - 4, cl)
        self.objects[obj] = cl
        self.mem.w32(cl + OBJECTS, self.mem.r32(cl + OBJECTS) + 1)
        self.cpu.w_reg(0, obj)

    def dispose_object(self):
        obj = self.cpu.r_reg(8)
        cl = self.objects.pop(obj)
        ancestor = cl
        while ancestor != ROOTCLASS:
            assert ancestor in self.classes, 'freed class during disposal'
            ancestor = self.mem.r32(ancestor + SUPER)
        self.mem.w32(cl + OBJECTS, self.mem.r32(cl + OBJECTS) - 1)
        del self.allocations[obj - 12]

    def external_class(self):
        self.mem.w16(EXTERNAL + OPEN_COUNT,
                     self.mem.r16(EXTERNAL + OPEN_COUNT) + 1)
        self.cpu.w_reg(0, EXTCLASS)

    def cleanup(self):
        assert not self.classes, 'OS teardown with live classes'
        assert self.mem.r16(MASTER + OPEN_COUNT) == 0
        self.cleanup_calls += 1

    def free_mem(self):
        assert self.cleanup_calls == 1
        assert self.cpu.r_reg(9) == MASTER
        self.library_freed = True

    def call(self, name, *args):
        for i, value in enumerate(args):
            self.mem.w32(STACK + 4 * i, value)
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_' + name], STACK)
        result = self.machine.execute(100000)
        assert self.machine.was_exit(result), name
        assert self.cpu.r_sp() == STACK, (name, 'stack')
        assert self.lock_depth == 0, (name, 'semaphore')
        return self.cpu.r_reg(0)

    def string(self, text):
        self.mem.w_block(TEXT + 64, text.encode() + b'\0')
        return TEXT + 64

    def get(self, name='Area.mui'):
        return self.call('MUI_GetClass', self.string(name))

    def create(self, name='Area.mui', supermcc=0):
        return self.call('MUI_CreateCustomClass', 0,
                         self.string(name) if name else 0, supermcc, 0, 0)

    def assert_expunged(self):
        assert self.call('LibExpunge') == 0x1234, 'library remains pinned'
        assert self.library_freed and self.cleanup_calls == 1
        assert not self.allocations, 'unreclaimed class allocations'


def cycles(h):
    h.call('LibOpen')
    h.call('LibOpen')
    for _ in range(40):
        objects = [h.call('MUI_NewObjectA', h.string('Area.mui'), 0)
                   for _ in range(2)]
        assert all(objects)
        assert h.mem.r16(MASTER + OPEN_COUNT) == 2, 'cached classes pin library'
        assert h.call('LibExpunge') == 0  # Both clients still own the library.
        for obj in objects:
            h.call('MUI_DisposeObject', obj)
        assert len(h.classes) == 2 and len(h.allocations) == 2
    h.call('LibClose')
    assert not h.library_freed, 'first client close must not expunge'
    assert h.call('LibClose') == 0x1234, 'last close must honor delayed expunge'
    assert not h.allocations and h.library_freed


def custom(h):
    h.call('LibOpen')
    for name in ['Area.mui', 'Example.mcc']:
        cl = h.get(name)
        h.call('MUI_FreeClass', cl)
        for failure in ['allocation', 'MakeClass']:
            h.fail_alloc = failure == 'allocation'
            h.fail_class = '*' if failure == 'MakeClass' else None
            before = set(h.allocations)
            assert h.create(name) == 0
            assert set(h.allocations) == before, 'failed custom allocation leaked'
            if name == 'Area.mui':
                assert h.mem.r32(cl + REFS) == 0, 'failed custom class leaked ref'
            else:
                assert h.mem.r16(EXTERNAL + OPEN_COUNT) == 0, 'MCC open leaked'
        mcc = h.create(name)
        custom_class = h.mem.r32(mcc + 24)
        h.mem.w32(custom_class + OBJECTS, 1)
        assert h.call('MUI_DeleteCustomClass', mcc) == 0
        h.mem.w32(custom_class + OBJECTS, 0)
        assert h.call('MUI_DeleteCustomClass', mcc) != 0
        assert h.mem.r32(cl + REFS) == 0
        assert h.mem.r16(EXTERNAL + OPEN_COUNT) == 0
    # Private superclasses are borrowed. Failed and successful child creation
    # must neither release their owner's reference nor make deletion possible.
    parent = h.create()
    cl = h.mem.r32(parent + 24)
    for failure in ['allocation', 'MakeClass']:
        h.fail_alloc = failure == 'allocation'
        h.fail_class = '*' if failure == 'MakeClass' else None
        before = set(h.allocations)
        assert h.create(None, parent) == 0
        assert set(h.allocations) == before
        assert cl in h.classes
    child = h.create(None, parent)
    assert h.call('MUI_DeleteCustomClass', parent) == 0
    assert h.call('MUI_DeleteCustomClass', child) != 0
    assert h.call('MUI_DeleteCustomClass', parent) != 0
    h.call('LibClose')
    h.assert_expunged()


def builtin(h):
    h.call('LibOpen')
    h.fail_class = 'Area.mui'
    # Call builtin lookup directly so failure does not trigger MCC fallback.
    assert h.call('ZUNE_GetBuiltinClass', h.string('Area.mui'), MASTER) == 0
    assert len(h.classes) == 1  # Cached Notify superclass.
    cl = next(iter(h.classes))
    assert h.mem.r32(cl + REFS) == 0, 'failed builtin leaked superclass ref'
    assert h.mem.r16(MASTER + OPEN_COUNT) == 1
    h.call('LibClose')
    h.assert_expunged()


def expunge(h):
    h.call('LibOpen')
    cl = h.get()
    h.call('LibClose')
    assert h.call('LibExpunge') == 0, 'outstanding lookup must block expunge'
    assert not h.library_freed
    h.call('MUI_FreeClass', cl)
    for field in [OBJECTS, SUBS]:
        h.mem.w32(cl + field, 1)
        assert h.call('LibExpunge') == 0, 'live object/subclass must block expunge'
        assert cl in h.classes
        h.mem.w32(cl + field, 0)
    # Unexpected FreeClass refusal must preserve list membership and order.
    h.refuse_free = cl
    before = (h.mem.r32(cl), h.mem.r32(cl + 4))
    assert h.call('LibExpunge') == 0
    assert before == (h.mem.r32(cl), h.mem.r32(cl + 4))
    assert h.mem.r32(cl + FLAGS) & 1
    h.refuse_free = None
    h.busy = True
    assert h.call('LibExpunge') == 0, 'expunge must not wait for a class lock'
    assert cl in h.classes
    h.busy = False
    # Reopening clears delayed expunge, so the following close only caches.
    h.call('LibOpen')
    assert not h.mem.r8(MASTER + LIB_FLAGS) & 8
    assert h.call('LibClose') == 0
    assert not h.library_freed
    h.assert_expunged()


def hierarchy(h):
    h.call('LibOpen')
    count = 0
    for name, descriptor in sorted(h.symbols.items()):
        if name.startswith('__MUI_') and name.endswith('_desc'):
            cl = h.call('ZUNE_GetBuiltinClass', h.mem.r32(descriptor), MASTER)
            if cl:
                count += 1
                h.call('MUI_FreeClass', cl)
    assert count > 30, 'exercise the native built-in class graph'
    assert h.mem.r16(EXTERNAL + OPEN_COUNT) == 0
    assert h.mem.r16(MASTER + OPEN_COUNT) == 1
    assert all(h.mem.r32(cl + REFS) == 0 for cl in h.classes)
    h.call('LibClose')
    h.assert_expunged()


def empty(h):
    h.call('LibOpen')
    h.call('LibClose')
    h.assert_expunged()


def check(path, cases):
    for name in cases:
        h = Harness(path)
        try:
            globals()[name](h)
        finally:
            h.machine.cleanup()
    print(f'{path.name}: class lifetime ({", ".join(cases)}) OK')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('libraries', type=Path, nargs='+')
    parser.add_argument('--case', choices=['cycles', 'custom', 'builtin', 'expunge',
                                                'hierarchy', 'empty'])
    args = parser.parse_args()
    for path in args.libraries:
        check(path, [args.case] if args.case else
              ['cycles', 'custom', 'builtin', 'expunge', 'hierarchy', 'empty'])

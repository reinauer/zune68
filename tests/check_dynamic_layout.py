#!/usr/bin/env python3
"""Check live cycle-chain ownership and Group ExitChange on linked 68000 code."""
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import EXEC, INTUITION

CLASS, OBJECT, DATA = 0x70000, 0x71000, 0x71040
POOL, CHAIN, ORDER = DATA + 406, DATA + 410, DATA + 656
SETUP, CYCLE = 1 << 28, 1 << 5


class LayoutHarness(NotifyHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 64)
        self.mem.w32(CHAIN, CHAIN + 4)
        self.mem.w32(CHAIN + 8, CHAIN)
        self.mem.w32(POOL, 0x1234)
        self.trap(EXEC - 258, self.rem_head)
        self.trap(EXEC - 708, self.alloc_vec)
        self.trap(EXEC - 714, self.free_pooled)
        self.trap(EXEC - 624, self.copy)
        self.trap(INTUITION - 648, lambda: self.cpu.w_reg(0, 0))
        self.trap(self.symbols['_DoMethod'], self.method)
        self.mem.w32(OBJECT + 28, DATA)  # render info is first window member
        self.mem.w32(DATA, OBJECT)

    def copy(self):
        self.mem.w_block(self.cpu.r_reg(9), self.mem.r_block(
            self.cpu.r_reg(8), self.cpu.r_reg(0)))

    def free_pooled(self):
        self.free_vec()

    def rem_head(self):
        head = self.cpu.r_reg(8)
        node = self.mem.r32(head)
        if self.mem.r32(node):
            self.cpu.w_reg(9, node)
            self.remove()
            self.cpu.w_reg(0, node)
        else:
            self.cpu.w_reg(0, 0)

    def method(self):
        self.events.append((self.argument(0), self.argument(1)))

    def child(self):
        obj = self.blob(bytes(128))
        self.mem.w32(obj + 28, DATA)
        self.mem.w32(obj + 64, SETUP | CYCLE)
        # MUI_EventHandlerNode: ehn_Object at 12, ehn_Events at 20.
        handler = self.blob(bytes(32))
        self.mem.w32(handler + 12, obj)
        return obj, handler

    def control(self, name, handler):
        return self.call('Window__MUIM_' + name + 'ControlCharHandler',
                         CLASS, OBJECT, self.words(0, handler))

    def chain(self):
        objects = []
        node = self.mem.r32(CHAIN)
        while self.mem.r32(node):
            objects.append(self.mem.r32(node + 8))
            node = self.mem.r32(node)
        return objects

    def order(self, *objects):
        return self.call('Window__MUIM_SetCycleChain', CLASS, OBJECT,
                         self.words(0, *objects, 0))


def check(path):
    h = LayoutHarness(path)
    a, ah = h.child()
    b, bh = h.child()
    c, ch = h.child()
    assert h.order(b, a, b)
    assert h.chain() == [b, a], 'requested order and duplicate suppression'
    h.control('Rem', ah)
    h.control('Rem', bh)
    assert not h.chain()
    h.control('Add', ah)
    h.control('Add', ch)
    h.control('Add', bh)
    assert h.chain() == [b, a], 'setup order must not override explicit order'
    h.fail_alloc = True
    assert not h.order(c)
    assert h.chain() == [b, a], 'OOM must retain old chain'
    h.control('Rem', bh)
    assert h.chain() == [a], 'removed child must leave the live chain'
    assert h.order()
    assert not h.chain()
    h.cpu.w_reg(9, h.mem.r32(ORDER))
    h.free_vec()
    h.check_freed()
    assert not h.allocations
    h.machine.cleanup()

    h = LayoutHarness(path)
    # Group instance starts beyond Area; no membership change is required
    # for ExitChange to request a layout. A changing parent defers it.
    h.mem.w16(CLASS + 32, 256)
    h.mem.w32(OBJECT + 64, SETUP)
    h.mem.w32(OBJECT + 16, 0)
    h.call('Group__MUIM_ExitChange', CLASS, OBJECT, h.words(0))
    assert len(h.events) == 1
    parent = h.blob(bytes(400))
    h.mem.w32(OBJECT + 16, parent)
    h.mem.w32(parent + 256 + 8, 1 << 4)
    h.call('Group__MUIM_ExitChange', CLASS, OBJECT, h.words(0))
    assert len(h.events) == 1
    h.mem.w32(parent + 256 + 8, 0)
    h.call('Group__MUIM_ExitChange', CLASS, OBJECT, h.words(0))
    assert len(h.events) == 2
    # A custom layout hook receives the live child list and inner geometry.
    children = h.blob(bytes(12))
    hook, entry = h.blob(bytes(20)), h.blob(bytes(4))
    h.mem.w32(children, children + 4)
    h.mem.w32(children + 8, children)
    h.mem.w32(OBJECT + 256 + 4, hook)
    h.mem.w32(hook + 8, entry)
    h.mem.w16(OBJECT + 28 + 28, 100)  # mad_Box.Width
    h.mem.w16(OBJECT + 28 + 30, 50)
    h.mem.w8(OBJECT + 28 + 34, 8)  # mad_subwidth
    h.mem.w8(OBJECT + 28 + 35, 6)
    def get_children():
        h.mem.w32(h.cpu.r_reg(9), children)
        h.cpu.w_reg(0, 1)
    def layout():
        assert h.cpu.r_reg(8) == hook and h.cpu.r_reg(10) == OBJECT
        msg = h.cpu.r_reg(9)
        assert h.mem.r32(msg) == 2 and h.mem.r32(msg + 4) == children
        assert (h.mem.r32(msg + 20), h.mem.r32(msg + 24)) == (92, 44)
        h.events.append('layout hook')
    h.trap(INTUITION - 654, get_children)
    h.mem.w_block(0x72000 - 102, b'\x2f\x28\x00\x08\x4e\x75')
    h.trap(entry, layout)
    h.call('Group__MUIM_Layout', CLASS, OBJECT, h.words(0))
    assert h.events[-1] == 'layout hook'
    h.machine.cleanup()
    print(f'{path}: explicit cycle order, cleanup and dynamic relayout pass')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        check(Path(filename))

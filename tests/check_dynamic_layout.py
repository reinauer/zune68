#!/usr/bin/env python3
"""Check live cycle-chain ownership and Group ExitChange on linked 68000 code."""
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import EXEC, INTUITION

CLASS, OBJECT, DATA = 0x70000, 0x71000, 0x71040
MRI = DATA + 4  # the render info follows the area render info pointer
# wd_Chain (a block of object pointers) and wd_ChainCount
CHAIN, CHAIN_COUNT = DATA + 410, DATA + 414
SETUP, CYCLE = 1 << 28, 1 << 5


class LayoutHarness(NotifyHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 64)
        self.trap(EXEC - 624, self.copy)
        self.trap(INTUITION - 648, lambda: self.cpu.w_reg(0, 0))
        self.trap(self.symbols['_DoMethod'], self.method)
        self.mem.w32(OBJECT + 28, MRI)
        self.mem.w32(MRI, OBJECT)

    def copy(self):
        self.mem.w_block(self.cpu.r_reg(9), self.mem.r_block(
            self.cpu.r_reg(8), self.cpu.r_reg(0)))

    def method(self):
        self.events.append((self.argument(0), self.argument(1)))

    def child(self):
        obj = self.blob(bytes(128))
        self.mem.w32(obj + 28, MRI)
        self.mem.w32(obj + 64, SETUP | CYCLE)
        # MUI_EventHandlerNode: ehn_Object at 12, ehn_Events at 20.
        handler = self.blob(bytes(32))
        self.mem.w32(handler + 12, obj)
        return obj, handler

    def control(self, name, handler):
        return self.call('Window__MUIM_' + name + 'ControlCharHandler',
                         CLASS, OBJECT, self.words(0, handler))

    def chain(self):
        block = self.mem.r32(CHAIN)
        return [self.mem.r32(block + 4 * i)
                for i in range(self.mem.r16(CHAIN_COUNT))]

    def order(self, *objects):
        return self.call('Window__MUIM_SetCycleChain', CLASS, OBJECT,
                         self.words(0, *objects, 0))


def check(path):
    h = LayoutHarness(path)
    a, ah = h.child()
    b, bh = h.child()
    c, ch = h.child()
    # at the window's setup (not open yet) the chain is the setup order
    h.control('Add', ah)
    h.control('Add', ch)
    h.control('Add', bh)
    assert h.chain() == [a, c, b], 'setup order'
    h.control('Rem', bh)
    assert h.chain() == [a, c], 'removed child must leave the live chain'
    # MUI 3.8's SetCycleChain only sets MUIA_CycleChain (TRUE on the
    # listed objects, FALSE on the window's others): no order of its own
    assert h.order(b, a, b)
    assert h.chain() == [a, c], 'SetCycleChain keeps the tree order'
    h.control('Rem', ah)
    h.control('Rem', ch)
    assert not h.chain() and not h.mem.r32(CHAIN), 'empty chain is freed'
    # More objects than the first block holds keep their order
    many = [h.child() for _ in range(11)]
    for obj, handler in many:
        h.control('Add', handler)
    assert h.chain() == [obj for obj, _ in many], 'grown chain order'
    for obj, handler in many[::2]:
        h.control('Rem', handler)
    assert h.chain() == [obj for obj, _ in many[1::2]], 'removal order'
    for obj, handler in many[1::2]:
        h.control('Rem', handler)
    assert not h.chain() and not h.mem.r32(CHAIN)
    # Gadget IDs: the lowest free one, and the documented reuse bug (a
    # freed gap is recorded after the larger ID, so it is handed out twice)
    def alloc_id():
        return h.call('Window__MUIM_AllocGadgetID', CLASS, OBJECT, h.words(0))
    def free_id(gid):
        h.call('Window__MUIM_FreeGadgetID', CLASS, OBJECT, h.words(0, gid))
    assert [alloc_id() for _ in range(10)] == list(range(1, 11))
    free_id(4)
    assert alloc_id() == 4 and alloc_id() == 4
    for gid in [1, 2, 3, 4, 4, 5, 6, 7, 8, 9, 10, 99]:
        free_id(gid)
    assert not h.mem.r32(DATA + 414), 'empty ID record is freed'
    h.check_freed()
    assert not h.allocations
    h.machine.cleanup()

    h = LayoutHarness(path)
    # Group instance starts beyond Area; no membership change is required
    # for ExitChange to request a layout. A changing parent defers it.
    h.mem.w16(CLASS + 32, 272)
    h.mem.w32(OBJECT + 64, SETUP)
    h.mem.w32(OBJECT + 16, 0)
    h.call('Group__MUIM_ExitChange', CLASS, OBJECT, h.words(0))
    assert len(h.events) == 1
    parent = h.blob(bytes(400))
    h.mem.w32(OBJECT + 16, parent)
    h.mem.w32(parent + 272 + 8, 1 << 4)
    h.call('Group__MUIM_ExitChange', CLASS, OBJECT, h.words(0))
    assert len(h.events) == 1
    h.mem.w32(parent + 272 + 8, 0)
    h.call('Group__MUIM_ExitChange', CLASS, OBJECT, h.words(0))
    assert len(h.events) == 2
    # A custom layout hook receives the live child list and inner geometry.
    children = h.blob(bytes(12))
    hook, entry = h.blob(bytes(20)), h.blob(bytes(4))
    h.mem.w32(children, children + 4)
    h.mem.w32(children + 8, children)
    h.mem.w32(OBJECT + 272 + 4, hook)
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
        h.cpu.w_reg(0, 1)
    h.trap(INTUITION - 654, get_children)
    h.mem.w_block(0x72000 - 102, b'\x2f\x28\x00\x08\x4e\x75')
    h.trap(entry, layout)
    assert h.call('Group__MUIM_Layout', CLASS, OBJECT, h.words(0)) == 1
    assert h.events[-1] == 'layout hook'
    h.trap(entry, lambda: h.cpu.w_reg(0, 0))
    assert h.call('Group__MUIM_Layout', CLASS, OBJECT, h.words(0)) == 0

    # A custom virtual layout may shrink its requested content extent,
    # but its public virtual dimensions must still cover the viewport.
    # This follows the grow/shrink sequence in native/relayout.c.
    h.mem.w32(OBJECT + 272 + 8, 1 << 6)  # GROUP_VIRTUAL
    h.mem.w32(OBJECT + 272 + 68, h.words(*[0] * 16))  # its struct GroupVirt
    requested = [20, 10]
    def virtual_layout():
        msg = h.cpu.r_reg(9)
        assert (h.mem.r32(msg + 20), h.mem.r32(msg + 24)) == (92, 44)
        h.mem.w32(msg + 20, requested[0])
        h.mem.w32(msg + 24, requested[1])
    h.trap(entry, virtual_layout)
    result = h.words(0)
    def virtual_dimension(attr):
        assert h.call('Group__OM_GET', CLASS, OBJECT,
                      h.words(0, attr, result))
        return h.mem.r32(result)
    for width, height, expected in [(20, 10, (92, 44)),
                                     (500, 1000, (500, 1000)),
                                     (20, 10, (92, 44))]:
        requested[:] = width, height
        h.call('Group__MUIM_Layout', CLASS, OBJECT, h.words(0))
        actual = (virtual_dimension(0x80427c49),
                  virtual_dimension(0x80423038))
        assert actual == expected, (actual, expected)
    h.machine.cleanup()
    print(f'{path}: tree-ordered cycle chain, cleanup and dynamic relayout pass')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        check(Path(filename))

#!/usr/bin/env python3
"""Check that Group disconnection keeps the children's _parent() links."""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness, INTUITION

CLASS, GROUP, LIST = 0x70000, 0x71000, 0x72000
DISCONNECT = 0x12345678  # message forwarded unchanged
PARENT = 16  # MUI_NotifyData.mnd_ParentObject


def check(path):
    h = Harness(path)
    calls = []

    def argument(n):
        return h.mem.r32(h.cpu.r_sp() + 4 + n * 4)

    def get_attr():  # MUIA_Family_List of the group's family
        h.mem.w32(h.cpu.r_reg(9), LIST)
        h.cpu.w_reg(0, 1)

    h.trap(h.symbols['_DoMethodA'], lambda: calls.append(('method', argument(0))))
    h.trap(h.symbols['_DoSuperMethodA'],
           lambda: calls.append(('super', argument(1))))
    h.trap(INTUITION - 654, get_attr)
    h.mem.w16(CLASS + 32, 256)

    # Two children: struct _Object nodes (MinNode, class) precede each object.
    nodes = [LIST + 0x100, LIST + 0x200]
    children = [node + 12 for node in nodes]
    h.mem.w32(LIST, nodes[0])
    h.mem.w32(LIST + 4, 0)
    h.mem.w32(LIST + 8, nodes[1])
    h.mem.w32(nodes[0], nodes[1])
    h.mem.w32(nodes[0] + 4, LIST)
    h.mem.w32(nodes[1], LIST + 4)
    h.mem.w32(nodes[1] + 4, nodes[0])
    for child in children:
        h.mem.w32(child + PARENT, GROUP)

    msg = 0x73000
    h.mem.w_block(msg, struct.pack('>I', DISCONNECT))
    assert h.call('Group__MUIM_DisconnectParent', CLASS, GROUP, msg) == 1
    assert calls == [('method', c) for c in children] + [('super', GROUP)], calls
    for child in children:
        assert h.mem.r32(child + PARENT) == GROUP, hex(child)
    h.machine.cleanup()
    print(f'{path}: disconnection keeps group parent links')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        check(Path(filename))

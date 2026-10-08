#!/usr/bin/env python3
"""Check the linked classic virtual-group scroll-container protocol.

Superclass layout and OS GetAttr are modeled; the real Virtgroup dispatcher
owns registration and forms the message only after explicit layouts.
"""
import sys
from pathlib import Path
from check_class_lifetime import INTUITION, MASTER, STACK
from check_notifications import NotifyHarness, CLASS, OBJECT

REGISTER, INFORM = 0x80420e88, 0x804218a6
LAYOUT, SET = 0x90420200, 0x103
LEFT, TOP, WIDTH, HEIGHT = 0x80429371, 0x80425200, 0x80427c49, 0x80423038
PARENT = 0x78000


class VirtualHarness(NotifyHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 256)
        self.mem.w16(OBJECT + 56, 100)
        self.mem.w16(OBJECT + 58, 50)
        self.mem.w8(OBJECT + 62, 8)
        self.mem.w8(OBJECT + 63, 6)
        self.attrs = {LEFT: 0, TOP: 0, WIDTH: 92, HEIGHT: 44}
        self.forwarded = []
        self.trap(INTUITION - 654, self.get_attr)

    def get_attr(self):
        assert self.cpu.r_reg(8) == OBJECT
        self.mem.w32(self.cpu.r_reg(9), self.attrs[self.cpu.r_reg(0)])
        self.cpu.w_reg(0, 1)

    def super_method(self):
        msg = self.argument(2)
        method = self.mem.r32(msg)
        if method == SET:
            tags = self.mem.r32(msg + 4)
            while self.mem.r32(tags):
                attr, value = self.mem.r32(tags), self.mem.r32(tags + 4)
                if attr != 1:  # TAG_IGNORE
                    self.forwarded.append((attr, value))
                if attr in (LEFT, TOP):
                    self.attrs[attr] = value
                tags += 8
        else:
            assert method == LAYOUT, hex(method)
        self.cpu.w_reg(0, 0)

    def method(self):
        obj, msg = self.argument(0), self.argument(1)
        self.events.append((obj, tuple(self.mem.r32(msg + i * 4)
                                       for i in range(7))))

    def dispatch(self, method, *args):
        self.cpu.w_reg(8, CLASS)
        self.cpu.w_reg(10, OBJECT)
        self.cpu.w_reg(9, self.words(method, *args))
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_Virtgroup_Dispatcher'], STACK)
        result = self.machine.execute(100000)
        assert self.machine.was_exit(result), 'virtual dispatcher'
        assert self.cpu.r_sp() == STACK

    def set(self, *tags):
        self.dispatch(SET, self.words(*tags, 0, 0), 0)


def check(path):
    h = VirtualHarness(path)
    assert h.mem.r32(h.symbols['__MUI_Virtgroup_desc'] + 8) == 4
    h.dispatch(LAYOUT)
    assert not h.events, 'unregistered groups must not notify parents'
    h.set(REGISTER, PARENT)
    assert not h.forwarded, 'private registration must not reach children'
    for width, height in [(92, 44), (92, 44), (1000, 2000)]:
        h.attrs[WIDTH], h.attrs[HEIGHT] = width, height
        h.dispatch(LAYOUT)
        assert h.events[-1] == (PARENT, (INFORM, 0, 0, 92, 44, width, height))
    assert len(h.events) == 3, 'unchanged layouts also inform'
    h.set(LEFT, 23, TOP, 37)
    assert len(h.events) == 3, 'setting scroll offsets does not invoke layout or inform'
    h.dispatch(LAYOUT)
    assert h.events[-1] == (PARENT, (INFORM, 23, 37, 92, 44, 1000, 2000))
    h.set(REGISTER, 0)
    h.dispatch(LAYOUT)
    assert len(h.events) == 4, 'clearing registration stops informs'
    h.machine.cleanup()
    print(f'{path}: virtual scroll registration and layout informs pass')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        check(Path(filename))

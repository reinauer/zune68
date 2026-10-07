#!/usr/bin/env python3
"""Run linked Notify methods on a 68000, including callback-driven mutation.

OS allocation and dispatch are modelled. Callback scripts call the real C
methods on the emulated stack; freed blocks are poisoned and checked.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness, EXEC, INTUITION

CLASS, OBJECT, UTILITY = 0x70000, 0x71000, 0x72000
ATTR, OTHER = 0x80420301, 0x80420302
EVERY, TRIGGER, NOT_TRIGGER = 0x49893131, 0x49893131, 0x49893133
NO_NOTIFY = 0x804237f9


class NotifyHarness(Harness):
    def __init__(self, path):
        self.freed = {}
        super().__init__(path)
        self.scratch = 0x73000
        self.events = []
        self.actions = {}
        self.dead = False
        self.mem.w32(self.symbols['_UtilityBase'], UTILITY)
        self.mem.w16(CLASS + 32, 0)
        self.trap(UTILITY - 48, self.next_tag)
        self.trap(EXEC - 258, self.rem_head)
        self.trap(self.symbols['_DoSuperMethodA'], self.super_method)
        self.trap(self.symbols['_DoMethodA'], self.method)
        self.ret = self.blob(b'\x4e\x75')
        self.mem.w16(self.symbols['_DoMethodA'] + 2, 0x4ef9)
        self.mem.w32(self.symbols['_DoMethodA'] + 4, self.ret)

    def blob(self, data):
        address = self.scratch
        self.scratch += (len(data) + 3) & ~3
        assert self.scratch < 0x7f000
        self.mem.w_block(address, bytes(data))
        return address

    def words(self, *values):
        return self.blob(struct.pack('>' + 'I' * len(values), *values))

    def argument(self, n):
        return self.mem.r32(self.cpu.r_sp() + 4 + n * 4)

    def next_tag(self):
        storage = self.cpu.r_reg(8)
        tag = self.mem.r32(storage)
        self.cpu.w_reg(0, tag if tag and self.mem.r32(tag) else 0)
        self.mem.w32(storage, tag + 8)

    def free_vec(self):
        address = self.cpu.r_reg(9)
        if address:
            size = self.allocations[address]
            self.freed[address] = bytes([0xa5]) * size
            self.mem.w_block(address, self.freed[address])
            super().free_vec()

    def rem_head(self):
        head = self.cpu.r_reg(8)
        node = self.mem.r32(head)
        if self.mem.r32(node):
            self.cpu.w_reg(9, node)
            self.remove()
            self.cpu.w_reg(0, node)
        else:
            self.cpu.w_reg(0, 0)

    def super_method(self):
        self.dead = True
        self.mem.w_block(OBJECT, b'\xa5' * 28)

    def method(self):
        destination, msg = self.argument(0), self.argument(1)
        event = self.mem.r32(msg)
        self.events.append((destination, event, self.mem.r32(msg + 4),
                            self.mem.r32(msg + 8)))
        action = self.actions.get(event, self.ret)
        self.mem.w32(self.symbols['_DoMethodA'] + 4, action)

    def script(self, *calls):
        code = bytearray()
        for method, message in calls:
            for arg in [message, OBJECT, CLASS]:
                code += b'\x2f\x3c' + struct.pack('>I', arg)
            code += b'\x4e\xb9' + struct.pack('>I', self.symbols['_' + method])
            code += b'\x4f\xef\x00\x0c'
        code += b'\x4e\x75'
        return self.blob(code)

    def add_message(self, event, attr=ATTR, dest=0x123456, trigger=EVERY):
        return self.words(0, attr, trigger, dest, 3, event, TRIGGER,
                          NOT_TRIGGER)

    def add(self, event, **kwargs):
        return self.call('Notify__MUIM_Notify', CLASS, OBJECT,
                         self.add_message(event, **kwargs))

    def set_message(self, value=7, attr=ATTR, quiet=False):
        tags = [attr, value]
        if quiet:
            tags += [NO_NOTIFY, 1]
        return self.words(0, self.words(*tags, 0, 0), 0)

    def fire(self, **kwargs):
        self.call('Notify__OM_SET', CLASS, OBJECT, self.set_message(**kwargs))
        self.check_freed()

    def kill(self, attr=ATTR, dest=None):
        if dest is None:
            return 'Notify__MUIM_KillNotify', self.words(0, attr)
        return 'Notify__MUIM_KillNotifyObj', self.words(0, attr, dest)

    def dispose(self):
        return 'Notify__OM_DISPOSE', self.words(0x102)

    def check_freed(self):
        for address, data in self.freed.items():
            assert bytes(self.mem.r_block(address, len(data))) == data, 'write after free'

    def finish(self):
        if not self.dead:
            method, msg = self.dispose()
            self.call(method, CLASS, OBJECT, msg)
        self.check_freed()
        assert not self.allocations, 'notification leak'
        self.machine.cleanup()


def check(path):
    # Built-in versions describe Zune68's implementation series, not the
    # library release number or a claimed proprietary MUI feature level.
    for name in ['Notify', 'Family', 'Group', 'Text', 'Rectangle', 'Window']:
        h = NotifyHarness(path)
        storage = h.words(0)
        for attr in [0x80422301, 0x80427eaa]:
            assert h.call(name + '__OM_GET', CLASS, OBJECT,
                          h.words(0x104, attr, storage))
            assert h.mem.r32(storage) == 1
        h.finish()

    for failed_call in [1, 2, 3]:  # list header, node, parameter copy
        h = NotifyHarness(path)
        calls = [0]
        def allocate():
            calls[0] += 1
            h.fail_alloc = calls[0] == failed_call
            h.alloc_vec()
        h.trap(EXEC - 684, allocate)
        assert not h.add(1)
        h.fire()
        assert not h.events
        h.finish()

    h = NotifyHarness(path)
    message = h.add_message(1)
    h.mem.w32(message + 16, 0xffffffff)
    assert not h.call('Notify__MUIM_Notify', CLASS, OBJECT, message)
    h.finish()

    h = NotifyHarness(path)
    h.add(1)
    h.add(2)
    h.fire(quiet=True)
    assert not h.events
    h.fire()
    h.fire()
    assert [e[1:] for e in h.events] == [(1, 7, 0), (2, 7, 0)] * 2
    h.finish()

    h = NotifyHarness(path)
    h.add(1, attr=0x80420313)  # MUIA_UserData
    h.fire(attr=0x80420313, value=5)
    h.fire(attr=0x80420313, value=5)
    h.fire(attr=0x80420313, value=6)
    assert [e[2] for e in h.events] == [5, 6]
    h.finish()

    h = NotifyHarness(path)
    h.add(1, dest=1)  # Self
    h.add(2, dest=3)  # Application, absent until attached
    h.add(3, dest=2)  # Window, resolved via attribute on any Notify subclass
    def window_attr():
        h.mem.w32(h.cpu.r_reg(9), 0x234567)
        h.cpu.w_reg(0, 1)
    h.trap(INTUITION - 654, window_attr)
    h.fire()
    assert [e[0] for e in h.events] == [OBJECT, 0x234567]
    global_info = h.words(0, 0x123456)
    h.mem.w32(OBJECT, global_info)
    h.events.clear()
    h.fire()
    assert [e[0] for e in h.events] == [OBJECT, 0x123456, 0x234567]
    h.finish()

    for action in ['self', 'next', 'all', 'append', 'replace', 'dispose', 'nested']:
        h = NotifyHarness(path)
        h.add(1, dest=111)
        h.add(2, dest=222)
        if action == 'self':
            calls = [h.kill(dest=111)]
        elif action == 'next':
            calls = [h.kill(dest=222)]
        elif action == 'all':
            calls = [h.kill(), h.kill()]
        elif action == 'append':
            calls = [('Notify__MUIM_Notify', h.add_message(3))]
        elif action == 'replace':
            calls = [h.kill(dest=222), ('Notify__MUIM_Notify', h.add_message(3))]
        elif action == 'dispose':
            calls = [h.dispose()]
        else:
            h.add(3, attr=OTHER)
            calls = [('Notify__OM_SET', h.set_message(attr=OTHER))]
            h.actions[3] = h.script(h.kill(dest=111), h.kill(dest=222))
        h.actions[1] = h.script(*calls)
        h.fire()
        expected = {'self': [1, 2], 'next': [1], 'all': [1],
                    'append': [1, 2], 'replace': [1], 'dispose': [1],
                    'nested': [1, 3]}[action]
        assert [e[1] for e in h.events] == expected, (action, h.events)
        h.finish()

    h = NotifyHarness(path)
    h.add(1)
    h.add(2)
    h.actions[1] = h.script(('Notify__OM_SET', h.set_message(value=9)))
    h.fire()
    assert [e[1:] for e in h.events] == [(1, 7, 0), (2, 9, 0), (2, 7, 0)]
    h.finish()

    h = NotifyHarness(path)
    # Resolve symbolic destinations at delivery time, including reparenting.
    h.mem.w32(OBJECT + 16, 123)
    h.add(1, dest=4)  # MUIV_Notify_Parent
    h.fire()
    assert h.events[0][0] == 123
    h.mem.w32(OBJECT + 16, 456)
    h.fire()
    assert h.events[-1][0] == 456
    h.mem.w32(OBJECT + 16, 0)
    h.fire()
    assert len(h.events) == 2
    h.finish()
    print(f'{path}: notification mutation, re-entry, disposal and destinations pass')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        check(Path(filename))

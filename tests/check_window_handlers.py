#!/usr/bin/env python3
"""Execute linked Window dispatch while client callbacks remove/free handlers.

GUI services are modeled. Removal methods and all dispatch passes run as real
68000 code, including nested dispatch and the focus-changing parent walk.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness, EXEC, INTUITION

CLASS, OBJECT, ROOT, IWINDOW = 0x70000, 0x71000, 0x71800, 0x71a00
DATA = OBJECT + 28
EH, CC, ACTIVE, DEFAULT, CURSORS = [DATA + n for n in (422, 434, 466, 474, 690)]
GLOBAL, PREFS, MESSAGE, NESTED, DEADKEY = 0x78000, 0x79000, 0x7b000, 0x7b100, 0x7b200
HANDLE, PARENT = 0x80426d66, 0x8042e35f
RAW, NORMAL = 0x400, 0x4000


class HandlerHarness(Harness):
    def __init__(self, path, mode='normal'):
        super().__init__(path)
        self.mode = mode
        self.events, self.poisoned, self.parent_reads = [], set(), []
        self.actions = {}
        self.scratch = 0x90000
        self.window_freed = False
        self.mem.w16(CLASS + 32, 28)
        self.mem.w32(OBJECT, GLOBAL)
        self.mem.w32(GLOBAL + 20, PREFS)
        self.mem.w32(DATA + 522, ROOT)
        self.mem.w32(DATA + 16, IWINDOW)
        self.mem.w32(IWINDOW + 120, OBJECT)
        for head in (EH, CC):
            self.mem.w32(head, head + 4)
            self.mem.w32(head + 8, head)
        self.nodes = [0x72000 + i * 0x100 for i in range(3)]
        self.clients = [0x73000 + i * 0x100 for i in range(3)]
        for node, client in zip(self.nodes, self.clients):
            self.mem.w32(node + 12, client)
            self.mem.w32(node + 16, CLASS if mode == 'control' else 0)
            self.mem.w32(node + 20, ord('a') if mode == 'control'
                           else NORMAL if mode == 'normal' else RAW)
            self.mem.w32(client + 64, 0x4200)
            head = CC if mode == 'control' else EH
            self.link(node, self.mem.r32(head + 8), head + 4)
        if mode in ('active', 'parent'):
            self.mem.w32(ACTIVE, self.clients[0])
        if mode == 'default':
            self.mem.w32(DEFAULT, self.clients[0])
        for message in (MESSAGE, NESTED):
            self.mem.w32(message + 20, NORMAL if mode == 'normal' else RAW)
            self.mem.w32(message + 28, DEADKEY)
            self.mem.w32(message + 44, IWINDOW)
        self.trap(EXEC - 378, lambda: None)  # ReplyMsg
        self.trap(EXEC - 690, self.free_cycle_chain)
        self.trap(self.symbols['_MUI_DisposeObject'], lambda: None)
        self.trap(self.symbols['_DoSuperMethodA'], self.dispose_window)
        self.trap(EXEC - 624, lambda: self.mem.w_block(
            self.cpu.r_reg(9), bytes(self.mem.r_block(
                self.cpu.r_reg(8), self.cpu.r_reg(0)))))  # CopyMem
        self.trap(INTUITION - 150, lambda: None)  # ModifyIDCMP
        self.trap(INTUITION - 648, lambda: self.cpu.w_reg(0, 0))
        self.trap(INTUITION - 654, self.get_attr)
        self.trap(self.symbols['_ConvertKey'], lambda: self.cpu.w_reg(
            0, ord('a') if self.mode == 'control' else 0))
        self.trap(self.symbols['_DoMethod'], lambda: self.method(False))
        self.trap(self.symbols['_CoerceMethod'], lambda: self.method(True))
        self.ret = self.blob(bytes.fromhex('70004e75'))

    def free_cycle_chain(self):
        assert self.cpu.r_reg(9) == 0, 'unexpected cycle-chain allocation'

    def dispose_window(self):
        self.window_freed = True
        self.poisoned.add(OBJECT)
        self.mem.w_block(OBJECT, b'\xa5' * (28 + 694))
        self.cpu.w_reg(0, 0)

    def blob(self, contents):
        address = self.scratch
        self.scratch += (len(contents) + 3) & ~3
        self.mem.w_block(address, bytes(contents))
        return address

    def words(self, *values):
        return self.blob(struct.pack('>' + 'I' * len(values), *values))

    def get_attr(self):
        obj, attr, storage = self.cpu.r_reg(8), self.cpu.r_reg(0), self.cpu.r_reg(9)
        assert obj not in self.poisoned, 'GetAttr on freed active object'
        if attr == PARENT:
            self.parent_reads.append(obj)
        self.mem.w32(storage, 0)
        self.cpu.w_reg(0, 1)

    def method(self, coerce):
        sp = self.cpu.r_sp()
        offset = 8 if coerce else 4
        obj, method = self.mem.r32(sp + offset), self.mem.r32(sp + offset + 4)
        target = self.ret
        if method == HANDLE:
            assert obj not in self.poisoned, 'event delivered to freed object'
            index = self.clients.index(obj)
            self.events.append('ABC'[index])
            action = self.actions.get(index)
            if action:
                target = action()
            self.cpu.w_reg(0, 0)
        elif obj == ROOT:
            self.cpu.w_reg(0, self.mem.r32(sp + offset + 8))
        else:
            self.cpu.w_reg(0, 0)
        # Preserve result for non-event helper calls.
        if method != HANDLE:
            target = self.blob(bytes.fromhex('4e75'))
        gate = self.symbols['_CoerceMethod' if coerce else '_DoMethod'] + 2
        self.mem.w16(gate, 0x4ef9)
        self.mem.w32(gate + 2, target)

    def script(self, remove=None, nested=False, eat=False, focus=False, dispose=False):
        code = bytearray()
        if remove is not None:
            node = self.nodes[remove]
            msg = self.words(0, node)
            for value in (msg, OBJECT, CLASS):
                code += bytes.fromhex('2f3c') + struct.pack('>I', value)
            name = 'Window__MUIM_RemControlCharHandler' if self.mode == 'control' else 'Window__MUIM_RemEventHandler'
            code += bytes.fromhex('4eb9') + struct.pack('>I', self.symbols['_' + name])
            code += bytes.fromhex('4fef000c')
            hook = self.blob(bytes(4))
            def poison():
                self.mem.w_block(node, b'\xa5' * 24)
                if focus:
                    self.mem.w32(ACTIVE, self.clients[1])
                    self.poisoned.add(self.clients[0])
                    self.mem.w_block(self.clients[0], b'\xa5' * 96)
            self.trap(hook, poison)
            code += bytes.fromhex('4eb9') + struct.pack('>I', hook)
        if dispose:
            for value in (self.words(0x102), OBJECT, CLASS):
                code += bytes.fromhex('2f3c') + struct.pack('>I', value)
            code += bytes.fromhex('4eb9') + struct.pack(
                '>I', self.symbols['_Window__OM_DISPOSE'])
            code += bytes.fromhex('4fef000c')
        if nested:
            code += bytes.fromhex('2f3c') + struct.pack('>I', NESTED)
            code += bytes.fromhex('4eb9') + struct.pack('>I', self.symbols['__zune_window_message'])
            code += bytes.fromhex('588f')
        code += bytes.fromhex('70014e75' if eat else '70004e75')
        return self.blob(code)

    def dispatch(self):
        self.call('_zune_window_message', MESSAGE)
        if self.window_freed:
            assert bytes(self.mem.r_block(OBJECT, 28 + 694)) == b'\xa5' * (28 + 694), 'dispatch wrote freed Window data'
        else:
            assert self.mem.r32(CURSORS) == 0, 'dispatch cursor escaped its stack frame'

    def finish(self):
        self.machine.cleanup()


def check(path):
    for mode in ('normal', 'other', 'active', 'default', 'control'):
        for removed, expected in ((0, 'ABC'), (1, 'AC')):
            h = HandlerHarness(path, mode)
            h.actions[0] = lambda h=h, removed=removed: (
                h.script(remove=removed) if h.events.count('A') == 1 else h.ret)
            h.dispatch()
            if mode == 'active' and removed == 1:
                expected = 'AAC'
            assert ''.join(h.events) == expected, (mode, removed, h.events)
            h.finish()
    for mode in ('normal', 'other', 'control'):
        h = HandlerHarness(path, mode)
        def nested_action():
            if h.events.count('A') == 1:
                return h.script(nested=True)
            return h.script(remove=1)
        h.actions[0] = nested_action
        h.dispatch()
        assert ''.join(h.events) == 'AACC', (mode, 'nested', h.events)
        h.finish()
    for mode in ('normal', 'other', 'active', 'default', 'control'):
        h = HandlerHarness(path, mode)
        h.actions[0] = lambda: h.script(remove=0, eat=True)
        h.dispatch()
        assert h.events == ['A'], (mode, 'eat', h.events)
        h.dispatch()
        assert ''.join(h.events) == 'ABC', (mode, 'after eat', h.events)
        h.finish()
    for mode in ('normal', 'other', 'active', 'default', 'control'):
        for eat in (False, True):
            h = HandlerHarness(path, mode)
            h.actions[0] = lambda: h.script(dispose=True, eat=eat)
            h.dispatch()
            assert h.events == ['A'], (mode, 'window disposal', h.events)
            assert h.window_freed
            h.finish()
    for mode in ('normal', 'other', 'control'):
        h = HandlerHarness(path, mode)
        h.actions[0] = lambda: (h.script(nested=True)
            if h.events.count('A') == 1 else h.script(dispose=True))
        h.dispatch()
        assert h.events == ['A', 'A'], (mode, 'nested disposal', h.events)
        assert h.window_freed
        h.finish()
    h = HandlerHarness(path, 'parent')
    h.actions[0] = lambda: h.script(remove=0, focus=True) if h.events.count('A') == 2 else h.ret
    h.dispatch()
    assert ''.join(h.events) == 'AAC', h.events
    assert not h.parent_reads, 'old parent traversed after focus change'
    h.finish()
    print(f'{path.name}: handler removal, nested cursors, Eat and focus lifetime passed')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        check(Path(filename))

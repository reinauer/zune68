#!/usr/bin/env python3
"""Run internal queued-window ownership paths in the linked 68000 library.

The real dispatcher closes, queues, cancels and reopens modeled windows.
External caller payloads are deliberately tracked to detect accidental frees.
"""
import struct
import sys
from pathlib import Path
from check_application_disposal import DisposalHarness, PUSH
from check_application_services import CLASS, OBJECT, DATA, FIELDS, INPUT

SET_CONFIG, OPEN_WINDOWS = 0x90420100, 0x90420101
UNPUSH, WINDOW_OPEN, FAMILY_LIST = 0x804211dd, 0x80428aa0, 0x80424b9e
SET_CONFIG_ITEM = 0x80424a80


class ReopenHarness(DisposalHarness):
    def __init__(self, path):
        super().__init__(path)
        self.family = self.blob(bytes(32))
        self.children = self.blob(bytes(16))
        self.windows = [self.blob(bytes(64)) + 12 for _ in range(3)]
        self.members = self.windows[:]
        self.open_state = dict(zip(self.windows, (True, True, False)))
        self.opened = []
        self.removed_on_open = None
        self.fail_after = None
        self.cfg = self.blob(bytes(16))
        self.prefs = self.blob(bytes(1024))
        self.field('family', self.family)
        self.mem.w32(DATA + 16, self.cfg)
        self.membership(self.members)
        self.trap(self.symbols['_DoMethod'], lambda: self.cpu.w_reg(0, 1))
        self.trap(self.symbols['_DoMethodA'], self.run_queued)

    def membership(self, members):
        self.members = members[:]
        self.new_list(self.children)
        for window in members:
            node = window - 12
            self.link(node, self.mem.r32(self.children + 8), self.children + 4)

    def get_attr(self):
        obj, attr, store = (self.cpu.r_reg(8), self.cpu.r_reg(0), self.cpu.r_reg(9))
        if obj == self.family and attr == FAMILY_LIST:
            value = self.children
        elif attr == WINDOW_OPEN:
            assert obj in self.members, 'dereferenced removed window'
            value = int(self.open_state[obj])
        elif obj == self.cfg and attr == 0x90420901:
            value = self.prefs
        else:
            raise AssertionError((hex(obj), hex(attr)))
        self.mem.w32(store, value)
        self.cpu.w_reg(0, 1)

    def set_attrs(self):
        obj, tags = self.cpu.r_reg(8), self.cpu.r_reg(9)
        attr, value = self.mem.r32(tags), self.mem.r32(tags + 4)
        assert obj in self.members and attr == WINDOW_OPEN, 'using stale window'
        self.open_state[obj] = bool(value)
        if value:
            self.opened.append(obj)
            if self.removed_on_open:
                self.membership([w for w in self.members if w != self.removed_on_open])
        self.cpu.w_reg(0, 1)

    def alloc_vec(self):
        if self.fail_after is not None:
            if self.fail_after == 0:
                self.fail_after = None
                self.cpu.w_reg(0, 0)
                return
            self.fail_after -= 1
        super().alloc_vec()

    def run_queued(self):
        obj, message = self.argument(0), self.argument(1)
        assert obj == OBJECT and self.mem.r32(message) == OPEN_WINDOWS
        # Call the register dispatcher from the real queue executor's C call.
        code = bytes.fromhex('48e77ffe')
        for opcode, value in ((0x207c, CLASS), (0x247c, OBJECT), (0x227c, message)):
            code += struct.pack('>HI', opcode, value)
        code += bytes.fromhex('4eb9') + struct.pack('>I', self.symbols['_Application_Dispatcher'])
        code += bytes.fromhex('4cdf7ffe4e75')
        entry = self.blob(code)
        self.mem.w_block(self.symbols['_DoMethodA'] + 2,
                        bytes.fromhex('4ef9') + struct.pack('>I', entry))

    def queue(self, via_item=False):
        if via_item:
            self.dispatch(SET_CONFIG_ITEM, 0x1e, self.blob(b'topaz.font\0'))
        else:
            self.dispatch(SET_CONFIG, self.cfg)
        return self.mem.r32(DATA + FIELDS['methods'])

    def dispose(self):
        # This test owns no real Family or Configdata objects; remove their
        # modeled pointers before running the actual queue-disposal path.
        self.field('family', 0)
        self.mem.w32(DATA + 16, 0)
        self.dispatch(0x102)


def check(path):
    for via_item in (False, True):
        for action in ('execute', 'remove', 'remove during open', 'unpush',
                       'dispose', 'enqueue failure'):
            h = ReopenHarness(path)
            if action == 'enqueue failure':
                h.fail_after = 3  # List + two open-window nodes, then MQNode.
            node = h.queue(via_item)
            assert not any(h.open_state.values())
            if action == 'enqueue failure':
                assert node == DATA + FIELDS['methods'] + 4
                assert not h.allocations
            else:
                assert len(h.allocations) == 4
                assert h.mem.r32(node + 12) == 0x80000002  # Owned list.
                assert h.mem.r32(h.mem.r32(node + 16) + 4) in h.allocations
                if action == 'unpush':
                    assert h.dispatch(UNPUSH, OBJECT, 0, OPEN_WINDOWS) == 1
                elif action == 'dispose':
                    h.dispose()
                    assert h.disposals == 1
                else:
                    if action == 'remove':
                        h.membership([h.windows[0], h.windows[2]])
                        h.mem.w_block(h.windows[1], bytes([0xa5]) * 32)
                    if action == 'remove during open':
                        h.removed_on_open = h.windows[1]
                    h.dispatch(INPUT, h.words(0))
                    expected = h.windows[:2] if action == 'execute' else h.windows[:1]
                    assert h.opened == expected, (action, h.opened, expected)
                assert not h.allocations, (action, h.allocations)
            h.finish()

    # A public caller can enqueue even this same method ID without giving
    # the queue ownership of its argument pointer. Cancellation must not free it.
    for action in ('unpush', 'dispose'):
        h = ReopenHarness(path)
        payload = h.alloc(16)
        node = h.dispatch(PUSH, OBJECT, 2, OPEN_WINDOWS, payload)
        assert h.mem.r32(node + 12) == 2
        if action == 'unpush':
            assert h.dispatch(UNPUSH, OBJECT, node, 0) == 1
            assert payload in h.allocations
            h.cpu.w_reg(9, payload)
            h.free_vec()
        else:
            # The superclass assertion is normally stricter; the caller's
            # allocation legitimately survives disposal of this application.
            h.trap(h.symbols['_DoSuperMethodA'], lambda: h.cpu.w_reg(0, 0))
            h.dispose()
            assert set(h.allocations) == {payload}
            h.cpu.w_reg(9, payload)
            h.free_vec()
        h.finish()
    print(f'{path.name}: internal reopen lists freed on execution/cancel/dispose/'
          'enqueue failure; removed windows skipped and caller payloads retained')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

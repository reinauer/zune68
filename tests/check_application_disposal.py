#!/usr/bin/env python3
"""Discard real linked PushMethod allocations during Application disposal.

Unrelated OS resources are absent or modeled. Calls run through the native
register dispatcher; pending destinations must never receive their calls.
"""
import sys
from pathlib import Path
from check_application_services import ApplicationHarness, CLASS, OBJECT, DATA, FIELDS
from check_class_lifetime import EXEC

PUSH = 0x80429ef8


class DisposalHarness(ApplicationHarness):
    def __init__(self, path):
        super().__init__(path)
        self.disposals = 0
        self.constructing = False
        self.field('rexx_port', 0)
        self.field('app_port', 0)
        self.mem.w32(DATA + 8, 0)
        self.mem.w32(CLASS + 8, self.symbols['_Application_Dispatcher'])
        self.trap(EXEC - 324, lambda: None)  # PushMethod wakeup.
        self.trap(EXEC - 672, lambda: None)  # DeleteMsgPort(NULL).
        self.trap(self.symbols['_DoSuperMethodA'], self.superclass)
        self.trap(self.symbols['_DoMethodA'], self.unexpected_method)

    def unexpected_method(self):
        raise AssertionError('queued destination invoked during disposal')

    def superclass(self):
        method = self.mem.r32(self.argument(2))
        if method == 0x101:
            assert self.constructing
            self.cpu.w_reg(0, OBJECT)
        else:
            assert method == 0x102
            self.disposals += 1
            assert not self.allocations, 'queued storage survives to superclass'
            head = DATA + FIELDS['methods']
            assert self.mem.r32(head) == head + 4
            assert self.mem.r32(head + 8) == head
            self.cpu.w_reg(0, 0)


def check(path):
    for counts in ((), (1,), (0, 1, 3, 15)):
        h = DisposalHarness(path)
        queued = []
        for index, count in enumerate(counts):
            values = tuple(0x81720000 + index * 16 + n for n in range(count))
            node = h.dispatch(PUSH, 0xdead0000 + index * 4, count, *values)
            assert node and h.allocations[node] == 20 + count * 4
            queued.append(node)
        assert len(h.allocations) == len(queued)
        # Each allocation is freed once and poisoned by the common harness.
        h.dispatch(0x102)
        assert h.disposals == 1 and set(h.freed) == set(queued)
        h.finish()

    # The earliest constructor failure happens before InitSemaphore. Its
    # empty method queue must still be safe for the actual disposal path.
    h = DisposalHarness(path)
    h.mem.w_block(DATA, bytes(6310))
    h.constructing = True
    h.trap(h.symbols['_MUI_NewObjectA'], lambda: h.cpu.w_reg(0, 0))
    assert h.dispatch(0x101, h.words(0, 0), 0) == 0
    assert h.disposals == 1
    h.finish()
    print(f'{path}: pending methods discarded on application disposal')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

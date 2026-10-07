#!/usr/bin/env python3
"""Exercise linked Application input services with modelled AmigaOS ports.

Calls the real register dispatcher and hook trampoline on a 68000. This
does not replace Workbench/ARexx/AmigaGuide integration testing.
"""
import sys
from pathlib import Path
from check_notifications import NotifyHarness
from check_class_lifetime import EXEC, INTUITION, MASTER, STACK

OBJECT, CLASS, DATA = 0x70000, 0x71e00, 0x70040
DOS, REXX, GUIDE = 0x72200, 0x72400, 0x72a00
WINDOW_PORT, APP_PORT, REXX_PORT = 0x72600, 0x72700, 0x72800
INPUT, BUFFERED, RETURN_ID = 0x80423ba6, 0x80427e59, 0x804276ef
# Checked using offsetof with the target compiler and application.c.
FIELDS = dict(family=28, handlers=32, methods=44, ids=102, timer=122,
              rexx_port=6232, rexx_msg=6236, rexx_hook=6240, commands=6244,
              rexx_string=6248, app_port=6254, drop=6270, help=166)


class ApplicationHarness(NotifyHarness):
    def __init__(self, path):
        super().__init__(path)
        self.mem.w16(CLASS + 32, 64)
        self.queues = {}
        self.replies = []
        self.deliveries = []
        self.hooks = []
        self.guide_events = []
        self.fail_parse = False
        self.fail_guide = False
        self.attributes = {}
        self.menu_items = {}
        self.mem.w32(self.symbols['_DOSBase'], DOS)
        self.mem.w32(self.symbols['_RexxSysBase'], REXX)
        for offset in [FIELDS['handlers'], FIELDS['methods'], FIELDS['ids']]:
            self.new_list(DATA + offset)
        self.mem.w32(DATA + 8, WINDOW_PORT)  # mgi_WindowsPort
        self.field('app_port', APP_PORT)
        self.field('rexx_port', REXX_PORT)
        for bit, port in enumerate([WINDOW_PORT, APP_PORT, REXX_PORT], 20):
            self.mem.w8(port + 15, bit)
            self.queues[port] = []
        for offset, fn in [(258, self.rem_head), (372, self.get_msg),
                           (378, self.reply), (306, lambda: self.cpu.w_reg(0, 0)),
                           (132, lambda: None), (138, lambda: None),
                           (624, self.copy), (552, self.open_guide),
                           (414, self.close_guide)]:
            self.trap(EXEC - offset, fn)
        self.trap(self.symbols['__zune_window_message'], self.window_message)
        self.trap(INTUITION - 648, self.set_attrs)
        self.trap(INTUITION - 654, self.get_attr)
        self.trap(0x72000 - 168, self.strnicmp)
        # utility.library CallHookPkt: invoke h_Entry with A0/A2/A1 intact.
        self.mem.w_block(0x72000 - 102, b'\x2f\x28\x00\x08\x4e\x75')
        self.trap(REXX - 168, lambda: self.cpu.w_reg(0, 1))
        self.trap(REXX - 126, self.argstring)
        self.trap(DOS - 228, lambda: self.cpu.w_reg(0, self.alloc(32)))
        self.trap(DOS - 234, self.free_dos)
        self.trap(DOS - 798, self.read_args)
        self.trap(DOS - 858, self.free_args)
        self.trap(GUIDE - 54, self.open_help)
        self.trap(GUIDE - 66, lambda: self.guide_events.append('close'))
        self.trap(self.symbols['_DoSuperMethodA'], lambda: self.cpu.w_reg(0, 0))
        self.trap(self.symbols['_DoMethod'], self.menu_method)

    def field(self, name, value):
        self.mem.w32(DATA + FIELDS[name], value)

    def new_list(self, address):
        self.mem.w32(address, address + 4)
        self.mem.w32(address + 4, 0)
        self.mem.w32(address + 8, address)

    def rem_head(self):
        head = self.cpu.r_reg(8)
        node = self.mem.r32(head)
        if self.mem.r32(node):
            self.cpu.w_reg(9, node)
            self.remove()
            self.cpu.w_reg(0, node)
        else:
            self.cpu.w_reg(0, 0)

    def get_msg(self):
        queue = self.queues.get(self.cpu.r_reg(8), [])
        self.cpu.w_reg(0, queue.pop(0) if queue else 0)

    def reply(self):
        address = self.cpu.r_reg(9)
        assert address not in self.replies, 'double reply'
        self.replies.append(address)

    def window_message(self):
        self.deliveries.append(('window', self.argument(0)))

    def copy(self):
        self.mem.w_block(self.cpu.r_reg(9), self.mem.r_block(
            self.cpu.r_reg(8), self.cpu.r_reg(0)))

    def set_attrs(self):
        obj, tags = self.cpu.r_reg(8), self.cpu.r_reg(9)
        attr, value = self.mem.r32(tags), self.mem.r32(tags + 4)
        if attr == 0x80421955:  # MUIA_AppMessage
            assert value not in self.replies, 'message replied before delivery'
        self.deliveries.append((obj, attr, value))
        self.cpu.w_reg(0, 0)

    def get_attr(self):
        key = (self.cpu.r_reg(8), self.cpu.r_reg(0))
        self.mem.w32(self.cpu.r_reg(9), self.attributes.get(key, 1))
        self.cpu.w_reg(0, 1)

    def menu_method(self):
        assert self.argument(1) == 0x8042c196  # FindUData
        self.cpu.w_reg(0, self.menu_items.get((self.argument(0), self.argument(2)), 0))

    def strnicmp(self):
        a = bytes(self.mem.r_block(self.cpu.r_reg(8), self.cpu.r_reg(0)))
        b = bytes(self.mem.r_block(self.cpu.r_reg(9), self.cpu.r_reg(0)))
        self.cpu.w_reg(0, 0 if a.lower() == b.lower() else 1)

    def argstring(self):
        value = bytes(self.mem.r_block(self.cpu.r_reg(8), self.cpu.r_reg(0)))
        self.cpu.w_reg(0, self.blob(value + b'\0'))

    def free_dos(self):
        self.cpu.w_reg(9, self.cpu.r_reg(2))
        self.free_vec()

    def read_args(self):
        rdargs = self.cpu.r_reg(3)
        args = self.cpu.r_reg(2)
        self.mem.w32(args, self.mem.r32(rdargs))
        self.cpu.w_reg(0, 0 if self.fail_parse else rdargs)

    def free_args(self):
        rdargs = self.cpu.r_reg(1)
        text = self.mem.r32(rdargs)
        assert text in self.allocations, 'argument storage freed before FreeArgs'

    def open_guide(self):
        assert self.mem.r_cstr(self.cpu.r_reg(9)) == 'amigaguide.library'
        self.cpu.w_reg(0, 0 if self.fail_guide else GUIDE)

    def close_guide(self):
        assert self.cpu.r_reg(9) == GUIDE
        self.guide_events.append('library closed')

    def open_help(self):
        nag = self.cpu.r_reg(8)
        assert self.mem.r_cstr(self.mem.r32(nag + 4)) == 'manual.guide'
        assert self.mem.r_cstr(self.mem.r32(nag + 36)) == 'chapter'
        assert self.mem.r32(nag + 40) == 7
        self.guide_events.append('open')
        self.cpu.w_reg(0, 0x1234)

    def hook(self, result=0, string=None, parsed=False):
        hook, entry = self.blob(bytes(20)), self.blob(bytes(4))
        self.mem.w32(hook + 8, entry)

        def callback():
            assert self.cpu.r_reg(8) == hook and self.cpu.r_reg(10) == OBJECT
            message = self.cpu.r_reg(9)
            assert self.mem.r32(DATA + FIELDS['rexx_msg'])
            if parsed:
                assert self.mem.r_cstr(self.mem.r32(message)) == 'hello\n'
            self.hooks.append(message)
            if string is not None:
                text = self.alloc(len(string) + 1)
                self.mem.w_block(text, string.encode() + b'\0')
                self.field('rexx_string', text)
            self.cpu.w_reg(0, result)

        self.trap(entry, callback)
        return hook

    def dispatch(self, method, *args):
        message = self.words(method, *args)
        self.cpu.w_reg(8, CLASS)
        self.cpu.w_reg(10, OBJECT)
        self.cpu.w_reg(9, message)
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_Application_Dispatcher'], STACK)
        result = self.machine.execute(200000)
        assert self.machine.was_exit(result), 'application dispatcher'
        self.check_freed()
        return self.cpu.r_reg(0)

    def rexx(self, command):
        msg = self.blob(bytes(128))
        self.mem.w32(msg + 28, 1 << 17)  # RXFF_RESULT
        self.mem.w32(msg + 40, self.blob(command.encode() + b'\0'))
        self.queues[REXX_PORT].append(msg)
        return msg

    def finish(self):
        self.check_freed()
        assert not self.allocations, 'application service leak'
        self.machine.cleanup()


def check(path):
    h = ApplicationHarness(path)
    storage = h.words(0)
    for attr in [0x80422301, 0x80427eaa]:
        assert h.dispatch(0x104, attr, storage)
        assert h.mem.r32(storage) == 1
    h.dispatch(RETURN_ID, 101)
    h.dispatch(RETURN_ID, 102)
    messages = [h.blob(bytes(64)) for _ in range(3)]
    h.queues[WINDOW_PORT] = list(messages)
    h.dispatch(BUFFERED)
    assert h.deliveries == [('window', n) for n in messages]
    signals = h.words(0)
    assert h.dispatch(INPUT, signals) == 101 and h.mem.r32(signals) == 0
    assert h.dispatch(INPUT, signals) == 102
    assert h.dispatch(INPUT, signals) == 0
    h.finish()

    h = ApplicationHarness(path)
    family, children = h.blob(bytes(16)), h.blob(bytes(12))
    nodes = [h.blob(bytes(32)), h.blob(bytes(32))]
    h.new_list(children)
    for node in nodes:
        h.link(node, h.mem.r32(children + 8), children + 4)
    h.field('family', family)
    h.attributes[(family, 0x80424b9e)] = children
    for n, node in enumerate(nodes):
        h.attributes[(node + 12, 0x8042855e)] = 100 + n
        h.menu_items[(100 + n, 42)] = 200 + n
        h.attributes[(200 + n, 0x8042562a)] = n
        h.attributes[(200 + n, 0x8042ae0f)] = 1 - n
    assert h.dispatch(0x8042c0a7, 42) == 0  # first found, even if unchecked
    assert h.dispatch(0x8042a58f, 42) == 1
    h.dispatch(0x8042a707, 42, 1)
    assert [e[0] for e in h.deliveries] == [200, 201]
    h.finish()

    for command, known, parsed, fail in [('UNKNOWN', False, False, False),
            ('pING', True, False, False), ('PIN', False, False, False),
            ('PING hello', True, True, False), ('PING hello', True, True, True)]:
        h = ApplicationHarness(path)
        fallback = h.hook(string='fallback')
        h.field('rexx_hook', fallback)
        if known or command == 'PIN':
            hook = h.hook(string='command', parsed=parsed)
            table = h.words(h.blob(b'PING\0'), h.blob(b'ARG/A\0') if parsed else 0,
                            1 if parsed else 0, hook, 0, 0, 0, 0)
            h.field('commands', table)
        h.fail_parse = fail
        msg = h.rexx(command)
        h.dispatch(BUFFERED)
        assert h.replies == [msg]
        assert h.mem.r32(msg + 32) == (10 if fail else 0)
        if fail:
            assert not h.hooks and not h.mem.r32(msg + 36)
        else:
            expected = 'command' if known else 'fallback'
            assert h.mem.r_cstr(h.mem.r32(msg + 36)) == expected
        assert not h.mem.r32(DATA + FIELDS['rexx_msg'])
        assert not h.mem.r32(DATA + FIELDS['rexx_string'])
        h.finish()

    for failed_call in [1, 2, 3]:  # args, source string, RDArgs
        h = ApplicationHarness(path)
        hook = h.hook(parsed=True)
        table = h.words(h.blob(b'PING\0'), h.blob(b'ARG/A\0'),
                        1, hook, 0, 0, 0, 0)
        h.field('commands', table)
        calls = [0]
        def allocate():
            calls[0] += 1
            h.fail_alloc = calls[0] == failed_call
            h.alloc_vec()
        def alloc_dos():
            h.cpu.w_reg(0, 32)
            allocate()
        h.trap(EXEC - 684, allocate)
        h.trap(DOS - 228, alloc_dos)
        msg = h.rexx('PING hello')
        h.dispatch(BUFFERED)
        assert h.replies == [msg] and not h.hooks
        assert h.mem.r32(msg + 32) == 10 and not h.mem.r32(msg + 36)
        h.finish()

    h = ApplicationHarness(path)
    h.field('rexx_hook', h.hook(result=10, string='error'))
    msg = h.rexx('UNKNOWN')
    h.dispatch(BUFFERED)
    assert h.mem.r32(msg + 32) == 10 and not h.mem.r32(msg + 36)
    h.finish()

    h = ApplicationHarness(path)
    msg = h.rexx('UNKNOWN')
    h.dispatch(BUFFERED)
    assert h.mem.r32(msg + 32) == 10 and h.replies == [msg]
    h.finish()

    h = ApplicationHarness(path)
    h.field('drop', 0x123456)
    for kind, count in [(8, 1), (8, 0), (7, 1)]:
        msg = h.blob(bytes(64))
        h.mem.w16(msg + 20, kind)
        h.mem.w32(msg + 30, count)
        h.mem.w32(msg + 22, 0x234567)
        h.queues[APP_PORT].append(msg)
    h.dispatch(BUFFERED)
    assert len(h.replies) == 3
    assert h.deliveries[0][:2] == (0x123456, 0x80421955)
    assert h.deliveries[1][0] == OBJECT  # restore on double click
    assert h.deliveries[2][:2] == (0x234567, 0x80421955)
    h.finish()

    h = ApplicationHarness(path)
    h.field('help', h.blob(b'manual.guide\0'))
    assert h.dispatch(0x80426479, 0, 0, h.blob(b'chapter\0'), 7)
    assert h.guide_events == ['open', 'close', 'library closed']
    assert [e[2] for e in h.deliveries] == [1, 0]
    h.fail_guide = True
    assert not h.dispatch(0x80426479, 0, 0, 0, 0)
    assert len(h.deliveries) == 2
    h.finish()
    print(f'{path}: buffered input, ARexx, AppIcon drops, menus and help pass')


if __name__ == '__main__':
    for filename in sys.argv[1:]:
        check(Path(filename))

#!/usr/bin/env python3
"""Execute linked Prefs actions on a 68000 with modeled config/GUI services.

This checks write/reload ownership and action sequences, not a GUI session.
"""
import sys
from pathlib import Path

from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from amitools.vamos.machine.machine import Machine

BASE, STACK = 0x10000, 0xf0000
MASTER, INTUITION, NAME, PAGE = 0x68000, 0x69000, 0x70000, 0x71000
SAVE, LOAD = 0x90420906, 0x90420907
TO_GADGETS, FROM_GADGETS = 0x80427043, 0x80425242
RETURN_ID, CONFIGDATA = 0x804276ef, 0x90420100


class PrefsHarness:
    def __init__(self, path):
        image = BinFmt().load_image(str(path))
        reloc = Relocate(image)
        self.symbols = {s.name.decode(): address + s.offset
                        for seg, address in zip(image.get_segments(),
                                                reloc.get_seq_addrs(BASE))
                        if seg.symtab for s in seg.symtab.symbols}
        self.machine = Machine()
        self.mem, self.cpu = self.machine.mem, self.machine.cpu
        self.mem.w_block(BASE, bytes(reloc.relocate_one_block(BASE)))
        self.mem.w32(self.symbols['_MUIMasterBase'], MASTER)
        self.mem.w32(self.symbols['_IntuitionBase'], INTUITION)
        self.mem.w_block(NAME, b'TESTAPP\0')
        entries = self.symbols['_main_page_entries']
        # The first real page is sufficient. Preserve the compiled array
        # stride by terminating it at its existing second entry.
        self.mem.w32(entries + 8, PAGE)
        self.mem.w32(entries + 314, 0)
        self.events, self.configs = [], {}
        self.next_config = 0x72000
        self.value = 10
        self.trap(self.symbols['_MUI_NewObject'], self.new_config)
        self.trap(self.symbols['_DoMethod'], self.method)
        self.trap(self.symbols['_snprintf'], self.format_path)
        self.trap(self.symbols['_aslfilerequest'], self.file_request)
        self.trap(MASTER - 36, self.dispose)
        self.trap(INTUITION - 648, self.set_attrs)

    def trap(self, address, callback):
        index = self.machine.traps.alloc(lambda opcode, pc: callback())
        self.mem.w16(address, 0xa000 | index)
        self.mem.w16(address + 2, 0x4e75)

    def arg(self, index):
        return self.mem.r32(self.cpu.r_sp() + 4 + 4 * index)

    def string(self, address):
        data = bytearray()
        while self.mem.r8(address):
            data.append(self.mem.r8(address))
            address += 1
        return data.decode()

    def new_config(self):
        config = self.next_config
        self.next_config += 16
        self.configs[config] = 10
        self.events.append(('new', config))
        self.cpu.w_reg(0, config)

    def dispose(self):
        config = self.cpu.r_reg(8)
        del self.configs[config]
        self.events.append(('dispose', config))

    def set_attrs(self):
        tags = self.cpu.r_reg(9)
        assert self.mem.r32(tags) == CONFIGDATA
        config = self.mem.r32(tags + 4)
        assert config in self.configs
        self.events.append(('reload', config))
        self.cpu.w_reg(0, 1)

    def format_path(self):
        # Exercise the real callers while excluding libc and filesystem I/O.
        template = self.string(self.arg(2))
        self.mem.w_block(self.arg(0), (template % 'TESTAPP').encode() + b'\0')
        self.cpu.w_reg(0, 0)

    def file_request(self):
        self.mem.w_block(self.arg(3), b'RAM:import.prefs\0')
        self.cpu.w_reg(0, 1)

    def method(self):
        obj, method = self.arg(0), self.arg(1)
        if method == TO_GADGETS:
            assert obj == PAGE
            self.value = self.configs[self.arg(2)]
            self.events.append(('gadgets', self.value))
        elif method == FROM_GADGETS:
            assert obj == PAGE
            self.configs[self.arg(2)] = self.value
        elif method == SAVE:
            self.events.append(('save', self.string(self.arg(2)),
                                self.configs[obj]))
        elif method == LOAD:
            self.configs[obj] = 30
        elif method == RETURN_ID:
            assert self.arg(2) == 0xffffffff
            self.events.append(('quit',))
        else:
            raise AssertionError(hex(method))
        self.cpu.w_reg(0, 1)

    def call(self, name, *args):
        for index, value in enumerate(args):
            self.mem.w32(STACK + index * 4, value)
        self.machine.prepare(self.symbols['_' + name], STACK)
        result = self.machine.execute(100000)
        assert self.machine.was_exit(result), name
        assert self.cpu.r_sp() == STACK, name

    def changes(self):
        return [event for event in self.events if event[0] in ('save', 'reload')]


def check(path):
    for action in ('main_cancel_pressed', 'main_revert_pressed'):
        h = PrefsHarness(path)
        try:
            h.call('load_prefs', NAME)
            h.events.clear()
            h.value = 20  # Edits to gadgets alone are not a live Test.
            h.call(action)
            assert h.value == 10
            assert h.changes() == [], 'untested edits must not write or reload'
            if 'cancel' in action:
                assert ('quit',) in h.events

            h.value = 20
            h.call('main_test_pressed')
            assert ('save', 'ENV:zune/TESTAPP.prefs', 20) in h.events
            h.events.clear()
            h.call(action)
            assert h.value == 10
            assert h.changes()[0] == ('save', 'ENV:zune/TESTAPP.prefs', 10)
            assert len(h.changes()) == 2, 'restore once and reload own prefs'
            h.events.clear()
            h.call('main_cancel_pressed')
            assert h.changes() == [], 'Revert must clear the live-Test state'
        finally:
            h.machine.cleanup()

    h = PrefsHarness(path)
    try:
        h.call('load_prefs', NAME)
        h.events.clear()
        h.call('main_open_menu')
        assert h.value == 30
        assert ('save', 'ENV:zune/TESTAPP.prefs', 30) in h.events
        created = [event[1] for event in h.events if event[0] == 'new']
        assert len(created) == 3  # import, save, own live configuration
        assert created[0] not in h.configs, 'import must release its temporary'
        assert created[1] not in h.configs, 'save must release its temporary'
        assert created[2] in h.configs, 'Application owns its live configuration'
        h.events.clear()
        h.call('main_cancel_pressed')
        assert h.changes()[0] == ('save', 'ENV:zune/TESTAPP.prefs', 10)

        for action, paths in (
                ('main_use_pressed', ['ENV:zune/TESTAPP.prefs']),
                ('main_save_pressed', ['ENVARC:zune/TESTAPP.prefs',
                                       'ENV:zune/TESTAPP.prefs'])):
            h.events.clear()
            h.value = 40
            h.call(action)
            assert h.changes() == [('save', name, 40) for name in paths]
            assert ('quit',) in h.events
    finally:
        h.machine.cleanup()
    print(f'{path.name}: Cancel/Revert, Test/import restoration and Save/Use OK')


if __name__ == '__main__':
    check(Path(sys.argv[1]))

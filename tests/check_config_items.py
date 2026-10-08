#!/usr/bin/env python3
"""Execute native preference lookup and Notify forwarding on a 68000.

Real dispatchers resolve builtin defaults and scalar values. Only the
Dataspace storage service is modeled, including custom binary settings.
"""
import struct
import sys
from pathlib import Path
from check_class_lifetime import Harness, EXEC

CLASS, OBJECT, CONFIG, GLOBAL = 0x70000, 0x71000, 0x72000, 0x73000
MESSAGE, STORE, PAYLOAD = 0x74000, 0x75000, 0x76000
GET_CONFIG, FIND = 0x80423edb, 0x8042832c
SENTINEL = 0xabcdef01


class ConfigHarness(Harness):
    def __init__(self, path):
        super().__init__(path)
        self.items = {}
        self.mem.w32(CLASS + 8, self.symbols['_Configdata_Dispatcher'])
        self.mem.w32(CONFIG - 4, CLASS)
        self.mem.w32(OBJECT, GLOBAL)
        self.mem.w32(GLOBAL + 16, CONFIG)
        self.trap(self.symbols['_DoSuperMethodA'], self.find)

    def find(self):
        sp = self.cpu.r_sp()
        assert self.mem.r32(sp + 8) == CONFIG
        msg = self.mem.r32(sp + 12)
        assert self.mem.r32(msg) == FIND
        self.cpu.w_reg(0, self.items.get(self.mem.r32(msg + 4), 0))

    def query(self, item, direct=False, storage=STORE):
        self.mem.w32(STORE, SENTINEL)
        self.mem.w_block(MESSAGE, struct.pack('>3I', GET_CONFIG, item, storage))
        if direct:
            result = self.call('DoMethodA', CONFIG, MESSAGE)
        else:
            result = self.call('Notify__MUIM_GetConfigItem', CLASS, OBJECT,
                               MESSAGE)
        return result, self.mem.r32(STORE)


def check(path):
    h = ConfigHarness(path)
    # A fresh application must return usable margins without a saved prefs
    # file. Zero is also a successful scalar result, not a failed lookup.
    for direct in (False, True):
        for item, expected in ((1, 4), (2, 4), (3, 3), (4, 3),
                               (5, 4), (6, 1), (7, 4), (8, 4), (9, 0)):
            assert h.query(item, direct) == (1, expected), item
        ok, ptr = h.query(36, direct)
        assert ok and ptr and h.mem.r8(ptr) == 0  # Default PublicScreen.
        # Reference defaults: symmetric padding except the slider knob.
        frames = {0x18: '300000', 0x2b: '202222', 0x2c: '202222',
                  0x2d: '212222', 0x2e: '302222', 0x2f: '212222',
                  0x30: '202222', 0x31: '202222', 0x32: '210000',
                  0x33: '314444', 0x34: '112222', 0x35: '212222',
                  0x36: '400000', 0x90: '202211'}
        for item, expected in frames.items():
            ok, ptr = h.query(item, direct)
            assert ok and bytes(h.mem.r_block(ptr, 7)) == expected.encode() + b'\0'
        # Presets remain distinct even without saved preferences.
        for item, expected in ((0x20, b'helvetica/9\0'),
                               (0x22, b'helvetica/9\0'),
                               (0x23, b'helvetica/15\0'),
                               (0x1e, b'\0'), (0x80, b'\0')):
            ok, ptr = h.query(item, direct)
            assert ok and bytes(h.mem.r_block(ptr, len(expected))) == expected
        assert h.query(0x12345678, direct) == (0, SENTINEL)
        assert h.query(7, direct, storage=0) == (0, SENTINEL)

    assert h.query(0x50a) == (1, 5)
    h.mem.w32(PAYLOAD, 0)
    h.items[0x50a] = PAYLOAD
    assert h.query(0x50a) == (1, 0)

    # Builtin numeric data is decoded, while custom numeric/binary prefs
    # keep the historical pointer contract used by external classes.
    h.mem.w32(PAYLOAD, 17)
    h.items[7] = PAYLOAD
    assert h.query(7) == (1, 17)
    h.mem.w32(PAYLOAD, 0)
    assert h.query(7) == (1, 0)
    h.items[0xad001234] = PAYLOAD
    assert h.query(0xad001234) == (1, PAYLOAD)
    h.mem.w_block(PAYLOAD + 16, b'Public Workbench\0')
    h.items[36] = PAYLOAD + 16
    assert h.query(36) == (1, PAYLOAD + 16)

    h.mem.w_block(PAYLOAD + 64, b'custom/10\0')
    h.items[0x20] = PAYLOAD + 64
    assert h.query(0x20) == (1, PAYLOAD + 64)

    h.mem.w_block(PAYLOAD + 96, b'202211\0')
    h.items[0x2b] = PAYLOAD + 96
    assert h.query(0x2b) == (1, PAYLOAD + 96)  # Retain saved frame padding.

    # Objects need not have an application yet; invalid routing must not
    # dereference null global info or recurse back into Notify.
    h.mem.w32(OBJECT, 0)
    assert h.query(7) == (0, SENTINEL)
    h.mem.w32(OBJECT, GLOBAL)
    h.mem.w32(GLOBAL + 16, 0)
    assert h.query(7) == (0, SENTINEL)
    h.mem.w32(GLOBAL + 16, OBJECT)
    assert h.query(7) == (0, SENTINEL)
    assert not h.allocations
    h.machine.cleanup()
    check_font_fallback(path)
    check_popup_dimensions(path)
    check_cycle_padding(path)
    print(f'{path}: preference scalar/default/pointer checks passed')


def check_font_fallback(path):
    h = Harness(path)
    mri, prefs, dos, diskfont = 0x77000, 0x78000, 0x79000, 0x7a000
    normal, opened = 0x7b000, 0x7b100
    h.mem.w32(OBJECT, GLOBAL)
    h.mem.w32(OBJECT + 28, mri)
    h.mem.w32(GLOBAL + 20, prefs)
    h.mem.w32(mri + 52 + 4, normal)  # Normal owns its cached font.
    h.mem.w32(h.symbols['_DOSBase'], dos)
    h.mem.w32(h.symbols['_DiskfontBase'], diskfont)
    disk_calls = []
    disk_result = 0

    def filepart():
        addr = h.cpu.r_reg(1)
        h.cpu.w_reg(0, addr + h.mem.r_cstr(addr).rfind('/') + 1)

    def pathpart():
        addr = h.cpu.r_reg(1)
        h.cpu.w_reg(0, addr + h.mem.r_cstr(addr).rfind('/'))

    def number():
        text = h.mem.r_cstr(h.cpu.r_reg(1))
        h.mem.w32(h.cpu.r_reg(2), int(text))
        h.cpu.w_reg(0, len(text))

    def openfont():
        attr = h.cpu.r_reg(8)
        disk_calls.append((h.mem.r_cstr(h.mem.r32(attr)),
                           h.mem.r16(attr + 4)))
        h.cpu.w_reg(0, disk_result)

    h.trap(dos - 870, filepart)
    h.trap(dos - 876, pathpart)
    h.trap(dos - 816, number)
    h.trap(diskfont - 30, openfont)
    for preset, size in ((3, 9), (5, 9), (6, 15)):
        h.mem.w_block(PAYLOAD, f'helvetica/{size}\0'.encode())
        h.mem.w32(prefs + 4 * preset, PAYLOAD)
        assert h.call('zune_font_get', OBJECT, (-preset) & 0xffffffff) == normal
        assert disk_calls[-1] == ('helvetica.font', size)
        assert h.mem.r32(mri + 52 + 4 * preset) == 0
        assert not h.allocations

    count = len(disk_calls)
    h.fail_alloc = True
    assert h.call('zune_font_get', OBJECT, 0xfffffffd) == normal
    assert len(disk_calls) == count and not h.allocations

    disk_result = opened
    assert h.call('zune_font_get', OBJECT, 0xfffffffd) == opened
    assert h.mem.r32(mri + 52 + 12) == opened
    count = len(disk_calls)
    assert h.call('zune_font_get', OBJECT, 0xfffffffd) == opened
    assert len(disk_calls) == count and not h.allocations
    h.machine.cleanup()


def check_popup_dimensions(path):
    h = Harness(path)
    # MUII_PopUp defaults to vector 7. Image padding is added by Area;
    # the symbol itself has a 10x10 minimum on the reference library.
    spec = h.call('zune_imspec_create_vector', 7)
    assert spec
    assert h.call('zune_imspec_vector_get_minmax', spec, STORE)
    assert struct.unpack('>6h', bytes(h.mem.r_block(STORE, 12))) == (
        10, 10, 10000, 10000, 10, 10)
    h.call('zune_imspec_cleanup', spec)
    assert not h.allocations
    h.machine.cleanup()


def check_cycle_padding(path):
    h = Harness(path)
    top, bottom, frame = 0x80421eb6, 0x8042f2c0, 0x8042ac64
    received = []

    def superclass():
        # Capture the constructor's real merged tag list at Group's boundary.
        # Stop here: child creation and Group layout are separate GUI checks.
        msg = h.mem.r32(h.cpu.r_sp() + 12)
        assert h.mem.r32(msg) == 0x101  # OM_NEW
        tags = h.mem.r32(msg + 4)
        values = []
        while tags:
            tag, value = h.mem.r32(tags), h.mem.r32(tags + 4)
            if tag == 0:
                break
            if tag == 2:
                tags = value
                continue
            values.append((tag, value))
            tags += 8
        received.append(values)
        h.cpu.w_reg(0, 0)

    h.trap(EXEC - 624, lambda: h.mem.w_block(h.cpu.r_reg(9),
        h.mem.r_block(h.cpu.r_reg(8), h.cpu.r_reg(0))))
    h.trap(h.symbols['_MUI_NewObjectA'], lambda: h.cpu.w_reg(0, 0))
    h.trap(h.symbols['_DoSuperMethodA'], superclass)
    for padding in ((), ((top, 5), (bottom, 3))):
        words = [value for pair in padding for value in pair] + [0, 0]
        h.mem.w_block(PAYLOAD, struct.pack('>' + 'I' * len(words), *words))
        h.mem.w_block(MESSAGE, struct.pack('>3I', 0x101, PAYLOAD, 0))
        assert h.call('Cycle__OM_NEW', CLASS, 0, MESSAGE) == 0
        tags = received[-1]
        assert (frame, 1) in tags  # Cycle still uses the configured Button frame.
        assert [(key, value) for key, value in tags
                if key in (top, bottom)] == list(padding)
    assert len(received) == 2 and not h.allocations
    h.machine.cleanup()


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

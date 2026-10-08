#!/usr/bin/env python3
"""Exercise the linked TheBar preference parsers on a modelled 68000."""
import sys
from pathlib import Path
from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from amitools.vamos.machine.machine import Machine

BASE, STACK = 0x10000, 0xf0000
CLASS, OBJECT, MESSAGE, SPEC = 0x70000, 0x71000, 0x72000, 0x73000
SET_SPEC = 0xf76b01ca


def check(build):
    image = BinFmt().load_image(str(build / 'Libs/MUI/TheBar.mcp'))
    reloc = Relocate(image)
    symbols = {s.name.decode(): address + s.offset
               for seg, address in zip(image.get_segments(), reloc.get_seq_addrs(BASE))
               if seg.symtab for s in seg.symtab.symbols}
    machine = Machine()
    mem, cpu = machine.mem, machine.cpu
    events = []

    def setter(opcode, pc):
        events.append(pc)
        cpu.w_reg(0, 1)

    try:
        mem.w_block(BASE, bytes(reloc.relocate_one_block(BASE)))
        intuition = 0x75000
        mem.w32(symbols['_IntuitionBase'], intuition)
        for address in [intuition - 648, symbols['_SetSuperAttrs']]:
            trap = machine.traps.alloc(setter)
            mem.w16(address, 0xa000 | trap)
            mem.w16(address + 2, 0x4e75)
        utility = 0x76000
        for name in ['_UtilityBase', '___UtilityBase']:
            mem.w32(symbols[name], utility)
        def divide(opcode, pc):
            left, right = cpu.r_reg(0), cpu.r_reg(1)
            cpu.w_reg(0, left // right)
            cpu.w_reg(1, left % right)
        trap = machine.traps.alloc(divide)
        mem.w16(utility - 156, 0xa000 | trap)
        mem.w16(utility - 154, 0x4e75)
        def multiply(opcode, pc):
            cpu.w_reg(0, (cpu.r_reg(0) * cpu.r_reg(1)) & 0xffffffff)
        trap = machine.traps.alloc(multiply)
        mem.w16(utility - 138, 0xa000 | trap)
        mem.w16(utility - 136, 0x4e75)
        mem.w16(CLASS + 32, 256)
        mem.w32(MESSAGE, SET_SPEC)
        mem.w32(MESSAGE + 8, 0)
        mem.w32(MESSAGE + 12, 1)  # Interpret pen settings as an image spec.
        for dispatcher, good, bad, writes in [
            ('_patternsDispatcher', ['0:128', '0:145'],
             ['0x128', '0 128', '0', '', '0:', '0:127', '0:146'], 1),
            ('_bitmapDispatcher', ['5:example'],
             ['5xexample', '5 example', '5', '', '5:'], 2),
            ('_penadjustDispatcher', ['2:m3', '2:p127'],
             ['2xm3', '2 m3', '2', '', '2:', '2:m8', '2:p128'], 2),
        ]:
            for text in [*good, *bad, None]:
                events.clear()
                mem.w_block(SPEC, b'!' * 64)
                if text is not None:
                    mem.w_block(SPEC, text.encode() + b'\0')
                mem.w32(MESSAGE + 4, SPEC if text is not None else 0)
                cpu.w_reg(8, CLASS)
                cpu.w_reg(9, MESSAGE)
                cpu.w_reg(10, OBJECT)
                machine.prepare(symbols[dispatcher], STACK)
                result = machine.execute(100000)
                assert machine.was_exit(result), (dispatcher, text, result)
                assert cpu.r_sp() == STACK
                expected = int(text in good)
                assert cpu.r_reg(0) == expected, (dispatcher, text, cpu.r_reg(0))
                assert len(events) == (writes if expected else 0), (dispatcher, text, events)
                if text is not None:
                    assert bytes(mem.r_block(SPEC, len(text) + 1)) == text.encode() + b'\0'
        root = Path(__file__).resolve().parents[1]
        # TheBar carries an old NBitmap header for its own build; it must not
        # replace the SDK header belonging to the NBitmap implementation.
        assert (build / 'SDK/include/mui/NBitmap_mcc.h').read_bytes() == (
            root / 'workbench/classes/zune/nlist/include/mui/NBitmap_mcc.h').read_bytes()
        for name in ['TheBar_mcc.h', 'TheBar_mcp.h', 'Toolbar_mcc.h']:
            assert (build / 'SDK/include/mui' / name).read_bytes() == (
                root / 'workbench/classes/zune/thebar/include/mui' / name).read_bytes()
        assert (build / 'Docs/thebar/COPYING').is_file()
    finally:
        machine.cleanup()
    print('TheBar: native preference delimiter/range checks and SDK ownership passed')


if __name__ == '__main__':
    check(Path(sys.argv[1]))

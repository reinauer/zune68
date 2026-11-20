#!/usr/bin/env python3
"""Fuzz the linked parsers that read outside input on a 68000.

Pen, frame and gradient specs and text engine strings (every escape but
\\33O[...], whose argument is a pointer by MUI's definition) are placed at
the very end of emulated RAM, so a read past the terminator leaves RAM and
stops the machine. Each call must return, touch only the memory it is given
(input, output, stack and its own allocations, traced access by access) and
leave no allocation behind. Inputs mix random bytes with mutated valid specs.

FUZZ_CASES (default 400 per parser) and FUZZ_SEED (default 1) select the
inputs. A failure prints the seed, the case number and the input; case n of
a parser is the same for any FUZZ_CASES above n.
"""
import os
import random
import sys
from pathlib import Path

from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from check_class_lifetime import BASE, MASTER, STACK
from check_textengine import TextHarness

CASES = int(os.environ.get('FUZZ_CASES', '400'))
SEED = int(os.environ.get('FUZZ_SEED', '1'))
FAILURE_LIMIT = 3
OUTPUT, PREPARSE = 0x70000, 0x7e000
DOS, UTILITY = 0x76000, 0x78000
# Mocked library bases and jump tables, readable by any call.
OS_TABLES = [(0x60000, OUTPUT - 0x60000), (DOS - 0x400, UTILITY - DOS + 0x400)]
STACK_LOW = STACK - 0x4000
# sizeof() of the m68k structures the parsers fill.
PEN_INTERN, FRAME_INTERN, IMSPEC_INTERN = 26, 12, 50
FRAME_TYPES = 28  # FST_COUNT
ZTEXT_ARG_NONE, ZTEXT_ARG_HICHAR, ZTEXT_ARG_HICHARIDX = 0, 1, 2

HEX = b'0123456789abcdefABCDEF'


class FuzzHarness(TextHarness):
    def __init__(self, path):
        super().__init__(path)
        self.ram_end = self.machine.ram_total
        self.code_end = BASE + Relocate(
            BinFmt().load_image(str(path))).get_total_size()
        self.exit_trap = self.machine.get_run_exit_addr()
        # libnix strtoul reports overflow in the library's errno.
        self.errno = self.symbols['___errno__data']
        self.readable = self.writable = ()
        self.violations = []
        self.mem.w32(self.symbols['_DOSBase'], DOS)
        self.mem.w32(self.symbols['_UtilityBase'], UTILITY)
        # A call to a function not mocked here hits ILLEGAL and stops.
        for base, size in ((DOS, 0x400), (UTILITY, 0x200)):
            self.mem.w_block(base - size, b'\x4a\xfc' * (size // 2))
        self.trap(DOS - 816, self.str_to_long)
        # SMult32 and UDivMod32 serve libgcc; ToUpper and ToLower HiChar.
        self.trap(UTILITY - 138, lambda: self.cpu.w_reg(0, (
            self.cpu.r_reg(0) * self.cpu.r_reg(1)) & 0xffffffff))
        self.trap(UTILITY - 156, self.udivmod)
        self.trap(UTILITY - 174, lambda: self.cpu.w_reg(
            0, bytes([self.cpu.r_reg(0) & 255]).upper()[0]))
        self.trap(UTILITY - 180, lambda: self.cpu.w_reg(
            0, bytes([self.cpu.r_reg(0) & 255]).lower()[0]))
        self.machine.set_cpu_mem_trace_hook(self.trace)

    def udivmod(self):
        q, r = divmod(self.cpu.r_reg(0), self.cpu.r_reg(1) or 1)
        self.cpu.w_reg(0, q)
        self.cpu.w_reg(1, r)

    def allowed(self, write, address, end):
        """May the call under test access [address, end)?"""
        if not write:
            if BASE <= address and end <= self.code_end:
                return True
            if address == self.exit_trap or any(
                    start <= address and end <= start + size
                    for start, size in OS_TABLES):
                return True
        if STACK_LOW <= address and end <= STACK + 16:
            return True
        if write and address == self.errno:
            return True
        for start, size in self.writable if write else self.readable:
            if start <= address and end <= start + size:
                return True
        return any(start <= address and end <= start + size
                   for start, size in self.allocations.items())

    def check(self, write, address, size):
        if not self.allowed(write, address, address + size):
            self.violations.append(('W' if write else 'R', size, address,
                                    self.cpu.r_pc()))
            return False
        return True

    def trace(self, mode, width, address, value):
        self.check(mode == 'W', address, 1 << width)

    def guest_byte(self, address):
        """Read a byte for a mocked OS call, as the trace checks the CPU."""
        return self.mem.r8(address) if self.check(False, address, 1) else 0

    def str_to_long(self):
        """dos StrToLong(string d1, value d2): characters used, -1 if none."""
        s, out = self.cpu.r_reg(1), self.cpu.r_reg(2)
        i = 0
        while self.guest_byte(s + i) in b' \t':
            i += 1
        negative = self.guest_byte(s + i) == ord('-')
        if self.guest_byte(s + i) in b'+-':
            i += 1
        start, value = i, 0
        while 48 <= self.guest_byte(s + i) <= 57:
            value = (value * 10 + self.guest_byte(s + i) - 48) & 0xffffffff
            i += 1
        if i == start:
            self.cpu.w_reg(0, 0xffffffff)
            return
        if self.check(True, out, 4):
            self.mem.w32(out, -value & 0xffffffff if negative else value)
        self.cpu.w_reg(0, i)

    def copy_mem(self):
        source, dest, size = (self.cpu.r_reg(8), self.cpu.r_reg(9),
                              self.cpu.r_reg(0))
        if self.check(False, source, size) and self.check(True, dest, size):
            super().copy_mem()

    def place(self, data):
        """Put data and its terminator at the end of RAM."""
        data += b'\0'
        address = self.ram_end - len(data)
        self.mem.w_block(address, data)
        return address

    def run(self, name, args, reads=(), writes=(), budget=200000):
        """Return (d0, problem); problem is None if the call behaved."""
        self.readable = list(reads) + list(writes)
        self.writable = list(writes)
        self.violations = []
        for i, value in enumerate(args):
            self.mem.w32(STACK + 4 * i, value)
        self.cpu.w_reg(14, MASTER)
        self.machine.prepare(self.symbols['_' + name], STACK)
        try:
            result = self.machine.execute(budget)
        except Exception as error:  # A read outside RAM raises here.
            return None, f'{name} did not return: {error}'
        if not self.machine.was_exit(result):
            return None, f'{name} did not return within {budget} cycles'
        if self.cpu.r_sp() != STACK or self.lock_depth:
            return None, f'{name} returned with a wrong stack or semaphore'
        if self.violations:
            mode, size, address, pc = self.violations[0]
            return None, (f'{name}: {len(self.violations)} stray accesses, '
                          f'first {mode}({size}) at ${address:06x} '
                          f'from pc ${pc:06x}')
        return self.cpu.r_reg(0), None


def mutate(rnd, data, alphabet):
    data = bytearray(data)
    for _ in range(rnd.randint(1, 3)):
        position = rnd.randint(0, len(data))
        byte = rnd.choice(alphabet) if rnd.random() < 0.8 else rnd.randrange(1, 256)
        operation = rnd.randrange(5)
        if operation == 0:
            del data[position:position + rnd.randint(1, 3)]
        elif operation == 1:
            data.insert(position, byte)
        elif operation == 2 and position < len(data):
            data[position] = byte
        elif operation == 3:
            del data[position:]
        else:
            start = rnd.randint(0, len(data))
            data[position:position] = data[start:start + rnd.randint(1, 8)]
    return bytes(data)


def fuzz_input(rnd, valid, alphabet, longest):
    """A valid spec, a mutated one or random bytes, never containing NUL."""
    choice = rnd.random()
    if choice < 0.15:
        return valid(rnd)
    if choice < 0.65:
        return mutate(rnd, valid(rnd), alphabet)
    length = rnd.randint(0, longest)
    if choice < 0.9:
        return bytes(rnd.choice(alphabet) for _ in range(length))
    return bytes(rnd.randrange(1, 256) for _ in range(length))


def hex_digits(rnd, most=8):
    text = '%x' % rnd.getrandbits(4 * rnd.randint(1, most))
    return ''.join(c.upper() if rnd.random() < 0.3 else c for c in text)


def valid_pen(rnd):
    kind = rnd.randrange(6)
    if kind == 0:
        return b'r%06x' % rnd.getrandbits(24)
    if kind == 1:
        return b'%06X' % rnd.getrandbits(24)
    if kind == 2:
        return ('r' * rnd.randint(0, 1) + ','.join(
            hex_digits(rnd) for _ in range(3))).encode()
    return b'%c%d' % (b'mps'[kind - 3], rnd.randint(-3, 300))


def valid_frame(rnd):
    digits = '0123456789abcdefghijklmnopqr'
    spec = digits[rnd.randrange(FRAME_TYPES)] + '01'[rnd.randint(0, 1)]
    spec += ''.join(rnd.choice('0123456789abcdef')
                    for _ in range(rnd.choice((4, 4, 5, 6))))
    return spec.encode()


def valid_gradient(rnd):
    angle = rnd.choice(['H', 'h', 'V', 'v', str(rnd.randint(0, 359))])
    start = ','.join(hex_digits(rnd) for _ in range(3))
    end = ','.join(hex_digits(rnd) for _ in range(3))
    return f'{angle},{start}-{end}'.encode()


SIMPLE = [
    # label, function, valid spec, mutation alphabet, longest random input,
    # output size
    ('pen spec', 'zune_pen_spec_to_intern', valid_pen,
     HEX + b'rmps,;+- \t', 40, PEN_INTERN),
    ('frame spec', 'zune_frame_spec_to_intern', valid_frame,
     HEX + b'ghqrsz \t', 12, FRAME_INTERN),
    ('gradient spec', 'zune_gradient_string_to_intern', valid_gradient,
     HEX + b'HhVv,-+xX \t', 60, IMSPEC_INTERN),
]


def fuzz_simple(path, label, function, valid, alphabet, longest, size):
    h = FuzzHarness(path)
    rnd = random.Random(f'{SEED}/{label}')
    accepted, failures = 0, []
    for case in range(CASES):
        data = fuzz_input(rnd, valid, alphabet, longest)
        address = h.place(data)
        h.mem.w_block(OUTPUT, bytes(size))
        result, problem = h.run(function, (address, OUTPUT),
                                reads=[(address, len(data) + 1)],
                                writes=[(OUTPUT, size)])
        if not problem and h.allocations:
            problem = f'{function} left {len(h.allocations)} allocations'
        if problem:
            failures.append(f'{label} case {case} (FUZZ_SEED={SEED}): '
                            f'{problem}\n  input {data!r}')
            h.machine.cleanup()
            if len(failures) == FAILURE_LIMIT:
                return failures, accepted
            h = FuzzHarness(path)
            continue
        accepted += bool(result & 0xffff)
    h.machine.cleanup()
    # Valid specs must pass, or the fuzzer never gets past the first byte.
    if not failures and not 0 < accepted < CASES:
        failures.append(f'{label}: {accepted} of {CASES} inputs accepted')
    return failures, accepted


# Escapes without an argument; digits select a DrawInfo pen.
PLAIN_ESCAPES = b'crlnUubitTsSgGBFVvhH-0123456789'


def text_token(rnd):
    choice = rnd.random()
    if choice < 0.35:
        return b'\33' + bytes([rnd.choice(PLAIN_ESCAPES)])
    if choice < 0.45:
        spec = rnd.choice([b'6:%d' % rnd.randint(0, 40), b'2:r%06x' %
                           rnd.getrandbits(24), b'1:%d' % rnd.randint(0, 9)])
        if rnd.random() < 0.3:
            spec += b',STATE=%d' % rnd.randint(0, 1)
        return mutate(rnd, b'\33I[' + spec + b']', b'[],:=0123456789STAE')
    if choice < 0.55:
        argument = rnd.choice([b'%06x' % rnd.getrandbits(24),
                               b'%08x' % rnd.getrandbits(32),
                               b'%02x------' % rnd.getrandbits(8),
                               b'%d' % rnd.randint(-5, 300)])
        token = b'\33' + rnd.choice(b'pP').to_bytes(1, 'big') + b'[' + argument + b']'
        return mutate(rnd, token, HEX + b'[]-') if rnd.random() < 0.3 else token
    if choice < 0.58:
        return b'\33A[' + rnd.choice([b'0', b'$0', hex_digits(rnd).encode()]) + b']'
    if choice < 0.63:
        return b'\33' + bytes([rnd.randrange(1, 256)])
    if choice < 0.73:
        return rnd.choice([b'\n', b'\t', b'\33', b'[', b']', b'_', b','])
    return bytes(rnd.choice(b'abcxyz _019\xe9') for _ in range(rnd.randint(1, 8)))


def text_input(rnd):
    """Return preparse, content, argtype and argument byte."""
    tokens = [text_token(rnd) for _ in range(rnd.randint(0, 14))]
    if rnd.random() < 0.1:  # Long enough for the heap copy and node overflow.
        tokens *= rnd.randint(2, 12)
    content = b''.join(tokens)
    if rnd.random() < 0.15:
        content = bytes(rnd.randrange(1, 256) for _ in range(rnd.randint(0, 80)))
    preparse = b''.join(b'\33' + bytes([rnd.choice(PLAIN_ESCAPES[:-11])])
                        for _ in range(rnd.choice((0, 0, 1, 2))))
    content = content.replace(b'\33O', b'\33o')
    argtype = rnd.choice((ZTEXT_ARG_NONE,) * 3 +
                         (ZTEXT_ARG_HICHAR, ZTEXT_ARG_HICHARIDX))
    argument = rnd.choice(content or b'_') if rnd.random() < 0.7 else ord('_')
    return preparse, content, argtype, argument


def text_problem(h, text, source):
    """Check the lines and chunks: text chunks hold only parsed input."""
    total, nodes = 0, 0
    line = h.mem.r32(text + 4)  # ZText.lines
    while h.mem.r32(line):
        chunk = h.mem.r32(line + 10)  # ZTextLine.chunklist
        while h.mem.r32(chunk):
            nodes += 1
            if nodes > 4 * len(source) + 8:
                return 'zune_text_new built a cyclic or oversized list'
            string, length = h.mem.r32(chunk + 8), h.mem.r16(chunk + 42)
            if string:
                data = bytes(h.mem.r_block(string, length + 1))
                if b'\0' in data[:-1] or data[-1]:
                    return f'chunk {data!r} holds a terminator'
                total += length
            chunk = h.mem.r32(chunk)
        line = h.mem.r32(line)
    if total > len(source) + 4 * source.count(b'\t'):
        return f'chunks hold {total} bytes of {len(source)} bytes of input'
    return None


def fuzz_text(path):
    h = FuzzHarness(path)
    rnd = random.Random(f'{SEED}/text')
    failures = []
    for case in range(CASES):
        preparse, content, argtype, argument = text_input(rnd)
        assert b'\33O' not in preparse + content
        h.next_address = 0x80000  # Allocations are gone after each case.
        h.mem.w_block(PREPARSE, preparse + b'\0')
        address = h.place(content)
        reads = [(PREPARSE, len(preparse) + 1), (address, len(content) + 1)]
        text, problem = h.run('zune_text_new',
                              (PREPARSE, address, argtype, argument),
                              reads=reads, budget=4000000)
        if not problem and text:
            problem = text_problem(h, text, preparse + content)
            if not problem:
                _, problem = h.run('zune_text_destroy', (text,), reads=reads,
                                   budget=4000000)
        if not problem and h.allocations:
            problem = f'zune_text_destroy left {len(h.allocations)} allocations'
        if problem:
            failures.append(f'text engine case {case} (FUZZ_SEED={SEED}): '
                            f'{problem}\n  preparse {preparse!r} content '
                            f'{content!r} argtype {argtype} arg {argument:#x}')
            h.machine.cleanup()
            if len(failures) == FAILURE_LIMIT:
                return failures
            h = FuzzHarness(path)
    h.machine.cleanup()
    return failures


def check(path):
    failures, summary = [], []
    for label, function, valid, alphabet, longest, size in SIMPLE:
        found, accepted = fuzz_simple(path, label, function, valid, alphabet,
                                      longest, size)
        failures += found
        summary.append(f'{label} {accepted}/{CASES} accepted')
    failures += fuzz_text(path)
    for failure in failures:
        print(failure)
    assert not failures, f'{path.name}: {len(failures)} parser failures'
    print(f'{path.name}: {4 * CASES} fuzzed parser inputs passed '
          f'(seed {SEED}; {", ".join(summary)}; text engine)')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))

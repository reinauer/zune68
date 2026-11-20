#!/usr/bin/env python3
"""Fuzz the MUI 3.8 settings importer (workbench/prefs/Zune/mui38prefs.c) on the host.

The converter is built with the address and undefined-behaviour sanitizers
where the host compiler has them (plainly otherwise) and given mutations of
a valid MUI 3.8 file: changed bytes, truncations, chunk and item lengths set
to huge, odd, zero or negative values, strings without their terminator,
added and shuffled chunks, and random data after a valid start. Each input
is converted from a buffer of exactly its size; the converter may reject a
file but must stay inside its buffers, and what it writes must be a well
formed settings file. FUZZ_CASES (default 3000) and FUZZ_SEED (default 1)
in the environment.
"""
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_mui38_import import build, chunk, form, sample  # noqa: E402

CASES = int(os.environ.get('FUZZ_CASES', '3000'))
SEED = int(os.environ.get('FUZZ_SEED', '1'))


def mutate(rnd, src):
    data = bytearray(src)
    kind = rnd.randrange(9)
    if len(data) < 24 and kind not in (1, 5):
        kind = 5
    if kind == 0:                         # changed bytes
        for _ in range(rnd.randint(1, 8)):
            data[rnd.randrange(len(data))] = rnd.randrange(256)
    elif kind == 1:                       # truncated
        data = data[:rnd.randrange(len(data) + 1)]
    elif kind == 2:                       # a length (chunk or item) rewritten
        for _ in range(rnd.randint(1, 3)):
            i = rnd.randrange(4, len(data) - 3)
            v = rnd.choice([0, 1, 2, 3, 0x7fffffff, 0x80000000, 0xffffffff, 0xfffffffe,
                            len(data), len(data) * 2, rnd.randrange(0x10000)])
            data[i:i + 4] = struct.pack('>I', v)
    elif kind == 3:                       # a string item without its terminator
        i = data.find(b'TESTSCR\0')
        if i >= 0:
            data[i + 7] = ord('X')
        i = data.find(b'.image\0')
        if i >= 0:
            data[i + 6] = ord('Y')
    elif kind == 4:                       # chunks added or moved
        extra = rnd.choice([
            chunk(b'MUIC', bytes(rnd.randrange(256) for _ in range(rnd.randrange(40)))),
            chunk(b'MUIW', bytes(rnd.randrange(256) for _ in range(rnd.randrange(90)))),
            chunk(b'PRHD', b''), chunk(b'JUNK', b'x' * rnd.randrange(50))])
        i = rnd.randrange(12, len(data))
        data[i:i] = extra
        data[4:8] = struct.pack('>I', len(data) - 8)
    elif kind == 5:                       # random bytes after a valid start
        n = rnd.randrange(0, 400)
        data = bytearray(b'FORM' + struct.pack('>I', n + 4) + b'PREF'
                         + bytes(rnd.randrange(256) for _ in range(n)))
    elif kind == 6:                       # an item's length odd, tiny or huge
        i = data.find(b'MUIC')
        if i >= 0 and i + 40 < len(data):
            j = i + 8 + 12 * rnd.randrange(0, 3)
            data[j + 4:j + 8] = struct.pack('>I', rnd.choice([0, 1, 3, 5, 7, 255, 4000,
                                                              0x7ffffff0]))
    elif kind == 7:                       # a FORM length that lies
        data[4:8] = struct.pack('>I', rnd.choice([0, 4, 8, 0xffffffff, 0x7fffffff,
                                                  len(data) + 100]))
    else:                                 # several of the above
        for _ in range(rnd.randint(2, 4)):
            data = bytearray(mutate(rnd, bytes(data)))
    return bytes(data)


def main():
    rnd = random.Random(SEED)
    tmp = tempfile.mkdtemp(prefix='mui38fuzz')
    try:
        how = 'under the sanitizers'
        exe = build(tmp, ['-O1', '-g', '-fsanitize=address,undefined',
                          '-fno-sanitize-recover=all'])
        if not exe:
            how = 'without sanitizers'
            exe = build(tmp, ['-O1'])
        if not exe:
            print('check_mui38_fuzz: no host C compiler, skipped')
            return
        src = sample()
        cases = [src, form(), form(chunk(b'MUIC', b''))]
        cases += [mutate(rnd, src) for _ in range(CASES)]
        batch = Path(tmp) / 'cases'
        batch.write_bytes(b''.join(struct.pack('>I', len(c)) + c for c in cases))
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                   UBSAN_OPTIONS='print_stacktrace=1')
        for args in ([], ['nofiles']):
            r = subprocess.run([str(exe), 'batch', str(batch), *args], capture_output=True,
                               text=True, env=env, timeout=300)
            if r.returncode:
                keep = Path(tempfile.mkdtemp(prefix='mui38fuzz-fail'))
                shutil.copy(batch, keep / 'cases')
                raise SystemExit(f'check_mui38_fuzz: seed {SEED} {args}: exit {r.returncode}, '
                                 f'cases kept in {keep}\n{r.stdout}{r.stderr[-3000:]}')
            total, ok = map(int, r.stdout.split())
            assert total == len(cases), r.stdout
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'check_mui38_fuzz: {len(cases)} mutated MUI 3.8 settings files converted '
          f'{how} (seed {SEED}, {ok} accepted)')


if __name__ == '__main__':
    main()

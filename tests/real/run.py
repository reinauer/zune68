#!/usr/bin/env python3
"""Start real MUI programs on a WinUAE fixture and record how they fare.

  run.py --fixture FIX --target zune|mui38 [--group mui3] [--first N]
         [--ids ID,...] [--batch 15] [--out DIR]

The programs come from programs.tsv, unpacked by fetch.py into the cache
(~/.cache/zune68-real). Each program's directory is copied to
Work:real/p/<id>, the classes and libraries of support.tsv to
Work:real/libs (last on LIBS:, so classes the system or Zune68 brings
are used first). On the zune target Zune68's build (--lib) is staged as
in tests/native/run_probes.py; on mui38 the fixture's MUI 3.8 is used.

The guest helper wintool (wintool.c) runs a batch of programs in one
boot: per program the free memory, the time until its first window, the
windows, a snapshot of the screen (<id>.png), the quit (Ctrl-C, then
close-window messages) and the free memory afterwards, plus libraries
and classes that failed to open ("missing"). A guest that stops making
progress (crash, hang) or resets is stopped, the program is recorded as
such, and the rest go on in a fresh boot; so does a batch after a program
that does not quit.

Results: DIR/results.tsv (default build/real/<target>), DIR/<id>.png,
DIR/<id>.con (the program's output), DIR/<id>.log (wintool's lines).
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'tests', 'native'))
sys.path.insert(0, HERE)
from uaefixture import Fixture, WINUAE, stage_zune  # noqa: E402
from fetch import CACHE, programs, support  # noqa: E402

CC = 'm68k-amigaos-gcc'


def build_wintool(out):
    src = os.path.join(HERE, 'wintool.c')
    if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(src):
        os.makedirs(os.path.dirname(out), exist_ok=True)
        subprocess.run([CC, '-noixemul', '-Os', '-s', src, '-o', out, '-lamiga'], check=True)


def copy_support(cache, dest):
    """Copy the support classes to dest/MUI and libraries to dest."""
    shutil.rmtree(dest, ignore_errors=True)
    os.makedirs(os.path.join(dest, 'MUI'))
    absent = []
    for r in support():
        src = os.path.join(cache, r['key'], 'x', r['path'])
        if not os.path.isfile(src):
            absent.append(r['name'])
            continue
        to = os.path.join(dest, 'MUI') if r['kind'] == 'class' else dest
        shutil.copy(src, os.path.join(to, r['name']))
    return absent


def snap_png(snap, png):
    """Convert wintool's planar snapshot to a palette PNG."""
    data = open(snap, 'rb').read()
    nl = data.index(b'\n')
    _, w, h, depth = data[:nl].split()
    w, h, depth = int(w), int(h), int(depth)
    ncol, rb = 1 << depth, (w + 7) // 8
    pal = data[nl + 1:nl + 1 + ncol * 3]
    planes = data[nl + 1 + ncol * 3:]
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        row = [planes[(y * depth + d) * rb:(y * depth + d + 1) * rb] for d in range(depth)]
        for x in range(w):
            byte, bit = x >> 3, 7 - (x & 7)
            v = 0
            for d in range(depth):
                v |= ((row[d][byte] >> bit) & 1) << d
            rows.append(v)

    def chunk(kind, body):
        c = kind + body
        return struct.pack('>I', len(body)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    with open(png, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0)))
        f.write(chunk(b'PLTE', pal))
        f.write(chunk(b'IDAT', zlib.compress(bytes(rows), 9)))
        f.write(chunk(b'IEND', b''))


def command(r):
    """AmigaDOS command line for a program row: quoted path, then arguments."""
    exe, _, args = r['exe'].partition(' ')
    return ('"Work:real/p/%s/%s" %s' % (r['id'], exe, args)).rstrip()


def parse(lines):
    """Turn wintool's lines for one program into a result dict."""
    res = {'started': 'no', 'time': '', 'mem': '', 'memopen': '', 'memafter': '',
           'windows': [], 'quit': '', 'missing': [], 'status': ''}
    for line in lines:
        w = line.split(' ', 1)
        k, v = w[0], (w[1] if len(w) > 1 else '')
        if k in ('mem', 'memopen', 'memafter'):
            res[k] = int(v)
        elif k == 'window':
            res['started'], res['time'] = 'yes', '%.1f' % (int(v) / 1000)
        elif k == 'win':
            res['windows'].append(v)
        elif k == 'quit':
            res['quit'] = v
        elif k in ('exited', 'nowindow', 'nostart', 'nodir'):
            res['status'] = k
            res['time'] = '%.1f' % (int(v) / 1000) if v.isdigit() else ''
        elif k == 'missing':
            res['missing'].append(v)
        elif k == 'stuck':
            res['status'] = 'stuck'
        elif k == 'closed':
            res['windows'].append('(closed again by itself)')
        elif k == 'left':
            res['windows'].append('left: ' + v)
    if not res['status']:
        res['status'] = 'ok' if res['quit'] else 'incomplete'
    return res


def run_batch(fix, rows, assigns, outdir, stall_extra):
    """One boot for the programs in rows, as far as it gets.
    Returns ({id: (lines, how)}, ids not reached)."""
    real = os.path.join(fix.work, 'real')
    out = os.path.join(real, 'out')
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    with open(os.path.join(real, 'batch.txt'), 'w', encoding='latin-1') as f:
        for r in rows:
            f.write('%s\t%s\tWork:real/p/%s\t%s\n' % (r['id'], r['wait'], r['id'], command(r)))
    stage = ['FailAt 21',
             'If EXISTS Work:real/booted',
             '  Echo reset >>Work:real/out/progress.txt', '  Quit', 'EndIf',
             'Echo >Work:real/booted'] + assigns + [
             'Assign LIBS: Work:real/libs ADD',
             'Work:real/wintool batch Work:real/batch.txt Work:real/out',
             'Echo batchend >>Work:real/out/progress.txt']
    booted = os.path.join(real, 'booted')
    if os.path.exists(booted):
        os.remove(booted)
    waits = {r['id']: int(r['wait']) for r in rows}
    print('booting %s for %d programs' % (os.path.basename(fix.dir), len(rows)), flush=True)
    fix.boot(stage)
    text, since, current, why = '', time.time(), None, None
    boot_limit = 240
    while True:
        now = fix.read('real/out/progress.txt')
        if now != text:
            for line in now[len(text):].splitlines():
                if line.startswith('start '):
                    current = line.split()[1]
                    print('  ' + current, flush=True)
            text, since = now, time.time()
        lines = text.splitlines()
        if lines and lines[-1] in ('batchdone', 'batchend', 'reboot') or 'reset' in lines:
            why = 'reset' if 'reset' in lines else 'done'
            break
        limit = (waits.get(current, 60) + stall_extra) if current else boot_limit
        if time.time() - since > limit:
            why = 'stall'
            break
        if fix.proc.poll() is not None:
            why = 'emulator exited'
            break
        time.sleep(1)
    fix.stop()
    results, block, cur = {}, [], None
    for line in text.splitlines():
        if line.startswith('start '):
            cur, block = line.split()[1], []
        elif line.startswith('end ') and cur:
            results[cur] = (block, 'ok')
            cur = None
        elif cur:
            block.append(line)
            if line == 'stuck':
                results[cur] = (block, 'stuck')
                cur = None
    if cur:
        results[cur] = (block, 'reset' if why == 'reset' else 'hang' if why == 'stall' else why)
    if why == 'stall' and not results and not text:
        print('%s: the guest did not start wintool' % fix.dir, flush=True)
    for r in rows:
        for ext in ('.con', '.snap'):
            src = os.path.join(out, r['id'] + ext)
            if os.path.exists(src):
                shutil.copy(src, os.path.join(outdir, r['id'] + ext))
    left = [r['id'] for r in rows if r['id'] not in results]
    return results, left


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--fixture', required=True, help='fixture directory')
    p.add_argument('--target', required=True, choices=('zune', 'mui38'))
    p.add_argument('--lib', default=os.path.join(ROOT, 'build', 'muimaster'),
                   help='Zune68 build directory for --target zune')
    p.add_argument('--cache', default=CACHE)
    p.add_argument('--ids', help='comma-separated program ids')
    p.add_argument('--group', default='mui3', help='programs of this group (default mui3)')
    p.add_argument('--first', type=int, help='only the first N programs of the selection')
    p.add_argument('--batch', type=int, default=15, help='programs per boot')
    p.add_argument('--stall', type=int, default=90,
                   help='seconds beyond a program\'s wait without progress: a hang')
    p.add_argument('--out', help='result directory (default build/real/<target>)')
    p.add_argument('--emulator', default=WINUAE)
    a = p.parse_args()

    rows = programs()
    if a.ids:
        want = a.ids.split(',')
        rows = [r for r in rows if r['id'] in want]
    elif a.group:
        rows = [r for r in rows if r['group'] == a.group]
    if a.first:
        rows = rows[:a.first]
    outdir = a.out or os.path.join(ROOT, 'build', 'real', a.target)
    os.makedirs(outdir, exist_ok=True)
    fix = Fixture(a.fixture, a.emulator)
    real = os.path.join(fix.work, 'real')
    shutil.rmtree(os.path.join(real, 'p'), ignore_errors=True)
    os.makedirs(os.path.join(real, 'p'))
    build_wintool(os.path.join(real, 'wintool'))
    absent = copy_support(a.cache, os.path.join(real, 'libs'))
    if absent:
        print('support files not in the cache: %s' % ' '.join(absent))
    assigns = stage_zune(fix, a.lib) if a.target == 'zune' else []

    results, todo = {}, []
    for r in rows:
        src = os.path.join(a.cache, r['key'], 'x', r['dir'])
        if not os.path.isdir(src):
            results[r['id']] = ([], 'not fetched')
            continue
        shutil.copytree(src, os.path.join(real, 'p', r['id']), symlinks=True)
        todo.append(r)
    fails = 0
    while todo:
        batch = todo[:a.batch]
        got, left = run_batch(fix, batch, assigns, outdir, a.stall)
        results.update(got)
        done = [r for r in batch if r['id'] in got]
        if not done:
            # a boot that ran nothing: try again, then give up but keep the
            # results so far
            fails += 1
            if fails == 3:
                print('no progress in three boots; stopping', flush=True)
                break
            continue
        fails = 0
        todo = [r for r in todo if r['id'] not in got]

    with open(os.path.join(outdir, 'results.tsv'), 'w', encoding='utf-8') as f:
        f.write('# id\tstarted\tseconds\tfree before\tfree with windows\tfree after\t'
                'delta\tquit\tstatus\twindows\tmissing\n')
        print('%-16s %-7s %6s %9s %-6s %-11s %s' % ('id', 'started', 'secs', 'delta', 'quit',
                                                  'status', 'windows / missing'))
        for r in rows:
            lines, how = results.get(r['id'], ([], 'not run'))
            with open(os.path.join(outdir, r['id'] + '.log'), 'w', encoding='latin-1') as lf:
                lf.write('\n'.join(lines) + '\n')
            res = parse(lines)
            if how not in ('ok', 'stuck'):
                res['status'] = how
            snap = os.path.join(outdir, r['id'] + '.snap')
            if os.path.exists(snap):
                snap_png(snap, os.path.join(outdir, r['id'] + '.png'))
                os.remove(snap)
            delta = (res['memafter'] - res['mem']) if res['mem'] != '' and res['memafter'] != '' else ''
            f.write('\t'.join(str(x) for x in (
                r['id'], res['started'], res['time'], res['mem'], res['memopen'],
                res['memafter'], delta, res['quit'], res['status'],
                ' | '.join(res['windows']), ' '.join(res['missing']))) + '\n')
            print('%-16s %-7s %6s %9s %-6s %-11s %s' % (
                r['id'], res['started'], res['time'], delta, res['quit'], res['status'],
                ' | '.join(res['windows'][:2]) +
                ('  missing: ' + ', '.join(res['missing']) if res['missing'] else '')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

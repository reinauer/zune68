#!/usr/bin/env python3
"""Start real MUI programs on a WinUAE fixture and record how they fare.

  run.py --fixture FIX --target zune|mui38 [--group mui3] [--first N]
         [--ids ID,...] [--repeat N] [--batch 15] [--out DIR]

The programs come from programs.tsv, unpacked by fetch.py into the cache
(~/.cache/zune68-real). Each program's directory is copied to
Work:real/p/<id>, the classes and libraries of support.tsv to
Work:real/libs (last on LIBS:, so classes the system or Zune68 brings
are used first), its devices to Work:real/devs and fonts to
Work:real/fonts (last on DEVS: and FONTS:). An assign row's files go to
Work:real/assign, and the first part of its name is assigned there
(AmiSSL/Certs: AmiSSL: is Work:real/assign/AmiSSL, as AmiSSL's installer
assigns its drawer). Work:real/picture.iff is a
small picture for programs that want one as argument. On the zune
target Zune68's build (--lib) is staged as in tests/native/run_probes.py,
without the bundled classes that only share their name with a support
class (NAMESAKES); on mui38 the fixture's MUI 3.8 is used.

The guest helper wintool (wintool.c) runs a batch of programs in one
boot: per program the free memory, the time until its first window, the
windows, a snapshot of the screen (<id>.png), the quit (Ctrl-C,
close-window messages, answered requesters, the Quit menu item) and the
free memory afterwards, plus libraries and classes that failed to open
("missing"). --repeat N runs each program N times in a row in one boot
(ids <id>, <id>.2, ...): from the second run on its classes are loaded
already, so what it leaves behind is a leak. A guest that stops making
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

# Bundled AROS classes named like an unrelated third-party class. The
# installer keeps the third-party one (its version is higher), so the
# programs that need it get it here too.
NAMESAKES = ('Calendar.mcc',)  # AROS 1.0, Alfonso Ranieri's 18.x

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'tests', 'native'))
sys.path.insert(0, HERE)
from uaefixture import Fixture, WINUAE, stage_zune  # noqa: E402
from fetch import CACHE, programs, support  # noqa: E402

CC = 'm68k-amigaos-gcc'

# Over the fixture's run.uae: an A1200's 2 MB chip memory (in 512 KB units),
# 8 MB fast memory, a 68882 FPU, and UAE's bsdsocket.library on the host's
# network, for the programs that need a TCP/IP stack to start.
EMULATOR_OPTIONS = ('chipmem_size=4', 'fastmem_size=8', 'fpu_model=68882',
                    'bsdsocket_emu=true')


def build_wintool(out):
    src = os.path.join(HERE, 'wintool.c')
    if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(src):
        os.makedirs(os.path.dirname(out), exist_ok=True)
        subprocess.run([CC, '-noixemul', '-Os', '-s', src, '-o', out, '-lamiga'], check=True)


# where each kind of support.tsv goes below Work:real
SUPPORT_DIRS = {'class': 'libs/MUI', 'library': 'libs', 'device': 'devs', 'font': 'fonts',
                'assign': 'assign'}


def copy_support(cache, real):
    """Copy support.tsv's files to real/libs/MUI (classes), real/libs,
    real/devs, real/fonts and real/assign; names may have a directory
    part, and an assign row may name a directory."""
    for d in set(p.split('/')[0] for p in SUPPORT_DIRS.values()):
        shutil.rmtree(os.path.join(real, d), ignore_errors=True)
    absent = []
    for r in support():
        src = os.path.join(cache, r['key'], 'x', r['path'])
        to = os.path.join(real, SUPPORT_DIRS[r['kind']], r['name'])
        if r['kind'] == 'assign' and os.path.isdir(src):
            shutil.copytree(src, to)
            continue
        if not os.path.isfile(src):
            absent.append(r['name'])
            continue
        os.makedirs(os.path.dirname(to), exist_ok=True)
        shutil.copy(src, to)
    for d in SUPPORT_DIRS.values():
        os.makedirs(os.path.join(real, d), exist_ok=True)
    return absent


def support_assigns(real):
    """Stage lines for the assigns of the assign rows that were copied."""
    names = sorted(set(r['name'].split('/')[0] for r in support() if r['kind'] == 'assign'))
    return ['Assign %s: Work:real/assign/%s' % (n, n) for n in names
            if os.path.isdir(os.path.join(real, 'assign', n))]


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


def write_picture(path, w=64, h=32):
    """A small 4-colour IFF ILBM (diagonal stripes), for programs that want
    a picture to show."""
    rb = (w + 15) // 16 * 2
    body = bytearray()
    for y in range(h):
        for plane in range(2):
            row = bytearray(rb)
            for x in range(w):
                if ((x + y) // 8 % 4) >> plane & 1:
                    row[x >> 3] |= 0x80 >> (x & 7)
            body += row

    def chunk(kind, data):
        return kind + struct.pack('>I', len(data)) + data + (b'\0' if len(data) & 1 else b'')
    bmhd = struct.pack('>HHhhBBBBHBBhh', w, h, 0, 0, 2, 0, 0, 0, 0, 10, 11, w, h)
    cmap = bytes([170, 170, 170, 0, 0, 0, 255, 255, 255, 100, 136, 187])
    form = b'ILBM' + chunk(b'BMHD', bmhd) + chunk(b'CMAP', cmap) + chunk(b'BODY', bytes(body))
    with open(path, 'wb') as f:
        f.write(b'FORM' + struct.pack('>I', len(form)) + form)


def command(r):
    """AmigaDOS command line for a program row: quoted path, then arguments."""
    exe, _, args = r['exe'].partition(' ')
    return ('"Work:real/p/%s/%s" %s' % (r['pid'], exe, args)).rstrip()


def assigns_of(r):
    """(name, path below the program's directory) of a row's assigns column."""
    out = []
    for a in filter(None, r.get('assigns', '').split(',')):
        name, _, path = a.partition('=')
        out.append((name, '' if path in ('', '.') else path))
    return out


def parse(lines):
    """Turn wintool's lines for one program into a result dict."""
    res = {'started': 'no', 'time': '', 'mem': '', 'memopen': '', 'memafter': '',
           'windows': [], 'quit': '', 'missing': [], 'status': ''}
    for k in ('mem', 'memafter'):
        for part in ('chip', 'largest', 'largestchip'):
            res[k + part] = ''
    for line in lines:
        w = line.split(' ', 1)
        k, v = w[0], (w[1] if len(w) > 1 else '')
        if k in ('mem', 'memopen', 'memafter'):
            # total, chip, largest block, largest chip block
            nums = [int(x) for x in v.split()]
            res[k] = nums[0]
            if k != 'memopen':
                for part, n in zip(('chip', 'largest', 'largestchip'), nums[1:]):
                    res[k + part] = n
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
            f.write('%s\t%s\tWork:real/p/%s\t%s\n' % (r['id'], r['wait'], r['pid'], command(r)))
    stage = ['FailAt 21',
             'If EXISTS Work:real/booted',
             '  Echo reset >>Work:real/out/progress.txt', '  Quit', 'EndIf',
             'Echo >Work:real/booted'] + assigns + [
             # as OS 3.1's Startup-Sequence: datatypes, gadgets, images
             'Assign LIBS: SYS:Classes ADD',
             'Assign LIBS: Work:real/libs ADD',
             'Assign DEVS: Work:real/devs ADD',
             'Assign FONTS: Work:real/fonts ADD'] + [
             'Assign %s: "Work:real/p/%s%s"' % (name, r['pid'], '/' + path if path else '')
             for r in rows for name, path in assigns_of(r)] + [
             'Work:real/wintool batch Work:real/batch.txt Work:real/out',
             'Echo batchend >>Work:real/out/progress.txt']
    booted = os.path.join(real, 'booted')
    if os.path.exists(booted):
        os.remove(booted)
    waits = {r['id']: int(r['wait']) for r in rows}
    print('booting %s for %d programs' % (os.path.basename(fix.dir), len(rows)), flush=True)
    fix.boot(stage, EMULATOR_OPTIONS)
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
    p.add_argument('--repeat', type=int, default=1,
                   help='run each program N times in a row in one boot')
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
    for r in rows:
        r['pid'] = r['id']
    if a.repeat > 1:
        rows = [dict(r, id=r['id'] + ('.%d' % n if n > 1 else ''))
                for r in rows for n in range(1, a.repeat + 1)]
        # a program's runs stay in one boot
        a.batch = max(a.batch // a.repeat, 1) * a.repeat
    outdir = a.out or os.path.join(ROOT, 'build', 'real', a.target)
    os.makedirs(outdir, exist_ok=True)
    fix = Fixture(a.fixture, a.emulator)
    real = os.path.join(fix.work, 'real')
    shutil.rmtree(os.path.join(real, 'p'), ignore_errors=True)
    os.makedirs(os.path.join(real, 'p'))
    build_wintool(os.path.join(real, 'wintool'))
    absent = copy_support(a.cache, real)
    write_picture(os.path.join(real, 'picture.iff'))
    if absent:
        print('support files not in the cache: %s' % ' '.join(absent))
    assigns = (stage_zune(fix, a.lib) if a.target == 'zune' else []) + support_assigns(real)
    for name in NAMESAKES if a.target == 'zune' else ():
        if os.path.isfile(os.path.join(real, 'libs', 'MUI', name)):
            os.remove(os.path.join(fix.work, 'mui', 'Libs', 'MUI', name))

    results, todo = {}, []
    for r in rows:
        src = os.path.join(a.cache, r['key'], 'x', r['dir'])
        if not os.path.isdir(src):
            results[r['id']] = ([], 'not fetched')
            continue
        dest = os.path.join(real, 'p', r['pid'])
        if not os.path.isdir(dest):
            shutil.copytree(src, dest, symlinks=True)
            for _, path in assigns_of(r):
                os.makedirs(os.path.join(dest, path), exist_ok=True)
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
                'delta\tquit\tstatus\twindows\tmissing\tchip before\tchip after\t'
                'largest before\tlargest after\tlargest chip before\tlargest chip after\n')
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
                ' | '.join(res['windows']), ' '.join(res['missing']),
                res['memchip'], res['memafterchip'], res['memlargest'],
                res['memafterlargest'], res['memlargestchip'],
                res['memafterlargestchip'])) + '\n')
            print('%-16s %-7s %6s %9s %-6s %-11s %s' % (
                r['id'], res['started'], res['time'], delta, res['quit'], res['status'],
                ' | '.join(res['windows'][:2]) +
                ('  missing: ' + ', '.join(res['missing']) if res['missing'] else '')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

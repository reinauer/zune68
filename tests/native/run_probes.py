#!/usr/bin/env python3
"""Run native probes on Zune68 and compare them with MUI 3.8's output.

Each probe is built from tests/native/<name>.c and run in one boot of a
WinUAE fixture (see uaefixture.py), in a directory of its own on Work:,
so probes that write <name>.txt and probes that print work alike. The
MUI 3.8 output is the reference, kept in tests/native/ref/<name>.txt.
Where Zune68 differs on purpose, ref/zune68/<name>.txt holds its expected
output and is compared instead.

  run_probes.py --zune FIX                    compare Zune68 with ref/
  run_probes.py --mui38 FIX --update-ref      write ref/ from MUI 3.8
  run_probes.py ... --probes listprop,lvfix   only these

FIX is a fixture directory. On the Zune68 fixture Work:libs gets
build/muimaster/muimaster.library and Work:mui/Libs/MUI its classes, put
first on LIBS: and MUI:; on the MUI 3.8 fixture LIBS: and MUI: stay as the
system has them. --update-ref boots the MUI 3.8 fixture --repeat times
(default 2) and writes only the references that came out the same in
every boot. A probe that stops the guest (no progress for --timeout
seconds) is recorded as hung, and the rest run in a fresh boot.
"""
import argparse
import difflib
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from uaefixture import Fixture, WINUAE, stage_zune  # noqa: E402

# Probes whose MUI 3.8 output is the same in every boot and depends on no
# addresses, timing or MUI 4/5 features.
PROBES = [
    'appbase', 'appsignals', 'backfillrect', 'balancebounds', 'baldraw', 'balweight',
    'barfmt', 'centrestyle', 'cleanupri', 'colgroup', 'compoundwalk', 'cornerbg', 'defclamp',
    'demolayout', 'disappear', 'emptyfixprobe', 'gaugetext', 'getunknown',
    'ghostclasses', 'ghostdump', 'hdist', 'imgdef', 'imgsizes', 'imgspec', 'layoutmsg',
    'listbounds', 'listempty', 'listfmt', 'listjump', 'listkey', 'listpartial',
    'listprop', 'lvfix', 'muierror', 'newset', 'patternphase', 'radiopix', 'regpix', 'regrows',
    'rootframe', 'stringscroll', 'textstyles', 'titleframe', 'ulprobe', 'undermin',
    'virtbounds', 'virtualnotify', 'weightset', 'widgetsizes', 'winarea', 'winclamp',
    'windowpercent', 'winsize',
]

CC = 'm68k-amigaos-gcc'
WORKDIR = 'rp'      # Work:rp/<probe>/ on the guest


def build(names, incdir, outdir):
    os.makedirs(outdir, exist_ok=True)
    for name in names:
        src = os.path.join(HERE, name + '.c')
        out = os.path.join(outdir, name)
        if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(src):
            continue
        cmd = [CC, '-noixemul', '-Os', '-s', '-DMUI_OBSOLETE', '-I' + incdir, src, '-o', out, '-lamiga']
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if r.returncode:
            sys.stdout.write(r.stdout)
            raise SystemExit('build of %s failed' % name)


# lines that only tell the library version (19.x for MUI 3.8, 21.x for Zune68)
VERSION = re.compile(r'^library[ =]\d+\.\d+$')


def collect(fix, name):
    """The probe's output: what it printed, then every file it wrote,
    without lines that give the library's version."""
    d = os.path.join(fix.work, WORKDIR, name)
    text = fix.read(os.path.join(WORKDIR, name, 'stdout.txt'))
    for f in sorted(os.listdir(d)):
        if f not in (name, 'stdout.txt'):
            text += '--- %s\n' % f + fix.read(os.path.join(WORKDIR, name, f))
    return ''.join(l for l in text.splitlines(True) if not VERSION.match(l.rstrip('\n')))


def run_set(fix, names, bindir, assigns, timeout):
    """Run the probes in as few boots as possible. Returns {name: output}."""
    top = os.path.join(fix.work, WORKDIR)
    shutil.rmtree(top, ignore_errors=True)
    for name in names:
        os.makedirs(os.path.join(top, name))
        shutil.copy(os.path.join(bindir, name), os.path.join(top, name))
    results, todo = {}, list(names)
    progress = os.path.join(WORKDIR, 'progress.txt')
    while todo:
        if os.path.exists(os.path.join(fix.work, progress)):
            os.remove(os.path.join(fix.work, progress))
        # A guest that rebooted by itself runs Work:stage again: it stops at once.
        stage = ['FailAt 21',
                 'If EXISTS Work:%s/booted' % WORKDIR,
                 '  Echo reboot >>Work:' + progress, '  Quit', 'EndIf',
                 'Echo >Work:%s/booted' % WORKDIR] + assigns
        for name in todo:
            stage += ['Echo start %s >>Work:%s' % (name, progress),
                      'CD Work:%s/%s' % (WORKDIR, name),
                      '%s >stdout.txt' % name,
                      'Echo end %s $RC >>Work:%s' % (name, progress)]
        stage += ['CD Work:', 'Echo done >>Work:' + progress]
        booted = os.path.join(top, 'booted')
        if os.path.exists(booted):
            os.remove(booted)
        print('booting %s for %d probes' % (fix.dir, len(todo)), flush=True)
        fix.boot(stage)
        text, why = fix.wait(progress, lambda t: 'done' in t.split() or 'reboot' in t,
                             timeout)
        fix.stop()
        started = None
        for line in text.splitlines():
            w = line.split()
            if len(w) >= 2 and w[0] == 'start':
                started = w[1]
            elif len(w) >= 2 and w[0] == 'end':
                results[w[1]] = collect(fix, w[1]) + '[rc %s]\n' % (w[2] if len(w) > 2 else '?')
                todo.remove(w[1])
                started = None
        if why == 'done' and not todo:
            break
        if started is None:
            raise SystemExit('%s: guest did not start the probes (%s)' % (fix.dir, why))
        print('  %s %s' % (started, 'crashed' if 'reboot' in text else 'hung'), flush=True)
        results[started] = collect(fix, started) + '[hung or crashed]\n'
        todo.remove(started)
    return results


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--zune', help='Zune68 fixture directory')
    p.add_argument('--mui38', help='MUI 3.8 fixture directory')
    p.add_argument('--update-ref', action='store_true',
                   help='write ref/ from the MUI 3.8 fixture')
    p.add_argument('--repeat', type=int, default=2,
                   help='MUI 3.8 boots that must agree for --update-ref')
    p.add_argument('--probes', help='comma-separated probe names (default: %d probes)'
                   % len(PROBES))
    p.add_argument('--lib', default=os.path.join(ROOT, 'build', 'muimaster'),
                   help='Zune68 build directory (muimaster.library, Libs/MUI)')
    p.add_argument('--emulator', default=WINUAE)
    p.add_argument('--timeout', type=int, default=120,
                   help='seconds without progress before a probe counts as hung')
    p.add_argument('--refdir', default=os.path.join(HERE, 'ref'))
    p.add_argument('--out', help='also save the Zune68 output of each probe here')
    p.add_argument('--max-diff', type=int, default=40,
                   help='diff lines shown per probe (default 40)')
    a = p.parse_args()
    if a.update_ref and not a.mui38:
        p.error('--update-ref needs --mui38')
    if not a.zune and not a.update_ref:
        p.error('give --zune, or --mui38 with --update-ref')
    names = a.probes.split(',') if a.probes else PROBES
    for n in names:
        if not os.path.isfile(os.path.join(HERE, n + '.c')):
            p.error('no probe %s' % n)
    bindir = os.path.join(ROOT, 'build', 'probes')
    build(names, os.path.join(a.lib, 'include'), bindir)

    if a.update_ref:
        fix = Fixture(a.mui38, a.emulator)
        runs = [run_set(fix, names, bindir, [], a.timeout) for _ in range(a.repeat)]
        os.makedirs(a.refdir, exist_ok=True)
        for n in names:
            outs = [r.get(n) for r in runs]
            if any(o != outs[0] for o in outs):
                print('%-16s unstable on MUI 3.8, reference not written' % n)
            elif '[hung or crashed]' in outs[0]:
                print('%-16s hung or crashed on MUI 3.8, reference not written' % n)
            else:
                with open(os.path.join(a.refdir, n + '.txt'), 'w', encoding='latin-1') as f:
                    f.write(outs[0])
                print('%-16s reference written' % n)

    if a.zune:
        fix = Fixture(a.zune, a.emulator)
        assigns = stage_zune(fix, a.lib)
        out = run_set(fix, names, bindir, assigns, a.timeout)
        same = differ = 0
        for n in names:
            if a.out:
                os.makedirs(a.out, exist_ok=True)
                with open(os.path.join(a.out, n + '.txt'), 'w', encoding='latin-1') as f:
                    f.write(out[n])
            ref = os.path.join(a.refdir, n + '.txt')
            own = os.path.join(a.refdir, 'zune68', n + '.txt')
            if os.path.exists(own):
                ref = own
            if not os.path.exists(ref):
                print('%-16s no reference' % n)
                continue
            want = open(ref, encoding='latin-1').read()
            if want == out[n]:
                same += 1
                print('%-16s same' % n)
                continue
            differ += 1
            print('%-16s differs' % n)
            diff = list(difflib.unified_diff(want.splitlines(), out[n].splitlines(),
                                             os.path.relpath(ref, a.refdir), 'zune68/' + n,
                                             n=0, lineterm=''))
            for line in diff[:a.max_diff]:
                print('    ' + line)
            if len(diff) > a.max_diff:
                print('    ... %d more diff lines' % (len(diff) - a.max_diff))
        print('%d same, %d differ' % (same, differ))
        return 1 if differ else 0
    return 0


if __name__ == '__main__':
    sys.exit(main())

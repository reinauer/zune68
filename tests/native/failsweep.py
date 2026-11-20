#!/usr/bin/env python3
"""Run a failn sweep on a WinUAE fixture, booting again after a crash.

The fixture is a WinUAE configuration whose Startup-Sequence ends with
"Execute Work:stage" (Work: being WORKDIR on the host). For each boot the
script writes Work:stage to copy LIBDIR's files to RAM:, put them first on
LIBS:, and run "failn sweep N0 N1 PROG" with its output in Work:. When
the output stops growing for --stall seconds the run that was started last
crashed or hung the machine: it is recorded as "N=<n> crash", the emulator
is stopped and the sweep goes on from the next N in a fresh boot.

  failsweep.py --uae FIX/run.uae --work FIX/work --failn failn \
      --prog failprog [--lib build/muimaster/muimaster.library] \
      [--from 1] [--to 0] --out sweep.txt

With --keep the script stops at the first crash and leaves the emulator
running, for a debugger to read failn's record (see failn.c) and the
machine's state.

--lib may be given several times (or left out to test the fixture's own
muimaster.library, e.g. MUI 3.8); each file goes on LIBS: under its own
name, so a build to test must be named muimaster.library. The result file holds failn's lines, the
crash lines, and is ready for failsym.py.
"""

import argparse
import os
import re
import shutil
import subprocess
import time

WINUAE = '/Applications/WinUAE.app/Contents/MacOS/WinUAE'


def stop(uae):
    subprocess.run(['pkill', '-f', 'WinUAE -f %s' % uae], check=False)
    time.sleep(1)


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--uae', required=True)
    p.add_argument('--work', required=True)
    p.add_argument('--failn', required=True)
    p.add_argument('--prog', required=True)
    p.add_argument('--lib', action='append', default=[])
    p.add_argument('--from', dest='n0', type=int, default=1)
    p.add_argument('--to', dest='n1', type=int, default=0)
    p.add_argument('--stall', type=int, default=90)
    p.add_argument('--out', required=True)
    p.add_argument('--keep', action='store_true',
                   help='stop at the first crash, emulator left running')
    args = p.parse_args()

    uae = os.path.abspath(args.uae)
    sw = os.path.join(args.work, 'failsweep')
    shutil.rmtree(os.path.join(sw, 'libs'), ignore_errors=True)
    os.makedirs(os.path.join(sw, 'libs'))
    shutil.copy(args.failn, os.path.join(sw, 'failn'))
    shutil.copy(args.prog, os.path.join(sw, 'prog'))
    for lib in args.lib:
        shutil.copy(lib, os.path.join(sw, 'libs'))
    result = open(args.out, 'w')
    n, last = args.n0, args.n1
    while last == 0 or n <= last:
        part = os.path.join(sw, 'part.txt')
        for name in (part, os.path.join(sw, 'ran')):
            if os.path.exists(name):
                os.remove(name)
        # once per boot: a crash that resets the machine must not start
        # the sweep again
        stage = ['If EXISTS Work:failsweep/ran', 'Quit', 'EndIf',
                 'Echo >Work:failsweep/ran',
                 'Copy Work:failsweep/libs RAM:fslibs ALL QUIET',
                 'Copy Work:failsweep/failn Work:failsweep/prog RAM:',
                 'Assign LIBS: RAM:fslibs',
                 'Assign LIBS: SYS:Libs ADD',
                 'RAM:failn sweep %d %d RAM:prog >Work:failsweep/part.txt' % (n, last),
                 'Echo done >>Work:failsweep/part.txt']
        if not args.lib:
            del stage[6:8]          # the fixture's own LIBS:, e.g. MUI:Libs
        with open(os.path.join(args.work, 'stage'), 'w') as f:
            f.write('\n'.join(stage) + '\n')
        stop(uae)
        emu = subprocess.Popen([WINUAE, '-f', uae], stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
        size, since, started, done = -1, time.time(), None, False
        while True:
            time.sleep(2)
            text = open(part, encoding='latin-1').read() if os.path.exists(part) else ''
            if len(text) < size:
                break               # the machine was reset
            if len(text) != size:
                size, since = len(text), time.time()
            if text.rstrip().endswith('done'):
                done = True
                break
            if time.time() - since > (args.stall if text else 240):
                break
        if done or not args.keep:
            emu.kill()
            stop(uae)
        lines = text.splitlines()
        started = None
        for line in lines:
            if line == 'done':
                continue
            if line.startswith('rec=') and args.keep and not done:
                print(line, flush=True)  # failn's record of the failed call
            m = re.match(r'N=(\d+) start$', line)
            if m:
                started = int(m.group(1))
                continue
            m = re.match(r'count=(\d+)', line)
            if m and last == 0:
                last = int(m.group(1))
            if re.match(r'N=\d+ ', line) or line.startswith('count='):
                if line.startswith('count=') and n != args.n0:
                    continue
                result.write(line + '\n')
                started = None
        result.flush()
        if done:
            break
        if started is None:
            if not lines:
                result.write('N=%d noboot\n' % n)
                break
            m = [int(x) for x in re.findall(r'^N=(\d+) rc', text, re.M)]
            started = (max(m) + 1) if m else n
        result.write('N=%d crash\n' % started)
        result.flush()
        print('crash at N=%d' % started, flush=True)
        if args.keep:
            break
        n = started + 1
    result.close()


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Run native probes in an exclusively owned, disposable UAE fixture.

The fixture must boot Bench:run-tests, mapping fixture/guest to Bench:.
Use an actual AmigaOS ROM and installation. CPU speed is unrestricted for
functional tests; use compare_perf.py for timing comparisons instead.
"""
import argparse
import fcntl
from pathlib import Path
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fixture', 'emulator', 'build', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--libraries', type=Path,
                        help='optional replacement libraries; omit for reference MUI')
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--argument', default='screen',
                        choices=('screen', 'muimaster.library', 'zunemaster.library'),
                        help='probe argument; older ABI probes take a library name')
    parser.add_argument('probes', nargs='+')
    args = parser.parse_args()
    if any(not name.replace('_', '').isalnum() for name in args.probes):
        parser.error('probe names must contain only letters, digits and underscores')
    guest = args.fixture / 'guest'
    output = guest / 'native-results.txt'
    script = guest / 'run-tests'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with (args.fixture / 'runner.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        destinations = [script, *(guest / ('mui5-' + name + '-probe')
                                   for name in args.probes)]
        libraries = [guest / 'package/Libs' / name for name in
                     ('muimaster.library', 'zunemaster.library')]
        if args.libraries:
            destinations += libraries
        saved = {p: p.read_bytes() if p.exists() else None for p in destinations}
        process = None
        try:
            if args.libraries:
                for dest in libraries:
                    shutil.copy2(args.libraries / dest.name, dest)
            commands = ['FailAt 21', 'Stack 131072', 'Assign MUI: Bench:package',
                        'Assign LIBS: Bench:package/Libs ADD']
            for name in args.probes:
                binary = 'mui5-' + name + '-probe'
                shutil.copy2(args.build / binary, guest / binary)
                commands += ['Echo START:' + name + ' >>Bench:native-results.txt',
                             'Bench:' + binary + ' ' + args.argument +
                             ' >>Bench:native-results.txt',
                             'Echo RC:$RC >>Bench:native-results.txt']
            commands += ['Wait 3', 'Echo ALLDONE >>Bench:native-results.txt']
            script.write_text('\n'.join(commands) + '\n')
            output.write_text('')
            with args.output.with_suffix('.emulator.log').open('w') as log:
                process = subprocess.Popen([str(args.emulator), '-f',
                    str(args.fixture / 'run.uae'), '-s', 'use_gui=no', '-s',
                    'cpu_speed=max', '-s', 'debugging_features=none'], stdout=log, stderr=log)
                deadline = time.monotonic() + args.timeout
                previous = ''
                while time.monotonic() < deadline:
                    result = output.read_text(errors='replace')
                    if result != previous:
                        print(result[len(previous):], end='', flush=True)
                        args.output.write_text(result)
                        previous = result
                    if 'ALLDONE' in result:
                        if result.count('RC:0') != len(args.probes):
                            raise RuntimeError('one or more probes failed: ' + str(args.output))
                        return
                    if process.poll() is not None:
                        raise RuntimeError('emulator exited')
                    time.sleep(1)
                raise RuntimeError('guest timeout: ' + str(args.output))
        finally:
            if process:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            for path, contents in saved.items():
                if contents is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_bytes(contents)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Run a native probe in an owned compare_toolkits fixture, restoring media."""
import argparse
import fcntl
import json
from pathlib import Path
import re
import shutil
import subprocess
import time
from compare_toolkits import digest, manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('fixture', 'emulator', 'binary', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--profile', required=True)
    screens = p.add_mutually_exclusive_group()
    screens.add_argument('--screen', action='store_true')
    screens.add_argument('--rtg', action='store_true')
    p.add_argument('--timeout', type=int, default=600)
    a = p.parse_args()
    root = a.fixture.resolve()
    guest = root / 'guest'
    if not re.fullmatch('[a-z0-9_-]+', a.profile):
        p.error('invalid profile name')
    config, disk, pristine = (root / n for n in ('run.uae', 'system.hdf', 'pristine.hdf'))
    if str(disk) not in config.read_text():
        p.error('config must reference disposable system.hdf')
    profile = guest / a.profile
    if not (profile / 'Libs/muimaster.library').is_file():
        p.error('missing profile library')
    a.output.mkdir(parents=True, exist_ok=True)
    meta = dict(binary_sha256=digest(a.binary), config_sha256=digest(config),
                emulator_sha256=digest(a.emulator), pristine_sha256=digest(pristine),
                profile=manifest(profile), screen=a.screen, rtg=a.rtg)
    with (root / 'runner.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        saved = {f: f.read_bytes() if f.exists() else None for f in
                 (guest / 'run-tests', guest / 'probe')}
        try:
            shutil.copy2(pristine, disk)
            shutil.copy2(a.binary, guest / 'probe')
            result = guest / 'probe-result.txt'
            result.write_text('')
            (guest / 'run-tests').write_text(
                'FailAt 21\nStack 131072\n'
                f'Assign MUI: Bench:{a.profile}\nAssign LIBS: Bench:{a.profile}/Libs\n'
                'Assign LIBS: SYS:Libs ADD\nAssign LIBS: SYS:Classes ADD\n'
                f'Bench:probe {"screen" if a.screen else "rtg" if a.rtg else ""} >Bench:probe-result.txt\n'
                'Echo ALLDONE >>Bench:probe-result.txt\n')
            with (a.output / 'emulator.log').open('w') as log:
                proc = subprocess.Popen([str(a.emulator), '-f', str(config), '-s',
                    'use_gui=no', '-s', 'debugging_features=none'], stdout=log, stderr=log)
                try:
                    end = time.monotonic() + a.timeout
                    while time.monotonic() < end:
                        contents = result.read_text(errors='replace')
                        if 'ALLDONE' in contents:
                            break
                        if proc.poll() is not None:
                            raise RuntimeError('emulator exited')
                        time.sleep(1)
                    else:
                        raise RuntimeError('probe timeout')
                finally:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()
                    (a.output / 'output.txt').write_text(result.read_text(errors='replace'))
            if manifest(profile) != meta['profile']:
                raise RuntimeError('profile changed during probe')
            meta['output_sha256'] = digest(a.output / 'output.txt')
            (a.output / 'inputs.json').write_text(json.dumps(meta, indent=2) + '\n')
            print(contents)
            if 'FAIL' in contents or 'PASS' not in contents:
                raise RuntimeError('probe failed')
        finally:
            for path, data in saved.items():
                if data is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_bytes(data)


if __name__ == '__main__':
    main()

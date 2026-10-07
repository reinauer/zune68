#!/usr/bin/env python3
"""Build the public workload using an unmodified MUI 3.8 SDK's definitions.

Only the obsolete GCC inline assembly is regenerated, from that SDK's own
FD and prototypes. No Zune68 public/private headers enter this build.
"""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True,
                        help='original MUI/Developer directory')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cc', default='m68k-amigaos-gcc')
    parser.add_argument('--fd2sfd', default='fd2sfd')
    parser.add_argument('--sfdc', default='sfdc')
    args = parser.parse_args()
    include = args.sdk.resolve() / 'C/Include'
    fd = args.sdk.resolve() / 'FD/muimaster_lib.fd'
    protos = include / 'clib/muimaster_protos.h'
    for path in [fd, protos, include / 'libraries/mui.h']:
        if not path.is_file():
            parser.error(f'missing original SDK file: {path}')
    output = args.output.resolve()
    overlay = output / 'mui38-include'
    (overlay / 'inline').mkdir(parents=True, exist_ok=True)
    sfd = overlay / 'muimaster.sfd'
    subprocess.run([args.fd2sfd, '--quiet', str(fd), str(protos), str(sfd)], check=True)
    subprocess.run([args.sfdc, '--quiet', '--target=m68k-amigaos', '--mode=macros',
                    '--output=' + str(overlay / 'inline/muimaster.h'), str(sfd)], check=True)
    subprocess.run([args.cc, '-std=gnu17', '-m68000', '-msoft-float', '-noixemul', '-Os',
                    '-Wall', '-Wno-pointer-sign', '-Werror',
                    '-I' + str(overlay), '-I' + str(include),
                    '-Wl,-u,___stkinit', '-o', str(output / 'compatcheck-mui38'),
                    str(ROOT / 'tests/native/compatcheck.c'), '-lmui'], check=True)
    print(f'MUI 3.8 definitions and ABI assertions compile: {output / "compatcheck-mui38"}')


if __name__ == '__main__':
    main()

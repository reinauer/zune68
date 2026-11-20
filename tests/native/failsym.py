#!/usr/bin/env python3
"""Name the muimaster.library functions in failn's output.

failn prints the address muimaster.library's LVO -126 (MUI_Layout) jumps
to as mui126=. With the link map of the same build, that gives the address
the library's code was loaded at, and each address of a line that lies in
the code is printed as function+offset.

Usage: failsym.py [--map build/muimaster/muimaster.library.map] FILE...
"""

import argparse
import bisect
import re
import sys


def read_map(path):
    """Return the sorted (address, name) list of the .text symbols, and the
    code size."""
    symbols, size, in_text = [], 0, False
    with open(path, encoding='latin-1') as f:
        for line in f:
            m = re.match(r'^\.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)', line)
            if m:
                in_text, size = True, int(m.group(2), 16)
                continue
            if in_text and re.match(r'^\.\w', line):
                break
            m = re.match(r'^\s+0x([0-9a-f]+)\s+([A-Za-z_][\w.$]*)\s*$', line)
            if in_text and m:
                symbols.append((int(m.group(1), 16), m.group(2)))
    symbols.sort()
    return symbols, size


def name(symbols, offset):
    i = bisect.bisect_right([a for a, _ in symbols], offset) - 1
    if i < 0:
        return None
    addr, sym = symbols[i]
    return '%s+0x%x' % (sym, offset - addr)


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--map', default='build/muimaster/muimaster.library.map')
    p.add_argument('files', nargs='+')
    args = p.parse_args()
    symbols, size = read_map(args.map)
    gate = dict((n, a) for a, n in symbols).get('Gate_MUI_Layout')
    if gate is None:
        sys.exit('failsym: no Gate_MUI_Layout in %s' % args.map)
    for path in args.files:
        with open(path, encoding='latin-1') as f:
            for line in f:
                line = line.rstrip('\n')
                m = re.search(r'mui126=([0-9a-f]{8})', line)
                base = int(m.group(1), 16) - gate if m and int(m.group(1), 16) else None

                def resolve(mo):
                    a = int(mo.group(1), 16)
                    if base is not None and base <= a < base + size:
                        return '%s(%s)' % (mo.group(1), name(symbols, a - base))
                    return mo.group(0)
                if base is not None:
                    head, _, tail = line.partition(' mui126=')
                    line = re.sub(r'\b([0-9a-f]{8})\b(?:=\S+)?', resolve, head)
                print(line)


if __name__ == '__main__':
    main()

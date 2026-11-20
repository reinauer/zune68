#!/usr/bin/env python3
"""Group memtally's blocks by the muimaster.library function that holds them.

memtally prints each live block with two caller addresses and the address
muimaster.library's LVO -126 jumps to (mui126=). With the link map of the
same build the callers become functions; the report sums the blocks per
caller pair, largest first.

Usage: memreport.py [--map build/muimaster/muimaster.library.map] [--top N] FILE
"""

import argparse
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from failsym import name, read_map  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--map', default='build/muimaster/muimaster.library.map')
    p.add_argument('--top', type=int, default=40)
    p.add_argument('file')
    args = p.parse_args()
    symbols, size = read_map(args.map)
    gate = dict((n, a) for a, n in symbols).get('Gate_MUI_Layout')
    lines = open(args.file, encoding='latin-1').read().splitlines()
    base = None
    for line in lines:
        m = re.match(r'mui126=([0-9a-f]{8})', line)
        if m and int(m.group(1), 16):
            base = int(m.group(1), 16) - gate

    def func(a):
        if a == 0:
            return '-'
        if base is not None and base <= a < base + size:
            return name(symbols, a - base).split('+')[0]
        return 'other:%08x' % a

    pairs = collections.defaultdict(lambda: [0, 0])
    firsts = collections.defaultdict(lambda: [0, 0])
    total = count = 0
    for line in lines:
        m = re.match(r'([0-9a-f]{8}) (\d+) ([0-9a-f]{8}) ([0-9a-f]{8})$', line)
        if not m:
            continue
        sz, c1, c2 = int(m.group(2)), int(m.group(3), 16), int(m.group(4), 16)
        f1, f2 = func(c1), func(c2)
        for d, key in ((pairs, (f1, f2)), (firsts, f1)):
            d[key][0] += sz
            d[key][1] += 1
        total += sz
        count += 1
    print('%d bytes in %d blocks' % (total, count))
    print('\nby first caller:')
    for k, (sz, n) in sorted(firsts.items(), key=lambda i: -i[1][0])[:args.top]:
        print('%8d %5d  %s' % (sz, n, k))
    print('\nby caller pair:')
    for (f1, f2), (sz, n) in sorted(pairs.items(), key=lambda i: -i[1][0])[:args.top]:
        print('%8d %5d  %s <- %s' % (sz, n, f1, f2))


if __name__ == '__main__':
    main()

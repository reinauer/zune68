#!/usr/bin/env python3
"""Compare compatcheck contract output from separate MUI and Zune68 runs."""
import argparse
import difflib
from pathlib import Path


def contracts(path):
    rows = [line.strip() for line in path.read_text().splitlines()
            if line.startswith('contract ')]
    names = [row.split()[1] for row in rows]
    if sorted(names) != ['lifecycle', 'notify', 'numeric', 'sorted-list']:
        raise ValueError(f'{path}: expected one complete compatcheck run')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('candidate', type=Path)
    args = parser.parse_args()
    try:
        a, b = contracts(args.reference), contracts(args.candidate)
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    if a != b:
        print('\n'.join(difflib.unified_diff(a, b, fromfile=str(args.reference),
                                           tofile=str(args.candidate), lineterm='')))
        return 1
    if any(row.split()[2] != '1' for row in a):
        print('Contract outputs match, but at least one contract failed.')
        return 1
    print('Contract outputs match and pass; compare timing and memory separately.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

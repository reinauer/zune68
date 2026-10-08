#!/usr/bin/env python3
"""Apply narrowly scoped native fixes without changing the pinned submodule."""
import argparse
from pathlib import Path


def prepare(source, output):
    counts = {'backgroundadjust.c': 2, 'penadjust.c': 1}
    expected = counts[source.name]
    data = source.read_bytes()
    old, new = b"(!*spec++==':')", b"(*spec++!=':')"
    if data.count(old) != expected:
        raise ValueError(f'{source}: expected {expected} delimiter checks; review upstream changes')
    # Negating the character before comparing it to ':' is always false.
    # Comparing the character directly also stops before advancing past NUL.
    result = data.replace(old, new)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_bytes() != result:
        output.write_bytes(result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    prepare(args.source, args.output)

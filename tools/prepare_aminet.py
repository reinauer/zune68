#!/usr/bin/env python3
"""Prepare fixed-name Aminet files from a versioned release, without uploading."""
import argparse
import re
import shutil
from pathlib import Path


def prepare(release_dir, output_dir, tag):
    if not re.fullmatch(r'v[0-9]+\.[0-9]+', tag):
        raise ValueError('expected a library version tag such as v19.81')
    version = tag[1:]
    archive = release_dir / f'Zune68-{version}-amigaos3-m68k.lha'
    readme = archive.with_suffix('.readme')
    text = readme.read_text(encoding='ascii')
    fields = dict(re.findall(r'^([A-Za-z-]+):\s*([^\n]*)$',
                             text.split('\n\n', 1)[0], re.MULTILINE))
    if fields.get('Version') != version:
        raise ValueError('release tag and Aminet readme version differ')
    if fields.get('Type') != 'util/libs':
        raise ValueError('expected Aminet category util/libs')
    if not archive.is_file() or not archive.stat().st_size:
        raise ValueError(f'missing or empty release archive: {archive}')
    output_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(archive, output_dir / 'Zune68.lha')
    shutil.copyfile(readme, output_dir / 'Zune68.readme')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release-dir', type=Path,
                        default=Path('build/muimaster/release'))
    parser.add_argument('--output-dir', type=Path, default=Path('build/aminet'))
    parser.add_argument('--tag', required=True)
    args = parser.parse_args()
    try:
        prepare(args.release_dir, args.output_dir, args.tag)
    except (OSError, ValueError) as error:
        parser.exit(1, f'prepare_aminet: {error}\n')


if __name__ == '__main__':
    main()

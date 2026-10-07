#!/usr/bin/env python3
"""Update the checked-in Workbench icons using icontool (authoring only)."""
import argparse
import subprocess
from pathlib import Path

DIST = Path(__file__).resolve().parents[1] / 'dist'

# Leave room for the existing Install and ReadMe icons above the drawers.
DRAWERS = {
    'Libs': (16, 64),
    'Prefs': (112, 64),
    'Locale': (208, 64),
    'Examples': (304, 64),
    'SDK': (16, 108),
    'Docs': (112, 108),
    'Tests': (208, 108),
}
LIST_DRAWERS = {'Libs', 'Locale', 'Examples', 'SDK', 'Docs', 'Tests'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--icontool', default='icontool')
    parser.add_argument('--drawers-only', action='store_true',
                        help='update drawer layout without importing PNGs')
    args = parser.parse_args()

    def edit(source, *options):
        subprocess.run([args.icontool, '--strict', *options,
                        str(source)], check=True)

    drawer = DIST / 'Zune68.info'
    edit(drawer, '--pos', '24,24', '--winsize', '420,180',
         '--edit', 'DrawerData:CurrentX=0',
         '--edit', 'DrawerData:CurrentY=0',
         '--edit', 'DrawerData:NewWindow:LeftEdge=40',
         '--edit', 'DrawerData:NewWindow:TopEdge=30')
    for name, (x, y) in DRAWERS.items():
        view = []
        if name in LIST_DRAWERS:
            # DDFLAGS_SHOWALL and DDVM_BYNAME from workbench/workbench.h.
            view = ['--edit', 'DrawerData2:Flags=2',
                    '--edit', 'DrawerData2:ViewModes=2']
        edit(drawer, '--pos', f'{x},{y}', *view,
             '-o', str(DIST / (name + '.info')))

    if not args.drawers_only:
        edit(DIST / 'Zune.info', '--pos', '24,16',
             '--edit', 'DiskObject:StackSize=65536',
             '--edit', 'DiskObject:Gadget:Width=54',
             '--edit', 'DiskObject:Gadget:Height=27',
             '--import-icon', str(DIST / 'icons/Zune.png'),
             '--import-select-icon', str(DIST / 'icons/Zune-selected.png'))


if __name__ == '__main__':
    main()

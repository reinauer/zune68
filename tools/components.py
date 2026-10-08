#!/usr/bin/env python3
"""Inventory and stage native optional components without touching source files."""
import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLASSES = ROOT / 'workbench/classes/zune'
PLUGINS = {
    'betterstring/mcc': 'BetterString.mcc',
    'betterstring/mcc/hotkeystring': 'HotkeyString.mcc',
    'betterstring/mcp': 'BetterString.mcp',
    'nlist/nbitmap_mcc': 'NBitmap.mcc',
    'nlist/nbalance_mcc': 'NBalance.mcc',
    'nlist/nlist_mcc': 'NList.mcc',
    'nlist/nlistview_mcc': 'NListview.mcc',
    'nlist/nfloattext_mcc': 'NFloattext.mcc',
    'nlist/nlisttree_mcc': 'NListtree.mcc',
    'nlist/nlisttree_mcp': 'NListtree.mcp',
    'nlist/nlistviews_mcp': 'NListviews.mcp',
    'texteditor/mcc': 'TextEditor.mcc',
    'texteditor/mcp': 'TextEditor.mcp',
    'thebar/mcc': 'TheBar.mcc',
    'thebar/mcc-virtual': 'TheBarVirt.mcc',
    'thebar/mcc/button': 'TheButton.mcc',
    'thebar/mcp': 'TheBar.mcp',
    'thebar/toolbar_mcc': 'Toolbar.mcc',
}
IMAGE_CLASSES = ('Rawimage.mcc', 'Pixmap.mui')
CLASS_FILES = (*PLUGINS.values(), *IMAGE_CLASSES)
EXAMPLES = ['HelloZune', 'HGroup', 'VGroup', 'VHGroup', 'VHGroup2', 'HVGroup', 'Notify', 'TheBarDemo']


# Package paths stay ASCII; Amiga Installer maps these to native OS3 names.
LANGUAGES = {'german': 'deutsch', 'french': 'français', 'spanish': 'español',
             'italian': 'italiano', 'danish': 'dansk', 'dutch': 'nederlands',
             'norwegian': 'norsk', 'swedish': 'svenska', 'polish': 'polski',
             'finnish': 'suomi', 'hungarian': 'magyar', 'turkish': 'türkçe'}


def catalog_jobs(build):
    prefs = ROOT / 'workbench/prefs/Zune/catalogs'
    for source in sorted(prefs.glob('*.ct')):
        language = {v: k for k, v in LANGUAGES.items()}.get(source.stem, source.stem)
        yield (build / 'Locale/Catalogs' / language / 'System/Prefs/Zune.catalog',
               [prefs / 'zune.cd', source], ['VERSION', '1', 'REVISION', '0'])
    for path, stem, destination in [
        ('workbench/libs/muimaster/catalogs', 'muimaster', 'System/Libs/muimaster.catalog'),
        ('workbench/classes/zune/prefswindow/catalogs', 'prefswindow', 'System/Classes/Zune/PrefsWindow.catalog'),
    ]:
        catalog_dir = ROOT / path
        for source in sorted(catalog_dir.glob('*.ct')):
            yield (build / 'Locale/Catalogs' / source.stem / destination,
                   [catalog_dir / (stem + '.cd'), source], [])
    # FlexCat uses the Amiga language name from each PO file for catalog lookup.
    for component, name in PLUGINS.items():
        if name.endswith('.mcp'):
            for source in sorted((CLASSES / component / 'locale').glob('*.po')):
                language = source.stem
                # PO basenames already follow the classes' catalog convention.
                target = name.replace('.mcp', '_mcp.catalog')
                yield (build / 'Locale/Catalogs' / language / target,
                       [source], ['POFILE'])


def catalog_paths(build):
    return [out.relative_to(build).as_posix() for out, _, _ in catalog_jobs(build)]


def catalogs(build, flexcat):
    for output, inputs, options in catalog_jobs(build):
        if (output.exists() and output.stat().st_mtime >=
                max(p.stat().st_mtime for p in inputs + [Path(__file__)])):
            continue
        output.parent.mkdir(parents=True, exist_ok=True)
        if options == ['POFILE']:
            command = [flexcat, 'POFILE', str(inputs[0])]
        else:
            command = [flexcat, *map(str, inputs), *options]
        with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
            candidate = Path(temporary) / output.name
            result = subprocess.run([*command, 'CATALOG', str(candidate)])
            # RETURN_WARN denotes missing translations, which use English.
            if result.returncode not in (0, 5) or not candidate.is_file():
                result.check_returncode()
                raise ValueError(f'{output}: catalog was not produced')
            data = candidate.read_bytes()
            if (data[:4] != b'FORM' or data[8:12] != b'CTLG' or
                    int.from_bytes(data[4:8], 'big') + 8 != len(data)):
                raise ValueError(f'{output}: invalid catalog')
            candidate.replace(output)


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or source.read_bytes() != target.read_bytes():
        shutil.copyfile(source, target)


def sdk(build):
    copy(ROOT / 'dist/Zune.info', build / 'Prefs/Zune.info')
    master = ROOT / 'workbench/libs/muimaster'
    for directory in ['clib', 'proto', 'pragmas', 'fd', 'libraries', 'inline']:
        for source in (master / 'include' / directory).glob('*'):
            if source.is_file():
                copy(source, build / 'SDK/include' / directory / source.name)
    for source in (build / 'include').rglob('*.h'):
        copy(source, build / 'SDK/include' / source.relative_to(build / 'include'))
    # Other class projects carry older dependency headers. Ship each
    # class's own public definition instead of whichever copy sorts last.
    owners = {'BetterString_mcc.h': 'betterstring',
              'HotkeyString_mcc.h': 'betterstring', 'NBitmap_mcc.h': 'nlist'}
    for family in ['betterstring', 'nlist', 'texteditor']:
        for source in (CLASSES / family / 'include/mui').glob('*.h'):
            if owners.get(source.name, family) == family:
                copy(source, build / 'SDK/include/mui' / source.name)
    for name in ['TheBar_mcc.h', 'TheBar_mcp.h', 'Toolbar_mcc.h']:
        copy(CLASSES / 'thebar/include/mui' / name, build / 'SDK/include/mui' / name)
    copy(CLASSES / 'rawimage/Rawimage_mcc.h', build / 'SDK/include/mui/Rawimage_mcc.h')
    copy(CLASSES / 'rawimage/MCC_Rawimage.doc', build / 'Docs/Rawimage/MCC_Rawimage.doc')
    copy(ROOT / 'vendor/bzip2/LICENSE', build / 'Docs/bzip2/LICENSE')
    copy(ROOT / 'vendor/bzip2/ORIGIN', build / 'Docs/bzip2/ORIGIN')
    for family in ['betterstring', 'nlist', 'texteditor', 'thebar']:
        for name in ['COPYING', 'AUTHORS', 'ChangeLog']:
            source = CLASSES / family / name
            if source.exists():
                copy(source, build / 'Docs' / family / name)
        for dirname in ['docs', 'doc']:
            for source in (CLASSES / family / dirname).glob('*'):
                if source.is_file():
                    copy(source, build / 'Docs' / family / source.name)
    copy(ROOT / 'tools/thebar/Native', build / 'Docs/thebar/Native')
    copy(ROOT / 'tests/native/thebar_demo.c', build / 'SDK/examples/TheBarDemo.c')
    for source in (master / 'tutorial/examples').glob('*'):
        if source.suffix in ('.c', '.h'):
            copy(source, build / 'SDK/examples' / source.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['list', 'catalogs', 'sdk'])
    parser.add_argument('--build', type=Path)
    parser.add_argument('--flexcat', default='flexcat')
    args = parser.parse_args()
    if args.action == 'list':
        print(' '.join(PLUGINS))
    elif args.action == 'catalogs':
        catalogs(args.build, args.flexcat)
    else:
        sdk(args.build)

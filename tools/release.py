#!/usr/bin/env python3
"""Assemble the native Zune68 package from native GCC build outputs."""
import argparse
import re
import shutil
import struct
import subprocess
import tempfile
import tarfile
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from components import PLUGINS, EXAMPLES, LANGUAGES, catalog_paths

LIBRARIES = ('zunemaster.library', 'muimaster.library')


def library_version(path):
    data = path.read_bytes()
    if data[:4] != struct.pack('>I', 1011):
        raise ValueError(f'{path}: expected an Amiga Hunk load file')
    pattern = rb'\$VER: ' + re.escape(path.name.encode()) + rb' (\d+)\.(\d+)\b'
    versions = set(re.findall(pattern, data))
    if len(versions) != 1:
        raise ValueError(f'{path}: missing or ambiguous matching $VER string')
    return '.'.join(part.decode() for part in versions.pop())


def component_files(build_dir):
    files = ['Prefs/Zune', 'Prefs/Zune.info']
    files += ['Libs/MUI/' + name for name in PLUGINS.values()]
    files += ['Examples/' + name for name in EXAMPLES]
    files += catalog_paths(build_dir)
    for directory in ['SDK', 'Docs']:
        entries = [p.relative_to(build_dir).as_posix()
                   for p in (build_dir / directory).rglob('*') if p.is_file()]
        if not entries:
            raise ValueError(f'{build_dir}: missing {directory}')
        files += entries
    for name in files:
        if not (build_dir / name).is_file():
            raise ValueError(f'{build_dir}: missing {name}')
    for name in ['Prefs/Zune', *['Libs/MUI/' + n for n in PLUGINS.values()]]:
        library_version(build_dir / name)
    return files


def install_script(files):
    script = (ROOT / 'dist/Install').read_text()
    if not files:
        return script
    preflight = []
    for name in files:
        preflight.append('(if (<> (exists (tackon #package "' + name + '")) 1)\n'
                         '    (abort "The package is incomplete. Extract the complete Zune68 archive."))')
        if name == 'Prefs/Zune' or name.startswith('Libs/MUI/'):
            preflight.append(f'(if (= (getversion (tackon #package "{name}")) 0)\n'
                             f'    (abort "Cannot read the bundled version of {name}."))')
    lines = ['(P_InstallFile "Prefs/Zune" "SYS:Prefs"',
             '    "Zune68 preferences editor, opened by application settings menus." "")']
    for name in PLUGINS.values():
        lines.append(f'(P_InstallFile "Libs/MUI/{name}" "LIBS:MUI" '
                     f'"Optional, demand-loaded MUI custom class." "{name}")')
    lines += ['(if (askbool (prompt "Install the bundled translations to LOCALE:Catalogs?\\nExisting catalogs are backed up once as .old.")',
              '    (help "Missing translations fall back to English. No saved preferences are replaced.")',
              '    (choices "Install" "Skip") (default 0))', '    (']
    directories = set()
    for name in files:
        if name.startswith('Locale/Catalogs/'):
            parts = Path(name).parts[2:]
            language = LANGUAGES.get(parts[0], parts[0])
            directory = 'LOCALE:Catalogs/' + '/'.join([language, *parts[1:-1]])
            # Make each intermediate directory explicitly for old Installer.
            for count in range(1, len(directory.split('/')) + 1):
                parent = '/'.join(directory.split('/')[:count])
                if parent not in directories:
                    lines.append(f'        (makedir "{parent}")')
                    directories.add(parent)
            lines.append(f'        (P_CopySupport "{name}" "{directory}")')
    lines += ['    )', ')',
              '(message "Examples, SDK, documentation and corresponding source remain in the extracted Zune68 drawer. No SDK assigns are changed.")']
    return script.replace('; COMPONENT_PREFLIGHT', '\n'.join(preflight)).replace(
        '; COMPONENT_INSTALL', '\n'.join(lines))


def strip_binaries(package, strip):
    """Strip only staged Hunk executables, including libraries and plugins."""
    for path in sorted(package.rglob('*')):
        if not path.is_file():
            continue
        with path.open('rb') as source:
            is_hunk = source.read(4) == struct.pack('>I', 1011)
        if is_hunk:
            subprocess.run([strip, '--strip-all', str(path)], check=True)


def release(build_dir, output_dir, lha, full=True, strip='m68k-amigaos-strip'):
    build_dir, output_dir = build_dir.resolve(), output_dir.resolve()
    versions = [library_version(build_dir / name) for name in LIBRARIES]
    if versions[0] != versions[1]:
        raise ValueError('library versions differ; rebuild both variants')
    if not (build_dir / 'opentest').is_file():
        raise ValueError(f'{build_dir}: missing opentest')
    files = component_files(build_dir) if full else []
    if full and not (build_dir / "compatcheck").is_file():
        raise ValueError(f"{build_dir}: missing compatcheck")
    archiver = shutil.which(lha)
    if not archiver:
        raise ValueError(f'{lha}: LHA archiver not found; set LHA to its path')
    stripper = shutil.which(strip) if strip else None
    if strip and not stripper:
        raise ValueError(f'{strip}: Amiga strip tool not found; set STRIP or --strip')
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / f'Zune68-{versions[0]}-amigaos3-m68k.lha'
    readme = (ROOT / 'Zune68.readme').read_text(encoding='ascii').replace(
        '[VERSION]', versions[0])
    # Always assemble a fresh tree: removed files must not survive a release.
    # Publish only after archiving and CRC checking complete successfully.
    with tempfile.TemporaryDirectory(prefix='.zune68-', dir=output_dir) as temp:
        stage = Path(temp)
        package = stage / 'Zune68'
        (package / 'Libs').mkdir(parents=True)
        (package / 'Tests').mkdir()
        for name in LIBRARIES:
            shutil.copyfile(build_dir / name, package / 'Libs' / name)
            (package / 'Libs' / name).chmod(0o755)
        shutil.copyfile(build_dir / 'opentest', package / 'Tests/opentest')
        (package / 'Tests/opentest').chmod(0o755)
        if full:
            shutil.copyfile(build_dir / 'compatcheck', package / 'Tests/compatcheck')
            (package / 'Tests/compatcheck').chmod(0o755)
        shutil.copyfile(ROOT / 'LICENSE', package / 'LICENSE')
        (package / 'Zune68.readme').write_text(readme, encoding='ascii')
        for name in ('ReadMe', 'ReadMe.info', 'Install.info', 'Compatibility'):
            shutil.copyfile(ROOT / 'dist' / name, package / name)
        (package / 'Install').write_bytes(install_script(files).encode('latin1'))
        for name in files:
            target = package / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(build_dir / name, target)
            if name.startswith(('Libs/', 'Prefs/', 'Examples/')) and not name.endswith('.info'):
                target.chmod(0o755)
        drawers = ['Libs', 'Tests']
        if full:
            drawers += ['Prefs', 'Locale', 'Examples', 'SDK', 'Docs']
        for name in drawers:
            shutil.copyfile(ROOT / 'dist' / (name + '.info'),
                            package / (name + '.info'))
        if stripper:
            strip_binaries(package, stripper)
        if full:
            # Ship corresponding sources, including the native build adapters.
            source = stage / 'Source'
            ignore = shutil.ignore_patterns('*.o', '*.a', '*.map', '*.gst',
                                           '__pycache__', '.git', '*.rej', '.obj*', 'bin_*', '*.library',
                                           '*.lib', '*.lnk', 'opentest', 'buildincludes')
            for directory in ['workbench', 'compiler', 'developer']:
                shutil.copytree(ROOT / directory, source / directory, ignore=ignore)
            shutil.copytree(ROOT / 'tools', source / 'tools', ignore=ignore)
            shutil.copytree(ROOT / 'tests', source / 'tests', ignore=ignore)
            shutil.copytree(ROOT / 'dist', source / 'dist', ignore=ignore)
            shutil.copytree(ROOT / '.github', source / '.github')
            for name in ['GNUmakefile', 'LICENSE', 'LICENSE.Author', 'LICENSE.GPL',
                         'LICENSE.LGPL', 'LEGAL', 'ACKNOWLEDGEMENTS',
                         '.gitmodules', 'Zune68.readme']:
                shutil.copyfile(ROOT / name, source / name)
            if (ROOT / 'BUILD.md').exists():
                shutil.copyfile(ROOT / 'BUILD.md', source / 'BUILD.md')
            with tarfile.open(package / 'Source.tar.gz', 'w:gz') as archive_source:
                archive_source.add(source, arcname='Source')
        shutil.copyfile(ROOT / 'dist/Zune68.info', stage / 'Zune68.info')
        temporary_archive = stage / archive.name
        subprocess.run([archiver, 'ao5', str(temporary_archive),
                        'Zune68', 'Zune68.info'], cwd=stage, check=True)
        subprocess.run([archiver, 't', str(temporary_archive)], check=True)
        temporary_readme = stage / archive.with_suffix('.readme').name
        temporary_readme.write_text(readme, encoding='ascii')
        temporary_archive.replace(archive)
        temporary_readme.replace(archive.with_suffix('.readme'))
    print(f'Release: {archive}')
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path,
                        default=ROOT / 'build/muimaster')
    parser.add_argument('--output-dir', type=Path,
                        default=ROOT / 'build/muimaster/release')
    parser.add_argument('--lha', default='lha')
    parser.add_argument('--strip', default='m68k-amigaos-strip',
                        help='Amiga strip tool for staged executables')
    parser.add_argument('--no-strip', dest='strip', action='store_const', const=None,
                        help='retain symbols in the package for debugging')
    parser.add_argument('--core-only', action='store_true',
                        help='package only the two libraries and opentest')
    args = parser.parse_args()
    try:
        release(args.build_dir, args.output_dir, args.lha, full=not args.core_only,
                strip=args.strip)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'release: {error}\n')


if __name__ == '__main__':
    main()

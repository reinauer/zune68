#!/usr/bin/env python3
"""Exercise the real script with an optional host CLI build of InstallerLG.

The host has no Amiga assigns or resident libraries. Only the LIBS: and MUI:
locations, the temporary protocol file, resident version lookup, and initial
UI mode are substituted in temporary copies. MUI: counts as assigned, so the
assign and S:User-Startup changes are not exercised here. No files outside
the fixture directories are installed.
"""
import argparse
import subprocess
import tempfile
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from components import CLASS_FILES, LANGUAGES, catalog_paths
from release import install_script

NAMES = ('zunemaster.library', 'muimaster.library')
WELCOME = '(welcome "Install Zune68\'s native AmigaOS libraries.")'


def version_file(name, version):
    return f'\0$VER: {name} {version} (03.10.2026)\0'.encode()


def localize(script, root, libs, mui, *, resident=0, pretend=False, level=2,
             mode=0):
    """Point the script's system locations at fixture directories.

    The CLI build of InstallerLG cannot answer askchoice with the first
    choice, so the Install / Set back mode is set directly.
    """
    script = script.replace('(if #previous\n', f'(set #mode {mode})\n(if 0\n')
    script = script.replace('(set #syslibs (expandpath "LIBS:"))',
                            f'(set #syslibs "{libs}")')
    script = script.replace('(set #muiroot (getassign "MUI" "a"))',
                            f'(set #muiroot "{mui}")')
    script = script.replace('(if (= (getassign "MUI" "a") "")', '(if 0')
    script = script.replace('"T:Zune68-Install.log"', f'"{root}/protocol.tmp"')
    script = script.replace('(getversion #residentname (resident))', str(resident))
    return script.replace(WELCOME, WELCOME + f'\n(set @pretend {int(pretend)})'
                          f'\n(set @user-level {level})')


def run(installer, directory, answers, encoding=None):
    # InstallerLG's host argument reader expects a relative script path.
    result = subprocess.run([str(installer), 'Install'], cwd=directory,
                            input=answers, text=True, encoding=encoding,
                            capture_output=True, timeout=15, check=True)
    out = result.stdout + result.stderr
    assert 'syntax error' not in out.lower(), out
    assert 'undefined variable' not in out.lower(), out
    return out


def scenario(installer, label, *, installed=None, answers='N\nN\n',
             backup=None, pretend=False, resident=0, novice=False,
             missing=False, invalid=False, directory=False):
    with tempfile.TemporaryDirectory(prefix='zune68-installer-') as temp:
        root = Path(temp)
        payload, target, mui = root / 'Libs', root / 'system-libs', root / 'mui'
        payload.mkdir()
        target.mkdir()
        mui.mkdir()
        for name in NAMES:
            (payload / name).write_bytes(version_file(name, '35.6'))
        destination = target / NAMES[0]
        backup_path = target / (NAMES[0] + '.zune68-old')
        if installed is not None:
            destination.write_bytes(installed)
        if backup is not None:
            backup_path.write_bytes(backup)
        if missing:
            (payload / NAMES[1]).unlink()
        if invalid:
            (payload / NAMES[1]).write_bytes(b'no version here')
        if directory:
            destination.mkdir()

        script = localize((ROOT / 'dist/Install').read_text(), root, target, mui,
                          resident=resident, pretend=pretend,
                          level=0 if novice else 2)
        (root / 'Install').write_text(script)
        out = run(installer, root, answers)
        assert all(len(p.name) <= 30 for p in target.iterdir()), 'OFS/FFS filename limit'
        if missing or invalid or directory:
            assert 'Aborting' in out, out
            assert not (target / NAMES[1]).exists()
            if not directory:
                assert not destination.exists()
        else:
            assert 'Bundled is v35.6.' in out, out
            if installed is not None:
                if b'$VER:' in installed:
                    old = installed.split(b' ')[2].decode()
                    assert f'v{old} already installed.' in out, out
                else:
                    assert 'version unknown' in out, out
            first, second = answers.splitlines()
            for name, answer in zip(NAMES, (first, second)):
                path = target / name
                original = installed if name == NAMES[0] else None
                expected = version_file(name, '35.6') if answer == 'Y' and not pretend else original
                assert path.exists() == (expected is not None), out
                if expected is not None:
                    assert path.read_bytes() == expected, out
                marker = mui / 'Zune68/Created/Libs' / name
                assert marker.exists() == (answer == 'Y' and not pretend
                                           and original is None), (name, out)
            expected_backup = backup
            if expected_backup is None and installed is not None and first == 'Y' and not pretend:
                expected_backup = installed
            assert backup_path.exists() == (expected_backup is not None), out
            if expected_backup is not None:
                assert backup_path.read_bytes() == expected_backup, out
            protocol = mui / 'Zune68/Install.log'
            assert protocol.exists() == ('Y' in answers and not pretend), out
            if pretend:
                assert 'Pretend mode finished' in out, out
            if resident:
                assert 'Currently in memory: v40.1' in out, out
            if label == 'downgrade':
                assert 'This is a downgrade.' in out, out
            if label == 'same version':
                assert 'This replaces the same version.' in out, out
        print(f'Installer: {label} passed')


def set_back_scenario(installer):
    """Install over an older MUI, then set back to exactly that state."""
    with tempfile.TemporaryDirectory(prefix='zune68-setback-') as temp:
        root = Path(temp)
        payload, target, mui = root / 'Libs', root / 'system-libs', root / 'mui'
        payload.mkdir()
        target.mkdir()
        mui.mkdir()
        for name in NAMES:
            (payload / name).write_bytes(version_file(name, '35.6'))
        mui_library = version_file(NAMES[1], '19.1')
        (target / NAMES[1]).write_bytes(mui_library)
        script = (ROOT / 'dist/Install').read_text()
        (root / 'Install').write_text(localize(script, root, target, mui))
        run(installer, root, 'Y\nY\n')
        assert (target / (NAMES[1] + '.zune68-old')).read_bytes() == mui_library
        assert (mui / 'Zune68/Created/Libs' / NAMES[0]).is_file()
        (root / 'Install').write_text(localize(script, root, target, mui, mode=1))
        out = run(installer, root, '')
        assert not (target / NAMES[0]).exists(), out
        assert (target / NAMES[1]).read_bytes() == mui_library, out
        assert not list(target.glob('*.zune68-old')), out
        assert not list((mui / 'Zune68/Created').rglob('*.library')), out
        protocol = (mui / 'Zune68/Install.log').read_text()
        assert ': install ---' in protocol and ': set back ---' in protocol, protocol
        assert 'Restored' in protocol and 'Removed' in protocol, protocol
        print('Installer: set back passed')


def components_scenario(installer, *, pretend=False, skip=False, missing=False,
                        invalid=False, set_back=False):
    with tempfile.TemporaryDirectory(prefix='zune68-components-install-') as temp:
        root = Path(temp)
        package = root / 'package'
        target = root / 'target'
        mui = root / 'mui'
        package.mkdir()
        target.mkdir()
        (mui / 'Libs/MUI').mkdir(parents=True)
        libs = target / 'Libs'
        libs.mkdir()
        files = ['Prefs/Zune', 'Prefs/Zune.info']
        files += ['Libs/MUI/' + name for name in CLASS_FILES]
        files += catalog_paths(package)
        for relative in ['Libs/' + name for name in NAMES] + files:
            path = package / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(version_file(path.name, '35.6'))
        def write_script(mode):
            script = localize(install_script(files), root, libs, mui,
                              pretend=pretend, mode=mode)
            script = script.replace('"SYS:Prefs"', f'"{target}/Prefs"')
            script = script.replace('"LOCALE:Catalogs', f'"{target}/Catalogs')
            # Host filenames use ASCII fixture names; Amiga script uses Latin-1.
            for english, native in LANGUAGES.items():
                script = script.replace('/' + native + '/', '/' + english + '/')
                script = script.replace('/' + native + '"', '/' + english + '"')
            (package / 'Install').write_bytes(script.encode('latin1'))

        write_script(0)
        # An older class in the first LIBS: directory hides MUI:Libs/MUI.
        old = libs / 'MUI/BetterString.mcc'
        old.parent.mkdir()
        old.write_bytes(version_file(old.name, '35.4'))
        old_icon = target / 'Prefs/Zune.info'
        old_icon.parent.mkdir()
        old_icon.write_bytes(b'original icon')
        old_catalog = target / 'Catalogs/german/BetterString_mcp.catalog'
        old_catalog.parent.mkdir(parents=True)
        old_catalog.write_bytes(b'original catalog')
        originals = {p: p.read_bytes() for p in target.rglob('*') if p.is_file()}
        if missing:
            (package / 'Libs/MUI/TextEditor.mcp').unlink()
        if invalid:
            (package / 'Libs/MUI/TextEditor.mcp').write_bytes(b'no version')
        answers = ('N\n' if skip else 'Y\n') * (len(CLASS_FILES) + 4)
        out = run(installer, package, answers, encoding='latin1')
        if missing:
            assert 'Aborting' in out and 'incomplete' in out, out
        elif invalid:
            assert 'Aborting' in out and 'Cannot read the bundled version' in out, out
        else:
            assert 'v35.4 already installed' in out and 'Bundled is v35.6' in out, out
            assert 'hides' in out, out
        if pretend or skip or missing or invalid:
            assert {p: p.read_bytes() for p in target.rglob('*') if p.is_file()} == originals, out
            assert not list(mui.rglob('Libs/MUI/*.mc?')), out
        else:
            assert (target / 'Prefs/Zune').is_file(), out
            assert (target / 'Prefs/Zune.info').is_file(), out
            for name in CLASS_FILES:
                assert (mui / 'Libs/MUI' / name).is_file(), (name, out)
                assert (mui / 'Zune68/Created/Libs/MUI' / name).is_file(), (name, out)
            assert not old.exists(), out
            assert (libs / 'MUI/BetterString.mcc.zune68-old').read_bytes() == version_file(old.name, '35.4')
            assert len(list((target / 'Catalogs').rglob('*.catalog'))) == len(catalog_paths(package)), out
            assert old_icon.with_name('Zune.info.old').read_bytes() == b'original icon'
            assert old_catalog.with_name(old_catalog.name + '.old').read_bytes() == b'original catalog'
            assert (mui / 'Zune68/Replaced/Catalogs/german/BetterString_mcp.catalog').is_file()
            assert (mui / 'Zune68/Install.log').is_file()
            if set_back:
                write_script(1)
                out = run(installer, package, '', encoding='latin1')
                after = {p: p.read_bytes() for p in target.rglob('*') if p.is_file()}
                assert after == originals, out
                assert not list(mui.rglob('Libs/MUI/*.mc?')), out
                assert not [p for p in (mui / 'Zune68').rglob('*')
                            if p.is_file() and p.name != 'Install.log'], out
        print(f'Installer components: pretend={pretend}, skip={skip}, '
              f'missing={missing}, invalid={invalid}, set_back={set_back} passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installer', required=True, type=Path,
                        help='host InstallerLG executable built with UI=CLI')
    args = parser.parse_args()
    installer = args.installer.resolve()
    old = version_file(NAMES[0], '35.4')
    scenario(installer, 'fresh install', answers='Y\nY\n')
    scenario(installer, 'skip both', installed=old)
    scenario(installer, 'upgrade', installed=old, answers='Y\nN\n')
    scenario(installer, 'downgrade', installed=version_file(NAMES[0], '36.0'), answers='Y\nN\n')
    scenario(installer, 'same version', installed=version_file(NAMES[0], '35.6'), answers='Y\nN\n')
    scenario(installer, 'unknown version', installed=b'original unversioned data', answers='Y\nN\n')
    scenario(installer, 'retain backup', installed=old, backup=b'original backup', answers='Y\nN\n')
    scenario(installer, 'resident version', resident=40 * 65536 + 1)
    scenario(installer, 'pretend', installed=old, answers='Y\nY\n', pretend=True)
    scenario(installer, 'novice still asks', novice=True, answers='Y\nN\n')
    scenario(installer, 'missing payload', missing=True)
    scenario(installer, 'unversioned payload', invalid=True)
    scenario(installer, 'destination is directory', directory=True)
    set_back_scenario(installer)
    components_scenario(installer)
    components_scenario(installer, set_back=True)
    components_scenario(installer, skip=True)
    components_scenario(installer, pretend=True)
    components_scenario(installer, missing=True)
    components_scenario(installer, invalid=True)


if __name__ == '__main__':
    main()

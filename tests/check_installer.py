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
from components import CLASS_FILES, LANGUAGES, MUI_CLASSES, catalog_paths
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


def scenario(installer, label, *, installed=None, backup=None, pretend=False,
             resident=0, novice=False, missing=False, invalid=False,
             directory=False):
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

        if novice:
            # InstallerLG also honours (set @user-level 1); Commodore's
            # Installer 43 only changes the level through (user).
            text = (ROOT / 'dist/Install').read_text()
            assert '(if (< @user-level 1) (user 1))' in text
            assert '(set @user-level' not in text
        script = localize((ROOT / 'dist/Install').read_text(), root, target, mui,
                          resident=resident, pretend=pretend,
                          level=0 if novice else 2)
        (root / 'Install').write_text(script)
        # Components install without questions; nothing reads these answers.
        out = run(installer, root, 'N\n' * 4)
        assert all(len(p.name) <= 30 for p in target.iterdir()), 'OFS/FFS filename limit'
        protocol = mui / 'Zune68/Install.log'
        if missing or invalid or directory:
            assert 'Aborting' in out, out
            assert not (target / NAMES[1]).exists()
            if not directory:
                assert not destination.exists()
            print(f'Installer: {label} passed')
            return
        for name in NAMES:
            original = installed if name == NAMES[0] else None
            expected = original if pretend else version_file(name, '35.6')
            assert (target / name).exists() == (expected is not None), out
            if expected is not None:
                assert (target / name).read_bytes() == expected, out
            marker = mui / 'Zune68/Created/Libs' / name
            assert marker.exists() == (not pretend and original is None), (name, out)
        expected_backup = backup
        if expected_backup is None and installed is not None and not pretend:
            expected_backup = installed
        assert backup_path.exists() == (expected_backup is not None), out
        if expected_backup is not None:
            assert backup_path.read_bytes() == expected_backup, out
        assert protocol.exists() == (not pretend), out
        if pretend:
            assert 'Pretend mode finished' in out, out
        else:
            log = protocol.read_text()
            assert f'Installed {target / NAMES[1]} v35.6\n' in log, log
            if installed is None:
                assert f'Installed {destination} v35.6\n' in log, log
            elif b'$VER:' in installed:
                old = installed.split(b' ')[2].decode()
                assert f'Installed {destination} v35.6, replacing v{old}' in log, log
            else:
                assert 'replacing a file of unknown version' in log, log
            assert ('Saved ' in log) == (installed is not None and backup is None), log
            assert ('v40.1 stays in memory until reboot' in log) == bool(resident), log
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
        run(installer, root, '')
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


def components_scenario(installer, *, pretend=False, missing=False,
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
        drawers = {p for p in target.rglob('*') if p.is_dir()}
        if missing:
            (package / 'Libs/MUI/TextEditor.mcp').unlink()
        if invalid:
            (package / 'Libs/MUI/TextEditor.mcp').write_bytes(b'no version')
        out = run(installer, package, '', encoding='latin1')
        if missing:
            assert 'Aborting' in out and 'incomplete' in out, out
        elif invalid:
            assert 'Aborting' in out and 'Cannot read the bundled version' in out, out
        if pretend or missing or invalid:
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
            log = (mui / 'Zune68/Install.log').read_text(encoding='latin1')
            assert 'v35.6, replacing v35.4' in log and 'it hid' in log, log
            # Written in parts: one header, and a line for every file.
            assert log.count('--- Zune68 ') == 1, log
            # Prefs/Zune.info is copied with Prefs/Zune and not listed.
            installed = len(NAMES) + 1 + len(CLASS_FILES) + len(catalog_paths(package))
            assert log.count('\nInstalled ') == installed, (log.count('\nInstalled '), installed)
            if set_back:
                write_script(1)
                out = run(installer, package, '', encoding='latin1')
                after = {p: p.read_bytes() for p in target.rglob('*') if p.is_file()}
                assert after == originals, out
                left = {p for p in target.rglob('*') if p.is_dir()}
                assert left == drawers, sorted(map(str, left - drawers))
                log = (mui / 'Zune68/Install.log').read_text(encoding='latin1')
                assert log.count('--- Zune68 ') == 2 and log.endswith('\n'), log
                assert not list(mui.rglob('Libs/MUI/*.mc?')), out
                assert not [p for p in (mui / 'Zune68').rglob('*')
                            if p.is_file() and p.name != 'Install.log'], out
        print(f'Installer components: pretend={pretend}, '
              f'missing={missing}, invalid={invalid}, set_back={set_back} passed')


NEWER = ('NList.mcc', 'TextEditor.mcc', 'Rawimage.mcc')


def newer_scenario(installer, answers, kept):
    """Newer copies of two third-party classes and of one MUI class."""
    assert 'Rawimage.mcc' in MUI_CLASSES and 'NList.mcc' not in MUI_CLASSES
    with tempfile.TemporaryDirectory(prefix='zune68-newer-') as temp:
        root = Path(temp)
        package, target, mui = root / 'package', root / 'target', root / 'mui'
        package.mkdir()
        target.mkdir()
        classes = mui / 'Libs/MUI'
        classes.mkdir(parents=True)
        libs = target / 'Libs'
        libs.mkdir()
        files = ['Prefs/Zune'] + ['Libs/MUI/' + name for name in CLASS_FILES]
        for relative in ['Libs/' + name for name in NAMES] + files:
            path = package / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(version_file(path.name, '35.6'))
        for name in NEWER:
            (classes / name).write_bytes(version_file(name, '36.0'))
        script = localize(install_script(files), root, libs, mui)
        script = script.replace('"SYS:Prefs"', f'"{target}/Prefs"')
        (package / 'Install').write_text(script)
        out = run(installer, package, answers)
        questions = out.count('It is newer than the bundled v35.6.')
        assert questions == len(answers.splitlines()), out
        assert 'Rawimage.mcc v36.0 is installed' not in out, out
        log = (mui / 'Zune68/Install.log').read_text()
        for name in NEWER:
            installed = (classes / name).read_bytes()
            backup = classes / (name + '.zune68-old')
            if name in kept:
                assert installed == version_file(name, '36.0'), (name, out)
                assert not backup.exists(), (name, out)
                assert f'Kept {classes / name} v36.0, newer than the bundled v35.6' in log, log
            else:
                assert installed == version_file(name, '35.6'), (name, out)
                assert backup.read_bytes() == version_file(name, '36.0'), (name, out)
                assert f'v35.6, replacing v36.0' in log, log
        for name in CLASS_FILES:
            if name not in NEWER:
                assert (classes / name).read_bytes() == version_file(name, '35.6'), name
        print(f'Installer newer classes: answers={answers!r} passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installer', required=True, type=Path,
                        help='host InstallerLG executable built with UI=CLI')
    args = parser.parse_args()
    installer = args.installer.resolve()
    old = version_file(NAMES[0], '35.4')
    scenario(installer, 'fresh install')
    scenario(installer, 'upgrade', installed=old)
    scenario(installer, 'newer installed', installed=version_file(NAMES[0], '36.0'))
    scenario(installer, 'same version', installed=version_file(NAMES[0], '35.6'))
    scenario(installer, 'unknown version', installed=b'original unversioned data')
    scenario(installer, 'retain backup', installed=old, backup=b'original backup')
    scenario(installer, 'resident version', resident=40 * 65536 + 1)
    scenario(installer, 'pretend', installed=old, pretend=True)
    scenario(installer, 'novice installs', novice=True)
    scenario(installer, 'missing payload', missing=True)
    scenario(installer, 'unversioned payload', invalid=True)
    scenario(installer, 'destination is directory', directory=True)
    set_back_scenario(installer)
    components_scenario(installer)
    components_scenario(installer, set_back=True)
    components_scenario(installer, pretend=True)
    components_scenario(installer, missing=True)
    components_scenario(installer, invalid=True)
    newer_scenario(installer, '0\n1\n', kept={'NList.mcc'})
    newer_scenario(installer, '\n\n', kept={'NList.mcc', 'TextEditor.mcc'})
    newer_scenario(installer, '2\n', kept={'NList.mcc', 'TextEditor.mcc'})
    newer_scenario(installer, '3\n', kept=set())


if __name__ == '__main__':
    main()

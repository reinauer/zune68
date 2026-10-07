#!/usr/bin/env python3
"""Exercise release input failures and archive replacement on the host."""
import importlib.util
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release', ROOT / 'tools/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
from prepare_aminet import prepare


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='zune68-release-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'objects with spaces'
        self.output = self.root / 'release with spaces'
        self.build.mkdir()
        self.output.mkdir()
        for name in release.LIBRARIES:
            self.library(name, '35.6')
        self.library('opentest', '1.0')

    def library(self, name, version):
        code = b'\x70\x00\x4e\x75' + f'$VER: {name} {version} (test)\0'.encode()
        code += b'\0' * (-len(code) % 4)
        symbol = b'fixture_symbol\0\0'
        words = len(code) // 4
        header = struct.pack('>8I', 1011, 0, 1, 0, 0, words, 1001, words)
        symbols = struct.pack('>2I', 1008, len(symbol) // 4) + symbol
        (self.build / name).write_bytes(header + code + symbols +
                                       struct.pack('>3I', 0, 0, 1010))

    def test_mismatched_versions(self):
        self.library('muimaster.library', '35.4')
        with self.assertRaisesRegex(ValueError, 'versions differ'):
            release.release(self.build, self.output, 'lha', full=False)
        self.assertEqual(list(self.output.iterdir()), [])

    def test_missing_version(self):
        (self.build / 'muimaster.library').write_bytes(struct.pack('>I', 1011))
        with self.assertRaisesRegex(ValueError, 'matching'):
            release.release(self.build, self.output, 'lha', full=False)

    def test_wrong_library_name(self):
        shutil.copyfile(self.build / 'muimaster.library',
                        self.build / 'zunemaster.library')
        with self.assertRaisesRegex(ValueError, 'matching'):
            release.release(self.build, self.output, 'lha', full=False)

    def test_missing_tool(self):
        with self.assertRaisesRegex(ValueError, 'archiver not found'):
            release.release(self.build, self.output, str(self.root / 'absent'), full=False)

    def test_missing_strip_tool(self):
        with self.assertRaisesRegex(ValueError, 'strip tool not found'):
            release.release(self.build, self.output, 'lha', full=False,
                            strip=str(self.root / 'absent'))
        self.assertEqual(list(self.output.iterdir()), [])

    def test_package_strips_copies_and_keeps_versions(self):
        originals = {p.name: p.read_bytes() for p in self.build.iterdir()}
        archive = release.release(self.build, self.output, 'lha', full=False)
        for name, original in originals.items():
            member = 'Zune68/' + ('Tests/' if name == 'opentest' else 'Libs/') + name
            data = subprocess.check_output(['lha', 'pq', str(archive), member])
            self.assertNotIn(b'fixture_symbol', data)
            self.assertIn(b'$VER: ' + name.encode(), data)
            self.assertEqual((self.build / name).read_bytes(), original)
        icon = subprocess.check_output(['lha', 'pq', str(archive), 'Zune68/Libs.info'])
        self.assertEqual(icon, (ROOT / 'dist/Libs.info').read_bytes())

    def test_strip_failure_preserves_previous_release(self):
        archive = self.output / 'Zune68-35.6-amigaos3-m68k.lha'
        archive.write_bytes(b'previous release')
        with self.assertRaises(subprocess.CalledProcessError):
            release.release(self.build, self.output, 'lha', full=False,
                            strip='/usr/bin/false')
        self.assertEqual(archive.read_bytes(), b'previous release')
        self.assertEqual(list(self.output.iterdir()), [archive])

    def test_debug_package_keeps_symbols(self):
        archive = release.release(self.build, self.output, 'lha', full=False,
                                  strip=None)
        data = subprocess.check_output(['lha', 'pq', str(archive),
                                        'Zune68/Tests/opentest'])
        self.assertEqual(data, (self.build / 'opentest').read_bytes())

    def test_incomplete_full_package(self):
        for name in ['SDK/include/example.h', 'Docs/COPYING']:
            path = self.build / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture')
        for name in ['Prefs/Zune', 'Prefs/Zune.info']:
            with self.assertRaisesRegex(ValueError, 'missing ' + name):
                release.release(self.build, self.output, 'lha')
            path = self.build / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture')
        with self.assertRaisesRegex(ValueError, 'missing Libs/MUI/BetterString.mcc'):
            release.release(self.build, self.output, 'lha')
        self.assertEqual(list(self.output.iterdir()), [])

    def test_archiver_failure_preserves_previous_release(self):
        archive = self.output / 'Zune68-35.6-amigaos3-m68k.lha'
        archive.write_bytes(b'previous release')
        with self.assertRaises(release.subprocess.CalledProcessError):
            release.release(self.build, self.output, '/usr/bin/false', full=False)
        self.assertEqual(archive.read_bytes(), b'previous release')
        self.assertEqual(list(self.output.iterdir()), [archive])

    def test_aminet_pair_and_version_guard(self):
        archive = self.output / 'Zune68-35.6-amigaos3-m68k.lha'
        archive.write_bytes(b'archive fixture')
        readme = archive.with_suffix('.readme')
        readme.write_text('Short: Zune68\nType: util/libs\nVersion: 35.6\n\nBody\n')
        destination = self.root / 'aminet'
        prepare(self.output, destination, 'v35.6')
        self.assertEqual((destination / 'Zune68.lha').read_bytes(), archive.read_bytes())
        self.assertEqual((destination / 'Zune68.readme').read_bytes(), readme.read_bytes())
        original = (destination / 'Zune68.readme').read_bytes()
        readme.write_text('Type: util/libs\nVersion: 35.5\n')
        with self.assertRaisesRegex(ValueError, 'version differ'):
            prepare(self.output, destination, 'v35.6')
        self.assertEqual((destination / 'Zune68.readme').read_bytes(), original)
        with self.assertRaisesRegex(ValueError, 'version tag'):
            prepare(self.output, destination, '../v35.6')


if __name__ == '__main__':
    unittest.main()

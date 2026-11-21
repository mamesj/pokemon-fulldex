from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GbaCodeTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_ruby_and_sapphire_are_games_not_hacks(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'gba_codes'
            subprocess.run(['c++', '-std=c++17', '-O1', '-pthread',
                            '-I' + str(ROOT / 'frontend'),
                            '-I' + str(ROOT / 'gba' / 'frontend'),
                            str(ROOT / 'tests/test_gba_codes.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DiagnosticsTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_report_has_fields_tail_and_saved_file(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'diagnostics'
            subprocess.run(['c++', '-std=c++17', '-O1',
                            '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_diagnostics.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe), folder], check=True)


if __name__ == '__main__':
    unittest.main()

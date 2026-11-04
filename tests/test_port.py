"""ROM-free portability regressions. Run: python3 -m unittest discover -s tests"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'tools'))
import recomp


class AssemblyFormatTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('clang') and sys.platform != 'win32', 'cross assembler needs clang')
    def test_macho_elf_and_coff_assemble_same_generated_file(self):
        with tempfile.TemporaryDirectory(prefix='ssrecomp object formats ') as tmp:
            dest = Path(tmp)
            sections = [recomp.Section('one', 0, b'\x01\x02\xff'), recomp.Section('two', 16, b'\x04\x05')]
            recomp.write_images(tmp, sections)
            for target in ('arm64-apple-macos11', 'x86_64-apple-macos11',
                           'x86_64-unknown-linux-gnu', 'x86_64-w64-windows-gnu'):
                subprocess.run(['clang', '-target', target, '-c', str(dest / 'rc_images.S'),
                                '-Wa,-I' + tmp, '-o', str(dest / (target + '.o'))], check=True,
                               capture_output=True)


if __name__ == '__main__':
    unittest.main()

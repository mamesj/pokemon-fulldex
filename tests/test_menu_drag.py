from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MenuDragTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_swipes_do_not_activate_settings(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'menu_drag'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            str(ROOT / 'tests/test_menu_drag.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

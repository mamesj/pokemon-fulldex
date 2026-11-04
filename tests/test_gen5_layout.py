from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Gen5LayoutTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'), 'C++ and SDL2 required')
    def test_gen5_layout(self):
        cflags = subprocess.check_output(['sdl2-config', '--cflags']).decode().split()
        libs = subprocess.check_output(['sdl2-config', '--libs']).decode().split()
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'test_gen5'
            subprocess.run(
                ['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'), str(ROOT / 'tests/test_gen5_layout.cpp'), '-o', str(exe)] + cflags + libs,
                check=True
            )
            subprocess.run([str(exe)], env={'SDL_VIDEODRIVER': 'dummy'}, check=True)


if __name__ == '__main__':
    unittest.main()

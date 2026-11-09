from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AudioFastForwardTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_compressed_batches(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'audio_fast_forward'
            subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(ROOT / 'tests/test_audio_fast_forward.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

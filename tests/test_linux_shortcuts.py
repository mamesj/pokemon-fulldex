from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
ROOT = Path(__file__).resolve().parents[1]
class LinuxShortcutTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('c++') and shutil.which('gio'), 'Linux C++ and GIO required')
    def test_shortcuts_launch_exact_rom(self):
        with tempfile.TemporaryDirectory(prefix="FullDex's shortcuts ") as folder:
            home=Path(folder); (home/'Desktop').mkdir()
            exe=home/'FullDex %f $test'
            subprocess.run(['c++','-std=c++17','-I'+str(ROOT/'frontend'),str(ROOT/'tests/test_linux_shortcuts.cpp'),str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(exe)],check=True)
            result=home/'arguments'
            env=dict(os.environ,HOME=str(home),XDG_DATA_HOME=str(home/'data'),FULLDEX_SHORTCUT_ARGS=str(result))
            subprocess.run([str(exe)],env=env,check=True)
            entries=sorted((home/'data/applications').glob('*.desktop'))
            self.assertEqual(len(entries),3)
            self.assertEqual(len(list((home/'Desktop').glob('*.desktop'))),3)
            seen=[]
            for entry in entries:
                if result.exists(): result.unlink()
                subprocess.run(['gio','launch',str(entry)],env=env,check=True)
                for _ in range(100):
                    if result.exists(): break
                    time.sleep(.02)
                args=result.read_text().splitlines()
                self.assertEqual(args[0],'--rom')
                seen.append(args[1])
                self.assertIn('Icon=',entry.read_text())
            expected=[str(home)+'/ROMs/Pokemon %f $cash `tick` \\"quoted\\".nds',str(home)+'/Other/Pokemon %f $cash `tick` \\"quoted\\".nds',str(home)+'/GBA/Emerald.gba']
            self.assertCountEqual(seen,expected)

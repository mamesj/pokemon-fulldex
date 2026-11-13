from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AndroidSaveFolderTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ required')
    def test_first_folder_and_existing_progress(self):
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            source = temp / 'check.cpp'
            source.write_text(r'''
#include "android_save_folder.h"
#include <cassert>
#include <fstream>
int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    using AndroidSaveFolder::Established;
    const fs::path root = fs::path(argv[1]) / "saves";
    const auto dir = root.string();
    assert(!Established("", ""));
    assert(!Established("", dir));
    fs::create_directory(root);
    assert(!Established("", dir));
    std::ofstream(root / "rom-check.txt") << "cache";
    std::ofstream(root / "gba-rom-list-123.txt") << "cache";
    assert(!Established("", dir));
    assert(Established("/previous/roms", dir));
    for (const char* name : {"soulsilver.sav", "EMERALD.SAV", "game.srm", "game.dsv",
                             "game.state", "game.st", "game.sbin", "game.sav.prelaunch.bak"}) {
        std::ofstream(root / name) << "existing progress";
        assert(Established("", dir));
        fs::remove(root / name);
    }
    fs::create_directory(root / "backups");
    std::ofstream(root / "backups" / "soulsilver-20261113.sav") << "backup";
    assert(Established("", dir));
}
''', encoding='utf-8')
            exe = temp / 'check'
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'),
                            str(source), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(temp)], check=True)

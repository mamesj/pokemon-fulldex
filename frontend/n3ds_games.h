#pragma once
// The 3DS games in the all-games list (launcher.h): the ROMs (.3ds, .cci, .cxi, .cia) found in the
// roms folder and the folders in it. Dekopon (the Switch's Azahar) plays them: its core is built into
// the program (SSR_DEKOPON on the Switch, SSR_DEKOPON_PC on a PC; tools/dekopon/), or, on the Switch
// without it, dekopon.nro is started with the ROM.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "launcher.h"

#ifdef SSR_DEKOPON
// Dekopon's entry (src/citra_switch/citra_switch.cpp, built with DEKOPON_EMBED): plays argv[1].
int DekoponMain(int argc, char* argv[]);
#endif

namespace N3dsGames
{

// Dekopon travels with this program: dekopon.nro in its folder (the all-games package has it),
// else the standalone install. Its settings and saves stay in /switch/dekopon either way.
inline std::string& OwnFolder() { static std::string f; return f; }
inline std::string DekoponPath()
{
    struct stat st;
    const std::string own = OwnFolder() + "/dekopon.nro";
    return stat(own.c_str(), &st) == 0 ? own : "sdmc:/switch/dekopon/dekopon.nro";
}

inline bool IsRom(const std::string& n)
{
    const size_t dot = n.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string e = n.substr(dot + 1);
    for (char& c : e) c = (char)tolower((unsigned char)c);
    return e == "3ds" || e == "cci" || e == "cxi" || e == "cia";
}

// The product code's four letters (ECRA: Omega Ruby) of an NCSD (.3ds, .cci) or NCCH (.cxi) file, or "".
inline std::string ProductCode(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    unsigned char h[0x200] = {};
    std::string code;
    auto at = [&](long off) { return fseek(f, off, SEEK_SET) == 0 && fread(h, 1, sizeof h, f) == sizeof h; };
    long ncch = -1;
    if (at(0) && !memcmp(h + 0x100, "NCSD", 4)) ncch = (long)(h[0x120] | h[0x121] << 8 | h[0x122] << 16 | (unsigned long)h[0x123] << 24) * 0x200;
    else if (!memcmp(h + 0x100, "NCCH", 4)) ncch = 0;
    if (ncch >= 0 && at(ncch) && !memcmp(h + 0x100, "NCCH", 4) && !memcmp(h + 0x150, "CTR-", 4))
        code.assign((const char*)h + 0x156, 4); // ("CTR-P-ECRA": the game's letters)
    fclose(f);
    for (char c : code)
        if (!isalnum((unsigned char)c)) return "";
    return code;
}

// The title the list shows for a product code (its first three letters; the fourth is the region).
inline const char* TitleOf(const std::string& code)
{
    static const struct { const char* three; const char* name; } titles[] = {
        { "ECR", "POKEMON OMEGA RUBY" }, { "ECL", "POKEMON ALPHA SAPPHIRE" }, { "EKJ", "POKEMON X" }, { "EK2", "POKEMON Y" },
        { "BND", "POKEMON SUN" }, { "BNE", "POKEMON MOON" } };
    for (const auto& t : titles)
        if (code.compare(0, 3, t.three) == 0) return t.name;
    return nullptr;
}

// Adds the 3DS ROMs in dir and the folders in it (two levels down; not the saves folders).
inline void Add(const std::string& dir, std::vector<Launcher::Game>& games, int depth = 2)
{
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    std::vector<std::string> names, subs;
    while (dirent* e = readdir(d))
    {
        if (e->d_name[0] == '.') continue;
        struct stat st;
        if (IsRom(e->d_name)) names.push_back(e->d_name);
        else if (depth > 1 && strcmp(e->d_name, "saves") && strcmp(e->d_name, "backups") && stat((dir + "/" + e->d_name).c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            subs.push_back(e->d_name);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    for (const std::string& n : names)
    {
        Launcher::Game g;
        g.platform = Launcher::N3DS;
        g.rom = dir + "/" + n;
        g.folder = dir;
        const std::string code = ProductCode(g.rom);
        g.code = code.empty() ? "3DS-" + n : code;
        const char* title = TitleOf(code);
        g.name = title ? title : n.substr(0, n.find_last_of('.'));
        if (!title)
            for (char& c : g.name) c = (char)toupper((unsigned char)c);
        g.known = true;
        games.push_back(std::move(g));
    }
    std::sort(subs.begin(), subs.end());
    for (const std::string& sub : subs) Add(dir + "/" + sub, games, depth - 1);
}

inline void AddAll(const std::string& appDir, std::vector<Launcher::Game>& games)
{
    OwnFolder() = appDir;
    for (const std::string& d : { appDir + "/roms", std::string("sdmc:/switch/dekopon/roms") })
        Add(d, games);
}

// Whether Dekopon is installed: a 3DS game plays only then.
inline bool Installed()
{
    struct stat st;
    return stat(DekoponPath().c_str(), &st) == 0;
}

}

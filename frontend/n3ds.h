// Pokemon X, Y, Omega Ruby, Alpha Sapphire, Ultra Sun and Ultra Moon (3DS) in FullDex's game list.
//
// The games are the omega-nx programs, each in a folder of its own with its saves and settings:
// FullDex lists them and starts them, and they come back to the list when the player quits.
//   Switch: the folders the FullDex Builder makes (omega-nx, pokemon-alpha-sapphire) in /switch, each
//     with a .nro, the player's game.3ds and titles/<title id>. The game is started as FullDex ends,
//     told where FullDex is (FULLDEX_RETURN), and starts FullDex again when it quits from its menu.
//   Windows: the program is in the download's 3ds\program folder. A dump (.3ds) found with the ROMs
//     gets a folder of its own, 3ds\<title id>, a copy of the program, where the game's code is built
//     once from the dump ("Pokemon ORAS.exe" --prepare, 5 to 15 minutes) and the game then plays.
//   Linux (x86_64): the same, with the Linux program (pokemon-oras, title.so) in 3ds/program beside
//     FullDex's own program (inside the AppImage too); each game's folder in FullDex's data folder.
#pragma once
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cctype>
#include <ctime>
#include <cstdint>
#include <set>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <sys/stat.h>
#ifdef __ANDROID__
#include <sys/statvfs.h>
#endif
#include <dirent.h>
#ifdef _WIN32
#include <windows.h>
#include "windows_runtime_log.h"
#endif
#if defined(__linux__) && !defined(__ANDROID__) && !defined(__SWITCH__)
#include <sys/wait.h>
#include <unistd.h>
#include <climits>
#endif
#include "launcher.h"
#include "runtime_kit.h"
#include "runtime_diagnostics.h"
#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__) && !defined(__SWITCH__))
#define SSR_N3DS_PC 1 // (the 3DS games' own program beside FullDex: Windows and Linux)
#endif

namespace N3ds
{
struct Title { const char* code; const char* id; const char* name; };
inline const Title kTitles[] = {
    { "EKJA", "0004000000055D00", "POKEMON X" },
    { "EK2A", "0004000000055E00", "POKEMON Y" },
    { "ECRA", "000400000011C400", "POKEMON OMEGA RUBY" },
    { "ECLA", "000400000011C500", "POKEMON ALPHA SAPPHIRE" },
    { "A2AA", "00040000001B5000", "POKEMON ULTRA SUN" },
    { "A2BA", "00040000001B5100", "POKEMON ULTRA MOON" },
};
inline const Title* ById(const std::string& id)
{
    for (const Title& t : kTitles)
        if (id == t.id) return &t;
    return nullptr;
}
inline std::string IdOf(const Launcher::Game& g)
{
    for (const Title& t : kTitles)
        if (g.code == t.code) return t.id;
    return "";
}

inline bool IsDir(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
inline bool IsFile(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode); }
inline bool EndsWith(const std::string& s, const char* e)
{
    const size_t n = strlen(e);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[s.size() - n + i]) != e[i]) return false;
    return true;
}

// The title id of a .3ds dump (8 bytes at 0x108, little-endian), as 16 capital hex digits; "" if unreadable.
inline std::string DumpTitle(const std::string& path)
{
#ifdef _WIN32
    FILE* f = _wfopen(std::filesystem::u8path(path).wstring().c_str(), L"rb");
#else
    FILE* f = fopen(path.c_str(), "rb");
#endif
    if (!f) return "";
    unsigned char b[8] = {};
    const bool ok = fseek(f, 0x108, SEEK_SET) == 0 && fread(b, 1, 8, f) == 8;
    fclose(f);
    if (!ok) return "";
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = v << 8 | b[i];
    char hex[17];
    snprintf(hex, sizeof hex, "%016llX", (unsigned long long)v);
    return hex;
}

// Whether a dump is encrypted, as a console's cartridge is (#155): 1 encrypted, 0 not, -1 not a .3ds dump at
// all. The first partition's NCCH header has a flag for "no encryption" (byte 0x18F, bit 2), which tools
// that decrypt a dump (GodMode9) usually set. Some decrypted dumps leave it clear: their exheader (the bytes
// after the NCCH header) is plaintext, so the program ID in it is the one in the NCCH header. An encrypted
// one's exheader is ciphertext, and the two do not match.
inline int DumpCrypt(const std::string& path)
{
#ifdef _WIN32
    FILE* f = _wfopen(std::filesystem::u8path(path).wstring().c_str(), L"rb");
#else
    FILE* f = fopen(path.c_str(), "rb");
#endif
    if (!f) return -1;
    unsigned char h[0x200] = {}, n[0x200] = {}, ex[0x208] = {};
    bool ok = fread(h, 1, sizeof h, f) == sizeof h && memcmp(h + 0x100, "NCSD", 4) == 0;
    const long at = ok ? (long)((uint32_t)h[0x120] | (uint32_t)h[0x121] << 8 | (uint32_t)h[0x122] << 16 | (uint32_t)h[0x123] << 24) * 0x200 : 0;
    ok = ok && at > 0 && fseek(f, at, SEEK_SET) == 0 && fread(n, 1, sizeof n, f) == sizeof n && memcmp(n + 0x100, "NCCH", 4) == 0;
    // (The exheader of an NCCH marked encrypted, read to see whether it is plaintext after all.)
    const bool plain = ok && !(n[0x18F] & 4) &&
        fseek(f, at + 0x200, SEEK_SET) == 0 && fread(ex, 1, sizeof ex, f) == sizeof ex &&
        memcmp(ex + 0x200, n + 0x118, 8) == 0;
    fclose(f);
    if (!ok) return -1;
    return (n[0x18F] & 4) || plain ? 0 : 1;
}

// Some decrypted dumps do not carry the NCCH "no encryption" marker, and the games' own program
// (Zakuro) asks for it. When the exheader proves the content is plaintext (its program ID is the
// NCCH header's, as in DumpCrypt), set the marker on the player's own dump: one byte, only when a
// game is about to be made or played. It is the marker GodMode9's decrypted dumps carry, and it
// is what the launcher's own check reads. Returns true when it wrote the marker.
inline bool MarkDecrypted(const std::string& path)
{
#ifdef _WIN32
    FILE* f = _wfopen(std::filesystem::u8path(path).wstring().c_str(), L"r+b");
#else
    FILE* f = fopen(path.c_str(), "r+b");
#endif
    if (!f) return false;
    unsigned char h[0x200] = {}, n[0x200] = {}, ex[0x208] = {};
    bool ok = fread(h, 1, sizeof h, f) == sizeof h && memcmp(h + 0x100, "NCSD", 4) == 0;
    const long at = ok ? (long)((uint32_t)h[0x120] | (uint32_t)h[0x121] << 8 | (uint32_t)h[0x122] << 16 | (uint32_t)h[0x123] << 24) * 0x200 : 0;
    ok = ok && at > 0 && fseek(f, at, SEEK_SET) == 0 && fread(n, 1, sizeof n, f) == sizeof n && memcmp(n + 0x100, "NCCH", 4) == 0;
    // (Only an unmarked dump whose exheader is plaintext: this never writes to an encrypted one.)
    if (ok && !(n[0x18F] & 4))
        ok = fseek(f, at + 0x200, SEEK_SET) == 0 && fread(ex, 1, sizeof ex, f) == sizeof ex &&
             memcmp(ex + 0x200, n + 0x118, 8) == 0;
    else
        ok = false;
    if (ok)
    {
        const unsigned char marked = (unsigned char)(n[0x18F] | 4);
        ok = fseek(f, at + 0x18F, SEEK_SET) == 0 && fwrite(&marked, 1, 1, f) == 1 && fflush(f) == 0;
        if (ok) fprintf(stderr, "[3ds] marked %s as decrypted (NoCrypto set)\n", path.c_str());
    }
    fclose(f);
    return ok;
}
// Why a 3DS file with the ROMs is not in the list (#155), said once on the list: the one that matters most (an
// Omega Ruby or Alpha Sapphire that cannot be played before another game, or a file that is no dump).
inline std::string& SkipNote() { static std::string s; return s; }
inline int& SkipRank() { static int r = 0; return r; }
inline void Skip(const std::string& path, const std::string& why, int rank = 1)
{
    const std::string name = path.substr(path.find_last_of("/\\") + 1); // (as it is: the list's notes set the case)
    fprintf(stderr, "[3ds] not listed: %s: %s\n", path.c_str(), why.c_str());
    if (rank > SkipRank()) { SkipNote() = name + ": " + why; SkipRank() = rank; }
}
// (A 3DS file among the ROMs: its title if FullDex plays it, else nullptr and why not said.)
// (A file that starts as a CIA does: its header's size, 0x2020, then its type and version, 0.)
inline bool LooksLikeCia(const std::string& path)
{
#ifdef _WIN32
    FILE* f = _wfopen(std::filesystem::u8path(path).wstring().c_str(), L"rb");
#else
    FILE* f = fopen(path.c_str(), "rb");
#endif
    if (!f) return false;
    unsigned char h[8] = {};
    const bool ok = fread(h, 1, sizeof h, f) == sizeof h;
    fclose(f);
    static const unsigned char cia[8] = { 0x20, 0x20, 0, 0, 0, 0, 0, 0 };
    return ok && memcmp(h, cia, sizeof cia) == 0;
}
// A cartridge dump named .cci is the same file as a .3ds: it is renamed to .3ds (the player was told
// to do this by hand before), unless a .3ds of that name is there already or the folder cannot be
// written. path becomes the new name. Returns false when it is left as it was.
inline bool RenameCci(std::string& path)
{
    if (!EndsWith(path, ".cci") || DumpCrypt(path) < 0) return false;
    const std::string to = path.substr(0, path.size() - 4) + ".3ds";
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::u8path(to), ec)) return false;
    std::filesystem::rename(std::filesystem::u8path(path), std::filesystem::u8path(to), ec);
    if (ec) { fprintf(stderr, "[3ds] could not rename %s to .3ds: %s\n", path.c_str(), ec.message().c_str()); return false; }
    fprintf(stderr, "[3ds] renamed %s to .3ds\n", path.c_str());
    path = to;
    return true;
}
// (path: a .cci dump is renamed to .3ds first, and path is then its new name; not with rename false,
// for a file FullDex was opened with, which a shortcut may name.)
inline const Title* Check(std::string& path, bool rename = true)
{
    static const char* const kCia = "A .CIA IS NOT PLAYED: THE 3DS GAMES NEED A .3DS DUMP OF THE CARTRIDGE";
    if (EndsWith(path, ".cia")) { Skip(path, kCia, 2); return nullptr; }
    if (EndsWith(path, ".cci") && !(rename && RenameCci(path))) { Skip(path, DumpCrypt(path) < 0 && LooksLikeCia(path) ? kCia : "RENAME IT TO .3DS", 2); return nullptr; }
    if (!EndsWith(path, ".3ds")) return nullptr;
    const int crypt = DumpCrypt(path);
    if (crypt < 0) { Skip(path, LooksLikeCia(path) ? kCia : "NOT A 3DS CARTRIDGE DUMP", LooksLikeCia(path) ? 2 : 1); return nullptr; }
    const Title* t = ById(DumpTitle(path));
    if (!t) { Skip(path, "NOT A 3DS GAME FULLDEX PLAYS (X, Y, OMEGA RUBY, ALPHA SAPPHIRE, ULTRA SUN, ULTRA MOON)"); return nullptr; }
    if (crypt == 1) { Skip(path, "ENCRYPTED: DECRYPT IT FIRST (GODMODE9 ON YOUR 3DS)", 3); return nullptr; }
    return t;
}

inline Launcher::Game Make(const Title& t, const std::string& rom, const std::string& folder)
{
    Launcher::Game g;
    g.platform = Launcher::N3DS;
    g.code = t.code;
    g.rom = rom;
    g.folder = folder;
    g.name = t.name;
    g.known = true;
    return g;
}

// The save. The game keeps it in its own folder (data/user/savedata/<title id>/main, 472 KB); FullDex keeps
// a copy with the other games' saves (omegaruby.sav, alphasapphire.sav), which Manage Saves, a saves folder
// of the player's own and cloud saves use. SyncSave makes the two the same: whichever was written last
// wins (a game played, or a save brought in by Import Save or cloud saves), the one it replaces backed up first.
// (beforePlay: a backup of the save the game is about to start with, as the DS games make.)
// (X and Y: 0x65600 bytes; Omega Ruby and Alpha Sapphire: 0x76000; Ultra Sun and Ultra Moon: 0x6CC00.)
inline bool SaveSizeOk(uintmax_t n) { return n == 0x65600 || n == 0x76000 || n == 0x6CC00; }
inline std::string SaveName(const std::string& code)
{
    return code == "EKJA" ? "pokemonx.sav" : code == "EK2A" ? "pokemony.sav" : code == "ECLA" ? "alphasapphire.sav" : code == "A2AA" ? "ultrasun.sav" : code == "A2BA" ? "ultramoon.sav" : "omegaruby.sav";
}
// The game's folder in /switch on a Switch (the FullDex Builder's), and its program's name.
inline std::string SwitchFolder(const std::string& code)
{
    return code == "EKJA" ? "pokemon-x" : code == "EK2A" ? "pokemon-y" : code == "ECLA" ? "pokemon-alpha-sapphire" : code == "A2AA" ? "pokemon-ultra-sun" : code == "A2BA" ? "pokemon-ultra-moon" : "omega-nx";
}
// (The dump's size, as the notes say it: X, Y, Omega Ruby and Alpha Sapphire 2 GB, Ultra Sun and Ultra Moon 4 GB.)
inline std::string DumpGb(const Launcher::Game& g) { return g.code == "A2AA" || g.code == "A2BA" ? "4" : "2"; }
inline std::string ProgramName(const std::string& code)
{
    return code == "EKJA" ? "Pokemon X" : code == "EK2A" ? "Pokemon Y" : code == "ECLA" ? "Pokemon Alpha Sapphire" : code == "A2AA" ? "Pokemon Ultra Sun" : code == "A2BA" ? "Pokemon Ultra Moon" : "Pokemon Omega Ruby";
}
inline std::string GameSave(const Launcher::Game& g) { return g.folder + "/data/user/savedata/" + IdOf(g) + "/main"; }
inline bool ReadSave(const std::string& p, std::vector<unsigned char>& d)
{
    std::error_code ec;
    const std::filesystem::path f = std::filesystem::u8path(p);
    if (!std::filesystem::is_regular_file(f, ec) || !SaveSizeOk(std::filesystem::file_size(f, ec)) || ec) return false;
    const size_t size = (size_t)std::filesystem::file_size(f, ec);
#ifdef _WIN32
    FILE* h = _wfopen(f.wstring().c_str(), L"rb");
#else
    FILE* h = fopen(p.c_str(), "rb");
#endif
    if (!h) return false;
    d.resize(size);
    const bool ok = fread(d.data(), 1, size, h) == size;
    fclose(h);
    return ok;
}
// A copy of a save in the backups folder beside the saves folder's copy (backups/omegaruby-20261116-112000.sav,
// as the DS games' are, so Restore A Backup lists it), unless the newest is the same; the newest 10 kept.
inline void Backup(const std::string& copy, const std::vector<unsigned char>& d)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = fs::u8path(copy).parent_path() / "backups";
    const std::string stem = fs::u8path(copy).stem().u8string();
    fs::create_directories(dir, ec);
    std::vector<fs::path> old;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    {
        const std::string n = it->path().filename().u8string();
        if (n.size() == stem.size() + 20 && n.compare(0, stem.size() + 1, stem + "-") == 0 && EndsWith(n, ".sav")) old.push_back(it->path());
    }
    std::sort(old.begin(), old.end());
    std::vector<unsigned char> last;
    if (!old.empty() && ReadSave(old.back().u8string(), last) && last == d) return;
    const time_t now = time(nullptr);
    char when[32];
    strftime(when, sizeof when, "%Y%m%d-%H%M%S", localtime(&now));
    const fs::path b = dir / fs::u8path(stem + "-" + when + ".sav");
#ifdef _WIN32
    FILE* h = _wfopen(b.wstring().c_str(), L"wb");
#else
    FILE* h = fopen(b.u8string().c_str(), "wb");
#endif
    if (!h) return;
    const bool ok = fwrite(d.data(), 1, d.size(), h) == d.size();
    fclose(h);
    if (!ok) { fs::remove(b, ec); return; }
    old.push_back(b);
    while (old.size() > 10) { fs::remove(old.front(), ec); old.erase(old.begin()); }
}
// (from over to: written beside it first, then put in its place and given from's time.)
inline bool CopySave(const std::string& from, const std::vector<unsigned char>& d, const std::string& to)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path t = fs::u8path(to), tmp = fs::u8path(to + ".new");
    fs::create_directories(t.parent_path(), ec);
#ifdef _WIN32
    FILE* h = _wfopen(tmp.wstring().c_str(), L"wb");
#else
    FILE* h = fopen((to + ".new").c_str(), "wb");
#endif
    if (!h) return false;
    const bool ok = fwrite(d.data(), 1, d.size(), h) == d.size();
    if (fclose(h) != 0 || !ok) { fs::remove(tmp, ec); return false; }
    ec.clear();
    fs::rename(tmp, t, ec);
    if (ec) { fs::remove(tmp, ec); return false; }
    const auto when = fs::last_write_time(fs::u8path(from), ec);
    if (!ec) fs::last_write_time(t, when, ec);
    return true;
}
// The game's program keeps the same copy itself as it plays (omega-nx savefile.rs: written when the game saves,
// taken in at its start when newer), in <FullDex>/saves unless OMEGA_NX_SAVE names the file: told the saves
// folder FullDex uses, so the two keep one copy, a saves folder of the player's own included.
// The game's menu in FullDex's language (omega-nx lang.rs): FULLDEX_LANG, en es it fr de ja ko.
inline const char* MenuLanguage() { return UiLang::Codes[UiLang::lang >= 0 && UiLang::lang < 7 ? UiLang::lang : 0]; }
inline void TellSave(const std::string& copy)
{
#ifdef _WIN32
    _wputenv_s(L"OMEGA_NX_SAVE", std::filesystem::u8path(copy).wstring().c_str());
    _putenv_s("FULLDEX_LANG", MenuLanguage());
#else
    setenv("OMEGA_NX_SAVE", copy.c_str(), 1);
    setenv("FULLDEX_LANG", MenuLanguage(), 1);
#endif
}
inline void SyncSave(const Launcher::Game& g, const std::string& copy, bool beforePlay = false)
{
    namespace fs = std::filesystem;
    if (g.platform != Launcher::N3DS || g.comingSoon || copy.empty() || IdOf(g).empty()) return;
    if (beforePlay) TellSave(copy);
    const std::string game = GameSave(g);
    std::vector<unsigned char> a, b;
    const bool hasGame = ReadSave(game, a), hasCopy = ReadSave(copy, b);
    if (beforePlay && hasGame && hasCopy && a == b) Backup(copy, a);
    if ((!hasGame && !hasCopy) || (hasGame && hasCopy && a == b)) return;
    std::error_code ec;
    const auto tg = hasGame ? fs::last_write_time(fs::u8path(game), ec) : fs::file_time_type::min();
    const auto tc = hasCopy ? fs::last_write_time(fs::u8path(copy), ec) : fs::file_time_type::min();
    if (hasGame && (!hasCopy || tg >= tc))
    {
        if (hasCopy) Backup(copy, b);
        const bool ok = CopySave(game, a, copy);
        if (ok && beforePlay) Backup(copy, a);
        fprintf(stderr, "[3ds] %s: save copied to %s%s\n", g.name.c_str(), copy.c_str(), ok ? "" : " (FAILED)");
        return;
    }
    // (Into the game: its save folder made as the game would make it, the first time.)
    const std::string fmt = g.folder + "/data/user/savedata/" + IdOf(g) + ".format";
    if (!IsFile(fmt))
    {
        fs::create_directories(fs::u8path(g.folder + "/data/user/savedata/" + IdOf(g)), ec);
        static const unsigned char kFormat[16] = { 0, 0, 4, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
#ifdef _WIN32
        FILE* h = _wfopen(fs::u8path(fmt).wstring().c_str(), L"wb");
#else
        FILE* h = fopen(fmt.c_str(), "wb");
#endif
        if (h) { fwrite(kFormat, 1, sizeof kFormat, h); fclose(h); }
    }
    if (hasGame) Backup(copy, a);
    const bool ok = CopySave(copy, b, game);
    if (ok && beforePlay) Backup(copy, b);
    fprintf(stderr, "[3ds] %s: save from %s put in the game%s\n", g.name.c_str(), copy.c_str(), ok ? "" : " (FAILED)");
}

// Whether this download has the 3DS games' program (AddPc found it).
inline bool& ProgramHere() { static bool b = false; return b; }

// A real game takes the place of its COMING SOON tile; and once their day has come, the tiles of
// games this program cannot play here go (a platform without the 3DS programs).
inline void DropPlaceholders(std::vector<Launcher::Game>& games)
{
    // (The real games' codes first: the loop below moves the games it keeps out of the list.)
    std::set<std::string> realCodes;
    for (const Launcher::Game& o : games)
        if (!o.comingSoon && o.platform == Launcher::N3DS) realCodes.insert(o.code);
    std::vector<Launcher::Game> keep;
    for (Launcher::Game& g : games)
    {
        if (g.comingSoon && g.platform == Launcher::N3DS)
        {
            const bool real = realCodes.count(g.code) > 0;
            // (Mac, iPhone, iPad, and Linux without the program (ARM): still coming, so the tiles stay,
            // saying Coming Soon with no countdown.)
            bool stays = false;
#if defined(__APPLE__)
            stays = true;
#elif defined(__linux__) && !defined(__ANDROID__)
            stays = !ProgramHere();
#endif
            if (stays)
            {
                if (real) continue;
                g.soonAt = 0;
            }
            else if (real) continue;
            else if (g.soonAt <= (long long)std::time(nullptr)) g.addRom = true; // (no dump yet: Add ROM)
        }
        keep.push_back(std::move(g));
    }
    games.swap(keep);
}

// What the Switch says about a 3DS game it found as a dump but cannot build itself: Put On My Switch,
// on a Windows PC. (Its sentences apart, two spaces between, so each is looked up in the language.)
inline std::string SetupName(const std::string& name) { std::string n = Launcher::TitleCase(name); if (n.rfind("Pokemon ", 0) == 0) n = n.substr(8); return n; }
inline std::string SetupHowTo(const std::string& name)
{
    return "The Switch can't build 3DS games itself. FullDex on a Windows PC does it in 5 to 10 minutes, from your own dump."
           "  1. Put this SD card in your PC.  2. In FullDex on the PC, open " + name + "'s Game Options and choose Put On My Switch."
           "  3. Put the card back here: " + name + " is ready to play.";
}
// Switch: dumps (.3ds) of the games in the ROM folders that no folder of /switch plays yet: listed,
// needing the one-time setup on a PC (choosing one says how).
inline void AddSwitchDumps(const std::vector<std::string>& dirs, std::vector<Launcher::Game>& games)
{
    for (const std::string& dir : dirs)
    {
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        while (dirent* e = readdir(d))
        {
            if (e->d_name[0] == '.' || !(EndsWith(e->d_name, ".3ds") || EndsWith(e->d_name, ".cci"))) continue;
            std::string path = dir + "/" + e->d_name;
            const Title* t = Check(path);
            if (!t) continue;
            bool have = false;
            for (const Launcher::Game& o : games) have |= o.platform == Launcher::N3DS && !o.comingSoon && o.code == t->code;
            if (have) continue;
            Launcher::Game g = Make(*t, path, dir);
            g.needsSetup = true;
            games.push_back(g);
            fprintf(stderr, "[3ds] %s: a dump to set up on a PC (%s)\n", t->name, path.c_str());
        }
        closedir(d);
    }
}

// Switch: each folder in root with game.3ds, titles/<a title id> and a .nro. One tile a game: the
// FullDex Builder's own folder (omega-nx, pokemon-alpha-sapphire) when there, else the newest .nro
// (an older test folder of the same game is left out of the list).
inline void AddSwitch(std::vector<Launcher::Game>& games, const std::string& root = "sdmc:/switch")
{
    DIR* d = opendir(root.c_str());
    if (!d) return;
    std::vector<std::string> names;
    while (dirent* e = readdir(d))
        if (e->d_name[0] != '.') names.push_back(e->d_name);
    closedir(d);
    std::sort(names.begin(), names.end());
    for (const Title& t : kTitles)
    {
        const std::string builderFolder = SwitchFolder(t.code);
        std::string best, bestNro;
        long long bestTime = -1;
        bool bestIsBuilders = false;
        for (const std::string& name : names)
        {
            const std::string folder = root + "/" + name;
            if (!IsFile(folder + "/game.3ds") || !IsDir(folder + "/titles/" + t.id)) continue;
            std::string nro;
            long long when = -1;
            if (DIR* f = opendir(folder.c_str()))
            {
                while (dirent* e = readdir(f))
                {
                    if (e->d_name[0] == '.' || !EndsWith(e->d_name, ".nro")) continue;
                    struct stat st;
                    if (stat((folder + "/" + e->d_name).c_str(), &st) != 0) continue;
                    if ((long long)st.st_mtime > when) { when = (long long)st.st_mtime; nro = e->d_name; }
                }
                closedir(f);
            }
            if (nro.empty()) continue;
            fprintf(stderr, "[3ds] %s in %s (%s)\n", t.name, folder.c_str(), nro.c_str());
            const bool builders = name == builderFolder;
            if (bestIsBuilders && !builders) continue;
            if (builders || when > bestTime) { best = folder; bestNro = nro; bestTime = when; bestIsBuilders = builders; }
        }
        if (best.empty()) continue;
        bool dup = false;
        for (const Launcher::Game& o : games) dup |= o.rom == best + "/" + bestNro;
        if (!dup) games.push_back(Make(t, best + "/" + bestNro, best));
        fprintf(stderr, "[3ds] %s: listed from %s\n", t.name, best.c_str());
    }
}

#ifdef __ANDROID__
// Android, with the Omega Ruby kit in the app (GameActivity.threeDsAvailable): a game already added
// (<app files>/3ds/<title id>/game.3ds, the kit's own folder), else a dump in the ROM folders, which
// is copied there the first time it is chosen (GameActivity.threeDsAdd).
// (Android: the game code Put On My Phone made on a PC, <title id>.so, found in the ROM folders: by title id.)
inline std::map<std::string, std::string>& PhoneCodes() { static std::map<std::string, std::string> m; return m; }
inline void AddAndroid(const std::vector<std::string>& romDirs, const std::string& appDir, std::vector<Launcher::Game>& games)
{
    SkipNote().clear(); SkipRank() = 0;
    PhoneCodes().clear();
    for (const std::string& dir : romDirs)
        if (DIR* d = opendir(dir.c_str()))
        {
            while (dirent* e = readdir(d))
                for (const Title& t : kTitles)
                    if (!strcasecmp(e->d_name, (std::string(t.id) + ".so").c_str())) PhoneCodes()[t.id] = dir + "/" + e->d_name;
            closedir(d);
        }
    auto have = [&](const Title& t) {
        for (const Launcher::Game& o : games) if (o.platform == Launcher::N3DS && o.code == t.code) return true;
        return false;
    };
    for (const Title& t : kTitles)
    {
        const std::string folder = appDir + "/3ds/" + t.id;
        if (IsFile(folder + "/game.3ds")) games.push_back(Make(t, folder + "/game.3ds", folder));
    }
    for (const std::string& dir : romDirs)
    {
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        while (dirent* e = readdir(d))
        {
            std::string path = dir + "/" + e->d_name;
            if (!IsFile(path)) continue;
            const Title* t = Check(path);
            if (!t || have(*t)) continue;
            games.push_back(Make(*t, path, appDir + "/3ds/" + t->id));
            fprintf(stderr, "[3ds] %s: %s\n", t->name, path.c_str());
        }
        closedir(d);
    }
}
inline bool Added(const Launcher::Game& g) { return IsFile(g.folder + "/game.3ds"); }
// Whether the phone has room to copy the dump in (its size and 300 MB to spare); how much is
// needed, in GB, for the note when it has not.
inline bool RoomFor(const Launcher::Game& g, const std::string& appDir, double& needGb)
{
    struct stat st;
    if (stat(g.rom.c_str(), &st) != 0) return true; // (the copy itself will say what went wrong)
    struct statvfs vs;
    if (statvfs(appDir.c_str(), &vs) != 0) return true;
    const double need = (double)st.st_size + 300.0 * 1024 * 1024;
    needGb = need / (1024.0 * 1024 * 1024);
    return (double)vs.f_bavail * vs.f_frsize >= need;
}
#endif

#ifdef SSR_N3DS_PC
#ifdef _WIN32
inline std::wstring W(const std::string& s) { return std::filesystem::u8path(s).wstring(); }
inline std::string ProgramDir(const std::string& appDir) { return appDir + "/3ds/program"; }
inline std::string Exe(const std::string& folder) { return folder + "/Pokemon ORAS.exe"; }
inline const char* kTitleLib = "title.dll";
#else
inline std::filesystem::path W(const std::string& s) { return std::filesystem::path(s); }
// (Linux: the program comes with FullDex's own, beside it, which in the AppImage cannot be written to;
// the games' folders are in FullDex's data folder.)
inline std::string ProgramDir(const std::string&)
{
    char b[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", b, sizeof b - 1);
    if (n <= 0) return "3ds/program";
    b[n] = 0;
    return std::filesystem::path(b).parent_path().string() + "/3ds/program";
}
inline std::string Exe(const std::string& folder) { return folder + "/pokemon-oras"; }
inline const char* kTitleLib = "title.so";
#endif
inline bool HaveProgram(const std::string& appDir) { std::error_code ec; return std::filesystem::is_regular_file(W(Exe(ProgramDir(appDir))), ec); }
inline bool Prepared(const Launcher::Game& g) { std::error_code ec; return std::filesystem::is_regular_file(W(g.folder + "/" + kTitleLib), ec); }

// Windows and Linux: the dumps (.3ds) in the ROM folders, each its folder 3ds/<title id> in FullDex's folder.
inline void AddPc(const std::vector<std::string>& romDirs, const std::string& appDir, std::vector<Launcher::Game>& games)
{
    ProgramHere() = HaveProgram(appDir);
    SkipNote().clear(); SkipRank() = 0;
    for (const std::string& dir : romDirs)
    {
        std::error_code ec;
        for (std::filesystem::directory_iterator it(W(dir), ec), end; !ec && it != end; it.increment(ec))
        {
            if (!it->is_regular_file(ec)) continue;
            std::string path = it->path().u8string();
            const Title* t = Check(path);
            if (!t) continue;
            if (!ProgramHere())
            {
#if defined(_WIN32) || defined(__x86_64__)
                Skip(path, "THE 3DS FOLDER FROM THE FULLDEX DOWNLOAD IS MISSING BESIDE THE PROGRAM", 3);
#endif
                continue;
            }
            bool dup = false;
            for (const Launcher::Game& o : games) dup |= o.platform == Launcher::N3DS && o.code == t->code;
            if (dup) continue;
            games.push_back(Make(*t, path, appDir + "/3ds/" + t->id));
            fprintf(stderr, "[3ds] %s: %s\n", t->name, path.c_str());
        }
    }
}

// The game's folder made (a copy of the program, when it has none yet) and told where the dump is.
inline bool MakeFolder(const std::string& appDir, const Launcher::Game& g)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(W(g.folder), ec);
    if (ec)
    {
        fprintf(stderr, "[3ds] cannot make %s: %d %s\n", g.folder.c_str(), ec.value(), ec.message().c_str());
        return false;
    }
    const fs::path from = W(ProgramDir(appDir)), to = W(g.folder);
    // An executable alone is not a complete kit. Retry missing tools after an interrupted
    // copy or extraction, while preserving existing settings and prepared game files.
    // A prepared game still works without the master kit, as before.
    if (fs::is_directory(from, ec) || !fs::is_regular_file(W(Exe(g.folder)), ec))
    {
        if (!RuntimeKit::CopyMissing(from, to, ec))
        {
            fprintf(stderr, "[3ds] copying the program to %s from %s: %d %s\n",
                    g.folder.c_str(), ProgramDir(appDir).c_str(), ec.value(), ec.message().c_str());
            return false;
        }
    }
#ifdef _WIN32
    FILE* f = _wfopen(W(g.folder + "/rom.txt").c_str(), L"wb");
#else
    FILE* f = fopen((g.folder + "/rom.txt").c_str(), "wb");
#endif
    if (!f) { fprintf(stderr, "[3ds] cannot write %s/rom.txt\n", g.folder.c_str()); return false; }
    fprintf(f, "%s\n", g.rom.c_str());
    fclose(f);
    // (The game's program checks the dump's "no encryption" marker, and some decrypted dumps lack
    // it: set it here, when the game is about to be made or played, never during the list's scan.)
    MarkDecrypted(g.rom);
    return true;
}

// Making the game's code: one at a time, on a thread of its own, the launcher showing how it goes.
// (A thread let go of when the program ends, still making a game's code or long finished: a joinable
// std::thread destroyed at exit ends the program with "terminate called", a crash on quitting.)
struct LooseThread : std::thread
{
    using std::thread::thread;
    using std::thread::operator=;
    ~LooseThread() { if (joinable()) detach(); }
};
struct Prep
{
    std::mutex m;
    LooseThread th;
    std::atomic<bool> running { false };
    bool finished = false, failed = false, told = true;
    int done = 0, total = 0;
    std::chrono::steady_clock::time_point started{};
    std::string code, name, step, error;
};
inline Prep& P() { static Prep p; return p; }

inline void StartPrepare(const std::string& appDir, const Launcher::Game& g)
{
    Prep& p = P();
    if (p.running) return;
    if (p.th.joinable()) p.th.join();
    {
        std::lock_guard<std::mutex> l(p.m);
        p.finished = p.failed = false;
        p.told = false;
        p.done = p.total = 0;
        p.code = g.code;
        p.name = g.name;
        p.step = "GETTING READY";
        p.error.clear();
        p.started = std::chrono::steady_clock::now();
    }
    p.running = true;
    p.th = std::thread([appDir, g]() {
        Prep& p = P();
        auto fail = [&](const std::string& why) {
            std::lock_guard<std::mutex> l(p.m);
            p.failed = true;
            p.error = why;
        };
        auto take = [&](const std::string& l) {
            fprintf(stderr, "[3ds] prepare: %s\n", l.c_str());
            std::lock_guard<std::mutex> lk(p.m);
            int a = 0, b = 0;
            if (l.rfind("progress ", 0) == 0 && sscanf(l.c_str() + 9, "%d %d", &a, &b) == 2) { p.done = a; p.total = b; }
            else if (l.rfind("step ", 0) == 0)
            {
                const std::string s = l.substr(5);
                p.step = s == "unpack-compiler" ? "UNPACKING THE COMPILER" : s == "translate" ? "TRANSLATING THE GAME'S CODE"
                       : s == "compile" ? "BUILDING THE GAME'S CODE" : s == "shaders" ? "FINDING THE SHADERS"
                       : s == "compile-shaders" ? "PREPARING THE SHADERS" : s == "finish" ? "FINISHING" : "WORKING";
            }
            else if (l.rfind("error ", 0) == 0) { p.failed = true; p.error = l.substr(6); }
        };
        if (!MakeFolder(appDir, g)) fail("COULDN'T MAKE ITS FOLDER");
#ifndef _WIN32
        else
        {
            // (Linux: its output read line by line, as on Windows.)
            int fd[2];
            if (pipe(fd) != 0) fail("COULDN'T START IT");
            else
            {
                const std::string exe = Exe(g.folder), rom = g.rom, dir = g.folder;
                const pid_t pid = fork();
                if (pid == 0)
                {
                    dup2(fd[1], 1);
                    dup2(fd[1], 2);
                    close(fd[0]);
                    close(fd[1]);
                    if (chdir(dir.c_str()) != 0) _exit(127);
                    execl(exe.c_str(), exe.c_str(), "--prepare", rom.c_str(), (char*)nullptr);
                    _exit(127);
                }
                close(fd[1]);
                if (pid < 0) { close(fd[0]); fail("COULDN'T START IT"); }
                else
                {
                    std::string line;
                    char buf[512];
                    ssize_t got;
                    while ((got = read(fd[0], buf, sizeof buf)) > 0)
                        for (ssize_t i = 0; i < got; i++)
                        {
                            if (buf[i] == '\n') { if (!line.empty() && line.back() == '\r') line.pop_back(); take(line); line.clear(); }
                            else line += buf[i];
                        }
                    if (!line.empty()) take(line);
                    close(fd[0]);
                    int st = 0;
                    waitpid(pid, &st, 0);
                    if (!(WIFEXITED(st) && WEXITSTATUS(st) == 0) && !p.failed) fail("IT STOPPED (SEE data/prepare.log IN ITS FOLDER)");
                }
            }
        }
#else
        else
        {
            SECURITY_ATTRIBUTES sa { sizeof sa, nullptr, TRUE };
            HANDLE rd = nullptr, wr = nullptr;
            CreatePipe(&rd, &wr, &sa, 0);
            SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
            STARTUPINFOW si {};
            si.cb = sizeof si;
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = wr;
            si.hStdError = wr;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            PROCESS_INFORMATION pi {};
            std::wstring cmd = L"\"" + W(Exe(g.folder)) + L"\" --prepare \"" + W(g.rom) + L"\"";
            const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, W(g.folder).c_str(), &si, &pi);
            CloseHandle(wr);
            if (!ok) fail("COULDN'T START IT");
            else
            {
                std::string line;
                char buf[512];
                DWORD got = 0;
                while (ReadFile(rd, buf, sizeof buf, &got, nullptr) && got > 0)
                    for (DWORD i = 0; i < got; i++)
                    {
                        if (buf[i] == '\n') { if (!line.empty() && line.back() == '\r') line.pop_back(); take(line); line.clear(); }
                        else line += buf[i];
                    }
                if (!line.empty()) take(line);
                WaitForSingleObject(pi.hProcess, INFINITE);
                DWORD code = 1;
                GetExitCodeProcess(pi.hProcess, &code);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                if (code != 0 && !p.failed) fail("IT STOPPED (SEE data\\prepare.log IN ITS FOLDER)");
            }
            CloseHandle(rd);
        }
#endif
        {
            std::lock_guard<std::mutex> l(p.m);
            p.finished = true;
        }
        p.running = false;
    });
}

#ifdef _WIN32
#include "n3ds_setup.h" // (Put On My Switch, Put On My Phone)
#endif

// What the launcher says about it: while it is being made, and once, when it ends. "" otherwise.
inline std::string Status()
{
    Prep& p = P();
    std::lock_guard<std::mutex> l(p.m);
    if (p.told) return "";
    std::string name = Launcher::TitleCase(p.name);
    for (char& c : name) c = (char)toupper((unsigned char)c);
    if (!p.finished)
    {
        std::string s = "PREPARING " + name + " (ONE TIME, 5 TO 15 MINUTES): " + p.step;
        if (p.total > 0) s += " " + std::to_string(p.done * 100 / p.total) + "%";
        return s;
    }
    p.told = true;
    return p.failed ? "COULDN'T PREPARE " + name + ": " + p.error : name + " IS READY: CHOOSE IT TO PLAY";
}
inline bool Preparing(const std::string& code) { Prep& p = P(); std::lock_guard<std::mutex> l(p.m); return p.running && p.code == code; }
inline bool Busy() { Prep& p = P(); std::lock_guard<std::mutex> l(p.m); return !p.told; }
// (The launcher's preparation card: the whole state at once, without ending it.)
struct StatusInfo
{
    bool active = false, running = false, finished = false, failed = false;
    int done = 0, total = 0;
    long long elapsed = 0; // seconds since it started
    std::string name, step, error;
};
inline StatusInfo Peek()
{
    Prep& p = P();
    std::lock_guard<std::mutex> l(p.m);
    StatusInfo s;
    s.active = !p.told;
    s.running = p.running;
    s.finished = p.finished;
    s.failed = p.failed;
    s.done = p.done;
    s.total = p.total;
    s.name = p.name;
    s.step = p.step;
    s.error = p.error;
    if (p.started.time_since_epoch().count())
        s.elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - p.started).count();
    return s;
}
inline void Ack() { Prep& p = P(); std::lock_guard<std::mutex> l(p.m); p.told = true; }

// Plays it (its window, until it quits); "Restart game" (exit code 3) starts it again.
inline int Play(const std::string& appDir, const Launcher::Game& g)
{
    if (!MakeFolder(appDir, g)) return -1; // (rom.txt says where the dump is now)
#ifndef _WIN32
    for (;;)
    {
        const std::string exe = Exe(g.folder), dir = g.folder;
        const auto runtimeLog = RuntimeDiagnostics::Before(g.folder);
        const pid_t pid = fork();
        if (pid == 0)
        {
            if (chdir(dir.c_str()) != 0) _exit(127);
            execl(exe.c_str(), exe.c_str(), (char*)nullptr);
            _exit(127);
        }
        if (pid < 0) return -1;
        int st = 0;
        waitpid(pid, &st, 0);
        RuntimeDiagnostics::AppendChanged(g.folder, runtimeLog);
        const int code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        fprintf(stderr, "[3ds] %s ended (%d)\n", g.name.c_str(), code);
        if (code != 3) return code;
    }
#else
    for (;;)
    {
        const auto runtimeLog = RuntimeDiagnostics::Before(g.folder);
        DWORD code = 0;
        if (!WindowsRuntimeLog::Run(W(Exe(g.folder)), W(g.folder), code)) return -1;
        RuntimeDiagnostics::AppendChanged(g.folder, runtimeLog);
        fprintf(stderr, "[3ds] %s ended (%lu)\n", g.name.c_str(), (unsigned long)code);
        if (code != 3) return (int)code;
    }
#endif
}
#endif
}

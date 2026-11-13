#include <chrono>
#include "picture_browser.h"
// The PC all-games program's game list. See pc_launcher.h.
#ifdef SSR_PC_LAUNCHER
#include "pc_launcher.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <map>
#include <set>
#include <filesystem>
#include <memory>
#include <thread>
#include <string>
#include <vector>
#include <sys/stat.h>

#ifdef __APPLE__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __ANDROID__
#include <jni.h>
#endif

// (stb_image's code here, for the list's pictures: only JPEG and PNG.)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "thirdparty/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "launcher.h"
#include "n3ds.h" // (Omega Ruby and Alpha Sapphire: listed and started)
#include "now_playing.h" // (the music bar on the home screen)
#include "discord_presence.h" // (Discord shows "In the launcher" or the game being played; needs an Application ID)
#include "cloud_autosync.h"
#include "library_history.h"
#include "android_second_screen.h"
#include "android_save_folder.h"
#include "ui_lang_sdl.h"
#include "controls.h"
#include "rom_library.h"
#include "direct_rom.h"
#include "grid_scroll.h"
#include "achievements.h"
#include "ssr_os.h"
#ifdef SSR_IOS
#include "ios_orient.h"
#endif
#if !defined(SSR_PHONE) && !defined(__SWITCH__)
#include "shortcuts.h"
#endif
#ifdef SSR_UPDATER
#include "updater.h"
#endif
#ifdef SSR_GBA
#include "gba_games.h" // Pokemon GBA Widescreen, built in: its GBA games (CMakeLists.txt, SSR_GBA_DIR)
#include "gba_ra.h"    // RetroAchievements for those games, the same account as the DS games
#endif

// How many chunks the program's SoulSilver tables have (none in a build without them).
extern const uint32_t rc_num_chunks;
bool rc_portable_knows(const char code[4]); // (runtime/recomp_api.h)

namespace PcLauncher
{
#ifdef SSR_PC_ART
// The game list's pictures built into the program (pc_art.cpp, made by CMakeLists.txt from the
// builder's SSR_PC_ART folder): the JPEG or PNG for a game code, or null.
const unsigned char* BuiltInArt(const char* code, size_t& size);
#endif
namespace
{
std::vector<std::string> g_args;
std::vector<char*> g_argv;
bool g_canChange = false, g_change = false;
std::string g_name;

bool Exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

void MakeDir(const std::string& path)
{
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0777);
#endif
}

// The program's own folder, without the slash at its end. (On Windows the program is built
// with a manifest that makes the C library's file names UTF-8, so that folders with any
// letters in their names work.)
std::string ProgramDir()
{
    std::string dir = ".";
    if (char* base = SDL_GetBasePath())
    {
        dir = base;
        SDL_free(base);
    }
    while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
    return dir;
}

// The settings file's fullscreen and window lines, so that the list opens as the game last was.
bool ConfigFullscreen(const std::string& cfg, int& w, int& h)
{
    bool on = true; // (borderless fullscreen unless the settings say otherwise, as the game)
    if (FILE* f = fopen(cfg.c_str(), "r"))
    {
        char line[256];
        while (fgets(line, sizeof line, f))
        {
            if (!strncmp(line, "fullscreen=", 11)) on = line[11] == '1';
            int ww = 0, wh = 0;
            if (sscanf(line, "window=%dx%d", &ww, &wh) == 2 && ww >= 320 && wh >= 240 && ww <= 16384 && wh <= 16384)
            {
                w = ww;
                h = wh;
            }
        }
        fclose(f);
    }
    // (Test: SSR_LIST_SIZE=1280x720 opens in a window of that size, for the test pictures.)
    if (const char* ts = getenv("SSR_LIST_SIZE")) { int tw = 0, th = 0; if (sscanf(ts, "%dx%d", &tw, &th) == 2 && tw >= 320 && th >= 240) { w = tw; h = th; on = false; } }
    return on;
}

// F11 in the list: the game then starts the same way (the settings file says so).
// Lines of the settings file set (key=value) or, for an empty value, taken out; the others kept.
void SetConfigValues(const std::string& cfg, const std::vector<std::pair<std::string, std::string>>& values)
{
    std::vector<std::string> lines;
    if (FILE* f = fopen(cfg.c_str(), "r"))
    {
        char line[1024];
        while (fgets(line, sizeof line, f))
        {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            bool set = false;
            for (const auto& kv : values) set |= l.size() > kv.first.size() && !l.compare(0, kv.first.size(), kv.first) && l[kv.first.size()] == '=';
            if (!set) lines.push_back(l);
        }
        fclose(f);
    }
    for (const auto& kv : values)
        if (!kv.second.empty()) lines.push_back(kv.first + "=" + kv.second);
    const std::string tmp = cfg + ".tmp";
    if (FILE* f = fopen(tmp.c_str(), "w"))
    {
        bool ok = true;
        for (const std::string& l : lines) if (fprintf(f, "%s\n", l.c_str()) < 0) ok = false;
        ok = fclose(f) == 0 && ok;
        std::error_code ec;
        if (ok) std::filesystem::rename(tmp, cfg, ec);
        if (!ok || ec) fprintf(stderr, "[launcher] settings could not be saved: %s\n", cfg.c_str());
    }
}

void SetConfigFullscreen(const std::string& cfg, bool on)
{
    std::vector<std::string> lines;
    bool found = false;
    if (FILE* f = fopen(cfg.c_str(), "r"))
    {
        char line[1024];
        while (fgets(line, sizeof line, f))
        {
            std::string l = line;
            if (!strncmp(line, "fullscreen=", 11))
            {
                l = std::string("fullscreen=") + (on ? "1" : "0") + "\n";
                found = true;
            }
            lines.push_back(l);
        }
        fclose(f);
    }
    if (!found) lines.push_back(std::string("fullscreen=") + (on ? "1" : "0") + "\n");
    if (FILE* f = fopen(cfg.c_str(), "w"))
    {
        for (const std::string& l : lines) fputs(l.c_str(), f);
        fclose(f);
    }
}

struct Folders
{
    std::string dir, roms, saves, funcs, cfg;
    std::string art; // where the list's own pictures are (icons/IREO.jpg ...)
};

#ifdef __ANDROID__
// The Android app's activity (tools/android/allgames/.../GameActivity.java): its static
// methods, called through JNI.
jclass ActivityClass(JNIEnv* env)
{
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!activity) return nullptr;
    jclass c = env->GetObjectClass(activity);
    env->DeleteLocalRef(activity);
    return c;
}

template <class F>
void CallActivity(const char* method, const char* signature, F call)
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) return;
    jclass c = ActivityClass(env);
    if (!c) return;
    jmethodID m = env->GetStaticMethodID(c, method, signature);
    if (m) call(env, c, m);
    if (env->ExceptionCheck())
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    env->DeleteLocalRef(c);
}

std::string JavaString(const char* method)
{
    std::string out;
    CallActivity(method, "()Ljava/lang/String;", [&](JNIEnv* env, jclass c, jmethodID m) {
        if (jstring str = (jstring)env->CallStaticObjectMethod(c, m))
        {
            const char* utf = env->GetStringUTFChars(str, nullptr);
            if (utf) out = utf;
            env->ReleaseStringUTFChars(str, utf);
            env->DeleteLocalRef(str);
        }
    });
    return out;
}

bool JavaBool(const char* method)
{
    bool out = false;
    CallActivity(method, "()Z", [&](JNIEnv* env, jclass c, jmethodID m) { out = env->CallStaticBooleanMethod(c, m); });
    return out;
}

void JavaVoid(const char* method)
{
    CallActivity(method, "()V", [&](JNIEnv* env, jclass c, jmethodID m) { env->CallStaticVoidMethod(c, m); });
}

void JavaBoolArg(const char* method, bool value)
{
    CallActivity(method, "(Z)V", [&](JNIEnv* env, jclass c, jmethodID m) { env->CallStaticVoidMethod(c, m, value ? JNI_TRUE : JNI_FALSE); });
}

void JavaStringArg(const char* method, const std::string& value)
{
    CallActivity(method, "(Ljava/lang/String;)V", [&](JNIEnv* env, jclass c, jmethodID m) {
        jstring s = env->NewStringUTF(value.c_str());
        env->CallStaticVoidMethod(c, m, s);
        env->DeleteLocalRef(s);
    });
}

// The game folder of a 3DS game whose first Play is copying it in: it starts when that is done.
std::string& N3dsAutoPlay() { static std::string folder; return folder; }

bool JavaBoolStringArg(const char* method, const std::string& value)
{
    bool out = false;
    CallActivity(method, "(Ljava/lang/String;)Z", [&](JNIEnv* env, jclass c, jmethodID m) {
        jstring s = env->NewStringUTF(value.c_str());
        out = env->CallStaticBooleanMethod(c, m, s);
        env->DeleteLocalRef(s);
    });
    return out;
}

// The ROM folder the player chose (GameActivity.romFolder), with the saves in a folder inside
// it; until one is chosen, the app's own folder.
void SetRomFolder(Folders& fo)
{
    fo.roms = JavaString("romFolder");
    fo.saves = Launcher::ConfigValue(fo.cfg, "saves_folder");
    if (fo.saves.empty()) fo.saves = (fo.roms.empty() ? fo.dir : fo.roms) + "/saves";
    fo.art = fo.roms.empty() ? fo.dir : fo.roms;
}

// The function tables, packed in the APK (assets/funcs), copied out to the app's folder for
// the emulator, which reads them by file name.
void ExtractFuncs(const Folders& fo)
{
    std::vector<const char*> codes = { "IREO", "IRDO", "IRBO", "IRAO", "CPUE", "ADAE", "IRAS", "CPUS", "ADAS", "APAE", "APAF", "APAS", "APAD", "ADAD", "ADAI", "IREF", "IRDF", "CPUF", "IRBF", "IRAF", "IRBS", "IRES", "IRDS" };
    for (const auto& p : RegionalProfiles::profiles) if (p.functions) codes.push_back(p.code);
    for (const char* code : codes)
    {
        SDL_RWops* rw = SDL_RWFromFile((std::string("funcs/") + code + ".txt").c_str(), "rb");
        if (!rw) continue;
        std::string data;
        char buf[65536];
        size_t n;
        while ((n = SDL_RWread(rw, buf, 1, sizeof buf)) > 0) data.append(buf, n);
        SDL_RWclose(rw);
        const std::string out = fo.funcs + "/" + code + ".txt";
        struct stat st;
        if (stat(out.c_str(), &st) == 0 && (size_t)st.st_size == data.size()) continue;
        if (FILE* f = fopen(out.c_str(), "wb"))
        {
            fwrite(data.data(), 1, data.size(), f);
            fclose(f);
        }
    }
}
#endif

// Whether this program plays a game: SoulSilver and HeartGold need the program's own tables,
// the others their function table in data/funcs.
bool Playable(const Folders& fo, const Launcher::Game& g)
{
    if (g.platform == Launcher::GBA) return true; // (any GBA game plays, with widescreen or without)
    if (g.platform == Launcher::N3DS) return g.known && !g.comingSoon; // (n3ds.h: listed only where it plays)
    if (!g.known || !g.problem.empty()) return false;
    if (IsGame(g.code, "IPGE") || IsGame(g.code, "IPKE")) return rc_num_chunks > 0 && rc_portable_knows(g.code.c_str());
    return Exists(fo.funcs + "/" + g.code + ".txt");
}

std::string PrepareLaunch(const Launcher::Game& game)
{
    FILE* rom = fopen(game.rom.c_str(), "rb");
    if (!rom) return "ROM IS MISSING OR CANNOT BE READ";
    fclose(rom);
    if (game.saveName.empty()) return "THIS GAME HAS NO SAVE FILE NAME";
    std::error_code ec;
    std::filesystem::create_directories(game.folder, ec);
    if (ec) return "CANNOT CREATE THE SAVES FOLDER";
    const std::string save = game.folder + "/" + game.saveName;
    if (std::filesystem::exists(save, ec))
    {
        const std::string backup = save + ".prelaunch.bak";
        std::filesystem::copy_file(save, backup + ".tmp", std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) std::filesystem::rename(backup + ".tmp", backup, ec);
        if (ec) return "CANNOT BACK UP SAVE: CHECK FREE SPACE AND ACCESS";
    }
    const std::string probe = save + ".write-check";
    // Not "wbx": the Windows C runtime rejects the "x" flag, which blocked every launch there.
    remove(probe.c_str());
    FILE* f = fopen(probe.c_str(), "wb");
    if (!f) return "SAVES FOLDER IS NOT WRITABLE";
    const bool ok = fclose(f) == 0;
    remove(probe.c_str());
    return ok ? "" : "CANNOT WRITE IN THE SAVES FOLDER";
}

bool EndsWithCi(const std::string& name, const char* ext)
{
    const size_t n = std::strlen(ext);
    if (name.size() < n) return false;
    for (size_t i = 0; i < n; i++)
        if (std::tolower((unsigned char)name[name.size() - n + i]) != std::tolower((unsigned char)ext[i]))
            return false;
    return true;
}

// A GBA ROM or Ultra Violet's patch, whatever the letter case of the extension.
bool IsGbaRomName(const std::string& name)
{
    const std::string base = name.substr(name.find_last_of("/\\") + 1);
    return EndsWithCi(base, ".gba") || EndsWithCi(base, ".agb") || EndsWithCi(base, ".ips");
}

bool SameContents(const std::filesystem::path& a, const std::filesystem::path& b)
{
    std::error_code ec;
    if (std::filesystem::file_size(a, ec) != std::filesystem::file_size(b, ec) || ec) return false;
    FILE* fa = fopen(a.string().c_str(), "rb");
    FILE* fb = fopen(b.string().c_str(), "rb");
    bool same = fa && fb;
    char x[4096], y[4096];
    while (same)
    {
        const size_t n = fread(x, 1, sizeof x, fa);
        if (fread(y, 1, n, fb) != n || memcmp(x, y, n)) same = false;
        else if (n == 0) break;
    }
    if (fa && ferror(fa)) same = false;
    if (fb && ferror(fb)) same = false;
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return same;
}

// Copies a ROM or save into the app. Different ROMs sharing a filename are never overwritten;
// an explicitly imported save is backed up before replacement.
bool ImportFile(const Folders& fo, const std::string& path)
{
    std::string from = path;
    if (from.compare(0, 7, "file://") == 0)
    {
        // A URI: its %20 and the like are the characters themselves.
        std::string plain;
        for (size_t i = 7; i < from.size(); i++)
        {
            if (from[i] == '%' && i + 2 < from.size() && isxdigit((unsigned char)from[i + 1]) && isxdigit((unsigned char)from[i + 2]))
            {
                plain += (char)std::stoi(from.substr(i + 1, 2), nullptr, 16);
                i += 2;
            }
            else plain += from[i];
        }
        from = plain;
    }
    const std::string base = from.substr(from.find_last_of("/\\") + 1);
    if (base.empty() || base[0] == '.') return false;
    std::string destDir;
    if (Launcher::EndsWithNds(base) || IsGbaRomName(base)) destDir = fo.roms;
    else if (EndsWithCi(base, ".sav")) destDir = fo.saves;
    else return false;
    MakeDir(destDir);
    const std::filesystem::path src(from), dst = std::filesystem::path(destDir) / base;
    std::error_code ec;
    if (std::filesystem::exists(src, ec) && std::filesystem::exists(dst, ec) && std::filesystem::equivalent(src, dst, ec))
        return true;
    if (!EndsWithCi(base, ".sav") && std::filesystem::exists(dst, ec))
    {
        if (SameContents(src, dst)) return true;
        fprintf(stderr, "[launcher] not imported: %s already names a different ROM; rename the incoming file or choose its folder\n", dst.string().c_str());
        return false;
    }
    // A save already there is the player's progress: keep it beside the new one.
    if (EndsWithCi(base, ".sav") && std::filesystem::exists(dst, ec) && !SameContents(src, dst))
    {
        std::filesystem::copy_file(dst, dst.string() + ".before-import.bak", std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            fprintf(stderr, "[launcher] could not back up %s: %s\n", dst.string().c_str(), ec.message().c_str());
            return false;
        }
    }
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        fprintf(stderr, "[launcher] could not import %s: %s\n", from.c_str(), ec.message().c_str());
        return false;
    }
    fprintf(stderr, "[launcher] imported %s\n", dst.string().c_str());
    return true;
}

// iOS puts a ROM opened from the Files app in Documents/Inbox, and may delete that folder
// later. Move those files into roms and saves, where the list keeps looking.
void TakeInbox(const Folders& fo)
{
    const std::string inbox = fo.dir + "/Inbox";
    DIR* d = opendir(inbox.c_str());
    if (!d) return;
    std::vector<std::string> names;
    while (dirent* e = readdir(d)) names.push_back(e->d_name);
    closedir(d);
    for (const std::string& n : names)
    {
        const std::string from = inbox + "/" + n;
        struct stat st;
        if (stat(from.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (ImportFile(fo, from)) remove(from.c_str());
    }
}

// Where the ROM hacks made from patch files go: hacks/ in the program's folder (not saves/, which
// cloud saves copy, nor the ROM folder, which a phone may not let it write in).
std::string HacksFolder(const Folders& fo)
{
    return (fo.dir.empty() ? std::string(".") : fo.dir) + "/hacks";
}

// The ROMs in the roms folder (and any left beside the program), their saves in saves/.
std::vector<Launcher::Game> FindGames(const Folders& fo, std::map<std::string, std::vector<uint32_t>>& artCache)
{
    std::vector<Launcher::Game> games;
    std::set<std::string> seen;
    // (Each DS ROM checked against the clean releases, in the background the first time: a hack
    // gets a save of its own. romhacks.h)
    Launcher::BeginScan(fo.saves);
    // A picture of the player's own (icons/IREO.jpg, or Black 2.jpg beside Black 2.nds), read once.
    auto artFor = [&](Launcher::Game& g) {
        auto it = artCache.find(g.rom);
        if (it == artCache.end())
        {
            Launcher::FindArt(g, fo.art);
#ifdef SSR_PC_ART
            // Without one, the picture built into the program (SSR_PC_ART), if any (a hack: its game's).
            size_t size = 0;
            if (g.art.empty())
                if (const unsigned char* data = BuiltInArt(g.code.c_str(), size)) Launcher::LoadArt(data, size, g.art);
#endif
            it = artCache.emplace(g.rom, g.art).first;
        }
        g.art = it->second;
    };
    std::vector<std::string> romDirs = RomLibrary::Folders(fo.roms, HacksFolder(fo));
    if (std::find(romDirs.begin(), romDirs.end(), fo.dir) == romDirs.end()) romDirs.push_back(fo.dir);
    for (const std::string& dir : romDirs)
    {
        if (dir.empty()) continue;
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        std::vector<std::string> names;
        while (dirent* e = readdir(d))
            if (Launcher::EndsWithNds(e->d_name)) names.push_back(e->d_name);
        closedir(d);
        std::sort(names.begin(), names.end());
        for (const std::string& n : names)
        {
            std::error_code canonicalError;
            const std::string canonical = std::filesystem::weakly_canonical(dir + "/" + n, canonicalError).string();
            if (canonicalError || !seen.insert(canonical).second) continue;
            Launcher::Game g;
            if (!Launcher::ReadGame(dir + "/" + n, fo.saves, g)) continue;
            artFor(g);
            games.push_back(std::move(g));
        }
    }
    // ROM hacks as patch files (.xdelta, .bps, .ups) beside the ROMs: each made once into hacks/.
    {
        const size_t before = games.size();
        Launcher::AddPatchedHacks(romDirs, HacksFolder(fo), fo.saves, games, false);
        for (size_t i = before; i < games.size(); i++) artFor(games[i]);
        Launcher::AdoptHackSaves(games, [](const Launcher::Game& g, const std::string& name) { return g.folder + "/" + name; });
    }
    Launcher::EndScan();
#ifdef SSR_GBA
    // The GBA games in the roms folder (saves in saves/: emerald.sav ...), in a GBA folder inside
    // it, and loose in the app folder. On iPhone the Files app shows that folder, and a ROM
    // opened from Files arrives in Inbox; TakeInbox moves those into roms.
    TakeInbox(fo);
    const size_t ds = games.size();
    GbaGames::AddWithGbaFolder(fo.roms, fo.saves, games, artCache);
    for (const std::string& sub : RomLibrary::Folders(fo.roms, HacksFolder(fo)))
    {
        if (sub == fo.roms) continue;
        const auto path = std::filesystem::u8path(sub);
        std::string leaf = path.filename().u8string();
        for (char& c : leaf) c = (char)std::tolower((unsigned char)c);
        if (path.parent_path() == std::filesystem::u8path(fo.roms) && leaf == "gba") continue;
        GbaGames::Add(sub, fo.saves, games, artCache);
    }
    if (!fo.dir.empty() && fo.dir != fo.roms) GbaGames::AddWithGbaFolder(fo.dir, fo.saves, games, artCache);
    {
        std::set<std::string> seenRom;
        std::vector<Launcher::Game> gba;
        for (size_t i = ds; i < games.size(); i++)
        {
            std::error_code ec;
            const std::string key = std::filesystem::weakly_canonical(games[i].rom, ec).string();
            if (!seenRom.insert(ec ? games[i].rom : key).second) continue;
            gba.push_back(std::move(games[i]));
        }
        games.resize(ds);
        for (auto& g : gba) games.push_back(std::move(g));
    }
    static size_t gbaSeen = (size_t)-1; // (said when the number changes: the folder is looked at again while the list is open)
    if (games.size() - ds != gbaSeen)
    {
        gbaSeen = games.size() - ds;
        if (!fo.dir.empty() && fo.dir != fo.roms)
            fprintf(stderr, "[launcher] GBA games: %zu in %s and %s\n", gbaSeen, fo.roms.c_str(), fo.dir.c_str());
        else
            fprintf(stderr, "[launcher] GBA games: %zu in %s\n", gbaSeen, fo.roms.c_str());
    }
#endif
#ifdef _WIN32
    // Omega Ruby and Alpha Sapphire: a dump (.3ds) with the ROMs, played by the 3ds folder's program.
    N3ds::AddPc(romDirs, fo.dir, games);
    for (Launcher::Game& g : games)
        if (g.platform == Launcher::N3DS) g.built = N3ds::Prepared(g);
#elif defined(__ANDROID__)
    // (Android: with the Omega Ruby kit in the app, its own folder and the ROM folders.)
    if (JavaBool("threeDsAvailable")) N3ds::AddAndroid(romDirs, fo.dir, games);
    for (Launcher::Game& g : games)
        if (g.platform == Launcher::N3DS) g.built = JavaBoolStringArg("threeDsBuilt", N3ds::IdOf(g));
#endif
    Launcher::AddComingSoon(games);
    N3ds::DropPlaceholders(games);
    for (Launcher::Game& g : games)
    {
        // (Games on their way, and the 3DS games: their art by their code, as for any game when the
        // player has one, else the box art built into the program.)
        if (!g.comingSoon && g.platform != Launcher::N3DS) continue;
        auto it = artCache.find("coming-soon:" + g.code);
        if (it == artCache.end())
        {
            Launcher::FindArt(g, fo.art);
#ifdef SSR_PC_ART
            size_t size = 0;
            if (g.art.empty())
                if (const unsigned char* data = BuiltInArt(g.code.c_str(), size)) Launcher::LoadArt(data, size, g.art);
#endif
            it = artCache.emplace("coming-soon:" + g.code, g.art).first;
        }
        g.art = it->second;
    }
    Launcher::Sort(games);
    return games;
}

void OpenFolder(const std::string& path)
{
#ifdef SSR_IOS
    // (iOS: the Files app, opened at the folder; the app's folder shows there as On My iPhone >
    // Pokémon Fulldex, the app's name.)
    std::string url = "shareddocuments://";
#else
    std::string url = "file://";
#endif
    if (path.empty() || path[0] != '/') url += "/"; // (C:/... on Windows)
    // (Escaped as a URL: a Mac opens none with a space in it, as in "Application Support".)
    static const char hex[] = "0123456789ABCDEF";
    for (char ch : path)
    {
        const unsigned char c = (unsigned char)(ch == '\\' ? '/' : ch);
        if (isalnum(c) || strchr("/-._~:", c) || c >= 0x80) url += (char)c;
        else { url += '%'; url += hex[c >> 4]; url += hex[c & 15]; }
    }
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (SDL_OpenURL(url.c_str()) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", url.c_str(), SDL_GetError());
#endif
}

// The list in its own window until a game is chosen: its index in games (which is updated as
// ROMs come and go), or -1 when the window is closed.
#ifdef _WIN32
#include <commdlg.h>
#include <SDL2/SDL_syswm.h>
// Change Picture's Import A Picture on Windows: the system's own file picker (comdlg32, loaded when
// it is needed, so the program links nothing more), over the launcher's window. Empty: none chosen.
static std::string PickPictureFile(SDL_Window* window)
{
    typedef BOOL(WINAPI * OpenFn)(LPOPENFILENAMEW);
    static HMODULE dll = LoadLibraryW(L"comdlg32.dll");
    OpenFn open = dll ? (OpenFn)GetProcAddress(dll, "GetOpenFileNameW") : nullptr;
    if (!open) return std::string();
    wchar_t file[1024] = L"";
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof o;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info)) o.hwndOwner = info.info.win.window;
    o.lpstrFilter = L"Pictures (PNG, JPG)\0*.png;*.jpg;*.jpeg\0\0";
    o.lpstrFile = file;
    o.nMaxFile = 1024;
    o.lpstrTitle = L"Choose a picture for the game";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!open(&o)) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n : 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, file, -1, &out[0], n, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}
#endif

#ifdef _WIN32
// My Themes' Import A Theme on Windows: the system's file picker for a .zip.
static std::string PickZipFile(SDL_Window* window)
{
    typedef BOOL(WINAPI * OpenFn)(LPOPENFILENAMEW);
    static HMODULE dll = LoadLibraryW(L"comdlg32.dll");
    OpenFn open = dll ? (OpenFn)GetProcAddress(dll, "GetOpenFileNameW") : nullptr;
    if (!open) return std::string();
    wchar_t file[1024] = L"";
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof o;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info)) o.hwndOwner = info.info.win.window;
    o.lpstrFilter = L"Fulldex themes (ZIP)\0*.zip\0\0";
    o.lpstrFile = file;
    o.nMaxFile = 1024;
    o.lpstrTitle = L"Choose a Fulldex theme";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!open(&o)) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n : 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, file, -1, &out[0], n, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}
#endif

#ifdef _WIN32
// Manage Saves' Import Save on Windows: the system's file picker for a save.
static std::string PickSaveFile(SDL_Window* window)
{
    typedef BOOL(WINAPI * OpenFn)(LPOPENFILENAMEW);
    static HMODULE dll = LoadLibraryW(L"comdlg32.dll");
    OpenFn open = dll ? (OpenFn)GetProcAddress(dll, "GetOpenFileNameW") : nullptr;
    if (!open) return std::string();
    wchar_t file[1024] = L"";
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof o;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info)) o.hwndOwner = info.info.win.window;
    o.lpstrFilter = L"Saves (SAV, DSV)\0*.sav;*.dsv\0All files\0*.*\0\0";
    o.lpstrFile = file;
    o.nMaxFile = 1024;
    o.lpstrTitle = L"Choose a save to import";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!open(&o)) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n : 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, file, -1, &out[0], n, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}
#endif

#ifdef _WIN32
// SAVES FOLDER on Windows: the system's folder picker.
#include <shlobj.h>
static std::string PickFolder(SDL_Window* window, const wchar_t* title = L"Choose the folder for your saves (they are copied there)")
{
    typedef PIDLIST_ABSOLUTE(WINAPI * BrowseFn)(LPBROWSEINFOW);
    typedef BOOL(WINAPI * PathFn)(PCIDLIST_ABSOLUTE, LPWSTR);
    typedef HRESULT(WINAPI * InitFn)(LPVOID, DWORD);
    typedef void(WINAPI * FreeFn)(LPVOID);
    static HMODULE shell = LoadLibraryW(L"shell32.dll"), ole = LoadLibraryW(L"ole32.dll");
    BrowseFn browse = shell ? (BrowseFn)GetProcAddress(shell, "SHBrowseForFolderW") : nullptr;
    PathFn path = shell ? (PathFn)GetProcAddress(shell, "SHGetPathFromIDListW") : nullptr;
    InitFn init = ole ? (InitFn)GetProcAddress(ole, "CoInitializeEx") : nullptr;
    FreeFn freeMem = ole ? (FreeFn)GetProcAddress(ole, "CoTaskMemFree") : nullptr;
    if (!browse || !path) return std::string();
    if (init) init(nullptr, COINIT_APARTMENTTHREADED);
    BROWSEINFOW bi = {};
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info)) bi.hwndOwner = info.info.win.window;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE id = browse(&bi);
    if (!id) return std::string();
    wchar_t dir[MAX_PATH * 2] = L"";
    const bool ok = path(id, dir);
    if (freeMem) freeMem(id);
    if (!ok) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, dir, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n : 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, dir, -1, &out[0], n, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}
#endif

#include "save_backups.h"

// The Updates page: the updater's state, as the list draws it (Android: its own, in Java).
static void FillUpdateStatus()
{
#if defined(SSR_UPDATER) && !defined(__ANDROID__)
    const Updater::Status st = Updater::Get();
    Launcher::GlassFocus& F = Launcher::Focus();
    F.upState = (int)st.state; F.upCan = st.canInstall; F.upText = st.text; F.upNotes = st.notes;
    F.upHave = st.currentBuild ? Updater::BuildLabel(st.currentBuild) : std::string();
    F.upNew = st.availableBuild ? Updater::BuildLabel(st.availableBuild) : std::string();
#endif
    // (SSR_LIST_NOTES=file: a new version with these notes, for the test picture.)
    if (const char* tn = getenv("SSR_LIST_NOTES"))
    {
        Launcher::GlassFocus& T = Launcher::Focus();
        std::vector<unsigned char> d;
        if (Launcher::ReadFile(tn, d)) T.upNotes.assign(d.begin(), d.end());
        T.upState = 3; T.upCan = true; T.upHave = "2026-11-05 18:20 UTC"; T.upNew = "2026-11-07 10:00 UTC";
        if (const char* ns = getenv("SSR_LIST_NSCROLL")) T.nScroll = atoi(ns);
    }
}

int PickGame(Folders& fo, std::vector<Launcher::Game>& games, int selected, bool& fullscreen, int winW, int winH)
{
#ifdef __ANDROID__
    JavaVoid("showLauncherOrientation");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1"); // (taps come as clicks here; the game takes fingers one by one)
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    fullscreen = true;
    // No ROM folder yet, or no access to files: the list asks for them (GameActivity).
    bool needFolder = (fo.roms.empty() || !JavaBool("canUseFiles")) && !getenv("SSR_ANDROID_ROMS");
    bool wasPicking = false;
    Uint32 lastFolderCheck = 0;
#elif defined(SSR_IOS)
    // Upright, on an iPhone or an iPad. The plist allows sideways too; a game asks for that.
    IosOrientLauncher();
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1"); // (taps come as clicks here; the game takes fingers one by one)
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
    fullscreen = true;
#endif
    #ifdef _WIN32
    // The program's own icon (resource 1, when it was built with one) for its windows.
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "1");
    // Windows' own screen scaling (150% on a 4K screen) otherwise draws the window small and stretches it, soft:
    // asked for as it is, per monitor, the launcher draws at the screen's real pixels. (A hint older SDL ignores.)
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
    #endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        fprintf(stderr, "[launcher] SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    UiLang::DetectSystem();
#ifdef _WIN32
    // (Real pixels now, so a window of 1280 x 720 would show smaller on a scaled screen: as big as it was.)
    if (!fullscreen)
    {
        float ddpi = 96.0f;
        if (SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr) == 0 && ddpi > 96.0f) { winW = (int)(winW * ddpi / 96.0f); winH = (int)(winH * ddpi / 96.0f); }
    }
#endif
    SDL_Window* window = SDL_CreateWindow("Pokémon Fulldex", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!window)
    {
        fprintf(stderr, "[launcher] window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    // Back from a game, the list is a new window: take the focus (as the game's window does),
    // so that the keyboard and controllers work without a click first.
    SDL_RaiseWindow(window);
#ifdef SSR_IOS
    // The window exists now: ask iOS to turn, in case the device was already sideways.
    IosOrientLauncher();
#endif
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
#ifdef SSR_IOS
        fprintf(stderr, "[launcher] accelerated renderer failed: %s; trying software\n", SDL_GetError());
#endif
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer)
    {
        fprintf(stderr, "[launcher] renderer initialisation failed: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_RendererInfo renderInfo = {};
    if (SDL_GetRendererInfo(renderer, &renderInfo) == 0)
        fprintf(stderr, "[launcher] renderer %s, max texture %dx%d\n", renderInfo.name, renderInfo.max_texture_width, renderInfo.max_texture_height);
#ifdef SSR_IOS
    fprintf(stderr, "[launcher] iOS window=%u renderer-window=%u association=%s\n",
        SDL_GetWindowID(window), SDL_GetWindowID(SDL_RenderGetWindow(renderer)),
        SDL_RenderGetWindow(renderer) == window ? "correct" : "WRONG");
    int loggedDrawableW = -1, loggedDrawableH = -1;
    bool loggedPresent = false, loggedDrawableError = false;
#endif
    SDL_Texture* texture = nullptr;
    Canvas canvas;
    std::unique_ptr<AndroidSecondScreen> libraryDisplay;
    Uint32 libraryDisplayCheck=0;
    Canvas libraryCard;libraryCard.Resize(640,480);
    std::string libraryCardKey;
    std::map<std::string, std::vector<uint32_t>> artCache;
    std::vector<SDL_GameController*> pads;
    std::vector<SDL_Joystick*> joys; // (gamepads SDL has no layout for)
    // The tab shown (Launcher::TABS: ALL first, the list opens on it; back from a GBA game, the
    // tab it was chosen on) and how far down it is scrolled. selected: a game in that tab, or -1
    // when it has none.
    Launcher::LoadTabs(fo.saves);
    static int lastTab = std::max(0, std::min(Launcher::TabCount() - 1, atoi(Launcher::ConfigValue(fo.cfg, "launcher_tab2").c_str())));
    int tab = lastTab, scroll = 0, result = -1;
    // false while the list is where a finger (or the mouse, held) dragged it, rather than where
    // the chosen game is; a key, button or tap that chooses a game makes it follow again.
    bool follow = true;
    std::string note;
#ifdef __ANDROID__
    auto chooseRomFolder = [&]() {
        // Pin an established save location before an import can fill storage or change the ROM root.
        if (Launcher::ConfigValue(fo.cfg, "saves_folder").empty() && !fo.saves.empty() &&
            AndroidSaveFolder::Established(fo.roms, fo.saves)) {
            SetConfigValues(fo.cfg, {{"saves_folder", fo.saves}});
            if (Launcher::ConfigValue(fo.cfg, "saves_folder") != fo.saves) {
                note = "COULD NOT KEEP THE SAVE FOLDER"; return;
            }
        }
        JavaVoid("chooseFolder");
    };
#endif

#ifdef SSR_UPDATER
    unsigned updateSeen = 0;
    bool updateAsked = false;
    Menu updateMenu;
#endif
    LibraryHistory library;
    auto reorderLibrary=[&]() {
        const std::string keep=selected>=0 && selected<(int)games.size()?games[selected].rom:"";
        library.Load(fo.saves+"/fulldex-library.txt");
        Launcher::ApplyLocal(games, fo.saves); // (the player's names; the games taken off the list left out)
        Launcher::ApplyArt(games, fo.saves); // (the pictures chosen with Change Picture)
        Launcher::SortKey() = Launcher::ConfigValue(fo.cfg, "list_sort");
        Launcher::LoadTabs(fo.saves);
        Launcher::Sort(games);
        library.Apply(games,Launcher::ConfigValue(fo.cfg,"list_sort"));
        for(size_t i=0;i<games.size();i++)if(games[i].rom==keep)selected=(int)i;
        scroll=0;follow=true;
    };
    Launcher::BuildRomList(games, fo.saves); // (the ROMs page: before those taken off the list are left out)
    // (Twenty hours played: the one Support message. SSR_LIST_BEER: shown at once, for the test picture.)
    if (PlayTime::AskNow(fo.saves) || getenv("SSR_LIST_BEER")) Launcher::OpenBeer();
    reorderLibrary();
    auto useRomFolder = [&](const std::string& dir) {
        std::error_code ec;
        if (dir.empty() || !std::filesystem::is_directory(std::filesystem::u8path(dir), ec)) { note = "ROM FOLDER IS NOT AVAILABLE"; return; }
        DIR* readable = opendir(dir.c_str());
        if (!readable) { note = "ROM FOLDER CANNOT BE READ"; return; }
        closedir(readable);
        SetConfigValues(fo.cfg, {{"roms_folder", dir}});
        if (Launcher::ConfigValue(fo.cfg, "roms_folder") != dir) { note = "COULD NOT SAVE THE ROM FOLDER"; return; }
        fo.roms = dir;
        artCache.clear();
        games = FindGames(fo, artCache);
        selected = -1;
        Launcher::BuildRomList(games, fo.saves);
        reorderLibrary();
        selected = Launcher::FirstIn(Launcher::MakeView(canvas, games, tab));
        Launcher::Focus().prow = 0;
        note = "ROM FOLDER CHANGED";
    };
    std::string folderBrowse;
    int folderFirst = 0, folderStep = 1;
    auto showRomFolder = [&]() {
        auto& F = Launcher::Focus();
        F.dialog = 19; F.dTitle = "Choose ROMs Folder"; F.dMsg = folderBrowse;
        F.dItems = {"Use This Folder", "Parent Folder", "Cancel"};
        F.dPaths = {"#use", "#parent", "#close"}; F.drow = 0;
        std::vector<std::string> dirs;
        std::error_code ec;
        for (std::filesystem::directory_iterator it(std::filesystem::u8path(folderBrowse), std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
            if (it->is_directory(ec)) dirs.push_back(it->path().u8string());
        std::sort(dirs.begin(), dirs.end());
        const int pageSize = std::clamp((canvas.h - 120) / 56 - 5, 1, 5);
        folderStep = pageSize;
        const int first = folderFirst;
        for (int i = first; i < (int)dirs.size() && i < first + pageSize; ++i) {
            F.dItems.push_back(std::filesystem::u8path(dirs[i]).filename().u8string()); F.dPaths.push_back(dirs[i]);
        }
        if (folderFirst > 0) { F.dItems.push_back("Previous Folders"); F.dPaths.push_back("#previous"); }
        if (first + pageSize < (int)dirs.size()) { F.dItems.push_back("More Folders"); F.dPaths.push_back("#next"); }
    };
    std::string pictureBrowse, pictureRom;
    int pictureFirst = 0, pictureStep = 1;
    auto showPictureFolder = [&]() {
        auto& F = Launcher::Focus();
        F.dialog = 20; F.dTitle = "Choose A Picture"; F.dMsg = pictureBrowse;
        F.dItems = {"Parent Folder", "Cancel"}; F.dPaths = {"#parent", "#close"}; F.drow = 0;
        const auto entries = PictureBrowser::Entries(pictureBrowse);
        const int pageSize = std::clamp((canvas.h - 120) / 56 - 5, 1, 5);
        pictureStep = pageSize;
        const int first = pictureFirst;
        for (int i = first; i < (int)entries.size() && i < first + pageSize; ++i) {
            F.dItems.push_back(entries[i].label); F.dPaths.push_back(entries[i].path);
        }
        if (pictureFirst > 0) { F.dItems.push_back("Previous Files"); F.dPaths.push_back("#previous"); }
        if (first + pageSize < (int)entries.size()) { F.dItems.push_back("More Files"); F.dPaths.push_back("#next"); }
    };
    auto toggleFavourite=[&]() {
        if(selected<0||selected>=(int)games.size()||games[selected].comingSoon)return;
        bool saved=library.Toggle(games[selected].rom);
        note=saved?(library.Favourite(games[selected].rom)?"ADDED TO HOME":"REMOVED FROM HOME"):"COULD NOT SAVE HOME";
        reorderLibrary();
    };
    auto view = [&]() { return Launcher::MakeView(canvas, games, tab); };
    // Tests, without anyone there: SSR_LIST_TAB=n opens on tab n; SSR_LIST_PICK=a game code chooses
    // that game the first time the list is shown; SSR_LIST_SHOT=file.bmp keeps the list as drawn
    // (after the pick, the next time) in that picture and closes it.
    static int listsShown = 0;
    listsShown++;
#if defined(__ANDROID__) || defined(SSR_IOS)
    Launcher::PhoneUi() = true; // (a phone: bigger text and buttons, the Tiles layout)
#endif
    if (getenv("SSR_LIST_PHONE")) Launcher::PhoneUi() = true; // (test)
#ifdef __SWITCH__
    Launcher::PhoneUi() = true; // (the Switch: the same bigger text and buttons as a phone, so the menus read in handheld)
#endif
    const char* testShot = getenv("SSR_LIST_SHOT");
    const char* testPick = listsShown == 1 ? getenv("SSR_LIST_PICK") : nullptr;
    if (const char* t = getenv("SSR_LIST_TAB")) tab = std::max(0, std::min(Launcher::TabCount() - 1, atoi(t)));
    // (SSR_LIST_SEARCH=words: the list searched, for the test picture.)
    if (const char* sq = getenv("SSR_LIST_SEARCH")) Launcher::SearchText() = sq;
    // (SSR_LIST_FAKE=n: n games from the first, the middle one chosen, for the test picture.)
    if (const char* fk = getenv("SSR_LIST_FAKE")) { const int nf = atoi(fk); if (!games.empty() && nf > 1) { const Launcher::Game b0 = games[0]; for (int i = 1; i < nf; i++) { Launcher::Game g = b0; g.rom += "#" + std::to_string(i); g.customName = "Test Game " + std::to_string(i + 1); games.push_back(g); } selected = nf / 2; } }
    // (SSR_LIST_POPUP=n: the chosen game's menu open on row n, for the test picture.)
    if (const char* pp = getenv("SSR_LIST_POPUP")) Launcher::Focus().popup = atoi(pp);
    // (SSR_LIST_PAGE=side,row: a page of the sidebar open on that row, for the test picture.)
    if (const char* pg = getenv("SSR_LIST_PAGE")) { Launcher::Focus().page = atoi(pg); if (const char* r = strchr(pg, ',')) Launcher::Focus().prow = atoi(r + 1); }
    // (SSR_LIST_HUB=code: the Hub signing in with its code shown; sync: every save synced at once (with
    // conflict: the first then shown as changed on both), for the test pictures.)
    // (Test: an accent shown, not kept.)
    // (Test: SSR_LIST_LOOK=key=value;key=value as in fulldex-look.txt, shown, not kept; SSR_LIST_LOOKEDIT: a wheel or the slider open.)
    if (const char* lo = getenv("SSR_LIST_LOOK"))
    {
        Launcher::LookLoad();
        std::map<std::string, std::string> pairs;
        std::string v = lo;
        for (size_t at = 0; at <= v.size();)
        {
            size_t end = v.find(';', at);
            if (end == std::string::npos) end = v.size();
            const std::string kv = v.substr(at, end - at);
            const size_t eq = kv.find('=');
            if (eq != std::string::npos) pairs[kv.substr(0, eq)] = kv.substr(eq + 1);
            at = end + 1;
        }
        Launcher::LookFrom(pairs);
    }
    // (Test: the Launcher page's sub-page; a theme saved as the list is first drawn.)
    if (const char* lg = getenv("SSR_LIST_LANG")) UiLang::Set(lg); // (test: the launcher in a language)
    if (const char* lm = getenv("SSR_LIST_LAUNCHER")) { Launcher::LauncherMode() = atoi(lm); Launcher::ThemesRefresh(); }
    // (Test: SSR_LIST_GALLERY_CODE=a code: typed in at Enter A Theme Code; SSR_LIST_GALLERY_SHARE=a theme of My Themes: shared.)
    if (const char* gc = getenv("SSR_LIST_GALLERY_CODE")) Launcher::GalleryEnterCode(gc);
    if (const char* gs = getenv("SSR_LIST_GALLERY_SHARE")) Launcher::GalleryShare(gs);
    if (const char* ts = getenv("SSR_LIST_THEMESAVE")) { Launcher::LookLoad(); Launcher::ThemeSaveStart(ts); }
    if (const char* ta = getenv("SSR_LIST_THEMEAPPLY")) { Launcher::LookLoad(); Launcher::ThemesRefresh(); Launcher::LookApplyTheme(ta); }
    if (const char* le = getenv("SSR_LIST_LOOKEDIT")) { Launcher::LookLoad(); Launcher::Look().edit = atoi(le); Launcher::Look().field = 1; Launcher::LookSyncHsv(); }
    if (const char* hb = getenv("SSR_LIST_HUB"))
    {
        Launcher::HubLoad();
        Launcher::HubState& H = Launcher::Hub();
        Launcher::HubMode() = !strncmp(hb, "trade", 5) ? 2 : !strncmp(hb, "move", 4) || !strcmp(hb, "top") ? 0 : 1;
        if (!strcmp(hb, "tradehost")) Launcher::TradeDo(2, 0);
        if (!strcmp(hb, "tradejoin")) Launcher::TradeDo(2, 1);
        if (!strcmp(hb, "tradeguide")) Launcher::TradeDo(2, 4);
        if (!strncmp(hb, "trade", 5) || !strcmp(hb, "top")) {}
        else if (!strncmp(hb, "move", 4))
        {
            // (move:from,to opens Move Pokemon; moved:from,to also moves the box's first Pokemon, for real, in the test folder's save copies.)
            Launcher::MoverShow(true);
            if (const char* q = strchr(hb, ':')) { Launcher::Mover().from = atoi(q + 1); if (const char* r = strchr(q, ',')) Launcher::Mover().to = atoi(r + 1); }
            if (!strncmp(hb, "moved", 5)) Launcher::MoverDo(Launcher::MOVER_FIRST, 0);
            if (!strncmp(hb, "moveparty", 9)) Launcher::Mover().box = -1;
            if (!strncmp(hb, "movepartymoved", 14)) { Launcher::Mover().box = -1; Launcher::MoverDo(Launcher::MOVER_FIRST, 0); }
            if (!strncmp(hb, "moveswapdo", 10)) Launcher::Mover().box = 0;
            if (!strncmp(hb, "moveswap", 8)) { const auto acts = Launcher::MoverActions(Launcher::MOVER_FIRST); for (size_t k = 0; k < acts.size(); k++) if (acts[k].rfind("Swap With", 0) == 0) Launcher::MoverDo(Launcher::MOVER_FIRST, (int)k); if (!strncmp(hb, "moveswapdo", 10)) Launcher::MoverDo(Launcher::MOVER_FIRST, 0); }
        }
        else if (!strcmp(hb, "code")) { std::lock_guard<std::mutex> l(H.m); H.busy = 1; H.code = "BQXW-KTRM"; H.url = "https://www.google.com/device"; }
        else
        {
            std::vector<std::string> paths;
            for (const Launcher::HubGame& g : Launcher::HubGames()) paths.push_back(g.path);
            Launcher::HubSyncJob(paths, -1);
            if (!strcmp(hb, "conflict") && !paths.empty())
            {
                std::lock_guard<std::mutex> l(H.m);
                CloudSaves::Result& r = H.results[paths[0]];
                r.outcome = CloudSaves::Outcome::Conflict;
                r.local.savedAt = (int64_t)time(nullptr) - 3600; r.local.device = "Windows PC";
                r.cloud.savedAt = (int64_t)time(nullptr) - 600; r.cloud.device = "Nintendo Switch";
                H.msg = "Synced. 1 changed on both: choose which to keep.";
            }
        }
    }
    // (SSR_LIST_DIALOG=n: a dialog open, for the test picture: 1 Remove Game, 2 Delete, 3 Rename.)
    int testBrowseDialog = 0;
    if (const char* dd = getenv("SSR_LIST_DIALOG")) { Launcher::Focus().dialog = atoi(dd); Launcher::Focus().text = "My HeartGold Run"; if (Launcher::Focus().dialog == 16) Launcher::OpenSortMenu(""); if (Launcher::Focus().dialog == 19 || Launcher::Focus().dialog == 20) testBrowseDialog = Launcher::Focus().dialog; }
    if (Launcher::Focus().dialog == 9) Launcher::OpenTabMenu(tab); // (test: 9 the shown tab's menu)
    if (const char* nt = getenv("SSR_LIST_NOTE")) note = nt; // (test: a message showing)
    if (const char* cs = getenv("SSR_TEST_COPYSAVES")) { std::string said; const int n = Launcher::CopySavesTo(fo.saves, cs, said); fprintf(stderr, "test: copysaves %d %s\n", n, said.c_str()); } // (test: SAVES FOLDER's copy)
    // (Tests: SSR_LIST_SAVES=game[,slot]: Manage Saves open on it; SSR_LIST_SAVESDO=row:n or slot:n, that done.)
    if (const char* sv = getenv("SSR_LIST_SAVES"))
    {
        const int gi = atoi(sv);
        if (gi >= 0 && gi < (int)games.size())
        {
            Launcher::OpenSaves(games[gi], fo.saves + "/" + games[gi].saveName);
            if (const char* k = strchr(sv, ',')) Launcher::Focus().pact = atoi(k + 1);
            if (const char* all = getenv("SSR_LIST_SAVESDO"))
            {
                // (Several, between |: row:n, slot:n, pick:<what a dialog row does>.)
                std::string rest = all;
                while (!rest.empty())
                {
                    const size_t bar = rest.find('|');
                    const std::string d = rest.substr(0, bar);
                    rest = bar == std::string::npos ? std::string() : rest.substr(bar + 1);
                    if (!d.compare(0, 4, "row:")) { Launcher::Focus().prow = atoi(d.c_str() + 4); Launcher::SavesEnter(atoi(d.c_str() + 4)); }
                    else if (!d.compare(0, 5, "slot:")) { Launcher::Focus().pact = atoi(d.c_str() + 5); Launcher::SavesSlotDo(atoi(d.c_str() + 5)); }
                    else if (!d.compare(0, 5, "pick:")) Launcher::SavesPick(d.substr(5));
                    fprintf(stderr, "test: saves %s: %s (request %s)\n", d.c_str(), Launcher::Saves().msg.c_str(), Launcher::Saves().request.c_str());
                    Launcher::Saves().request.clear();
                }
            }
        }
    }
    // (SSR_LIST_ITEM=n: the chosen game's menu row n chosen at once, for test pictures of its dialogs.)
    const char* testItem = listsShown == 1 ? getenv("SSR_LIST_ITEM") : nullptr;
    // (The game chosen last, if it is in that tab; else the tab's first. The games are in the
    // view's order.)
    if (selected < 0 || selected >= (int)games.size() || !Launcher::InTab(games, selected, tab))
    {
        selected = -1;
        for (size_t i = 0; i < games.size() && selected < 0; i++)
            if (Launcher::InTab(games, (int)i, tab)) selected = (int)i;
    }
    // L and R (Q and E, or a controller's shoulder buttons; a tap on a tab): the next tab, keeping
    // the chosen game if it is in it.
    // (why: said in game.log, with the tab.)
    bool onTabs = false; // the tabs chosen by the arrows (up from the first row of games)
    auto setTab = [&](int t, const char* why) {
        tab = (t + Launcher::TabCount()) % Launcher::TabCount();
        fprintf(stderr, "[launcher] tab %s (%s)\n", Launcher::TabName(tab), why);
        scroll = 0;
        follow = true;
        const Launcher::View v = view();
        if (v.SpotOf(selected) < 0) selected = Launcher::FirstIn(v);
        // (A tab without games: the arrows stay on the tabs, to go on to the next.)
        if (Launcher::FirstIn(v) < 0) onTabs = true;
        note.clear();
    };
    // Held directions repeat, as on the Switch.
    int held = 0, heldDir = 0;
    Uint32 lastScan = SDL_GetTicks();
    bool quit = false, appBackgrounded = false;
    // The settings page (Launcher::SettingsPage): H, F1 or a controller's Y; Esc or B back.
    // (Touch: a finger pressed at downY, dragged far enough to scroll the page.)
    int downY = 0, downScroll = 0;
    GridScrollMotion gridMotion;
    bool pressed = false, dragging = false;
    int tapPlay = -1; // (the game a finger is on that was already chosen: played when it lifts)
    bool guide = false;
    Launcher::SettingsPage page;
    Menu setMenu;             // (the glass launcher: the page as the pause menu's settings pages)
    std::vector<int> setMap;
    // The keys and buttons changed in a game's CONTROLS (controls.h): A plays, B goes back, Y opens
    // the settings, the directions move, as they do in the game. Keys and buttons not changed
    // keep doing what they always did here.
    Controls::Bindings ctl;
    ctl.Load(fo.cfg);
    float trig[2] = {}; // (the triggers as last seen: one pushed past half way is a press)
    // The title band's SETTINGS (and BACK on the settings page) can be chosen like a game: up
    // from the top row (the first option), then A.
    bool onSettings = false, onBack = false; // (the title band's SETTINGS / BACK: by the D-pad)
    auto toggleGuide = [&]() {
        guide = !guide;
        onBack = false;
        if (guide) page.Open(fo.cfg);
    };
    // (SSR_LIST_GUIDE=n: the settings open, on group n (-1: the list of groups), for the test picture.)
    if (const char* gd = getenv("SSR_LIST_GUIDE")) { guide = true; page.Open(fo.cfg); if (atoi(gd) >= 0) page.Enter(atoi(gd)); }
    // KEYS AND BUTTONS (the settings page's CONTROLS): each action's key or controller button,
    // changed as in the pause menu (main.cpp): choose one, then press what it should be (ESC or 5
    // seconds: as it was). Kept in the settings file's map_ lines (Controls::Bindings::SaveTo).
    bool keysOpen = false, keysKeyboard = false;
    int keysCapture = -1;
    Uint32 keysUntil = 0;
    Menu keysMenu;
    const int keysFirst = 1, keysReset = keysFirst + Controls::COUNT, keysBack = keysReset + 1;
    auto keysStyle = [&]() { return Controls::StyleOf(pads.empty() ? nullptr : pads[0]); };
    auto keysBuild = [&]() {
        keysMenu.items.clear();
        keysMenu.items.push_back({ "CHANGING", keysKeyboard ? "KEYBOARD" : "CONTROLLER" });
        for (int a = 0; a < Controls::COUNT; a++)
            keysMenu.items.push_back({ Controls::ActionName(a),
                keysCapture == a ? (keysKeyboard ? "PRESS A KEY" : "PRESS A BUTTON")
                : keysKeyboard ? Controls::KeyName(ctl.Key(a)) : Controls::InputName(ctl.Pad(a, ctl.byLabel), keysStyle()) });
        keysMenu.items.push_back({ "RESET ALL", "" });
        keysMenu.items.push_back({ "BACK", "" });
    };
    auto keysSaved = [&]() {
        ctl.SaveTo(fo.cfg);
        page.Open(fo.cfg, true); // (the settings page's copy of the file, so it keeps these)
        keysBuild();
    };
    auto keysActivate = [&](int row) {
        if (row == 0) keysKeyboard = !keysKeyboard;
        else if (row >= keysFirst && row < keysReset)
        {
            keysCapture = row - keysFirst;
            keysUntil = SDL_GetTicks() + 5000;
        }
        else if (row == keysReset)
        {
            ctl.Reset();
            keysSaved();
        }
        else if (row == keysBack) keysOpen = false;
        keysBuild();
    };
    // What is pressed while KEYS AND BUTTONS is up: a key or button to give (in = Controls::IN_*,
    // or -1 for a key), else moving through it. True if it was taken.
    auto keysInput = [&](int sc, int in) {
        if (!keysOpen) return false;
        if (keysCapture >= 0)
        {
            if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_AC_BACK) keysCapture = -1;
            else if (sc > 0 && keysKeyboard) { ctl.SetKey(keysCapture, sc); keysCapture = -1; keysSaved(); }
            else if (in >= 0 && !keysKeyboard) { ctl.SetPad(keysCapture, in, ctl.byLabel); keysCapture = -1; keysSaved(); }
            keysBuild();
            return true;
        }
        MenuInput nav;
        const int act = in >= 0 ? (ctl.PadCustom() ? ctl.ActionOfPad(in, ctl.byLabel)
                                   : in == Controls::IN_A ? Controls::A : in == Controls::IN_B ? Controls::B
                                   : in == Controls::IN_UP ? Controls::UP : in == Controls::IN_DOWN ? Controls::DOWN
                                   : in == Controls::IN_LEFT ? Controls::LEFT : in == Controls::IN_RIGHT ? Controls::RIGHT : -1)
                      : -1;
        nav.up = sc == SDL_SCANCODE_UP || act == Controls::UP;
        nav.down = sc == SDL_SCANCODE_DOWN || act == Controls::DOWN;
        nav.left = sc == SDL_SCANCODE_LEFT || act == Controls::LEFT;
        nav.right = sc == SDL_SCANCODE_RIGHT || act == Controls::RIGHT;
        nav.accept = sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || act == Controls::A || act == Controls::START;
        nav.back = sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_AC_BACK || act == Controls::B || act == Controls::SELECT;
        if (nav.back) { keysOpen = false; return true; }
        int dir = 0;
        const int hit = keysMenu.Update(nav, dir);
        if (hit >= 0) keysActivate(hit);
        return true;
    };
    // RETROACHIEVEMENTS' ACCOUNT (the settings page): LOG IN typed here as in the pause menu (the
    // user name, then the password, shown only as a count), sent on a thread of its own
    // (Achievements::LoginForToken); the name and token kept in the settings file as the pause
    // menu keeps them (ra_user=, ra_token=), with ra=1. Chosen when logged in: LOG OUT.
    struct RaLogin
    {
        std::atomic<bool> busy { false }, done { false };
        bool ok = false;
        std::string user, token, error;
    };
    std::shared_ptr<RaLogin> raState = std::make_shared<RaLogin>();
    bool raOpen = false;
    int raField = 0; // 0 the user name, 1 the password
    std::string raUser, raPass, raNote;
    Menu raMenu;
    auto raBuild = [&]() {
        raMenu.items.clear();
        std::string u = raUser.size() > 18 ? raUser.substr(0, 18) : raUser;
        raMenu.items.push_back({ "USER NAME", (u.empty() ? std::string("...") : u) + (raField == 0 ? "_" : ""), true });
        raMenu.items.push_back({ "PASSWORD", std::to_string(raPass.size()) + " CHARACTERS" + (raField == 1 ? "_" : ""), true });
        raMenu.items.push_back({ raState->busy ? "LOGGING IN..." : "LOG IN", "" });
        raMenu.items.push_back({ "CANCEL", "" });
        if (!raState->busy) raMenu.selected = std::min(raMenu.selected, 3);
    };
    auto raAccountShown = [&]() -> std::string {
        if (raState->busy) return "LOGGING IN...";
        const std::string user = Launcher::ConfigValue(fo.cfg, "ra_user");
        if (!Launcher::ConfigValue(fo.cfg, "ra_token").empty())
        {
            std::string u = user.size() > 14 ? user.substr(0, 14) : user;
            for (char& c : u) c = (char)toupper((unsigned char)c);
            return "LOG OUT " + u;
        }
        return "LOG IN";
    };
    auto raClose = [&]() {
        raOpen = false;
        std::fill(raPass.begin(), raPass.end(), '\0');
        raPass.clear();
        SDL_StopTextInput();
    };
    auto raSubmit = [&]() {
        if (raState->busy) return;
        if (raUser.empty()) { raField = 0; raNote = "TYPE YOUR USER NAME"; return; }
        if (raPass.empty()) { raField = 1; raNote = "TYPE YOUR PASSWORD"; return; }
        raNote = "LOGGING IN...";
        raState = std::make_shared<RaLogin>();
        raState->busy = true;
        std::shared_ptr<RaLogin> st = raState;
        const std::string user = raUser, pass = raPass;
        std::fill(raPass.begin(), raPass.end(), '\0');
        raPass.clear();
        std::thread([st, user, pass]() {
            std::string error;
            st->ok = Achievements::LoginForToken(user, pass, std::string("PokemonDSWidescreen/") + SSR_RA_VERSION, st->user, st->token, st->error);
            st->busy = false;
            st->done = true;
        }).detach();
        raBuild();
    };
    // The login's answer, looked at each pass.
    auto raPoll = [&]() {
        if (!raState->done) return;
        raState->done = false;
        if (raState->ok)
        {
            SetConfigValues(fo.cfg, { { "ra", "1" }, { "ra_user", raState->user }, { "ra_token", raState->token } });
            page.Open(fo.cfg, true);
            raNote.clear();
            raClose();
        }
        else
        {
            std::string err = raState->error;
            for (char& c : err) c = (char)toupper((unsigned char)c);
            raNote = "LOGIN FAILED: " + err;
            raField = 1;
            raBuild();
        }
    };
    auto raActivate = [&](int row) {
        if (row == 0 || row == 1) raField = row;
        else if (row == 2) raSubmit();
        else if (row == 3) raClose();
        if (raOpen) raBuild();
    };
    // The list's look (GAME LIST on the settings page: COLOUR, BACKGROUND), read again when one
    // of them changes or the ROM folder (where a phone's picture is) does.
    Launcher::Theme theme;
    Launcher::Background background;
    Launcher::LoadLook(fo.cfg, fo.art, theme, background);
    auto change = [&](int d) {
        const char* key = page.Change(d);
        if (key && !strncmp(key, "list_", 5)) Launcher::LoadLook(fo.cfg, fo.art, theme, background);
        if(key && !strcmp(key,"list_sort"))reorderLibrary();
        if(key && !strcmp(key,"!favourite"))toggleFavourite();
        if (key && !strcmp(key, "!ra_login"))
        {
            if (raState->busy) return;
            if (!Launcher::ConfigValue(fo.cfg, "ra_token").empty())
            {
                // LOG OUT: the token forgotten here (the pause menu's LOG OUT does the same).
                SetConfigValues(fo.cfg, { { "ra_token", "" } });
                page.Open(fo.cfg, true);
                return;
            }
            raOpen = true;
            raField = 0;
            raUser = Launcher::ConfigValue(fo.cfg, "ra_user");
            raPass.clear();
            raNote.clear();
            raMenu.selected = raUser.empty() ? 0 : 1;
            if (!raUser.empty()) raField = 1;
            SDL_StartTextInput();
            raBuild();
            return;
        }
        if (key && !strcmp(key, "!keys"))
        {
            ctl.Load(fo.cfg);
            keysOpen = true;
            keysCapture = -1;
            keysKeyboard = pads.empty() && joys.empty();
            keysMenu.selected = keysFirst;
            keysMenu.open = true;
            keysBuild();
            return;
        }
#if !defined(SSR_PHONE) && !defined(__SWITCH__)
        // DESKTOP SHORTCUTS: made at once; what came of it shown as the option's value.
        if (key && !strcmp(key, "!shortcuts"))
        {
            std::string where;
            const int made = Shortcuts::Make(fo.dir, games, where);
            page.items[page.selected].values[0].second =
                games.empty() ? "NO GAMES YET" : made > 0 ? std::to_string(made) + " MADE " + where : "FAILED: " + where;
            return;
        }
#endif
#if !defined(__ANDROID__) && !defined(SSR_IOS)
        // SAVES FOLDER: a folder of the player's own (copied into, nothing replaced), used from the next start; again: back beside the ROMs.
        if (key && !strcmp(key, "!saves_folder"))
        {
            std::string& shown = page.items[page.selected].values[0].second;
            if (!Launcher::ConfigValue(fo.cfg, "saves_folder").empty()) { SetConfigValues(fo.cfg, { { "saves_folder", "" } }); shown = "BESIDE THE ROMS FROM THE NEXT START"; return; }
#ifdef _WIN32
            const std::string dir = PickFolder(window);
            if (dir.empty()) return;
            std::string said;
            const int copied = Launcher::CopySavesTo(fo.saves, dir, said);
            if (copied < 0) { shown = said; return; }
            SetConfigValues(fo.cfg, { { "saves_folder", dir } });
            fprintf(stderr, "[launcher] saves folder: %s (%d files copied from %s)\n", dir.c_str(), copied, fo.saves.c_str());
            shown = std::to_string(copied) + " FILES COPIED: USED FROM THE NEXT START";
#else
            shown = "SET SAVES_FOLDER= IN SETTINGS.CFG";
#endif
            return;
        }
#endif
#if SDL_VERSION_ATLEAST(2, 0, 14)
        if (key && !strcmp(key, "!releases"))
        {
            if (SDL_OpenURL(Launcher::kReleasesUrl) != 0) fprintf(stderr, "[launcher] could not open the release page: %s\n", SDL_GetError());
            return;
        }
#endif
#ifdef __ANDROID__
        if (key && !strcmp(key, "!updates")) { JavaVoid("checkUpdates"); return; }
#elif defined(SSR_UPDATER)
        // CHECK FOR UPDATES: checks, then installs (Windows, Linux) or opens the release's page.
        if (key && !strcmp(key, "!updates"))
        {
            const std::string url = Updater::Choose();
#if SDL_VERSION_ATLEAST(2, 0, 14)
            if (!url.empty() && SDL_OpenURL(url.c_str()) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", url.c_str(), SDL_GetError());
#endif
            return;
        }
#endif
        // (BORDERLESS FULLSCREEN applies to this window too, as F11 does.)
        if (key && !strcmp(key, "fullscreen"))
        {
            int w = 0, h = 0;
            fullscreen = ConfigFullscreen(fo.cfg, w, h);
            SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }
    };
    std::function<void()> savesRequestFn; // (savesRequest, below: what Manage Saves leaves to do)
    auto launchSelected = [&]() {
        if (games.empty() || selected < 0 || selected >= (int)games.size()) return;
#ifdef _WIN32
        // Omega Ruby and Alpha Sapphire: their code is made once from the dump (n3ds.h), the list
        // staying open and saying how it goes; then they play.
        if (games[selected].platform == Launcher::N3DS && !games[selected].comingSoon)
        {
            if (N3ds::Busy()) { note = N3ds::Status(); if (note.empty()) note = "ONE GAME IS BEING PREPARED: WAIT FOR IT TO FINISH"; return; }
            if (!N3ds::Prepared(games[selected])) { N3ds::StartPrepare(fo.dir, games[selected]); note = N3ds::Status(); return; }
            result = selected;
            quit = true;
            return;
        }
#elif defined(__ANDROID__)
        // Omega Ruby and Alpha Sapphire, inside the app (the Omega Ruby kit): the dump copied into the
        // app once, then each time the game's own screen, the list waiting behind it.
        if (games[selected].platform == Launcher::N3DS && !games[selected].comingSoon)
        {
            const Launcher::Game& g = games[selected];
            std::string name = Launcher::TitleCase(g.name);
            for (char& c : name) c = (char)toupper((unsigned char)c);
            if (JavaBool("threeDsBusy")) { note = JavaString("threeDsNote"); if (note.empty()) note = "WAIT FOR " + name + " TO FINISH"; return; }
            if (!N3ds::Added(g))
            {
                double needGb = 0;
                char gb[16];
                if (!N3ds::RoomFor(g, fo.dir, needGb)) { snprintf(gb, sizeof gb, "%.1f", needGb); note = "NOT ENOUGH SPACE: " + name + " NEEDS " + gb + " GB FREE ON THE PHONE"; return; }
                // (Copied in once; it starts by itself when the copy is done.)
                JavaStringArg("threeDsAdd", g.rom);
                N3dsAutoPlay() = g.folder;
                note = "ADDING " + name + " TO FULLDEX (ONE TIME, ABOUT 2 GB): IT STARTS WHEN READY";
                return;
            }
            JavaStringArg("threeDsPlay", N3ds::IdOf(g));
            note = "STARTING " + name;
            return;
        }
#endif
        if (Playable(fo, games[selected]))
        {
            // (A ROM not yet checked is, now: a hack plays with a save of its own.)
            Launcher::ConfirmHack(games, (size_t)selected, fo.saves,
                                  [](const Launcher::Game& g, const std::string& name) { return g.folder + "/" + name; });
            if (Launcher::SavesPlayCheck(games[selected], fo.saves + "/" + games[selected].saveName)) return; // (a save with a problem: says so first)
            note = PrepareLaunch(games[selected]);
            if (!note.empty()) { fprintf(stderr, "[launcher] launch blocked: %s: %s\n", games[selected].rom.c_str(), note.c_str()); return; }
            result = selected;
            quit = true;
        }
        else
            note = games[selected].comingSoon ? games[selected].name + ": COMING SOON"
                 : !games[selected].problem.empty() ? "PATCH: " + games[selected].problem
                 : games[selected].known ? "THIS DOWNLOAD DOES NOT HAVE THIS GAME" : "THIS GAME IS NOT ONE THAT THIS PLAYS";
    };
    auto choose = [&]() {
        gridMotion.Stop();
        if (guide && onBack)
        {
            // (BACK: from a group to the groups, from the groups to the list.)
            if (page.Back()) onBack = false;
            else toggleGuide();
            return;
        }
        if (guide)
        {
            change(1);
            return;
        }
        // The glass launcher: A on a section of the sidebar, or on the banner's buttons.
        if (Launcher::Glassy(canvas))
        {
            Launcher::GlassFocus& F = Launcher::Focus();
            // A page or sidebar selection takes precedence over the Home tab bar.
            // Touch navigation can leave its previous keyboard focus behind.
            if (F.page >= 0 || F.side >= 0) { onTabs = onSettings = false; F.onSort = false; }
            if (onTabs && F.onSort && F.tool == 1 && !Launcher::TabHidden().empty()) { Launcher::OpenPlusMenu(); return; } // (+: new, or one hidden shown)
            if (onTabs && F.onSort && F.tool != 3)
            {
                // (The tab bar's + and Search: the keyboard, for a new tab or the search.)
                F.kbFor = F.tool == 1 ? 1 : 2;
                F.kbRom.clear();
                F.text = F.tool == 2 ? Launcher::SearchText() : std::string();
                F.dialog = 3; F.drow = 0; SDL_StartTextInput();
                return;
            }
            if (onTabs && !F.onSort) { Launcher::OpenTabMenu(tab); return; } // (a tab: move, hide; one of your own also rename, delete)
            if (onTabs && F.onSort)
            {
                // (Sort on the tab bar: a menu of the orders; what is chosen is kept in the settings as SORT is.)
                Launcher::OpenSortMenu(Launcher::ConfigValue(fo.cfg, "list_sort"));
                return;
            }
            // (A in a page's list: Report A Bug and Website open in the browser.)
            if (F.page >= 0 && F.side < 0)
            {
                if (F.page == Launcher::SIDE_ROMS)
                {
                    // (The ROMs page: A on the list goes to the chosen ROM's actions; on those, does it.)
                    const std::vector<Launcher::Game>& roms = Launcher::RomList();
                    if (F.pact < 0) { if (Launcher::RomActions() > 0) F.pact = 0; return; }
                    if (Launcher::CanChooseRomFolder() && F.pact == 0)
                    {
#ifdef __ANDROID__
                        // Keep saves at their current location when changing the shared ROM library.
                        SetConfigValues(fo.cfg, {{"saves_folder", fo.saves}});
                        chooseRomFolder();
#elif defined(_WIN32)
                        const std::string dir = PickFolder(window, L"Choose the folder containing your ROMs");
                        if (!dir.empty()) useRomFolder(dir);
#else
                        folderBrowse = fo.roms; folderFirst = 0; showRomFolder();
#endif
                        return;
                    }
                    if (F.prow < 0 || F.prow >= (int)roms.size()) return;
                    const std::string rom = roms[F.prow].rom;
                    const bool was = roms[F.prow].hidden;
                    if (F.pact == (Launcher::CanChooseRomFolder() ? 1 : 0))
                    {
                        const bool ok = Launcher::SetPair(fo.saves + "/fulldex-hidden.txt", rom, was ? "" : "1");
                        games = FindGames(fo, artCache);
                        Launcher::BuildRomList(games, fo.saves);
                        reorderLibrary();
                        note = !ok ? "COULD NOT SAVE THE LIST" : was ? "BACK IN THE LIST" : "HIDDEN FROM THE LIST";
                    }
                    else
                    {
                        std::string url = "file:///" + rom.substr(0, rom.find_last_of("/\\"));
                        for (char& ch : url) if (ch == '\\') ch = '/';
                        if (SDL_OpenURL(url.c_str()) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", url.c_str(), SDL_GetError());
                    }
                    return;
                }
                if (F.page == Launcher::SIDE_LAUNCHER)
                {
                    // (Your Picture just chosen: the system's picker for it.)
                    const int ld = Launcher::LookDo(F.prow, 1);
                    if (ld == 2) SDL_StartTextInput(); // (My Themes: the theme's name)
#ifdef _WIN32
                    if (ld == 4) { const std::string zip = PickZipFile(window); if (!zip.empty()) Launcher::ThemeImport(zip); }
#endif
                    if (ld == 3)
                    {
#ifdef _WIN32
                        const std::string path = PickPictureFile(window);
                        if (!path.empty() && !Launcher::LookAddPicture(path)) note = "COULD NOT USE THAT PICTURE";
#endif
                    }
                    return;
                }
                if (F.page == Launcher::SIDE_HUB)
                {
                    // (The Hub: A on the list goes to the chosen row's actions; on those, does it.)
                    if (F.pact < 0)
                    {
                        if (Launcher::HubEnter(F.prow)) { if (savesRequestFn) savesRequestFn(); return; }
                        if (!Launcher::HubActions(F.prow).empty()) F.pact = 0;
                        else if (Launcher::HubTop() && F.prow == 3 && SDL_OpenURL(Launcher::kDiscordUrl) != 0) fprintf(stderr, "[launcher] could not open Discord: %s\n", SDL_GetError());
                        return;
                    }
                    const std::string said = Launcher::HubDo(F.prow, F.pact);
                    if (!said.empty()) note = said;
                    if (F.dialog == 3) SDL_StartTextInput(); // (the Trade page's code)
                    return;
                }
                if (F.page == Launcher::SIDE_ABOUT && F.prow == 0) { Launcher::OpenSupportAsk(); return; } // (Buy Me A Coffee or PayPal)
                const std::string url = Launcher::PageUrl(F.page, F.prow);
                if (!url.empty() && SDL_OpenURL(url.c_str()) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", url.c_str(), SDL_GetError());
                return;
            }
            if (F.side >= 0)
            {
                if (F.side == Launcher::SIDE_SETTINGS) toggleGuide();
                else if (Launcher::OpenSide(F.side)) {}
                else note = std::string(Launcher::SideName(F.side)) + ": COMING SOON";
                return;
            }
            if (F.banner == 2)
            {
                // (Game Options: the menu of the game the banner offers.)
                const int cg = Launcher::ContinueGame(games, selected);
                if (cg >= 0) { selected = cg; F.banner = 0; F.popup = 0; follow = true; }
                return;
            }
            bool direct = false; // (Continue, or Play in the game's menu: played at once)
            if (F.banner == 1)
            {
                F.banner = 0;
                direct = true;
                const int cg = Launcher::ContinueGame(games, selected);
                if (cg >= 0) selected = cg;
            }
            if (F.popup >= 0)
            {
                const int item = Launcher::PopupItem(F.popup); // (the row shown -> what it does)
                F.popup = -1;
                if (item == 1) { toggleFavourite(); return; }
                const bool valid = selected >= 0 && selected < (int)games.size();
                if (item == 2 && valid) { F.kbFor = 0; F.dialog = 3; F.drow = 0; F.text = Launcher::ShortName(games[selected]); SDL_StartTextInput(); return; }
                if (item == 5 && valid) { Launcher::OpenAddToTab(games[selected].rom); return; }
                if (item == 6 && valid) { F.dialog = 1; F.drow = 0; return; }
#ifdef __ANDROID__
                // (Android's 3DS games: Use Game Code From PC (the FullDex Builder's .so) and Build Game Code.)
                if ((item == 3 || item == 4) && valid && games[selected].platform == Launcher::N3DS)
                {
                    const Launcher::Game& g = games[selected];
                    if (JavaBool("threeDsBusy")) { note = JavaString("threeDsNote"); if (note.empty()) note = "WAIT FOR THE GAME TO FINISH"; return; }
                    if (!N3ds::Added(g)) { note = "CHOOSE THE GAME ONCE FIRST: THAT ADDS IT TO FULLDEX"; return; }
                    if (item == 3) JavaStringArg("threeDsImport", N3ds::IdOf(g));
                    else if (g.built || JavaBoolStringArg("threeDsBuilt", N3ds::IdOf(g))) {} // (Game Code Built: nothing to do)
                    else JavaStringArg("threeDsBuild", N3ds::IdOf(g));
                    return;
                }
#endif
                if (item == 3 && valid) { F.dialog = 4; F.drow = 0; return; }
                if (item == 4 && valid && games[selected].platform == Launcher::N3DS)
                {
                    note = "3DS SAVES ARE KEPT IN THE GAME'S OWN FOLDER";
                    return;
                }
                if (item == 4 && valid)
                {
                    // Manage Saves: the game's saves on a page of their own (saves_page.h).
                    Launcher::OpenSaves(games[selected], fo.saves + "/" + games[selected].saveName);
                    return;
                }
                if (item >= 2) { if (valid) note = Launcher::PopupLabel(games[selected], item) + ": COMING SOON"; return; }
                direct = true;
            }
            else if (!direct && !onTabs && !onSettings && selected >= 0 && selected < (int)games.size() && games[selected].known && !games[selected].comingSoon)
            {
                F.popup = 0; // (A on a game: its menu, Play first)
                note.clear();
                return;
            }
        }
        if (onSettings)
        {
            toggleGuide();
            return;
        }
        if (onTabs)
        {
            // (A on a tab: down into its games.)
            if (Launcher::FirstIn(view()) >= 0)
            {
                onTabs = false;
                if (view().SpotOf(selected) < 0) selected = Launcher::FirstIn(view());
            }
            return;
        }
#ifdef __ANDROID__
        // (Only with no game to play. With a ROM in the app's own folder and no ROMs folder
        // chosen yet, Play asked for the folder instead of starting the game.)
        if (games.empty())
        {
            chooseRomFolder();
            return;
        }
#endif
        launchSelected();
    };
    auto move = [&](int dir) { // 1 left, 2 right, 3 up, 4 down
        gridMotion.Stop();
        if (guide)
        {
            if (onBack)
            {
                if (dir == 4) onBack = false;
                return;
            }
            if (dir == 1 || dir == 2) change(dir == 1 ? -1 : 1);
            else
            {
                const int before = page.selected;
                page.Move(dir == 3 ? -1 : 1);
                if (dir == 3 && page.selected == before) onBack = true; // (up from the first option)
            }
            return;
        }
        // The glass launcher: the sidebar on the left (left from the first column, the first tab or
        // Continue), the banner's Continue and Game Options above the games, the tabs above them.
        if (Launcher::Glassy(canvas))
        {
            Launcher::GlassFocus& F = Launcher::Focus();
            note.clear();
            if (!onTabs) F.onSort = false;
            if (F.popup >= 0)
            {
                if (dir == 3) F.popup = std::max(0, F.popup - 1);
                else if (dir == 4) F.popup = std::min(Launcher::PopupCount() - 1, F.popup + 1);
                return;
            }
            if (Launcher::PageMove(dir)) return;
            const bool banner = Launcher::ContinueGame(games, selected) >= 0;
            const bool panel = Launcher::PanelHome(); const bool tiles = Launcher::TilesHome(); const bool alt = panel || tiles;
            auto intoGames = [&]() {
                const Launcher::View v = view();
                if (Launcher::FirstIn(v) < 0) { if (banner) F.banner = 1; else onTabs = true; return; }
                if (v.SpotOf(selected) < 0) selected = Launcher::FirstIn(v);
                follow = true;
            };
            if (F.side >= 0)
            {
                if (panel || tiles)
                {
                    // (The Panel home's sections along the top: left and right along them, down to the tabs.)
                    if (dir == 1) F.side = std::max(0, F.side - 1);
                    else if (dir == 2) F.side = std::min((int)Launcher::SIDES - 1, F.side + 1);
                    else if (dir == 4 && panel) { F.side = -1; onTabs = true; }
                    else if (dir == 3 && tiles) { F.side = -1; if (banner) F.banner = 1; else intoGames(); }
                }
                else if (dir == 3) F.side = std::max(0, F.side - 1);
                else if (dir == 4) F.side = std::min((int)Launcher::SIDES - 1, F.side + 1);
                else if (dir == 2) { F.side = -1; intoGames(); }
                return;
            }
            if (onTabs)
            {
                if (F.onSort)
                {
                    // (Sort: left back to the tabs, down to the games.)
                    if (panel && dir == 3) { F.onSort = false; onTabs = false; F.side = Launcher::SIDE_HOME; }
                    else if (dir == 1) { if (F.tool > 1) F.tool--; else F.onSort = false; }
                    else if (dir == 2) F.tool = std::min(3, F.tool + 1);
                    else if (dir == 4) { F.onSort = false; onTabs = false; if (banner && !alt) F.banner = 1; else intoGames(); }
                    return;
                }
                if (panel && dir == 3) { onTabs = false; F.side = Launcher::SIDE_HOME; }
                else if (dir == 1 && tab == Launcher::FirstTab()) { if (!alt) { onTabs = false; F.side = Launcher::SIDE_HOME; } }
                else if (dir == 2 && tab == Launcher::LastTab()) { F.onSort = true; F.tool = 1; }
                else if (dir == 1 || dir == 2) setTab(Launcher::TabStep(tab, dir == 2 ? 1 : -1), "the arrows");
                else if (dir == 4) { onTabs = false; if (banner && !alt) F.banner = 1; else intoGames(); }
                return;
            }
            if (F.banner)
            {
                if (dir == 1) { if (F.banner == 2) F.banner = 1; else { F.banner = 0; if (alt) intoGames(); else F.side = Launcher::SIDE_HOME; } }
                else if (dir == 2) F.banner = 2;
                else if (dir == 3) { F.banner = 0; if (tiles) intoGames(); else onTabs = true; }
                else if (tiles && dir == 4) { F.banner = 0; F.side = Launcher::SIDE_HOME; }
                else if (dir == 4 && Launcher::FirstIn(view()) >= 0) { F.banner = 0; intoGames(); }
                return;
            }
            follow = true;
            const Launcher::View v = view();
            const int k = v.SpotOf(selected);
            if (tiles)
            {
                if (dir == 3) { onTabs = true; return; }
                if (dir == 4) { if (banner) F.banner = 1; else F.side = Launcher::SIDE_HOME; return; }
                const int to2 = v.spots.empty() ? -1 : Launcher::Step(v, selected, dir == 1 ? 3 : 4);
                if (to2 >= 0) selected = to2;
                return;
            }
            if (panel && dir == 1) return;
            if (panel && dir == 2 && banner) { F.banner = 1; return; }
            if (dir == 1 && (k < 0 || v.spots[k].x == v.g.gx)) { F.side = Launcher::SIDE_HOME; return; }
            const int to = v.spots.empty() ? -1 : Launcher::Step(v, selected, dir);
            if (to < 0) { if (dir == 3) { if (banner && !alt) F.banner = 1; else onTabs = true; } return; }
            selected = to;
            return;
        }
        if (onSettings)
        {
            if (dir == 4)
            {
                onSettings = false;
                onTabs = true;
            }
            return;
        }
        // The tabs: left and right change the tab, down goes into its games, up to SETTINGS.
        if (onTabs)
        {
            if (dir == 1 || dir == 2) setTab(Launcher::TabStep(tab, dir == 2 ? 1 : -1), "the arrows");
            else if (dir == 3)
            {
                onTabs = false;
                onSettings = true;
            }
            else if (dir == 4 && Launcher::FirstIn(view()) >= 0)
            {
                onTabs = false;
                if (view().SpotOf(selected) < 0) selected = Launcher::FirstIn(view());
            }
            return;
        }
        // Through the tab's games, across its headings (Launcher::Step); up from the top row: the
        // tabs (and up again, the title band's SETTINGS).
        follow = true;
        const Launcher::View v = view();
        const int to = v.spots.empty() ? -1 : Launcher::Step(v, selected, dir);
        if (to < 0)
        {
            if (dir == 3) onTabs = true;
            return;
        }
        if (to != selected)
        {
            selected = to;
            note.clear();
        }
    };
    // The glass launcher's dialogs (Remove Game, Delete ROM File, Rename): what their rows do.
    // (Rename: the pause menu's keyboard page; kbLayer capitals, small letters or signs.)
    Menu kb;
    std::vector<int> kbIds;
    int kbLayer = 0;
    auto kbBuild = [&]() { Launcher::BuildKeyboard(kb, kbIds, Launcher::Focus().text, kbLayer, Launcher::KbPrompt(), Launcher::KbDone()); };
    auto dialogClose = [&]() { Launcher::Focus().dialog = 0; kb.open = false; SDL_StopTextInput(); };
    auto importPicture = [&](const std::string& rom, const std::string& path) {
        std::string error;
        const bool ok = Launcher::ImportArt(rom, fo.saves, path, error);
        fprintf(stderr, "[launcher] picture for %s: %s (%s)\n", rom.c_str(), path.c_str(), ok ? "kept" : error.c_str());
        if (ok) { dialogClose(); reorderLibrary(); note = "PICTURE CHANGED"; }
        else { note = error; Launcher::Focus().dMsg = error; }
    };
    // What Manage Saves leaves for the platform (saves_page.h): a folder opened, or the game
    // started from a state (Play From Slot, Play Anyway).
    auto savesRequest = [&]() {
        std::string q;
        std::swap(q, Launcher::Saves().request);
        if (q.rfind("#folder:", 0) == 0)
        {
#ifdef __ANDROID__
            note = "SAVES ARE IN " + q.substr(8);
#else
            OpenFolder(q.substr(8));
#endif
        }
        else if (q.rfind("#play:", 0) == 0 || q == "#replay")
        {
            int idx = -1;
            for (size_t i = 0; i < games.size(); i++) if (games[i].rom == Launcher::Saves().rom) idx = (int)i;
            if (idx < 0) { note = "THAT GAME IS NOT IN THE LIST ANY MORE"; return; }
            if (q != "#replay") Launcher::PlayState() = q.substr(6);
            Launcher::GlassFocus& G = Launcher::Focus();
            Launcher::HubMode() = 0; G.page = -1; G.side = -1; G.banner = 0; G.pact = -1;
            selected = idx; follow = true;
            G.popup = 0; // (Play, as the game's menu does it)
            choose();
            if (!quit) Launcher::PlayState().clear(); // (not started: no state left over for the next game)
        }
    };
    savesRequestFn = savesRequest;
    auto dialogAct = [&]() {
        Launcher::GlassFocus& F = Launcher::Focus();
        const bool noGame = selected < 0 || selected >= (int)games.size();
        // (A tab's menu, and the keyboard for a tab's name, a new tab or the search, need no game: a tab with none in it is where they are wanted.)
        const bool tabOnly = F.dialog == 19 || F.dialog == 8 || F.dialog == 9 || F.dialog == 13 || F.dialog == 14 || F.dialog == 16 || (F.dialog == 3 && F.kbFor);
        if (noGame && !tabOnly) { dialogClose(); return; }
        if (F.dialog == 19)
        {
            const std::string pick = F.drow >= 0 && F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : "#close";
            if (pick == "#close") { dialogClose(); return; }
            if (pick == "#use") { dialogClose(); useRomFolder(folderBrowse); return; }
            if (pick == "#next") folderFirst += folderStep;
            else if (pick == "#previous") folderFirst = std::max(0, folderFirst - folderStep);
            else { folderBrowse = pick == "#parent" ? std::filesystem::u8path(folderBrowse).parent_path().u8string() : pick; if (folderBrowse.empty()) folderBrowse = "/"; folderFirst = 0; }
            showRomFolder(); return;
        }
        if (F.dialog == 20)
        {
            const std::string pick = F.drow >= 0 && F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : "#close";
            if (pick == "#close") { dialogClose(); return; }
            if (pick == "#next") pictureFirst += pictureStep;
            else if (pick == "#previous") pictureFirst = std::max(0, pictureFirst - pictureStep);
            else
            {
                std::error_code ec;
                if (pick != "#parent" && !std::filesystem::is_directory(std::filesystem::u8path(pick), ec))
                { importPicture(pictureRom, pick); return; }
                pictureBrowse = pick == "#parent" ? std::filesystem::u8path(pictureBrowse).parent_path().u8string() : pick;
                if (pictureBrowse.empty()) pictureBrowse = "/";
                pictureFirst = 0;
            }
            showPictureFolder(); return;
        }
        const std::string rom = noGame ? std::string() : games[selected].rom;
        auto settle = [&]() { const Launcher::View v = view(); if (v.SpotOf(selected) < 0) selected = Launcher::FirstIn(v); };
        if (F.dialog == 1)
        {
            if (F.drow == 0)
            {
                dialogClose();
                const bool ok = Launcher::SetPair(fo.saves + "/fulldex-hidden.txt", rom, "1");
                fprintf(stderr, "[launcher] taken off the list: %s (%s)\n", rom.c_str(), ok ? "saved" : "not saved");
                reorderLibrary();
                settle();
                note = ok ? "REMOVED FROM THE LIST. THE ROM IS STILL IN YOUR FOLDER" : "COULD NOT SAVE THE LIST";
            }
            else if (F.drow == 1) { F.dialog = 2; F.drow = 1; } // (Delete: sure? Cancel chosen first)
            else dialogClose();
            return;
        }
        if (F.dialog == 2)
        {
            dialogClose();
            if (F.drow != 0) return;
            std::error_code ec;
            std::filesystem::remove(std::filesystem::u8path(rom), ec);
            fprintf(stderr, "[launcher] delete %s: %s\n", rom.c_str(), ec ? ec.message().c_str() : "deleted");
            if (ec) { note = "COULD NOT DELETE THE ROM FILE"; return; }
            games.erase(games.begin() + selected);
            selected = -1;
            reorderLibrary();
            settle();
            note = "ROM FILE DELETED. ITS SAVES ARE KEPT";
            return;
        }
        if (F.dialog == 16)
        {
            // (Sort Games By: the order picked.)
            const std::string pick = F.drow >= 0 && F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : std::string("#close");
            dialogClose();
            if (pick.rfind("#sort:", 0) == 0)
            {
                SetConfigValues(fo.cfg, { { "list_sort", pick.substr(6) } });
                reorderLibrary();
                if (guide) page.Open(fo.cfg, true);
            }
            return;
        }
        if (F.dialog == 15)
        {
            // (A theme of My Themes: Apply, Show In Folder, Delete.)
            const std::string pick = F.drow >= 0 && F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : std::string("#close");
            if (pick == "#folder")
            {
                std::string url = "file:///" + Launcher::ThemesDir();
                for (char& ch : url) if (ch == '\\') ch = '/';
                if (SDL_OpenURL(url.c_str()) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", url.c_str(), SDL_GetError());
            }
            if (Launcher::ThemePick(pick) == 1) return;
            dialogClose();
            return;
        }
        if (F.dialog >= 10 && F.dialog <= 12)
        {
            // (The beer message: the page in the browser, or later.)
            const std::string pick = F.drow >= 0 && F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : std::string("#close");
            if (pick == "#beer" && SDL_OpenURL("https://buymeacoffee.com/mamesj") != 0) fprintf(stderr, "[launcher] could not open the support page: %s\n", SDL_GetError());
            if (pick.rfind("#open:", 0) == 0 && SDL_OpenURL(pick.c_str() + 6) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", pick.c_str() + 6, SDL_GetError());
            dialogClose();
            if (pick == "#quit") quit = true; // (Quit Fulldex?: yes)
            return;
        }
        if (F.dialog == 17 || F.dialog == 18)
        {
            // (Manage Saves' dialogs: saves_page.h does them, then the platform's part.)
            const std::string pick = F.drow >= 0 && F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : std::string("#close");
            if (pick == "#pick")
            {
                dialogClose();
#ifdef _WIN32
                const std::string path = PickSaveFile(window);
                if (!path.empty()) Launcher::SavesPick("#import:" + path);
#endif
            }
            else Launcher::SavesPick(pick);
            savesRequest();
            return;
        }
        if (F.dialog >= 5)
        {
            const std::string pick = F.drow >= 0 && F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : std::string("#close");
            const std::string save = noGame ? std::string() : fo.saves + "/" + games[selected].saveName;
            if (pick == "#close" || pick.empty()) { dialogClose(); return; }
            if (F.dialog == 8 || F.dialog == 9 || F.dialog == 13 || F.dialog == 14)
            {
                // (Add To Tab, or a tab of your own's menu.)
                const int r = Launcher::TabDialogPick(pick, tab, note);
                if (r == 2) { kb.open = false; SDL_StartTextInput(); }
                if (r != 1) { scroll = 0; settle(); }
                return;
            }
            if (pick == "#folder")
            {
                const std::string url = "file:///" + fo.saves;
                if (SDL_OpenURL(url.c_str()) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", url.c_str(), SDL_GetError());
                note = "SAVES FOLDER OPENED";
                return;
            }
            if (F.dialog == 5)
            {
                // (Sure? Cancel chosen first.)
                F.dTitle = "Go Back To This Backup?";
                F.dMsg = F.dItems[F.drow].substr(8) + ". Your save as it is now is kept beside it as a .before-restore copy.";
                F.dItems = { "Restore Backup", "Cancel" };
                F.dPaths = { pick, "#close" };
                F.dialog = 6;
                F.drow = 1;
                return;
            }
            std::error_code ec;
            const auto now = std::filesystem::file_size(std::filesystem::u8path(save), ec);
            const auto was = std::filesystem::file_size(std::filesystem::u8path(pick), ec);
            std::vector<unsigned char> restored;
            std::string error;
            const bool ok = SaveBackups::Restore(save, pick, now > 0 ? (size_t)now : (size_t)was, restored, error);
            fprintf(stderr, "[launcher] restore %s from %s: %s\n", save.c_str(), pick.c_str(), ok ? "done" : error.c_str());
            dialogClose();
            note = ok ? "SAVE RESTORED. THE ONE BEFORE IS KEPT AS A .BEFORE-RESTORE COPY" : error;
            return;
        }
        if (F.dialog == 4)
        {
            const std::string artList = fo.saves + "/fulldex-art.txt";
            if (F.drow == 0)
            {
#ifdef _WIN32
                const std::string path = PickPictureFile(window);
                if (path.empty()) return;
                importPicture(rom, path);
#else
                pictureBrowse = fo.roms.empty() ? fo.dir : fo.roms;
                pictureRom = rom; pictureFirst = 0; showPictureFolder();
#endif
                return;
            }
            if (F.drow == 1)
            {
                // (The browser, with a search for the game's box art: the player saves one, then imports it.)
                std::string q = Launcher::ShortName(games[selected]) + " Pokemon box art", url = "https://www.google.com/search?tbm=isch&q=";
                for (unsigned char ch : q)
                    if (isalnum(ch)) url += (char)ch;
                    else if (ch == ' ') url += '+';
                    else { char hex[4]; snprintf(hex, sizeof hex, "%%%02X", ch); url += hex; }
                if (SDL_OpenURL(url.c_str()) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", url.c_str(), SDL_GetError());
                note = "OPENED IN YOUR BROWSER: SAVE A PICTURE, THEN IMPORT IT HERE";
                return;
            }
            if (F.drow == 2 || F.drow == 3)
            {
                const bool ok = Launcher::SetPair(artList, rom, F.drow == 2 ? "placeholder" : "");
                dialogClose();
                reorderLibrary();
                note = !ok ? "COULD NOT SAVE THE PICTURE" : F.drow == 2 ? "PLAIN CARD" : "ORIGINAL PICTURE BACK";
                return;
            }
            dialogClose();
            return;
        }
        if (F.dialog == 3 && F.kbFor)
        {
            // (The keyboard for a new tab, the search or a tab's name.)
            const bool done = F.drow == 0;
            dialogClose();
            if (!done) { F.kbFor = 0; return; }
            Launcher::KeyboardDone(tab, note);
            reorderLibrary();
            settle();
            return;
        }
        if (F.dialog == 3)
        {
            std::string name = F.text;
            dialogClose();
            if (F.drow != 0) return;
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!name.empty() && name.front() == ' ') name.erase(0, 1);
            Launcher::Game own = games[selected];
            own.customName.clear();
            if (name == Launcher::ShortName(own)) name.clear();
            const bool ok = Launcher::SetPair(fo.saves + "/fulldex-names.txt", rom, name);
            reorderLibrary();
            note = !ok ? "COULD NOT SAVE THE NAME" : name.empty() ? "NAME RESET" : "RENAMED";
        }
    };
    auto modalMenuOpen = [&]() {
        bool open = raOpen || keysOpen || (Launcher::Focus().dialog > 0 && !guide);
#ifdef SSR_UPDATER
        open |= updateMenu.open;
#endif
        return open;
    };
    while (!quit)
    {
        bool hadModalMenu = modalMenuOpen();
        // Steam Input can deliver one press as both a keyboard shortcut and a controller
        // button. Apply each menu action once while draining this batch, even if the first
        // event opened a different page or dialog before its duplicate arrives.
        std::set<int> menuPresses;
        auto firstMenuPress = [&](int action) {
            if (action == Controls::START) action = Controls::A;
            if (action == Controls::SELECT || action == Controls::MENU) action = Controls::B;
            return action < 0 || menuPresses.insert(action).second;
        };
        SDL_Event e;
        // Idle waits must not cap an animated grid at 20 Hz. Vsync paces accelerated
        // renderers; the software fallback gets a short wait to avoid a busy loop.
        const int inputWait = gridMotion.Active() || dragging || (Launcher::TilesHome() && (Launcher::Carousel().active || Launcher::Carousel().drag != 0)) ?
            ((renderInfo.flags & SDL_RENDERER_PRESENTVSYNC) ? 0 : 8) : 50;
        bool any = SDL_WaitEventTimeout(&e, inputWait);
        while (any)
        {
            const bool rawMenu = modalMenuOpen();
            hadModalMenu |= rawMenu;
            int menuAction = -1;
            if (e.type == SDL_KEYDOWN)
            {
                if (rawMenu)
                {
                    // Dialogs use their fixed navigation keys, not the game's remappings.
                    switch (e.key.keysym.sym)
                    {
                    case SDLK_LEFT: menuAction = Controls::LEFT; break;
                    case SDLK_RIGHT: menuAction = Controls::RIGHT; break;
                    case SDLK_UP: menuAction = Controls::UP; break;
                    case SDLK_DOWN: case SDLK_TAB: menuAction = Controls::DOWN; break;
                    case SDLK_RETURN: case SDLK_KP_ENTER: menuAction = Controls::A; break;
                    case SDLK_ESCAPE: case SDLK_AC_BACK: menuAction = Controls::B; break;
                    default: break;
                    }
#ifdef SSR_UPDATER
                    if (updateMenu.open && e.key.keysym.sym == SDLK_w) menuAction = Controls::UP;
                    if (updateMenu.open && e.key.keysym.sym == SDLK_s) menuAction = Controls::DOWN;
#endif
                }
                else
                {
                    for (int a = 0; a < Controls::COUNT; a++)
                        if (ctl.KeyCustom(a) && ctl.key[a] == e.key.keysym.scancode) menuAction = a;
                    if (menuAction < 0)
                        switch (e.key.keysym.sym)
                        {
                        case SDLK_LEFT: case SDLK_a: menuAction = Controls::LEFT; break;
                        case SDLK_RIGHT: case SDLK_d: menuAction = Controls::RIGHT; break;
                        case SDLK_UP: case SDLK_w: menuAction = Controls::UP; break;
                        case SDLK_DOWN: case SDLK_s: menuAction = Controls::DOWN; break;
                        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: menuAction = Controls::A; break;
                        case SDLK_ESCAPE: case SDLK_AC_BACK: menuAction = Controls::B; break;
                        case SDLK_f: menuAction = Controls::X; break;
                        case SDLK_h: case SDLK_F1: menuAction = Controls::Y; break;
                        case SDLK_q: case SDLK_PAGEUP: menuAction = Controls::L; break;
                        case SDLK_e: case SDLK_PAGEDOWN: case SDLK_TAB: menuAction = Controls::R; break;
                        default: break;
                        }
                }
            }
            else if (e.type == SDL_CONTROLLERBUTTONDOWN)
            {
                const int input = e.cbutton.button;
                if (rawMenu)
                {
                    switch (input)
                    {
                    case SDL_CONTROLLER_BUTTON_A: case SDL_CONTROLLER_BUTTON_START: menuAction = Controls::A; break;
                    case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: menuAction = Controls::B; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: menuAction = Controls::UP; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: menuAction = Controls::DOWN; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: menuAction = Controls::LEFT; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: menuAction = Controls::RIGHT; break;
                    default: break;
                    }
                }
                else
                {
                    menuAction = ctl.PadCustom() ? ctl.ActionOfPad(input, ctl.byLabel) :
                        input == Controls::IN_A || input == Controls::IN_START ? Controls::A :
                        input == Controls::IN_B || input == Controls::IN_BACK ? Controls::B :
                        input == Controls::IN_X ? Controls::X : input == Controls::IN_Y ? Controls::Y :
                        input == Controls::IN_LB ? Controls::L : input == Controls::IN_RB ? Controls::R : -1;
                    // Ordinary controller directions are applied by the held-input poll below.
                    // Do not consume the edge here, before that poll can move the selection.
                    if (menuAction >= Controls::UP && menuAction <= Controls::RIGHT) menuAction = -1;
                }
            }
            if (!(keysOpen && keysCapture >= 0) && !firstMenuPress(menuAction))
            {
                any = SDL_PollEvent(&e);
                continue;
            }
            if (e.type == SDL_APP_WILLENTERBACKGROUND) appBackgrounded = true;
            if (e.type == SDL_APP_DIDENTERFOREGROUND) appBackgrounded = false;
            if (e.type == SDL_RENDER_DEVICE_RESET || e.type == SDL_RENDER_TARGETS_RESET || e.type == SDL_APP_DIDENTERFOREGROUND)
            {
                // Android can replace its GL surface while choosing a folder or resuming.
                if (texture) SDL_DestroyTexture(texture);
                texture = nullptr;
                fprintf(stderr, "[launcher] render surface reset; recreating launcher texture\n");
            }
            // (RETROACHIEVEMENTS' LOG IN takes the keyboard, the buttons and taps while it is up.)
            if (raOpen && e.type != SDL_QUIT)
            {
                if (e.type == SDL_TEXTINPUT && !raState->busy && raField <= 1)
                {
                    std::string& f = raField == 0 ? raUser : raPass;
                    for (const char* p = e.text.text; *p; p++)
                        if ((unsigned char)*p >= 32 && f.size() < 64) f += *p;
                    raNote.clear();
                    raBuild();
                }
                else if (e.type == SDL_KEYDOWN)
                {
                    const SDL_Scancode sc = e.key.keysym.scancode;
                    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_AC_BACK) raClose();
                    else if (sc == SDL_SCANCODE_BACKSPACE && raField <= 1)
                    {
                        std::string& f = raField == 0 ? raUser : raPass;
                        if (!f.empty()) f.pop_back();
                        raBuild();
                    }
                    else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
                    {
                        if (raField == 0 && !raUser.empty()) { raField = 1; raMenu.selected = 1; raBuild(); }
                        else raSubmit();
                    }
                    else if (sc == SDL_SCANCODE_TAB || sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_UP)
                    {
                        raMenu.selected = (raMenu.selected + (sc == SDL_SCANCODE_UP ? 3 : 1)) % 4;
                        if (raMenu.selected <= 1) raField = raMenu.selected;
                        raBuild();
                    }
                }
                else if (e.type == SDL_CONTROLLERBUTTONDOWN)
                {
                    const int b = e.cbutton.button;
                    if (b == SDL_CONTROLLER_BUTTON_B || b == SDL_CONTROLLER_BUTTON_BACK) raClose();
                    else if (b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START) raActivate(raMenu.selected);
                    else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP || b == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                    {
                        raMenu.selected = (raMenu.selected + (b == SDL_CONTROLLER_BUTTON_DPAD_UP ? 3 : 1)) % 4;
                        if (raMenu.selected <= 1) raField = raMenu.selected;
                        raBuild();
                    }
                }
                else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                {
                    int ww = 1, wh = 1;
                    SDL_GetWindowSize(window, &ww, &wh);
                    const int row = raMenu.ItemAt(e.button.x * canvas.w / std::max(1, ww), e.button.y * canvas.h / std::max(1, wh));
                    if (row >= 0)
                    {
                        raMenu.selected = row;
                        raActivate(row);
                        if (raOpen && row <= 1) SDL_StartTextInput(); // (a phone's keyboard, again)
                    }
                }
                else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEADDED ||
                         e.type == SDL_JOYDEVICEREMOVED || e.type == SDL_WINDOWEVENT)
                    goto normal;
                any = SDL_PollEvent(&e);
                continue;
            }
            // (A glass launcher dialog takes the keyboard, the buttons and taps while it is up.)
            if (Launcher::Focus().dialog && e.type != SDL_QUIT && !guide)
            {
                Launcher::GlassFocus& F = Launcher::Focus();
                if (F.dialog == 3)
                {
                    if (!kb.open) { kbLayer = 0; kbBuild(); }
                    auto erase = [&]() {
                        while (!F.text.empty() && ((unsigned char)F.text.back() & 0xC0) == 0x80) F.text.pop_back();
                        if (!F.text.empty()) F.text.pop_back();
                    };
                    auto press = [&](int hit) {
                        const int id = hit >= 0 && hit < (int)kbIds.size() ? kbIds[hit] : -2;
                        if (id == Launcher::KB_BACK) { dialogClose(); return; }
                        if (id == Launcher::KB_DONE) { F.drow = 0; dialogAct(); return; }
                        if (id == Launcher::KB_LAYER) kbLayer = kbLayer == 0 ? 1 : 0;
                        else if (id == Launcher::KB_SYMBOLS) kbLayer = kbLayer == 2 ? 1 : 2;
                        else if (id == Launcher::KB_DEL) erase();
                        else if (id == Launcher::KB_SPACE) { if (F.text.size() < 48) F.text += ' '; }
                        else if (id >= Launcher::KB_KEY0 && id < Launcher::KB_LAYER)
                        {
                            if (F.text.size() < 48) F.text += kb.items[hit].label;
                            if (kbLayer == 0 && isupper((unsigned char)kb.items[hit].label[0])) kbLayer = 1; // (a capital, then small letters)
                        }
                        kbBuild();
                    };
                    MenuInput nav;
                    if (e.type == SDL_TEXTINPUT)
                    {
                        for (const char* p = e.text.text; *p; p++)
                            if ((unsigned char)*p >= 32 && F.text.size() < 48) F.text += *p;
                        kbBuild();
                    }
                    else if (e.type == SDL_KEYDOWN)
                    {
                        const SDL_Scancode sc = e.key.keysym.scancode;
                        if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_AC_BACK) dialogClose();
                        else if (sc == SDL_SCANCODE_BACKSPACE) { erase(); kbBuild(); }
                        else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) { F.drow = 0; dialogAct(); }
                        else { nav.up = sc == SDL_SCANCODE_UP; nav.down = sc == SDL_SCANCODE_DOWN; nav.left = sc == SDL_SCANCODE_LEFT; nav.right = sc == SDL_SCANCODE_RIGHT; }
                    }
                    else if (e.type == SDL_CONTROLLERBUTTONDOWN)
                    {
                        const int b = e.cbutton.button;
                        if (b == SDL_CONTROLLER_BUTTON_B || b == SDL_CONTROLLER_BUTTON_BACK) dialogClose();
                        else if (b == SDL_CONTROLLER_BUTTON_START) { F.drow = 0; dialogAct(); }
                        else if (b == SDL_CONTROLLER_BUTTON_X) { erase(); kbBuild(); }
                        else
                        {
                            nav.accept = b == SDL_CONTROLLER_BUTTON_A;
                            nav.up = b == SDL_CONTROLLER_BUTTON_DPAD_UP; nav.down = b == SDL_CONTROLLER_BUTTON_DPAD_DOWN;
                            nav.left = b == SDL_CONTROLLER_BUTTON_DPAD_LEFT; nav.right = b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
                        }
                    }
                    else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                    {
                        int ww = 1, wh = 1;
                        SDL_GetWindowSize(window, &ww, &wh);
                        const int hit = kb.ItemAt(e.button.x * canvas.w / std::max(1, ww), e.button.y * canvas.h / std::max(1, wh));
                        if (hit >= 0) { kb.selected = hit; press(hit); }
                    }
                    else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEADDED ||
                             e.type == SDL_JOYDEVICEREMOVED || e.type == SDL_WINDOWEVENT)
                        goto normal;
                    if (kb.open && (nav.up || nav.down || nav.left || nav.right || nav.accept))
                    {
                        int dir = 0;
                        const int hit = kb.Update(nav, dir);
                        if (hit >= 0) press(hit);
                    }
                    any = SDL_PollEvent(&e);
                    continue;
                }
                const int rows = F.dialog >= 5 ? std::max(1, (int)F.dItems.size()) : F.dialog == 1 ? 3 : F.dialog == 4 ? 5 : 2;
                if (e.type == SDL_TEXTINPUT && F.dialog == 3)
                {
                    for (const char* p = e.text.text; *p; p++)
                        if ((unsigned char)*p >= 32 && F.text.size() < 48) F.text += *p;
                }
                else if (e.type == SDL_KEYDOWN)
                {
                    const SDL_Scancode sc = e.key.keysym.scancode;
                    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_AC_BACK) dialogClose();
                    else if (sc == SDL_SCANCODE_BACKSPACE && F.dialog == 3)
                    {
                        while (!F.text.empty() && ((unsigned char)F.text.back() & 0xC0) == 0x80) F.text.pop_back();
                        if (!F.text.empty()) F.text.pop_back();
                    }
                    else if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_TAB) F.drow = (F.drow + (sc == SDL_SCANCODE_UP ? rows - 1 : 1)) % rows;
                    else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) dialogAct();
                }
                else if (e.type == SDL_CONTROLLERBUTTONDOWN)
                {
                    const int b = e.cbutton.button;
                    if (b == SDL_CONTROLLER_BUTTON_B || b == SDL_CONTROLLER_BUTTON_BACK) dialogClose();
                    else if (b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START) dialogAct();
                    else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP || b == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                        F.drow = (F.drow + (b == SDL_CONTROLLER_BUTTON_DPAD_UP ? rows - 1 : 1)) % rows;
                }
                else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                {
                    int ww = 1, wh = 1;
                    SDL_GetWindowSize(window, &ww, &wh);
                    const int row = Launcher::DialogAt(e.button.x * canvas.w / std::max(1, ww), e.button.y * canvas.h / std::max(1, wh));
                    if (row >= 0) { F.drow = row; dialogAct(); }
                }
                else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEADDED ||
                         e.type == SDL_JOYDEVICEREMOVED || e.type == SDL_WINDOWEVENT)
                    goto normal;
                any = SDL_PollEvent(&e);
                continue;
            }
#ifdef SSR_UPDATER
            if (updateMenu.open && e.type != SDL_QUIT)
            {
                auto pick = [&](int row) {
                    updateMenu.open = false;
                    if (row != 0) return;
                    const std::string url = Updater::Choose();
#if SDL_VERSION_ATLEAST(2, 0, 14)
                    if (!url.empty() && SDL_OpenURL(url.c_str()) != 0)
                        fprintf(stderr, "[launcher] could not open %s: %s\n", url.c_str(), SDL_GetError());
#endif
                    if (!url.empty()) note = "OPEN THE DOWNLOAD IN YOUR BROWSER";
                };
                if (e.type == SDL_KEYDOWN && !e.key.repeat)
                {
                    if (e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_w) updateMenu.selected = 0;
                    else if (e.key.keysym.sym == SDLK_DOWN || e.key.keysym.sym == SDLK_s) updateMenu.selected = 1;
                    else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER || e.key.keysym.sym == SDLK_SPACE)
                        pick(updateMenu.selected);
                    else if (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_AC_BACK) updateMenu.open = false;
                }
                else if (e.type == SDL_CONTROLLERBUTTONDOWN)
                {
                    const int b = e.cbutton.button;
                    if (b == SDL_CONTROLLER_BUTTON_DPAD_UP) updateMenu.selected = 0;
                    else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN) updateMenu.selected = 1;
                    else if (b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START) pick(updateMenu.selected);
                    else if (b == SDL_CONTROLLER_BUTTON_B || b == SDL_CONTROLLER_BUTTON_BACK) updateMenu.open = false;
                }
                else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                {
                    int ww = 1, wh = 1;
                    SDL_GetWindowSize(window, &ww, &wh);
                    const int row = updateMenu.ItemAt(e.button.x * canvas.w / std::max(1, ww), e.button.y * canvas.h / std::max(1, wh));
                    if (row >= 0) { updateMenu.selected = row; pick(row); }
                    else if (row == -2) updateMenu.open = false;
                }
                else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEADDED ||
                         e.type == SDL_JOYDEVICEREMOVED || e.type == SDL_WINDOWEVENT)
                    goto normal;
                any = SDL_PollEvent(&e);
                continue;
            }
#endif
            // (KEYS AND BUTTONS takes the keys and buttons while it is up.)
            if (keysOpen && e.type != SDL_QUIT)
            {
                if (e.type == SDL_KEYDOWN && !e.key.repeat) keysInput(e.key.keysym.scancode, -1);
                else if (e.type == SDL_CONTROLLERBUTTONDOWN) keysInput(0, e.cbutton.button);
                else if (e.type == SDL_CONTROLLERAXISMOTION)
                {
                    const int t = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? 1 : -1;
                    if (t >= 0)
                    {
                        const float v = e.caxis.value / 32767.0f;
                        if (v > 0.5f && trig[t] <= 0.5f) keysInput(0, t ? Controls::IN_RT : Controls::IN_LT);
                        trig[t] = v;
                    }
                }
                else if (e.type == SDL_JOYBUTTONDOWN)
                {
                    bool generic = false;
                    for (SDL_Joystick* j : joys) generic |= SDL_JoystickInstanceID(j) == e.jbutton.which;
                    if (generic) keysInput(0, Controls::JoyButtonInput(e.jbutton.button));
                }
                else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && keysCapture < 0)
                {
                    // (A click or tap on a row: chosen and used.)
                    int ww = 1, wh = 1;
                    SDL_GetWindowSize(window, &ww, &wh);
                    const int row = keysMenu.ItemAt(e.button.x * canvas.w / std::max(1, ww), e.button.y * canvas.h / std::max(1, wh));
                    if (row >= 0) { keysMenu.selected = row; keysActivate(row); }
                    else if (row == -2) keysOpen = false;
                }
                else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEADDED ||
                         e.type == SDL_JOYDEVICEREMOVED || e.type == SDL_WINDOWEVENT)
                    goto normal; // (controllers coming and going are still followed)
                any = SDL_PollEvent(&e);
                continue;
            }
        normal:
            switch (e.type)
            {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_DROPFILE:
                // A ROM dropped on the window, or opened with the app from the Files app (iOS)
                // or the Finder (Mac). Copied in, then the list looks again.
                if (e.drop.file)
                {
                    if (!ImportFile(fo, e.drop.file)) note = "FILE NOT IMPORTED";
                    SDL_free(e.drop.file);
                    lastScan = 0;
                }
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (SDL_GameController* p = SDL_GameControllerOpen(e.cdevice.which))
                {
                    pads.push_back(p);
                    const char* name = SDL_GameControllerName(p);
                    fprintf(stderr, "[launcher] controller: %s\n", name ? name : "?");
                }
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                for (size_t i = 0; i < pads.size(); i++)
                    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == e.cdevice.which)
                    {
                        SDL_GameControllerClose(pads[i]);
                        pads.erase(pads.begin() + i);
                        break;
                    }
                break;
            case SDL_JOYDEVICEREMOVED:
                for (size_t i = 0; i < joys.size(); i++)
                    if (SDL_JoystickInstanceID(joys[i]) == e.jdevice.which)
                    {
                        SDL_JoystickClose(joys[i]);
                        joys.erase(joys.begin() + i);
                        break;
                    }
                break;
            case SDL_JOYDEVICEADDED:
                // Any other gamepad (SDL has no layout for it), as the games take it (main.cpp, Pads).
                if (!SDL_IsGameController(e.jdevice.which))
                {
                    SDL_Joystick* j = SDL_JoystickOpen(e.jdevice.which);
                    if (j && SDL_JoystickNumButtons(j) >= 2)
                    {
                        joys.push_back(j);
                        const char* name = SDL_JoystickName(j);
                        fprintf(stderr, "[launcher] joystick without a known layout: %s (%d buttons, %d axes, %d hats)\n",
                            name ? name : "?", SDL_JoystickNumButtons(j), SDL_JoystickNumAxes(j), SDL_JoystickNumHats(j));
                    }
                    else if (j) SDL_JoystickClose(j);
                }
                break;
            case SDL_KEYDOWN:
            {
                int act = -1;
                for (int a = 0; a < Controls::COUNT; a++)
                    if (ctl.KeyCustom(a) && ctl.key[a] == e.key.keysym.scancode) act = a;
                if (act >= 0)
                {
                    if (act == Controls::L || act == Controls::R) { if (!guide) setTab(Launcher::TabStep(tab, act == Controls::R ? 1 : -1), "a key"); }
                    else if (act == Controls::A || act == Controls::START) { if (!e.key.repeat) choose(); }
                    else if (act == Controls::B || act == Controls::MENU) { if (guide) { if (!page.Back()) guide = false; } else if (Launcher::Focus().popup >= 0) Launcher::Focus().popup = -1; else if (Launcher::PageBack()) {} else Launcher::OpenQuitAsk(); }
                    else if (act == Controls::X && !guide)toggleFavourite();
                else if (act == Controls::Y) toggleGuide();
                    else if (act >= Controls::UP && act <= Controls::RIGHT)
                        move(act == Controls::LEFT ? 1 : act == Controls::RIGHT ? 2 : act == Controls::UP ? 3 : 4);
                    break;
                }
                switch (e.key.keysym.sym)
                {
                case SDLK_LEFT: case SDLK_a: move(1); break;
                case SDLK_RIGHT: case SDLK_d: move(2); break;
                case SDLK_UP: case SDLK_w: move(3); break;
                case SDLK_DOWN: case SDLK_s: move(4); break;
                case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: if (!e.key.repeat) choose(); break;
                case SDLK_ESCAPE: case SDLK_AC_BACK: if (guide) { if (!page.Back()) guide = false; } else if (Launcher::Focus().popup >= 0) Launcher::Focus().popup = -1; else if (Launcher::PageBack()) {} else Launcher::OpenQuitAsk(); break;
                case SDLK_f: if(!guide)toggleFavourite(); break;
                case SDLK_h: case SDLK_F1: toggleGuide(); break;
                case SDLK_o: OpenFolder(fo.roms); break;
                case SDLK_q: case SDLK_PAGEUP: if (!guide) setTab(Launcher::TabStep(tab, -1), "a key"); break;
                case SDLK_e: case SDLK_PAGEDOWN: case SDLK_TAB: if (!guide) setTab(Launcher::TabStep(tab, 1), "a key"); break;
                case SDLK_F11:
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    SetConfigFullscreen(fo.cfg, fullscreen);
                    if (guide) page.Open(fo.cfg, true); // (the page shows the file as it now is)
                    break;
                default: break;
                }
                break;
            }
            case SDL_CONTROLLERAXISMOTION:
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_JOYBUTTONDOWN:
            {
                // A controller's button (Controls::IN_*): a trigger counts once pushed past half
                // way; a joystick SDL has no layout for goes by Controls::JoyButtonInput (a known
                // controller's own joystick events are left to its controller events).
                int in = -1;
                if (e.type == SDL_CONTROLLERAXISMOTION)
                {
                    const int t = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? 1 : -1;
                    if (t < 0) break;
                    const float v = e.caxis.value / 32767.0f;
                    if (v > 0.5f && trig[t] <= 0.5f) in = t ? Controls::IN_RT : Controls::IN_LT;
                    trig[t] = v;
                }
                else if (e.type == SDL_JOYBUTTONDOWN)
                {
                    bool generic = false;
                    for (SDL_Joystick* j : joys) generic |= SDL_JoystickInstanceID(j) == e.jbutton.which;
                    if (!generic) break;
                    in = Controls::JoyButtonInput(e.jbutton.button);
                }
                else in = e.cbutton.button;
                if (in < 0) break;
                // The buttons changed in CONTROLS do their actions; otherwise A (or START) plays, B
                // (or BACK) goes back and Y opens the settings.
                const int act = ctl.PadCustom() ? ctl.ActionOfPad(in, ctl.byLabel)
                              : in == Controls::IN_A || in == Controls::IN_START ? Controls::A
                              : in == Controls::IN_B || in == Controls::IN_BACK ? Controls::B
                              : in == Controls::IN_X ? Controls::X
                              : in == Controls::IN_Y ? Controls::Y
                              : in == Controls::IN_LB ? Controls::L : in == Controls::IN_RB ? Controls::R : -1;
                if (act == Controls::L || act == Controls::R)
                {
                    char why[64];
                    snprintf(why, sizeof why, "a controller's input %d, event %u", in, (unsigned)e.type);
                    if (!guide) setTab(Launcher::TabStep(tab, act == Controls::R ? 1 : -1), why);
                }
                else if (act == Controls::A || act == Controls::START) choose();
                else if (act == Controls::B || act == Controls::SELECT) { if (guide) { if (!page.Back()) guide = false; } else if (Launcher::Focus().popup >= 0) Launcher::Focus().popup = -1; else if (Launcher::PageBack()) {} else Launcher::OpenQuitAsk(); }
                else if (act == Controls::X && !guide)toggleFavourite();
                else if (act == Controls::Y) toggleGuide();
                break;
            }
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                // (Window points to the canvas's pixels, which are more on a high-DPI screen.)
                int ww = 1, wh = 1;
                SDL_GetWindowSize(window, &ww, &wh);
                const int mx = (e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x) * canvas.w / std::max(1, ww);
                const int my = (e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y) * canvas.h / std::max(1, wh);
                const int s = std::max(1, std::min(canvas.w, canvas.h) / 240);
                // The buttons in the title band (touch screens): SETTINGS at the right, and in the
                // settings BACK at the left; on Android FOLDER at the left of the list.
                // The Theme Editor's wheel and bars: pressed and dragged along (a mouse or a finger), the value follows.
                {
                    static bool lkDrag = false;
                    if (Launcher::Glassy(canvas) && !guide && Launcher::Focus().dialog == 0 && Launcher::Focus().page == Launcher::SIDE_LAUNCHER)
                    {
                        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { if (Launcher::LookDragBegin(mx, my)) { lkDrag = true; break; } }
                        else if (e.type == SDL_MOUSEMOTION && lkDrag) { Launcher::LookDragMove(mx, my); break; }
                        else if (e.type == SDL_MOUSEBUTTONUP && lkDrag) { lkDrag = false; Launcher::LookDragEnd(); break; }
                    }
                    else if (lkDrag && e.type == SDL_MOUSEBUTTONUP) { lkDrag = false; Launcher::LookDragEnd(); }
                }
                // The game's menu open: the mouse or a finger chooses in it; elsewhere closes it.
                if (Launcher::Glassy(canvas) && !guide && Launcher::Focus().popup >= 0)
                {
                    const int pi = Launcher::PopupAt(mx, my);
                    if (e.type == SDL_MOUSEMOTION && pi >= 0) Launcher::Focus().popup = pi;
                    if (e.type == SDL_MOUSEBUTTONUP)
                    {
                        if (pi >= 0) { Launcher::Focus().popup = pi; choose(); }
                        else Launcher::Focus().popup = -1;
                    }
                    break;
                }
                // The Tiles home: a finger flicked sideways over the covers goes along the games.
                if (Launcher::Glassy(canvas) && !guide && Launcher::TilesHome())
                {
                    static bool swOn = false;
                    static int swX = 0, swY = 0;
                    const Launcher::TilesBox tb = Launcher::TilesGeometry(canvas);
                    const float su = Launcher::GU(canvas);
                    // (One game along is from the middle cover to the one beside it.)
                    const float ks = std::min(1.0f, tb.cover / (250.0f * su));
                    const double oneGame = tb.cover / 2.0 + 24 * su * ks + 168 * su * ks / 2.0;
                    Launcher::CarouselMotion& cm = Launcher::Carousel();
                    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { swOn = my >= tb.coverY - (int)(10 * su) && my < tb.tileY; swX = mx; swY = my; }
                    else if (e.type == SDL_MOUSEMOTION && swOn)
                    {
                        // The covers go along under the finger (once it has moved enough not to be a tap).
                        const int dx = mx - swX, dy = my - swY;
                        if (cm.drag != 0 || (std::abs(dx) > (int)(10 * su) && std::abs(dx) > std::abs(dy)))
                        {
                            const Launcher::View v = view();
                            const int at = v.SpotOf(selected), last = (int)v.spots.size() - 1;
                            double d = -dx / oneGame;
                            // (Past the first or the last game the pull is only felt a little.)
                            if (at >= 0 && at + d < 0) d = -at + (at + d) * 0.3;
                            if (at >= 0 && at + d > last) d = last - at + (at + d - last) * 0.3;
                            cm.drag = d == 0 ? 1e-6 : d;
                        }
                    }
                    else if (e.type == SDL_MOUSEBUTTONUP && swOn)
                    {
                        swOn = false;
                        const int dx = mx - swX, dy = my - swY;
                        const bool held = cm.drag != 0;
                        const double pulled = cm.drag;
                        cm.Release(); // (on from where the finger left it, to the game it comes to rest on)
                        if (held || (std::abs(dx) > (int)(48 * su) && std::abs(dx) > std::abs(dy) * 3 / 2))
                        {
                            // (The games, never the tiles or the buttons: whatever was chosen with a tap or the arrows is let go.)
                            Launcher::Focus() = Launcher::GlassFocus{}; onSettings = onTabs = false; follow = true;
                            // How many games along: the nearest to where it was let go, and a flick is at least one.
                            int steps = (int)std::lround(pulled);
                            if (steps == 0 && std::abs(dx) > (int)(48 * su)) steps = dx < 0 ? 1 : -1;
                            for (int i = 0; i < std::abs(steps); i++) move(steps > 0 ? 2 : 1); // (left: the next game)
                            tapPlay = -1; pressed = dragging = false;
                            break;
                        }
                    }
                }
                // Scroll portrait detail panels, and the Dex's independent detail screen in either orientation.
                if (Launcher::Glassy(canvas) && !guide && Launcher::Focus().page >= 0 && ((canvas.h > canvas.w && Launcher::Focus().pdY > 0) || (Launcher::Focus().page == Launcher::SIDE_HUB && Launcher::HubMode() == 4)) && Launcher::Focus().dMax > 0
                    && !(Launcher::Focus().page == Launcher::SIDE_LAUNCHER && Launcher::LauncherMode() == 1 && Launcher::Look().edit >= 0))
                {
                    static bool dOn = false, dDrag = false; static int dY0 = 0, dS0 = 0;
                    Launcher::GlassFocus& DF = Launcher::Focus();
                    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { dOn = DF.page == Launcher::SIDE_HUB && Launcher::HubMode() == 4 ? Launcher::DexDetailAt(mx, my) : my >= DF.pdY; dY0 = my; dS0 = DF.dScroll; dDrag = false; }
                    else if (e.type == SDL_MOUSEMOTION && dOn)
                    {
                        if (!dDrag && std::abs(my - dY0) > 8 * s) { dDrag = true; dragging = true; }
                        if (dDrag) { DF.freeScroll = true; DF.dScroll = std::max(0, std::min(DF.dMax, dS0 - (my - dY0))); break; }
                    }
                    else if (e.type == SDL_MOUSEBUTTONUP && dOn) { dOn = false; if (dDrag) { dDrag = false; dragging = pressed = false; break; } }
                }
                // The ROMs, Hub and Launcher lists: a finger dragged up or down scrolls them (the chosen row stays where the list was put).
                if (Launcher::Glassy(canvas) && !guide && (Launcher::Focus().page == Launcher::SIDE_ROMS || Launcher::Focus().page == Launcher::SIDE_HUB || Launcher::Focus().page == Launcher::SIDE_LAUNCHER || Launcher::Focus().page == Launcher::SIDE_ABOUT) && Launcher::Focus().rVis > 0)
                {
                    static bool lsOn = false; static int lsY = 0, lsS = 0;
                    Launcher::GlassFocus& LF = Launcher::Focus();
                    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { const bool ab = LF.page == Launcher::SIDE_ABOUT; lsOn = ab ? (mx >= LF.abX && mx < LF.abX + LF.abW && my >= LF.pY) : (mx >= LF.pX && mx < LF.pX + LF.pW && my >= LF.pY); if (LF.page == Launcher::SIDE_HUB && Launcher::HubMode() == 4) lsOn = lsOn && my < LF.pY + LF.rVis * LF.pRow; lsY = my; lsS = LF.rScroll; dragging = false; }
                    else if (e.type == SDL_MOUSEMOTION && lsOn)
                    {
                        if (!dragging && std::abs(my - lsY) > 8 * s) dragging = true;
                        if (dragging) { LF.freeScroll = true; LF.rScroll = std::max(0, lsS - (my - lsY) / std::max(1, LF.page == Launcher::SIDE_ABOUT ? LF.abRow : LF.pRow)); break; }
                    }
                    else if (e.type == SDL_MOUSEBUTTONUP && lsOn) { lsOn = false; if (dragging) { dragging = pressed = false; break; } }
                }
                // The back arrow (a phone sideways): the same as Esc.
                if (Launcher::Glassy(canvas) && e.type == SDL_MOUSEBUTTONUP && !dragging && Launcher::BackAt(mx, my))
                {
                    SDL_Event k; SDL_zero(k);
                    k.type = SDL_KEYDOWN; k.key.state = SDL_PRESSED; k.key.keysym.sym = SDLK_ESCAPE; k.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
                    SDL_PushEvent(&k);
                    break;
                }
                // The About page's cards that open a page (credits, licences, the support codes, the bug page, the website).
                if (Launcher::Glassy(canvas) && !guide && e.type == SDL_MOUSEBUTTONUP && !dragging && Launcher::Focus().page == Launcher::SIDE_ABOUT && Launcher::Focus().dialog == 0)
                    if (const char* url = Launcher::AboutLinkAt(mx, my))
                    {
                        if (SDL_OpenURL(url) != 0) fprintf(stderr, "[launcher] could not open %s: %s\n", url, SDL_GetError());
                        break;
                    }
                // The music bar: previous, play / pause, next.
                if (Launcher::Glassy(canvas) && !guide && e.type == SDL_MOUSEBUTTONUP && !dragging)
                {
                    const int mb = Launcher::MusicAt(mx, my);
                    if (mb >= 0) { Launcher::Music().action = mb; break; }
                }
                // The glass launcher: a section in the sidebar (Settings opens the settings), or Continue.
                if (Launcher::Glassy(canvas) && !guide && e.type == SDL_MOUSEBUTTONUP && !dragging)
                {
                    const int side = Launcher::SideAt(canvas, mx, my);
                    if (side >= 0)
                    {
                        onTabs = onSettings = false;
                        Launcher::Focus().onSort = false;
                        Launcher::Focus().side = -1;
                        if (side == Launcher::SIDE_SETTINGS) toggleGuide();
                        else if (!Launcher::OpenSide(side)) note = std::string(Launcher::SideName(side)) + ": COMING SOON";
                        break;
                    }
                    // (The ROMs page: a click chooses a ROM, or does one of its actions.)
                    if (Launcher::Focus().page == Launcher::SIDE_ROMS || Launcher::Focus().page == Launcher::SIDE_HUB)
                    {
                        Launcher::GlassFocus& RF = Launcher::Focus();
                        const int a = Launcher::RomActionAt(mx, my), r = Launcher::RomRowAt(mx, my);
                        if (a >= 0) { RF.side = -1; RF.pact = a; choose(); }
                        else if (r >= 0) { const bool again = RF.page == Launcher::SIDE_HUB && (Launcher::HubTop() || Launcher::HubMode() == 3 || Launcher::HubMode() == 4) && RF.prow == r && RF.side < 0; RF.side = -1; RF.pact = -1; RF.prow = r; RF.freeScroll = false; if (again || (RF.page == Launcher::SIDE_HUB && Launcher::HubMode() == 4 && Launcher::DexInstant(r))) choose(); }
                        break;
                    }
                    if (Launcher::Focus().page == Launcher::SIDE_LAUNCHER)
                    {
                        // (A row's own page: its wheel, bars, presets; Picture asks for a file.)
                        const int lt = Launcher::LookTap(mx, my);
#ifdef _WIN32
                        if (lt == 3) { const std::string path = PickPictureFile(window); if (!path.empty() && !Launcher::LookAddPicture(path)) note = "COULD NOT USE THAT PICTURE"; }
#endif
                        if (lt) break;
                    }
                    // (A page's list: a click chooses a row; on the chosen one, opens it.)
                    if (Launcher::Focus().page >= 0)
                    {
                        const int r = Launcher::PageRowAt(mx, my);
                        if (r >= 0)
                        {
                            const bool again = Launcher::Focus().prow == r && Launcher::Focus().side < 0;
                            Launcher::Focus().freeScroll = false;
                            Launcher::Focus().side = -1;
                            Launcher::Focus().prow = r;
                            if (again) choose();
                        }
                        break;
                    }
                    if (const int tl = Launcher::ToolAt(mx, my)) { Launcher::Focus().side = -1; onSettings = false; onTabs = true; Launcher::Focus().onSort = true; Launcher::Focus().tool = tl; choose(); break; }
                    if (Launcher::GameOptionsAt(canvas, mx, my)) { Launcher::Focus().side = -1; onSettings = onTabs = false; Launcher::Focus().banner = 2; choose(); break; }
                    if (Launcher::ContinueAt(canvas, mx, my))
                    {
                        const int cg = Launcher::ContinueGame(games, selected);
                        // (As A on the banner's Continue: the game starts. With the focus cleared and
                        // nothing else, a tap or click here opened the game's menu instead.)
                        if (cg >= 0) { selected = cg; onSettings = onTabs = false; Launcher::Focus() = Launcher::GlassFocus{}; Launcher::Focus().banner = 1; choose(); }
                        break;
                    }
                }
                if (Launcher::Glassy(canvas) && !guide && Launcher::Focus().page >= 0) break; // (no games under a page)
                if (((guide && !Launcher::Glassy(canvas)) || !Launcher::Glassy(canvas)) && e.type == SDL_MOUSEBUTTONUP && my < (canvas.h > canvas.w ? 43 : 22) * s && !dragging)
                {
                    if (guide && mx < canvas.w / 4) { if (!page.Back()) guide = false; }
                    else if (!guide && mx > canvas.w * 3 / 4) toggleGuide();
#ifdef __ANDROID__
                    else if (!guide && mx < canvas.w / 4) chooseRomFolder();
#elif defined(SSR_IOS)
                    else if (!guide && mx < canvas.w / 4) OpenFolder(fo.roms);
#endif
                    break;
                }
                if (guide && Launcher::Glassy(canvas))
                {
                    // (The glass settings page: a tap or click chooses a row, again changes it; BACK goes back; a finger dragged scrolls the list.)
                    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { downY = my; downScroll = setMenu.scrollPx; pressed = true; dragging = false; }
                    else if (e.type == SDL_MOUSEMOTION && pressed)
                    {
                        if (!dragging && std::abs(my - downY) > 8 * s) dragging = true;
                        if (dragging) { setMenu.freeScroll = true; setMenu.scrollPx = downScroll - (my - downY); }
                    }
                    if (e.type == SDL_MOUSEBUTTONUP && dragging) { dragging = pressed = false; break; }
                    if (e.type == SDL_MOUSEBUTTONUP)
                    {
                        pressed = false;
                        // (A section of the sidebar: the settings close and it opens, no Back first.)
                        const int sd = Launcher::SideAt(canvas, mx, my);
                        if (sd >= 0 && sd != Launcher::SIDE_SETTINGS)
                        {
                            guide = false; onBack = false; Launcher::Focus().side = -1;
                            if (!Launcher::OpenSide(sd)) note = std::string(Launcher::SideName(sd)) + ": COMING SOON";
                            break;
                        }
                        // (A picture card of the preview tapped: that choice, stepping the row on until it shows.)
                        bool cardHit = false;
                        if (page.selected >= 0 && page.selected < (int)page.items.size() && page.items[page.selected].key && page.items[page.selected].key[0] != '!')
                            for (const auto& r : setMenu.pickRects)
                                if (mx >= r[0] && mx < r[0] + r[2] && my >= r[1] && my < r[1] + r[3])
                                {
                                    for (size_t k = 0; k < page.items[page.selected].values.size() && page.ValueIndex(page.items[page.selected]) != r[4]; k++) change(1);
                                    cardHit = true;
                                    break;
                                }
                        const int hit = cardHit ? -1 : setMenu.SettingsItemAt(mx, my);
                        if (hit >= 0 && hit < (int)setMap.size())
                        {
                            const int item = setMap[hit];
                            if (item < 0) { onBack = false; if (!page.Back()) guide = false; }
                            else if (page.items[item].key) { if (item == page.selected && !onBack) change(1); else { page.selected = item; onBack = false; } }
                        }
                    }
                    break;
                }
                if (guide)
                {
                    // The settings: a finger dragged scrolls; a tap chooses an option, a second
                    // tap on it changes it.
                    if (e.type == SDL_MOUSEBUTTONDOWN)
                    {
                        downY = my;
                        downScroll = page.scroll;
                        dragging = false;
                        pressed = true;
                    }
                    else if (e.type == SDL_MOUSEMOTION && pressed)
                    {
                        if (std::abs(my - downY) > page.lineH / 2) dragging = true;
                        if (dragging)
                        {
                            page.follow = false;
                            page.scroll = downScroll - (my - downY) / std::max(1, page.lineH);
                        }
                    }
                    else if (e.type == SDL_MOUSEBUTTONUP)
                    {
                        if (pressed && !dragging)
                        {
                            const int item = page.ItemAt(my);
                            if (item >= 0 && page.items[item].key)
                            {
                                if (item == page.selected) change(1);
                                else page.selected = item;
                                page.follow = false;
                            }
                        }
                        pressed = dragging = false;
                    }
                    break;
                }
                // A tab: shown.
                if (e.type == SDL_MOUSEBUTTONUP && !dragging)
                {
                    const int t = Launcher::TabAt(canvas, mx, my);
                    if (t >= 0)
                    {
                        if (t != tab) setTab(t, "a tap");
                        else Launcher::OpenTabMenu(t); // (the tab shown: its menu)
                        pressed = false;
                        break;
                    }
                }
                // A finger (or the mouse, held) dragged up or down scrolls the list; it chooses
                // nothing and plays nothing.
                const bool leftDown = e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT;
                const bool hz = Launcher::Glassy(canvas) && Launcher::Layout(canvas).horiz; // (a phone sideways: the row of cards scrolls along)
                if (leftDown && my >= (Launcher::Glassy(canvas) ? Launcher::Layout(canvas).top : 22 * s))
                {
                    downY = hz ? mx : my;
                    downScroll = scroll;
                    gridMotion.Begin(scroll, SDL_GetTicks());
                    pressed = true;
                    dragging = false;
                }
                else if (e.type == SDL_MOUSEMOTION && pressed)
                {
                    const int curP = hz ? mx : my;
                    if (!dragging && std::abs(curP - downY) > std::max(6, (int)(8 * Launcher::GU(canvas)))) dragging = true;
                    if (dragging)
                    {
                        follow = false;
                        tapPlay = -1;
                        scroll = std::max(0, downScroll - (curP - downY));
                        Launcher::FollowSelection(view(), selected, scroll, false);
                        gridMotion.Drag(scroll, SDL_GetTicks());
                        break;
                    }
                }
                if (e.type == SDL_MOUSEBUTTONUP && dragging)
                {
                    gridMotion.End(SDL_GetTicks());
                    dragging = pressed = false;
                    break;
                }
                const int i = Launcher::TileAt(view(), scroll, mx, my);
                if (e.type == SDL_MOUSEMOTION && !pressed && e.motion.which != SDL_TOUCH_MOUSEID && i >= 0)
                { selected = i; follow = false; onSettings = onTabs = false; Launcher::Focus() = Launcher::GlassFocus{}; }

                if (e.type == SDL_MOUSEBUTTONUP)
                {
                    // A tap on the chosen game plays it when the finger lifts (not as it lands:
                    // the finger would still be on the screen when the game starts, and touch it).
                    // (The glass launcher: a click on the chosen game opens its menu, as a tap does (glass).)
                    const bool play = tapPlay >= 0 && i == tapPlay && (e.button.which == SDL_TOUCH_MOUSEID || Launcher::Glassy(canvas));
                    tapPlay = -1;
                    dragging = pressed = false;
                    if (play) choose();
                    break;
                }
                // A tap on another game chooses that one; a double click plays.
                const bool touch = e.type == SDL_MOUSEBUTTONDOWN && (e.button.which == SDL_TOUCH_MOUSEID || (Launcher::Glassy(canvas) && e.button.button == SDL_BUTTON_LEFT));
                if (touch && i >= 0 && i == selected)
                {
                    tapPlay = i;
                    break;
                }
                if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && i >= 0 && e.button.clicks >= 2)
                {
                    choose();
                    break;
                }
                if (i >= 0 && (i != selected || onSettings) && (e.type == SDL_MOUSEBUTTONDOWN || (e.motion.xrel || e.motion.yrel)))
                {
                    selected = i;
                    onSettings = onTabs = false; Launcher::Focus() = Launcher::GlassFocus{};
                    note.clear();
                    // Pointer selection leaves the list under the finger; controller movement
                    // follows its chosen card. A partial card must not jump at touch-down.
                    if (e.type == SDL_MOUSEBUTTONDOWN) follow = false;
                }
#ifdef __ANDROID__
                // (Asking for the ROM folder: a tap anywhere.)
                if (e.type == SDL_MOUSEBUTTONDOWN && i < 0 && my >= (Launcher::Glassy(canvas) ? Launcher::Layout(canvas).top : 22 * s) && games.empty()) choose();
#endif
                break;
            }
            case SDL_MOUSEWHEEL:
                if (Launcher::Glassy(canvas) && !guide && Launcher::NotesScroll(e.wheel.y > 0 ? -2 : 2)) break; // (What's New)
                if (!guide && !setMenu.open && Launcher::Focus().page < 0 && Launcher::Focus().dialog == 0 && Launcher::Focus().popup < 0 && !Launcher::TilesOn())
                {
                    double steps = e.wheel.y;
#if SDL_VERSION_ATLEAST(2, 0, 18)
                    if (e.wheel.preciseY != 0) steps = e.wheel.preciseY;
#endif
                    if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) steps = -steps;
                    const auto g = Launcher::Layout(canvas);
                    gridMotion.Wheel(-steps * std::max(24.0, g.rowStep * 0.45), scroll, SDL_GetTicks());
                    follow = false;
                    break;
                }
                move(e.wheel.y > 0 ? 3 : 4);
                break;
            default:
                break;
            }
            any = SDL_PollEvent(&e);
        }
        if (quit) break;
#ifdef SSR_UPDATER
        // The update check and install (on threads of their own): shown on CHECK FOR UPDATES, a
        // new version said under the list too, and the list closed for the new program to start.
        {
            const Updater::Status u = Updater::Get();
            if (u.serial != updateSeen)
            {
                updateSeen = u.serial;
                for (auto* list : { &page.items, &page.all })
                    for (Launcher::Setting& s : *list)
                        if (s.key && !strcmp(s.key, "!updates") && !s.values.empty()) s.values[0].second = u.text;
                if (u.state == Updater::State::Available && !updateAsked && !getenv("SSR_UPDATE_TEST"))
                {
                    updateAsked = true;
                    updateMenu.items = { { u.canInstall ? Updater::PopupInstall() : Updater::PopupOpen(), "" },
                                         { Updater::PopupLater(), "" } };
                    updateMenu.selected = 0;
                    updateMenu.open = true;
                }
                if (u.state == Updater::State::Downloading || u.state == Updater::State::Installed) updateMenu.open = false;
                if (u.state == Updater::State::Installed && Updater::RestartPending()) break;
                // (Tests, without anyone there: SSR_UPDATE_TEST=1 installs what the check finds, as
                // CHECK FOR UPDATES chosen, and closes the list when there is nothing to install.)
                if (getenv("SSR_UPDATE_TEST"))
                {
                    if (u.state == Updater::State::Available && u.canInstall) Updater::Choose();
                    else if (u.state == Updater::State::UpToDate || u.state == Updater::State::Failed ||
                             u.state == Updater::State::Available)
                    {
                        fprintf(stderr, "[launcher] test: update check ended: %s\n", u.text.c_str());
                        break;
                    }
                }
            }
        }
#endif
        // Held directions on any controller (the D-pad or the left stick) repeat.
        int dir = 0;
        for (SDL_GameController* p : pads)
        {
            const int ax = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTX), ay = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTY);
            // (The buttons the directions have in CONTROLS: the D-pad unless changed.)
            auto down = [&](int a) {
                const int in = ctl.Pad(a, ctl.byLabel);
                if (in == Controls::IN_LT || in == Controls::IN_RT)
                    return SDL_GameControllerGetAxis(p, in == Controls::IN_LT ? SDL_CONTROLLER_AXIS_TRIGGERLEFT : SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
                return SDL_GameControllerGetButton(p, (SDL_GameControllerButton)in) != 0;
            };
            if (down(Controls::LEFT) || ax < -16000) dir = 1;
            else if (down(Controls::RIGHT) || ax > 16000) dir = 2;
            else if (down(Controls::UP) || ay < -16000) dir = 3;
            else if (down(Controls::DOWN) || ay > 16000) dir = 4;
            if (dir) break;
        }
        for (SDL_Joystick* j : joys)
        {
            if (dir) break;
            const Uint8 h = SDL_JoystickNumHats(j) > 0 ? SDL_JoystickGetHat(j, 0) : 0;
            const int ax = SDL_JoystickNumAxes(j) >= 2 ? SDL_JoystickGetAxis(j, 0) : 0, ay = SDL_JoystickNumAxes(j) >= 2 ? SDL_JoystickGetAxis(j, 1) : 0;
            if ((h & SDL_HAT_LEFT) || ax < -16000) dir = 1;
            else if ((h & SDL_HAT_RIGHT) || ax > 16000) dir = 2;
            else if ((h & SDL_HAT_UP) || ay < -16000) dir = 3;
            else if ((h & SDL_HAT_DOWN) || ay > 16000) dir = 4;
        }
        held = dir && dir == heldDir ? held + 1 : dir ? 1 : 0;
        heldDir = dir;
        if ((held == 1 || (held > 8 && held % 3 == 0)) && !hadModalMenu && !modalMenuOpen() &&
            firstMenuPress(dir == 1 ? Controls::LEFT : dir == 2 ? Controls::RIGHT : dir == 3 ? Controls::UP : Controls::DOWN)) move(dir);
        // (KEYS AND BUTTONS: the capture's 5 seconds.)
        if (keysOpen && keysCapture >= 0 && SDL_GetTicks() > keysUntil)
        {
            keysCapture = -1;
            keysBuild();
        }

#ifdef __ANDROID__
        // Back from Android's folder picker (or its "All files access" page): the folder it
        // gave, and the games in it at once.
        const bool pickingNow = JavaBool("picking");
        if ((wasPicking && !pickingNow) || (needFolder && SDL_GetTicks() - lastFolderCheck > 1000))
        {
            lastFolderCheck = SDL_GetTicks();
            SetRomFolder(fo);
            needFolder = (fo.roms.empty() || !JavaBool("canUseFiles")) && !getenv("SSR_ANDROID_ROMS");
            if (!needFolder)
            {
                MakeDir(fo.saves);
                Launcher::LoadLook(fo.cfg, fo.art, theme, background);
                lastScan = 0;
                fprintf(stderr, "[launcher] ROM folder %s\n", fo.roms.c_str());
            }
        }
        wasPicking = pickingNow;
        // A frontend can open a ROM while the library is already running. The Java side never
        // queues this while a game is active, so no unsaved game is interrupted.
        const std::string incoming = JavaString("takeLaunchRom");
        if (!incoming.empty()) {
            const int game = DirectRom::Ds(incoming, fo.saves, fo.art, games);
            if (game >= 0) { selected = game; launchSelected(); }
            else note = "COULD NOT OPEN THIS ROM";
        }

#endif
        // ROMs put in the folder (or taken out) show up by themselves.
        if (SDL_GetTicks() - lastScan > 5000)
        {
            lastScan = SDL_GetTicks();
            const std::string current = selected < 0 || selected >= (int)games.size() ? std::string() : games[selected].rom;
            games = FindGames(fo, artCache);
            Launcher::BuildRomList(games, fo.saves);
            reorderLibrary();
            selected = -1;
            for (size_t i = 0; i < games.size(); i++)
                if (games[i].rom == current) selected = (int)i;
            if (selected < 0 || view().SpotOf(selected) < 0) selected = Launcher::FirstIn(view());
        }

        if (appBackgrounded) { gridMotion.Stop(); SDL_Delay(50); continue; }
        int ow = 1280, oh = 720;
#ifdef SSR_IOS
        if (SDL_GetRendererOutputSize(renderer, &ow, &oh) != 0 || ow <= 0 || oh <= 0)
        {
            if (!loggedDrawableError) fprintf(stderr, "[launcher] waiting for drawable: %dx%d %s\n", ow, oh, SDL_GetError());
            loggedDrawableError = true;
            SDL_Delay(16);
            continue; // SDL_PollEvent on the next iteration pumps UIKit's lifecycle.
        }
        loggedDrawableError = false;
        if (ow != loggedDrawableW || oh != loggedDrawableH)
        {
            int pointsW = 0, pointsH = 0;
            SDL_GetWindowSize(window, &pointsW, &pointsH);
            fprintf(stderr, "[launcher] drawable window=%u points=%dx%d pixels=%dx%d\n",
                SDL_GetWindowID(window), pointsW, pointsH, ow, oh);
            loggedDrawableW = ow; loggedDrawableH = oh;
            loggedPresent = false;
        }
#else
        SDL_GetRendererOutputSize(renderer, &ow, &oh);
#endif
#ifdef SSR_PHONE
        // Bound software composition work on high density phone displays. (Drawn at 1280 on the long side and
        // stretched 2.3 times on a 2992 wide phone, the writing and icons were soft: 1920 is sharper, 1.6 times.)
        const double scale = std::min(1.0, 3200.0 / std::max(ow, oh)); // (the phone's own pixels: as sharp as the screen can show)
        ow = std::max(1, (int)(ow * scale));
        oh = std::max(1, (int)(oh * scale));
#endif
        if (ow != canvas.w || oh != canvas.h || !texture)
        {
            canvas.Resize(ow, oh);
            if (texture) SDL_DestroyTexture(texture);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ow, oh);
        }
        if (follow || setMenu.open || guide || Launcher::Focus().page >= 0 || Launcher::Focus().dialog || Launcher::Focus().popup >= 0)
            gridMotion.Stop();
        else
        {
            const auto v = view();
            gridMotion.Advance(SDL_GetTicks(), std::max(0, v.height - (v.g.horiz ? v.g.panelW : v.g.bottom - v.g.top)), scroll);
        }
        if (testBrowseDialog == 19) { folderBrowse = fo.roms; folderFirst = 0; showRomFolder(); testBrowseDialog = 0; }
        else if (testBrowseDialog == 20) { pictureBrowse = fo.roms; pictureRom = games.empty() ? "" : games[0].rom; pictureFirst = 0; showPictureFolder(); testBrowseDialog = 0; }
        // (SSR_LIST_ITEM: once the canvas has its size, so the glass launcher's menu is there to use.)
        if (testItem && selected >= 0) { Launcher::Focus().popup = atoi(testItem); testItem = nullptr; choose(); }
        // A controller's buttons named as it has them (PlayStation's CROSS, TRIANGLE, ...) and as
        // CONTROLS has them, while one is connected.
        const bool padIn = !pads.empty() || !joys.empty();
        auto btn = [&](int a) {
            const int def = a == Controls::A ? Controls::IN_A : a == Controls::B ? Controls::IN_B
                          : a == Controls::L ? Controls::IN_LB : a == Controls::R ? Controls::IN_RB : a == Controls::X ? Controls::IN_X : Controls::IN_Y;
            return std::string(Controls::InputName(ctl.PadCustom() ? ctl.Pad(a, ctl.byLabel) : def,
                Controls::StyleOf(pads.empty() ? nullptr : pads[0])));
        };
#ifdef SSR_IOS
        const std::string hint = games.empty() ? std::string("FOLDER: YOUR ROMS FOLDER IN THE FILES APP")
                               : padIn ? "TAP A GAME TWICE OR PRESS " + btn(Controls::A) + " TO PLAY    " + btn(Controls::Y) + ": SETTINGS"
                                       : std::string("TAP A GAME TWICE TO PLAY    SETTINGS: AT THE TOP");
        const std::string pageHint = padIn ? "UP / DOWN: CHOOSE    " + btn(Controls::A) + ": CHANGE    " + btn(Controls::B) + ": BACK"
                                           : std::string("TAP AN OPTION THEN TAP AGAIN TO CHANGE    DRAG TO SCROLL");
#elif defined(__ANDROID__)
        const std::string hint = games.empty() ? "TAP OR PRESS " + (padIn ? btn(Controls::A) : std::string("A")) + " TO CHOOSE YOUR ROMS FOLDER    BACK: QUIT"
                               : padIn ? "TAP A GAME TWICE OR PRESS " + btn(Controls::A) + " TO PLAY    " + btn(Controls::L) + " / " + btn(Controls::R) + ": TABS    BACK: QUIT"
                                       : std::string("TAP A GAME TWICE TO PLAY    SETTINGS: AT THE TOP    BACK: QUIT");
        const std::string pageHint = padIn ? "UP / DOWN: CHOOSE    " + btn(Controls::A) + ": CHANGE    " + btn(Controls::B) + ": BACK"
                                           : std::string("TAP AN OPTION THEN TAP AGAIN TO CHANGE    DRAG TO SCROLL    BACK: DONE");
#else
        const std::string hint = games.empty() ? std::string("O: OPEN THE ROMS FOLDER    H: SETTINGS    ESC: QUIT")
                               : padIn ? btn(Controls::A) + ": PLAY    " + btn(Controls::L) + " / " + btn(Controls::R) + ": TABS    " + btn(Controls::Y) + ": SETTINGS    " + btn(Controls::B) + ": QUIT"
                                       : std::string("ENTER: PLAY    Q / E: TABS    H: SETTINGS    ESC: QUIT");
        const std::string pageHint = page.group < 0 ? (padIn ? "UP / DOWN: CHOOSE    " + btn(Controls::A) + ": OPEN    " + btn(Controls::B) + ": BACK"
                                                             : std::string("UP / DOWN: CHOOSE    ENTER: OPEN    ESC: BACK"))
                                   : padIn ? "UP / DOWN: CHOOSE    LEFT / RIGHT / " + btn(Controls::A) + ": CHANGE    " + btn(Controls::B) + ": BACK"
                                           : std::string("UP / DOWN: CHOOSE    LEFT / RIGHT / ENTER: CHANGE    ESC: BACK");
#endif
        if (guide)
        {
            raPoll();
            for (Launcher::Setting& it : page.items)
            {
                if (it.key && !strcmp(it.key, "!ra_login") && !it.values.empty()) it.values[0].second = raAccountShown();
                if (it.key && !strcmp(it.key, "!saves_folder") && !it.values.empty() && it.values[0].second == "BESIDE THE ROMS") { const std::string sf = Launcher::ConfigValue(fo.cfg, "saves_folder"); if (!sf.empty()) it.values[0].second = sf; }
            }
            if (Launcher::Glassy(canvas))
            {
                // (The pause menu's glass settings pages, ui_settings.h, over the chosen game's picture blurred.)
                setMenu.touchRows = Launcher::PhoneUi() ? 1.5f : 1.0f; // (a phone: rows big enough for a finger, the page larger)
                setMenu.laneK = Launcher::PhoneUi() ? std::max(0.42f, Launcher::GU(canvas) * (480.0f / 720.0f) * 0.94f * 1.3f) : 0.0f; // (by the launcher's own unit: right upright too)
                { static int lastSel = -99; if (page.selected != lastSel) { lastSel = page.selected; setMenu.freeScroll = false; } }
                Launcher::BuildSettingsMenu(setMenu, setMap, page, onBack);
                Launcher::PreviewFrames(canvas, games);
                Launcher::DrawSettingsPage(canvas, setMenu, games, tab, scroll, page.group < 0 ? "Settings" : Launcher::SettingsGroups()[page.group].name,
                                           padIn ? btn(Controls::A) + (page.group < 0 ? ": OPEN    " : ": CHANGE    ") + btn(Controls::B) + ": BACK"
                                                 : std::string(page.group < 0 ? "ENTER: OPEN    ESC: BACK" : "ENTER: CHANGE    ESC: BACK"), theme);
            }
            else page.Draw(canvas, pageHint, theme);
            if (raOpen)
                raMenu.Draw(canvas, "RETROACHIEVEMENTS: LOG IN",
                            raField == 0 ? "TYPE YOUR USER NAME, THEN ENTER" : raField == 1 ? "TYPE YOUR PASSWORD, THEN ENTER" : "ENTER: LOG IN   ESC: CANCEL",
                            raNote);
            if (keysOpen)
            {
                std::string kh = keysCapture >= 0 ? (keysKeyboard ? "ESC: LEAVE IT AS IT WAS" : "OR WAIT 5 SECONDS: AS IT WAS")
                               : padIn ? btn(Controls::A) + ": CHANGE   " + btn(Controls::B) + ": BACK" : std::string("ENTER: CHANGE   ESC: BACK");
                if (keysCapture < 0 && keysMenu.selected == 0) kh = "CHANGE: KEYBOARD OR CONTROLLER";
                else if (keysCapture < 0)
                    if (const char* h = Controls::ActionHint(keysMenu.selected - keysFirst)) kh = h;
                keysMenu.Draw(canvas, "KEYS AND BUTTONS", kh);
            }
        }
        else if (!guide && Launcher::Focus().dialog == 3 && Launcher::Glassy(canvas))
        {
            // (Rename: the pause menu's keyboard page over the whole screen.)
            if (!kb.open) { kbLayer = 0; kbBuild(); }
            Launcher::GlassBackdrop(canvas, selected >= 0 && selected < (int)games.size() ? &games[selected] : nullptr); // (the game's picture, blurred)
            kb.Draw(canvas, Launcher::KbTitle(), padIn ? btn(Controls::A) + ": TYPE    " + btn(Controls::X) + ": DELETE    START: SAVE    " + btn(Controls::B) + ": CANCEL"
                                          : std::string("TYPE OR CLICK THE KEYS    ENTER: SAVE    ESC: CANCEL"));
        }
        else
        {
            FillUpdateStatus();
#ifdef __ANDROID__
        {
            // (A page opened on a phone held upright: the screen turns sideways for it, back again when it closes.)
            static int lastLand = 0;
            static const bool forceLand = getenv("SSR_ANDROID_LANDSCAPE") != nullptr; // (test)
            const int wantLand = forceLand ? 1 : 0; // (the pages stay upright now: they have their own upright layouts)
            if (wantLand != lastLand) { lastLand = wantLand; JavaVoid(wantLand ? "showPageOrientation" : "showLauncherOrientation"); }
            // Launcher > Camera Cutout: told to Android when it changes (and once at the start).
            static int lastCut = -1;
            Launcher::LookLoad();
            const int wantCut = Launcher::Look().avoidCutout ? 1 : 0;
            if (wantCut != lastCut) { lastCut = wantCut; JavaBoolArg("setAvoidCutout", wantCut != 0); }
        }
#endif
            // NOW PLAYING: the music app's song, now and then, for the bar on the home screen (SSR_LIST_NOWPLAYING: a made-up song, for the test picture).
            {
                Launcher::MusicBar& MB = Launcher::Music();
                NowPlaying::Start(); NowPlaying::Want(2);
                NowPlaying::Info np = NowPlaying::Now();
                const bool fake = getenv("SSR_LIST_NOWPLAYING") != nullptr;
                if (fake)
                {
                    np.active = true; np.playing = true; np.canPrev = np.canNext = true; np.title = "Midnight City"; np.artist = "M83"; np.app = "Spotify";
                    if (np.art.empty()) { np.artW = np.artH = 64; np.art.resize(64 * 64); for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) np.art[y * 64 + x] = 0xFF000000u | ((60 + x * 3) << 16) | ((40 + y) << 8) | (200 - y * 2); np.artVersion = 1; }
                }
                MB.shown = NowPlaying::Supported() || fake;
                MB.idle = !np.active && !np.needAccess;
                MB.blind = np.needAccess; MB.playing = np.playing; MB.canPrev = np.canPrev; MB.canNext = np.canNext;
                MB.title = np.title; MB.artist = np.artist; MB.app = np.app;
                static unsigned artSeen = ~0u;
                if (np.artVersion != artSeen) { artSeen = np.artVersion; MB.art = np.art; MB.artW = np.artW; MB.artH = np.artH; }
                if (MB.action >= 0) { if (!fake) NowPlaying::Command(MB.action); MB.action = -1; }
            }
#ifdef _WIN32
            if (N3ds::Busy())
            {
                const std::string prep = N3ds::Status(); if (!prep.empty()) note = prep;
                for (Launcher::Game& g : games) if (g.platform == Launcher::N3DS && !g.built) g.built = N3ds::Prepared(g);
            }
#elif defined(__ANDROID__)
            // (The 3DS games' copying and building: how it goes, twice a second.)
            static int n3dsPoll = 0;
            if (++n3dsPoll >= 30)
            {
                n3dsPoll = 0;
                const std::string n3 = JavaString("threeDsNote");
                if (!n3.empty()) note = n3;
                bool any = false;
                for (const Launcher::Game& g : games) any |= g.platform == Launcher::N3DS;
                if (any)
                {
                    const bool busy3ds = JavaBool("threeDsBusy");
                    // (Built, or code from a PC taken: the tile and menu say so.)
                    for (Launcher::Game& g : games)
                        if (g.platform == Launcher::N3DS && !busy3ds) g.built = JavaBoolStringArg("threeDsBuilt", N3ds::IdOf(g));
                    // (The first Play's copy done: the game starts.)
                    std::string& autoPlay = N3dsAutoPlay();
                    if (!autoPlay.empty() && !busy3ds)
                    {
                        for (const Launcher::Game& g : games)
                            if (g.platform == Launcher::N3DS && g.folder == autoPlay && N3ds::Added(g)) { JavaStringArg("threeDsPlay", N3ds::IdOf(g)); note = "STARTING " + g.name; }
                        autoPlay.clear();
                    }
                }
            }
#endif
            Launcher::Draw(canvas, games, tab, !Launcher::TilesOn() && (onSettings || onTabs || Launcher::Focus().side >= 0 || Launcher::Focus().banner) ? -1 : selected, scroll, note, (Launcher::Focus().popup >= 0 || Launcher::Focus().dialog || Launcher::Focus().page >= 0) ? (padIn ? btn(Controls::A) + ": CHOOSE    " + btn(Controls::B) + ": BACK" : std::string("ENTER: CHOOSE    ESC: BACK")) : hint, theme, follow, onTabs, onSettings);
        }
#ifdef SSR_UPDATER
        if (updateMenu.open)
            updateMenu.Draw(canvas, Updater::PopupTitle(),
                            padIn ? btn(Controls::A) + ": CHOOSE    " + btn(Controls::B) + ": LATER"
                                  : "ENTER: CHOOSE    ESC: LATER",
                            Updater::PopupNote(Updater::Get().canInstall));
#endif
        {
            // The title band's buttons (for touch screens and the mouse): SETTINGS, or BACK in
            // the settings; on Android FOLDER, to choose the ROM folder again.
            const int s = std::max(1, std::min(canvas.w, canvas.h) / 240);
            // (The chosen one, by the D-pad: framed as a game is.)
            const int buttonY = canvas.h > canvas.w ? 21 * s : 0;
            auto button = [&](int x, const char* text, bool on) {
                if (on) canvas.Fill(x - 3 * s, buttonY + 4 * s, Canvas::TextWidth(text, s) + 5 * s, 13 * s, theme.frame);
                Launcher::ModernText(canvas, x, buttonY + 7 * s, text, s, on ? theme.title : theme.pokemon ? 0xFFFFFFFFu : theme.hint);
            };
            if (guide && !Launcher::Glassy(canvas)) button(8 * s, "< BACK", onBack); // (the glass page has its own)
            else if (!Launcher::Glassy(canvas))
            {
                button(canvas.w - 8 * s - Canvas::TextWidth("SETTINGS", s), "SETTINGS", onSettings);
#ifdef SSR_PHONE
                Launcher::ModernText(canvas, 8 * s, buttonY + 7 * s, "FOLDER", s, theme.pokemon ? 0xFFFFFFFFu : theme.hint);
#endif
            }
        }
        // (The glass list says it itself, in its own place: this older text is for the plain list.)
#ifdef __ANDROID__
        const bool plainEmpty = !Launcher::Glassy(canvas) || needFolder; // (choosing the folder is still told here)
#else
        const bool plainEmpty = !Launcher::Glassy(canvas);
#endif
        if (games.empty() && !guide && plainEmpty)
        {
            const int s = std::max(1, std::min(canvas.w, canvas.h) / 240);
#ifdef __ANDROID__
            const bool asking = needFolder;
            const char* lines[] = { asking ? "CHOOSE YOUR ROMS FOLDER" : "NO GAMES FOUND", "",
                                    asking ? "TAP ANYWHERE: ALLOW ACCESS TO FILES, THEN PICK THE FOLDER" : "NO .NDS FILES IN",
                                    asking ? "WITH YOUR .NDS FILES. SAVES GO IN A SAVES FOLDER INSIDE IT." : fo.roms.c_str(),
                                    "",
                                    "IT PLAYS POKEMON BLACK / WHITE / BLACK 2 / WHITE 2,",
                                    "SOULSILVER / HEARTGOLD / PLATINUM",
                                    "SUPPORTED REGIONAL VERSIONS: SEE THE README" };
            const char* heading = lines[0];
#elif defined(SSR_IOS)
            const char* lines[] = { "NO GAMES FOUND", "",
                                    "PUT YOUR .NDS FILES IN THE FILES APP: ON MY IPHONE (OR IPAD)",
                                    "> POKEMON DS WIDESCREEN > ROMS. TAP FOLDER AT THE TOP TO GO THERE.",
                                    "THEY SHOW UP HERE BY THEMSELVES.", "",
                                    "IT PLAYS POKEMON BLACK / WHITE / BLACK 2 / WHITE 2,",
                                    "SOULSILVER / HEARTGOLD / PLATINUM",
                                    "SUPPORTED REGIONAL VERSIONS: SEE THE README" };
            const char* heading = lines[0];
#else
            const char* lines[] = { "NO GAMES FOUND", "",
                                    "PUT YOUR ROMS IN THE ROMS FOLDER BESIDE THIS PROGRAM.", "THEY SHOW UP HERE BY THEMSELVES.", "",
                                    "IT PLAYS POKEMON BLACK / WHITE / BLACK 2 / WHITE 2,",
                                    "SOULSILVER / HEARTGOLD / PLATINUM",
                                    "SUPPORTED REGIONAL VERSIONS: SEE THE README" };
            const char* heading = lines[0];
#endif
            int y = canvas.h / 3;
            for (const char* l : lines)
            {
                const int ls = l == heading ? s + 1 : s;
                int ts = ls;
                while (ts > 1 && Canvas::TextWidth(l, ts) > canvas.w - 8) ts--;
                canvas.Text((canvas.w - Canvas::TextWidth(l, ts)) / 2, y, l, ts, ls > s ? theme.title : theme.name);
                y += 12 * s;
            }
        }
        SDL_SetRenderDrawColor(renderer, 16, 16, 16, 255);
        if (!texture || SDL_UpdateTexture(texture, nullptr, canvas.px.data(), canvas.w * 4) != 0 ||
            SDL_RenderClear(renderer) != 0 || SDL_RenderCopy(renderer, texture, nullptr, nullptr) != 0)
        {
            fprintf(stderr, "[launcher] drawing failed: %s\n", SDL_GetError());
            if (renderInfo.flags & SDL_RENDERER_SOFTWARE)
            {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pokemon Fulldex", "The launcher could not draw. Restart the app. Details are in game.log.", window);
                break;
            }
            // A driver can accept renderer creation but fail its first texture or draw.
            SDL_DestroyRenderer(renderer);
            texture = nullptr;
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
            if (!renderer)
            {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pokemon Fulldex", "The launcher could not start its graphics. Details are in game.log.", window);
                break;
            }
            SDL_GetRendererInfo(renderer, &renderInfo);
            fprintf(stderr, "[launcher] switched to software renderer\n");
            continue;
        }
        if(SDL_GetTicks()-libraryDisplayCheck>=500 || libraryDisplayCheck==0)
        {
            libraryDisplayCheck=SDL_GetTicks();
            const bool enabled=Launcher::ConfigValue(fo.cfg,"extra_second_artwork")=="1";
            if(enabled&&!libraryDisplay)libraryDisplay=std::make_unique<AndroidSecondScreen>(true);
            else if(!enabled&&libraryDisplay)libraryDisplay.reset();
        }
        const uint64_t libraryState=libraryDisplay?libraryDisplay->State():0;
        if(libraryState & DsDisplayState::AcceptingFrames)
        {
            const Launcher::Game* g=selected>=0&&selected<(int)games.size()?&games[selected]:nullptr;
            const std::string key=g?g->rom+std::to_string(g->favourite)+std::to_string(g->lastPlayed):"empty";
            if(key!=libraryCardKey)
            {
                libraryCardKey=key;libraryCard.Clear();libraryCard.Fill(0,0,640,480,0xFF101722);
                libraryCard.Text(18,16,"POKEMON FULLDEX",3,0xFFFFD75E);
                if(g)
                {
                    auto picture=Launcher::GamePicture(*g,320);
                    if(picture.size()==320*320)for(int y=0;y<320;y++)for(int x=0;x<320;x++)
                        libraryCard.px[(size_t)(y+66)*640+x+160]=picture[(size_t)y*320+x]|0xFF000000;
                    std::string title=g->name;if(title.size()>48)title=title.substr(0,47)+".";
                    libraryCard.Text(18,400,title,2,0xFFFFFFFF);
                    libraryCard.Text(18,432,g->comingSoon?"COMING SOON":g->hasSave?"SAVE FOUND":"READY TO PLAY",2,0xFFB9C9DD);
                    if(g->favourite)libraryCard.Text(500,432,"FAVOURITE",1,0xFFFFD75E);
                }
                else libraryCard.Text(18,100,"CHOOSE A GAME ON THE MAIN SCREEN",2,0xFFFFFFFF);
            }
            libraryDisplay->Submit(libraryCard.px.data(),640,480);
        }
        SDL_RenderPresent(renderer);
#ifdef SSR_IOS
        if (!loggedPresent)
        {
            fprintf(stderr, "[launcher] frame presented window=%u renderer=%s canvas=%dx%d drawable=%dx%d\n",
                SDL_GetWindowID(window), renderInfo.name ? renderInfo.name : "unknown",
                canvas.w, canvas.h, loggedDrawableW, loggedDrawableH);
            loggedPresent = true;
        }
#endif
        // (SSR_LIST_KEYS=lrud...: those arrow presses, once, before the first picture, for the navigation tests.)
        static int keysAge = -1;
        if (keysAge >= 0) keysAge++;
        if (const char* tk = getenv("SSR_LIST_KEYS")) { static bool keysDone = false; if (!keysDone) { keysDone = true; keysAge = 0; for (const char* q = tk; *q; q++) move(*q == 'l' ? 1 : *q == 'r' ? 2 : *q == 'u' ? 3 : 4); } }
        // (SSR_LIST_CHOOSE=a game code: that game chosen once as a player would, its menu's Play: a 3DS
        // game not prepared yet starts preparing, and SSR_LIST_SHOT waits until that has ended.)
        if (const char* tc = getenv("SSR_LIST_CHOOSE"))
        {
            static bool chosenOnce = false;
            if (!chosenOnce)
            {
                chosenOnce = true;
                for (size_t i = 0; i < games.size(); i++)
                    if (games[i].code == tc) selected = (int)i;
                fprintf(stderr, "[launcher] test: %s chosen as a player would\n", tc);
                launchSelected();
                if (quit) break;
            }
        }
        if (testPick && *testPick)
        {
            for (size_t i = 0; i < games.size(); i++)
                if (games[i].code == testPick) result = (int)i;
            fprintf(stderr, "[launcher] test: %s chosen\n", testPick);
            break;
        }
        // (SSR_LIST_HUB=tradeauto: the first room found joined and the first Pokemon offered; SSR_LIST_SHOT_WAIT=seconds:
        // the picture only after that long, for the test of a trade with another device.)
        static const auto shotStart = std::chrono::steady_clock::now();
        static bool autoJoined = false, autoOffered = false;
        if (const char* at = getenv("SSR_LIST_HUB"); at && !strcmp(at, "tradeauto"))
        {
            const HubLink::Status ls = HubLink::Snapshot();
            if (ls.state == HubLink::State::Off && !autoJoined) Launcher::TradeDo(2, 1);
            if (ls.state == HubLink::State::Browsing && !ls.rooms.empty() && !autoJoined) { HubLink::Join(ls.rooms[0]); autoJoined = true; }
            if (ls.state == HubLink::State::Linked && !ls.mine.any && !autoOffered) { Launcher::TradeDo(Launcher::TRADE_FIRST, 0); autoOffered = true; }
        }
        // (SSR_LIST_HUB=tradeonline: code TEST42 at the relay, the first Pokemon offered, the trade accepted when it can be.)
        static bool autoAccepted = false;
        if (const char* at = getenv("SSR_LIST_HUB"); at && !strcmp(at, "tradeonline"))
        {
            const HubLink::Status ls = HubLink::Snapshot();
            if (ls.state == HubLink::State::Off && !autoJoined) { Launcher::TradeEnterCode(getenv("SSR_TRADE_CODE") ? getenv("SSR_TRADE_CODE") : "TEST42"); autoJoined = true; }
            if (ls.state == HubLink::State::Linked && !ls.mine.any && !autoOffered) { Launcher::TradeDo(Launcher::TRADE_FIRST, 0); autoOffered = true; }
            if (ls.state == HubLink::State::Linked && ls.mine.any && ls.theirs.any && ls.theyCanTake && !ls.iAccepted && !autoAccepted) { HubLink::Accept(true); autoAccepted = true; }
        }
        const char* shotWait = getenv("SSR_LIST_SHOT_WAIT");
#ifdef _WIN32
        const bool preparing = N3ds::P().running;
#else
        const bool preparing = false;
#endif
        if (testShot && *testShot && !preparing && (keysAge < 0 || keysAge >= 4) && (!shotWait || std::chrono::steady_clock::now() - shotStart >= std::chrono::seconds(atoi(shotWait))))
        {
            if (SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(canvas.px.data(), canvas.w, canvas.h, 32, canvas.w * 4, SDL_PIXELFORMAT_ARGB8888))
            {
                SDL_SaveBMP(sf, testShot);
                SDL_FreeSurface(sf);
            }
            fprintf(stderr, "[launcher] test: the list in %s\n", testShot);
            result = -1;
            break;
        }
    }
    if(libraryDisplay)libraryDisplay->Stop();
    lastTab = tab;
    SetConfigValues(fo.cfg, {{"launcher_tab2", std::to_string(tab)}});
    for (SDL_GameController* p : pads) SDL_GameControllerClose(p);
    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
#ifndef SSR_PHONE
    // (Not on a phone: SDL stays up for the game, which opens its own window in the same
    // activity (Android) or app (iOS).)
    SDL_Quit();
#endif
    return result;
}
}

bool Start(int& argc, char**& argv)
{
    // "--game IREO" (the desktop shortcuts, shortcuts.h): that game at once, without the list.
    // Other arguments (a ROM given by hand, the test harness): the program runs as it always has.
    std::string direct;
    // "--rom <file>": that ROM at once, without the list (a GBA one is imported into roms first).
    std::string directRom;
    if (argc == 3 && !strcmp(argv[1], "--rom")) directRom = argv[2];
    else if (argc == 3 && !strcmp(argv[1], "--game"))
    {
        direct = argv[2];
        for (char& c : direct) c = (char)toupper((unsigned char)c);
    }
    else if (argc >= 2)
    {
#ifdef SSR_GBA
        // A .gba (or Ultra Violet's .ips) opened with the app is a GBA game. Anything else,
        // including a DS ROM or the test harness, is handled as it always has been.
        bool onlyGba = true;
        for (int i = 1; i < argc; i++)
            if (!argv[i] || !IsGbaRomName(argv[i])) onlyGba = false;
        if (!onlyGba) return true;
#else
        return true;
#endif
    }
#ifdef _WIN32
    Shortcuts::SetAppId(direct);
#endif
    Folders fo;
#ifdef __ANDROID__
    // The app's own folder on the shared storage (Android/data/io.github.mamesj.pokemonds/
    // files: settings, game.log, the function tables), which a PC over USB can open; the
    // ROMs and saves in the folder the player chooses (GameActivity).
    {
        const char* ext = SDL_AndroidGetExternalStoragePath();
        const char* own = SDL_AndroidGetInternalStoragePath();
        fo.dir = ext ? ext : own ? own : ".";
    }
    // (Test: <the app's folder>/test-env.txt, lines NAME=value, set as environment variables: the SSR_LIST_* test hooks, and
    // SSR_ANDROID_ROMS=<a folder> to use it as the ROMs folder without the picker.)
    if (FILE* tf = fopen((fo.dir + "/test-env.txt").c_str(), "r"))
    {
        char ln[512];
        while (fgets(ln, sizeof ln, tf))
        {
            std::string t = ln;
            while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
            const size_t eq = t.find('=');
            if (eq != std::string::npos && eq > 0 && t[0] != '#') setenv(t.substr(0, eq).c_str(), t.substr(eq + 1).c_str(), 1);
        }
        fclose(tf);
    }
    fo.funcs = fo.dir + "/data/funcs";
    fo.cfg = fo.dir + "/settings.cfg";
    MakeDir(fo.dir);
    MakeDir(fo.dir + "/data");
    MakeDir(fo.funcs);
    ExtractFuncs(fo);
    SetRomFolder(fo);
    if (const char* ar = getenv("SSR_ANDROID_ROMS")) { fo.roms = ar; fo.saves = fo.roms + "/saves"; fo.art = fo.roms; MakeDir(fo.roms); }
    if (!fo.roms.empty()) MakeDir(fo.saves);
#else
    fo.dir = ProgramDir();
    fo.funcs = fo.dir + "/data/funcs";
#if defined(SSR_IOS)
    // iOS: the function tables in the app (read-only); the ROMs, saves, settings and game.log in
    // its Documents folder, which the Files app shows as On My iPhone > Pokémon Fulldex
    // (Info.plist: UIFileSharingEnabled), so ROMs can be put there and logs taken out.
    {
        const char* home = getenv("HOME");
        fo.dir = std::string(home ? home : ".") + "/Documents";
        MakeDir(fo.dir);
    }
#elif defined(__APPLE__)
    // The Mac app: the program's folder is inside the app (Contents/Resources, with the function
    // tables), which macOS may keep read-only, so the ROMs, saves and settings are in the user's
    // Application Support folder (O in the list opens its roms folder in the Finder).
    if (fo.dir.find(".app/Contents/") != std::string::npos)
    {
        const char* home = getenv("HOME");
        // (The program's name before it was Pokémon Fulldex: kept, so the ROMs, saves and
        // settings already there stay where they are.)
        fo.dir = std::string(home ? home : ".") + "/Library/Application Support/Pokemon DS Widescreen";
        for (size_t i = 1; i <= fo.dir.size(); i++) // (each folder on the way, as mkdir -p)
            if (i == fo.dir.size() || fo.dir[i] == '/') MakeDir(fo.dir.substr(0, i));
    }
#elif !defined(_WIN32)
    // From an AppImage (the Linux download for Gear Lever and the like): the program's folder is
    // the AppImage's own, read-only, so the ROMs, saves and settings are in the user's data
    // folder (~/.local/share/pokemon-ds-wide, O in the list opens its roms folder); the function
    // tables stay in the AppImage.
    if (const char* appImage = getenv("APPIMAGE"); appImage && *appImage)
    {
        const char* xdg = getenv("XDG_DATA_HOME");
        const char* home = getenv("HOME");
        const std::string base = xdg && *xdg ? xdg : std::string(home ? home : ".") + "/.local/share";
        fo.dir = base + "/pokemon-ds-wide";
        for (size_t i = 1; i <= fo.dir.size(); i++) // (each folder on the way, as mkdir -p)
            if (i == fo.dir.size() || fo.dir[i] == '/') MakeDir(fo.dir.substr(0, i));
    }
#endif
    fo.roms = fo.dir + "/roms";
    fo.saves = fo.dir + "/saves";
    fo.cfg = fo.dir + "/settings.cfg";
    if (const std::string rf = Launcher::ConfigValue(fo.cfg, "roms_folder"); !rf.empty())
    {
        std::error_code ec;
        if (std::filesystem::is_directory(std::filesystem::u8path(rf), ec)) fo.roms = rf;
        else fprintf(stderr, "[launcher] ROM folder is unavailable: %s\n", rf.c_str());
    }
    // SAVES FOLDER (the settings): a folder of the player's own, when it is there.
    if (const std::string sf = Launcher::ConfigValue(fo.cfg, "saves_folder"); !sf.empty())
    {
        std::error_code ec;
        if (std::filesystem::is_directory(std::filesystem::u8path(sf), ec)) { fo.saves = sf; fprintf(stderr, "[launcher] saves folder %s (the settings)\n", sf.c_str()); }
        else fprintf(stderr, "[launcher] saves folder %s is not there: the one beside the ROMs is used\n", sf.c_str());
    }
    fo.art = fo.dir;
    MakeDir(fo.roms);
    MakeDir(fo.saves);
#endif

    // The log goes to game.log beside the program (the last game's is kept as game-previous.log:
    // QUIT starts the program again for the list, whose few lines would otherwise replace it).
    const std::string logPath = fo.dir + "/game.log";
    struct stat ls;
    if (stat(logPath.c_str(), &ls) == 0 && ls.st_size > 1024)
    {
        const std::string prev = fo.dir + "/game-previous.log";
        remove(prev.c_str());
        rename(logPath.c_str(), prev.c_str());
    }
    if (freopen(logPath.c_str(), "w", stderr)) setvbuf(stderr, nullptr, _IOLBF, 0);
    fprintf(stderr, "[launcher] folder %s\n", fo.dir.c_str());
    if (fo.saves != fo.dir + "/saves") fprintf(stderr, "[launcher] saves folder %s\n", fo.saves.c_str()); // (SAVES FOLDER, or the phone's ROM folder)

#ifdef SSR_GBA
    for (int i = 1; i < argc; i++)
        if (argv[i] && IsGbaRomName(argv[i]) && directRom != argv[i]) ImportFile(fo, argv[i]);
#endif

    std::map<std::string, std::vector<uint32_t>> artCache;
    std::vector<Launcher::Game> games = FindGames(fo, artCache);
    bool copiedDirectGba = false;
    auto sameRom = [](const std::string& x, const std::string& y) {
        std::error_code e1, e2;
        const auto cx = std::filesystem::weakly_canonical(x, e1), cy = std::filesystem::weakly_canonical(y, e2);
        return !e1 && !e2 && cx == cy;
    };
#ifdef SSR_GBA
    // A shortcut into an existing library must not copy over another ROM with the same basename.
    if (!directRom.empty() && IsGbaRomName(directRom) &&
        std::none_of(games.begin(), games.end(), [&](const Launcher::Game& g) { return sameRom(g.rom, directRom); }))
    {
        copiedDirectGba = ImportFile(fo, directRom);
        if (copiedDirectGba) games = FindGames(fo, artCache);
    }
#endif

    // The list starts on the game played last.
    const std::string lastPath = fo.dir + "/last-game.txt";
    int chosen = 0;
    if (FILE* f = fopen(lastPath.c_str(), "r"))
    {
        char line[2048] = {};
        if (fgets(line, sizeof line, f))
        {
            std::string last = line;
            while (!last.empty() && (last.back() == '\n' || last.back() == '\r')) last.pop_back();
            for (size_t i = 0; i < games.size(); i++)
                if (games[i].rom == last) chosen = (int)i;
            Launcher::ContinueRom() = last;
        }
        fclose(f);
    }
    int winW = 1280, winH = 720;
    bool fullscreen = ConfigFullscreen(fo.cfg, winW, winH);
    int directGame = -1;
    // (The game itself, not a hack of it: a shortcut is for the game.)
    for (size_t i = 0; i < games.size() && !direct.empty(); i++)
        if (games[i].code == direct && Playable(fo, games[i]) && (directGame < 0 || (games[directGame].hack && !games[i].hack)))
            directGame = (int)i;
    if (!direct.empty() && directGame < 0) fprintf(stderr, "[launcher] --game %s: not in the list\n", direct.c_str());
    if (!directRom.empty())
    {
        const std::string copy = fo.roms + "/" + directRom.substr(directRom.find_last_of("/\\") + 1);
        for (size_t i = 0; i < games.size() && directGame < 0; i++)
            if (sameRom(games[i].rom, directRom) && Playable(fo, games[i])) directGame = (int)i;
        // Prefer an exact library path over an imported GBA copy with the same filename.
        if (directGame < 0 && copiedDirectGba && IsGbaRomName(directRom))
            for (size_t i = 0; i < games.size() && directGame < 0; i++)
                if (sameRom(games[i].rom, copy) && Playable(fo, games[i])) directGame = (int)i;
        if (directGame < 0) directGame = DirectRom::Ds(directRom, fo.saves, fo.art, games);
        if (directGame >= 0 && !Playable(fo, games[directGame])) directGame = -1;
        if (directGame < 0) fprintf(stderr, "[launcher] --rom %s: not in the list\n", directRom.c_str());
    }
#ifdef SSR_UPDATER
    // (At launch, as the list opens: a popup if a new version is out, never installed unasked.)
    Updater::Init(ProgramDir(), fo.dir);
#ifndef __ANDROID__
    if (directGame < 0) Updater::Check(true);
#endif
#endif
#ifdef __ANDROID__
    if (directGame < 0) JavaVoid("checkUpdatesQuiet");
#endif
    DiscordPresence::InLauncher();
    for (;;)
    {
        chosen = directGame >= 0 ? directGame : PickGame(fo, games, chosen, fullscreen, winW, winH);
#ifdef SSR_UPDATER
        if (chosen < 0 && Updater::RestartPending())
        {
            Updater::Restart();
            return false;
        }
#endif
        if (chosen < 0) return false;
        if (directGame >= 0 && games[chosen].platform != Launcher::N3DS)
        {
            Launcher::ConfirmHack(games, (size_t)chosen, fo.saves,
                                  [](const Launcher::Game& g, const std::string& name) { return g.folder + "/" + name; });
            if (Launcher::SavesPlayCheck(games[chosen], fo.saves + "/" + games[chosen].saveName)) { directGame = -1; continue; }
            const std::string error = PrepareLaunch(games[chosen]);
            if (!error.empty()) { fprintf(stderr, "[launcher] %s\n", error.c_str()); directGame = -1; continue; }
        }
#ifdef _WIN32
        // Omega Ruby or Alpha Sapphire: its own program plays it (n3ds.h), then the list again.
        if (games[chosen].platform == Launcher::N3DS)
        {
            const std::string rom = games[chosen].rom;
            if (FILE* f = fopen(lastPath.c_str(), "w"))
            {
                fprintf(f, "%s\n", rom.c_str());
                fclose(f);
            }
            DiscordPresence::Playing(Launcher::TitleCase(games[chosen].name));
            const auto start = std::chrono::steady_clock::now();
            N3ds::Play(fo.dir, games[chosen]);
            PlayTime::Add(fo.saves, (long long)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count());
            DiscordPresence::InLauncher();
            directGame = -1;
            games = FindGames(fo, artCache);
            chosen = 0;
            for (size_t i = 0; i < games.size(); i++)
                if (games[i].rom == rom) chosen = (int)i;
            continue;
        }
#endif
#if defined(__ANDROID__) || defined(SSR_IOS)
        // Sideways before either engine opens its window. On Android the hint alone can
        // leave the shared activity upright; on iOS the same, until the window asks.
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#ifdef __ANDROID__
        JavaVoid("showGameOrientation");
#else
        IosOrientGame();
#endif
#endif
#ifdef SSR_GBA
        // A GBA game: played here (Pokemon GBA Widescreen, built in), then the list again.
        if (games[chosen].platform == Launcher::GBA)
        {
            const std::string rom = games[chosen].rom;
            LibraryHistory recent;recent.Load(fo.saves+"/fulldex-library.txt");if(Launcher::ConfigValue(fo.cfg,"extra_library_history")=="1")recent.Played(rom);
            if (FILE* f = fopen(lastPath.c_str(), "w"))
            {
                fprintf(f, "%s\n", rom.c_str());
                fclose(f);
            }
            GbaWide::Options o;
            o.roms = rom.substr(0, rom.find_last_of("/\\"));
            o.saves = games[chosen].folder;
            o.data = fo.dir;
            // GBA SECOND SCREEN (extra_gba_mirror) needs the PokeDaisy GBA-tree change (GbaWide::Options::secondDisplay),
            // not in the release GBA inputs yet: deferred to the GBA companion work.
            o.fullscreen = ConfigFullscreen(fo.cfg, o.w, o.h);
            Launcher::SyncGbaSettings(fo.cfg, o.data);
            fprintf(stderr, "[launcher] GBA: %s\n", rom.c_str());
            // (Cloud saves: a newer save comes down first; the save goes up when the game closes.)
            CloudAuto::Start(fo.saves, games[chosen].folder + "/" + games[chosen].saveName);
            const auto gbaStart = std::chrono::steady_clock::now();
            GbaRa::Play(rom, o, fo.cfg);
            PlayTime::Add(fo.saves, (long long)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - gbaStart).count());
            CloudAuto::Saved();
            CloudAuto::Finish();
            directGame = -1;
            games = FindGames(fo, artCache);
            chosen = 0;
            for (size_t i = 0; i < games.size(); i++)
                if (games[i].rom == rom) chosen = (int)i;
            continue;
        }
#endif
        break;
    }
    g_canChange = true;
    Launcher::ConfirmHack(games, (size_t)chosen, fo.saves,
                          [](const Launcher::Game& g, const std::string& name) { return g.folder + "/" + name; });
    const Launcher::Game& game = games[chosen];
    LibraryHistory recent;recent.Load(fo.saves+"/fulldex-library.txt");if(Launcher::ConfigValue(fo.cfg,"extra_library_history")=="1")recent.Played(game.rom);
    if (const Launcher::KnownGame* k = Launcher::Known(game.code)) g_name = game.hack ? Launcher::TitleCase(game.name) : k->name;
    DiscordPresence::Playing(g_name.empty() ? std::string("a game") : g_name);
    if (FILE* f = fopen(lastPath.c_str(), "w"))
    {
        fprintf(f, "%s\n", game.rom.c_str());
        fclose(f);
    }
    fprintf(stderr, "[launcher] %s (game code %s), save %s/%s\n", game.rom.c_str(), game.code.c_str(), fo.saves.c_str(),
        game.saveName.c_str());
    g_args = { argc > 0 && argv[0] ? argv[0] : "pokemon-ds-wide", game.rom, "--save", fo.saves + "/" + game.saveName,
               "--config", fo.cfg };
    if (game.hack) g_args.push_back("--rom-hack"); // (says once that widescreen is tried on it)
    // (Play From Slot, Manage Saves: the game starts from that state.)
    if (!Launcher::PlayState().empty() && game.platform == Launcher::DS && StateSlots::Exists(Launcher::PlayState())) { g_args.push_back("--state-load"); g_args.push_back(Launcher::PlayState()); }
    Launcher::PlayState().clear();
    // (Tests: SSR_TEST_ARGS, more arguments for the game chosen, e.g. "--frames 600".)
    // (On a phone, which cannot set the environment: the first line of test-args.txt in the
    // app's own folder, e.g. "--env SSR_WIDE_THREADS=3". For measuring, never shipped in it.)
    std::string testArgs = getenv("SSR_TEST_ARGS") ? getenv("SSR_TEST_ARGS") : "";
#ifdef __ANDROID__
    if (FILE* tf = fopen((fo.dir + "/test-args.txt").c_str(), "r"))
    {
        char line[512];
        if (fgets(line, sizeof(line), tf))
        {
            testArgs = line;
            while (!testArgs.empty() && (testArgs.back() == '\n' || testArgs.back() == '\r')) testArgs.pop_back();
            fprintf(stderr, "[launcher] test-args.txt: %s\n", testArgs.c_str());
        }
        fclose(tf);
    }
#endif
    if (const char* extra = testArgs.empty() ? nullptr : testArgs.c_str())
    {
        std::string a;
        for (const char* c = extra;; c++)
        {
            if (*c && *c != ' ') a += *c;
            else if (!a.empty()) { g_args.push_back(a); a.clear(); }
            if (!*c) break;
        }
    }
#ifndef SSR_IOS
    // (melonDS's JIT; iOS lets no app make code as it runs, so there the interpreter plays.)
    g_args.push_back("--jit");
#endif
    // SoulSilver and HeartGold take their layouts from the program's own tables and must not be
    // given a function table (one switches to the Black games' layouts); the others take theirs.
    if (!IsGame(game.code, "IPGE") && !IsGame(game.code, "IPKE"))
    {
        g_args.push_back("--funcs");
        g_args.push_back(fo.funcs + "/" + game.code + ".txt");
    }
    for (auto& a : g_args) g_argv.push_back(a.data());
    g_argv.push_back(nullptr);
    argc = (int)g_args.size();
    argv = g_argv.data();
    return true;
}

bool CanChangeGame() { return g_canChange; }
const char* GameName() { return g_name.c_str(); }
void ChangeGame() { g_change = true; }

void Finish()
{
    DiscordPresence::Stop();
    if (!g_change) return;
    fflush(stderr);
    // The program again, without arguments: the list.
#if defined(__ANDROID__)
    JavaVoid("restart"); // (GameActivity: a new process, which opens on the list)
#elif defined(_WIN32)
    wchar_t self[32768];
    const DWORD n = GetModuleFileNameW(nullptr, self, 32768);
    if (n == 0 || n >= 32768) return;
    // (game.log let go first: the new program keeps it as game-previous.log, which Windows does
    // not allow while this one still has it open, and the game's log was lost.)
    fflush(stderr);
    freopen("NUL", "w", stderr);
    std::wstring cmd = L"\"" + std::wstring(self) + L"\"";
    STARTUPINFOW si = {};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {};
    // The game list comes back as a new process, and Windows gives a new process's window
    // the focus only if the one that had it allows it: without this the list opened behind
    // or without the focus, and a controller did nothing until it was clicked. (This process
    // had the last input, the menu's QUIT, so it may allow it.)
    AllowSetForegroundWindow(ASFW_ANY);
    if (CreateProcessW(self, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
#elif defined(__APPLE__)
    char self[4096];
    uint32_t size = sizeof self;
    if (_NSGetExecutablePath(self, &size) != 0) return;
    execl(self, self, (char*)nullptr);
#else
    char self[4096];
    const ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
    if (n <= 0) return;
    self[n] = 0;
    execl(self, self, (char*)nullptr);
#endif
}
}
#endif

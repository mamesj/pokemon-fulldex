#pragma once
#include "national_dex.h"
#include "national_dex_art.h"
#include <pthread.h>
#include <functional>
// The game picker of the all-games download (one program for every game): finds the players'
// ROMs, reads each one's name and icon from the ROM itself (every DS game carries a 32x32 icon
// and its title in its banner), and draws the list. Kept free of any platform so that it can be
// drawn into a picture on a PC as well; the Switch shows it with switch_platform.cpp before the
// game's window opens.
#include <algorithm>
#include <climits>
#include <filesystem>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>
#include <utility>
#include "art_smooth.h"
#include <cmath>
#include <vector>
#include <map>
#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <dirent.h>
#include "ssr_os.h"
#include "gamecode.h"
#include "romhacks.h"
#include <sys/stat.h>

#include "ui.h"
#include "optional_features.h"
#include "cloud_saves.h"
#include "hub_link.h"
#include "theme_pack.h"
#include "launcher_text.h"
#include "launcher_fonts.h"
#include "net_http.h"
#include <deque>
#include <set>
#include "support_qr.h"
#include "play_time.h"
#include "state_slots.h"
#include "save_backups.h"
#ifdef SSR_POKEMON_HUB
#include "pokemon_hub.h"
#include "companion_rom.h"
#endif
// (JPEG and PNG pictures for the list: the program that draws it defines STB_IMAGE_IMPLEMENTATION
// with STBI_ONLY_JPEG and STBI_ONLY_PNG before including this, in one file.)
#include "thirdparty/stb_image.h"

namespace Launcher
{

// Keep the established launcher font while the replacement design is unfinished.
inline void ModernText(Canvas& c, int x, int y, const std::string& text, int scale, uint32_t colour)
{
    c.Text(x, y, text, scale, colour);
}

// The consoles the list has games of, in the order its sections and tabs come in.
enum Platform { DS, GBA, N3DS, PLATFORMS };
inline const char* PlatformName(int p) { return p == DS ? "DS" : p == GBA ? "GBA" : "3DS"; }
// The tabs along the top: ALL (every section, each under its console's heading), then one per
// console.
// (HOME: the games the player added with X, their favourites; until they add any, the game played
// last. ROMHACKS: the hacks, whatever their console.)
enum TabId { TAB_HOME, TAB_ALL, TAB_DS, TAB_GBA, TAB_3DS, TAB_HACKS, TABS };
// Tabs of the player's own, after the built-in ones (tab TABS + k): a name and its games' ROM paths;
// kept in fulldex-tabs.txt beside the saves, a line a tab (its name, then its ROMs), tab-separated.
// A tab of the player's own: its games, and the built-in tab it comes after (Move Left / Right; Home stays first).
struct CustomTab { std::string name; std::vector<std::string> roms; int after = -1; int tag = 0; };
inline std::vector<CustomTab>& CustomTabs() { static std::vector<CustomTab> t; return t; }
inline std::string& TabsFile() { static std::string f; return f; }
inline int TabCount() { return TABS + (int)CustomTabs().size(); }
// Every tab in the order shown (the built-in ones 0 to TABS-1, the player's own TABS + their place) and
// those hidden: fulldex-tabs-order.txt with the tabs. The built-in ones are moved and hidden, never deleted.
inline std::vector<int>& TabOrderAll() { static std::vector<int> o; return o; }
inline std::vector<int>& TabHidden() { static std::vector<int> h; return h; }
inline bool TabIsHidden(int t) { const std::vector<int>& h = TabHidden(); return std::find(h.begin(), h.end(), t) != h.end(); }
inline void TabOrderFix()
{
    std::vector<int>& o = TabOrderAll();
    const int n = TabCount();
    std::vector<int> out;
    std::vector<bool> seen((size_t)n, false);
    for (const int t : o) if (t >= 0 && t < n && !seen[(size_t)t]) { out.push_back(t); seen[(size_t)t] = true; }
    for (int t = 0; t < n; t++) if (!seen[(size_t)t]) out.push_back(t); // (new ones at the end)
    o = out;
    std::vector<int>& h = TabHidden();
    h.erase(std::remove_if(h.begin(), h.end(), [n](int t) { return t < 0 || t >= n; }), h.end());
}
inline std::vector<int> TabOrder()
{
    TabOrderFix();
    std::vector<int> v;
    for (const int t : TabOrderAll()) if (!TabIsHidden(t)) v.push_back(t);
    if (v.empty()) v.push_back(TabOrderAll().empty() ? 0 : TabOrderAll()[0]);
    return v;
}
inline int TabPos(int t) { const std::vector<int> o = TabOrder(); for (int i = 0; i < (int)o.size(); i++) if (o[i] == t) return i; return 0; }
// The tab by places along from t (round the ends), and the first and last shown.
inline int TabStep(int t, int by) { const std::vector<int> o = TabOrder(); const int n = (int)o.size(); return o[((TabPos(t) + by) % n + n) % n]; }
inline int FirstTab() { return TabOrder().front(); }
inline int LastTab() { return TabOrder().back(); }
inline int TabDistance(int from, int to) { return TabPos(to) - TabPos(from); }
inline std::string TabOrderPath() { const std::string& f = TabsFile(); return f.size() < 4 ? std::string() : f.substr(0, f.size() - 4) + "-order.txt"; }
inline void LoadTabOrder()
{
    TabOrderAll().clear();
    TabHidden().clear();
    bool found = false;
    if (FILE* f = fopen(TabOrderPath().c_str(), "rb"))
    {
        char line[4096];
        while (fgets(line, sizeof line, f))
        {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            const size_t tb = l.find('\t');
            if (tb == std::string::npos) continue;
            const std::string key = l.substr(0, tb);
            if (key != "order" && key != "hidden") continue;
            if (key == "order") found = true;
            std::vector<int>& dst = key == "hidden" ? TabHidden() : TabOrderAll();
            for (size_t at = tb + 1; at < l.size();)
            {
                size_t end = l.find(',', at);
                if (end == std::string::npos) end = l.size();
                if (end > at) dst.push_back(atoi(l.c_str() + at));
                at = end + 1;
            }
        }
        fclose(f);
    }
    if (!found)
    {
        // (As it was: the built-in tabs, the player's own after the one each was moved past.)
        std::vector<int>& o = TabOrderAll();
        const std::vector<CustomTab>& tabs = CustomTabs();
        for (int b = 0; b < TABS; b++)
        {
            o.push_back(b);
            for (int k = 0; k < (int)tabs.size(); k++)
                if ((tabs[k].after < 0 || tabs[k].after >= TABS ? TABS - 1 : tabs[k].after) == b) o.push_back(TABS + k);
        }
    }
    TabOrderFix();
}
inline bool SaveTabOrder()
{
    if (TabOrderPath().empty()) return false;
    TabOrderFix();
    FILE* f = fopen(TabOrderPath().c_str(), "wb");
    if (!f) return false;
    std::string o, h;
    for (const int t : TabOrderAll()) o += (o.empty() ? "" : ",") + std::to_string(t);
    for (const int t : TabHidden()) h += (h.empty() ? "" : ",") + std::to_string(t);
    fprintf(f, "order\t%s\nhidden\t%s\n", o.c_str(), h.c_str());
    return fclose(f) == 0;
}
inline std::string TabsSig()
{
    // (Their order and those hidden too: drawn again when one is moved or hidden.)
    std::string s;
    for (const CustomTab& t : CustomTabs()) s += t.name + "|" + std::to_string(t.roms.size()) + ";";
    for (const int t : TabOrderAll()) s += std::to_string(t) + ",";
    s += "/";
    for (const int t : TabHidden()) s += std::to_string(t) + ",";
    return s;
}
inline void LoadTabs(const std::string& dir)
{
    TabsFile() = dir + "/fulldex-tabs.txt";
    std::vector<CustomTab>& tabs = CustomTabs();
    tabs.clear();
    if (FILE* f = fopen(TabsFile().c_str(), "rb"))
    {
        char line[8192];
        while (fgets(line, sizeof line, f))
        {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            CustomTab t;
            size_t at = 0;
            while (at <= l.size())
            {
                size_t end = l.find('\t', at);
                if (end == std::string::npos) end = l.size();
                const std::string part = l.substr(at, end - at);
                if (t.name.empty() && at == 0) t.name = part;
                else if (part.rfind("@after=", 0) == 0) t.after = atoi(part.c_str() + 7);
                else if (!part.empty()) t.roms.push_back(part);
                at = end + 1;
            }
            if (!t.name.empty()) tabs.push_back(t);
        }
        fclose(f);
    }
    LoadTabOrder();
}
inline bool SaveTabs()
{
    if (TabsFile().empty()) return false;
    SaveTabOrder();
    const std::string tmp = TabsFile() + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = true;
    for (const CustomTab& t : CustomTabs())
    {
        std::string l = t.name;
        if (t.after >= 0) l += "\t@after=" + std::to_string(t.after);
        for (const std::string& r : t.roms) l += "\t" + r;
        if (fprintf(f, "%s\n", l.c_str()) < 0) ok = false;
    }
    ok = fclose(f) == 0 && ok;
    if (!ok) return false;
    if (rename(tmp.c_str(), TabsFile().c_str()) != 0) { remove(TabsFile().c_str()); if (rename(tmp.c_str(), TabsFile().c_str()) != 0) return false; }
    return true;
}
// What the list is searched for (Search on the tab bar; "": not searched) and its order (list_sort):
// kept apart from the focus, which a click can start again.
inline std::string& SearchText() { static std::string s; return s; }
inline std::string& SortKey() { static std::string s; return s; }
inline const char* TabName(int t) { static const char* n[] = { "HOME", "ALL", "DS", "GBA", "3DS BETA", "ROMHACKS" }; return t >= 0 && t < TABS ? n[t] : t < TabCount() ? "CUSTOM" : "ALL"; }
inline std::string TabLabel(int t)
{
    static const char* n[] = { "Home", "All", "DS", "GBA", "3DS Beta", "Romhacks" };
    if (t >= 0 && t < TABS) return n[t];
    const int k = t - TABS;
    return k >= 0 && k < (int)CustomTabs().size() ? CustomTabs()[k].name : std::string("All");
}

struct Game
{
    int platform = DS;
    bool built = false;   // (a 3DS game: its game code is made, so it plays at its best)
    std::string rom;      // the ROM's full path
    std::string folder;   // where its save (and a funcs.txt of the player's own) live
    std::string code;     // the game code, e.g. IREO
    std::string name;     // as shown, e.g. POKEMON BLACK 2
    std::string saveName; // e.g. black2.sav
    bool known = false;   // one of the games this program plays
    bool hasSave = false;
    bool hasIcon = false;
    uint32_t icon[32 * 32] = {}; // ARGB, transparent where the icon is
    // A larger picture for the game, if the player has one (FindArt): ARGB, Art x Art pixels.
    std::vector<uint32_t> art;
    // Set when the same game is also in another folder: this one's folder, shown under it.
    std::string where;
    // A game on its way (AddComingSoon): its tile says so and it cannot be played.
    bool comingSoon = false;
    long long soonAt = 0; // (UTC seconds) a game coming on a set day: its tile counts down to it
    bool favourite = false;
    std::string customName; // the player's own name for it (Rename; fulldex-names.txt), empty: its own
    // Its own picture as found (art, hasIcon), kept so Change Picture can put it back (ApplyArt).
    std::vector<uint32_t> ownArt;
    bool ownIcon = false, ownKept = false;
    long long lastPlayed = 0;
    // (The ROMs page: taken off the list (fulldex-hidden.txt); the file's size.)
    bool hidden = false;
    long long fileSize = 0;
    // A ROM hack (romhacks.h): a modified ROM of a game this plays, with a save of its own (named
    // after its file) rather than the game's (baseSave). Widescreen is tried on it as on the game.
    bool hack = false;
    std::string baseSave;
    // A patch file that could not be made into a game: why (shown, and it cannot be played).
    std::string problem;
};

inline std::string& ContinueRom() { static std::string r; return r; } // the game played last (last-game.txt)
// The game whose picture is blurred behind the glass launcher; empty: the chosen one. (The Switch sets
// it a moment after the chosen game changes: making it again takes most of a redraw there.)
inline std::string& BackdropRom() { static std::string r; return r; }
// Where the arrows are on the glass launcher besides the games and the tabs: a section of the
// sidebar (side, -1: none) or the banner's Continue (banner 1) or Game Options (2).
// popup: the game's menu (A on a game) open on that row, -1 closed; where it was drawn (for taps).
// dialog: 1 Remove Game, 2 Delete ROM File (sure?), 3 Rename (text: what is typed); drow its row;
// dX..dRows where its rows were drawn (for taps).
struct GlassFocus { int side = -1; int banner = 0; int popup = -1; int popX = 0, popY = 0, popW = 0, popRow = 0;
                    int dialog = 0, drow = 0; std::string text; int dX = 0, dY = 0, dW = 0, dRow = 0, dRows = 0;
                    // (Dialogs 5 and up, Manage Saves: their title, words and rows as the launcher made them;
                    // dPaths each row's backup, or #folder / #close.)
                    std::string dTitle, dMsg; std::vector<std::string> dItems, dPaths;
                    // (The sidebar's pages: PageRows and below.)
                    int page = -1, prow = 0; int pX = 0, pY = 0, pW = 0, pRow = 0;
                    // (The Updates page: the updater's state as the platform gives it, -1 none here; Updater::State's order.)
                    int upState = -1; bool upCan = false; std::string upHave, upNew, upText, upNotes;
                    // (Its What's New: prow 2 when chosen; the first line shown, the last that can be, where it was drawn.)
                    int nScroll = 0, nMax = 0, nX = 0, nY = 0, nW = 0, nH = 0, nLine = 0;
                    // (The ROMs page: pact the chosen ROM's action, -1 the list; rScroll its first row shown, rVis how many;
                    // aX..aRow where the actions were drawn.)
                    bool freeScroll = false; int abX = 0, abW = 0, abRow = 0, abKey = -1; int pact = -1, rScroll = 0, rVis = 0, aX = 0, aY = 0, aW = 0, aRow = 0;
                    // (Sort at the tab bar's right end: chosen (with the tabs), the list's order, where it was drawn.)
                    bool onSort = false; std::string sortKey; int sortX = 0, sortY = 0, sortW = 0, sortH = 0;
                    // (The tab bar's tools: which is chosen while onSort (1 +, 2 Search, 3 Sort), where they were drawn; the
                    // keyboard's use (0 a game's name, 1 a new tab (kbRom put in it), 2 the search, 3 tab kbTab's name).)
                    int tool = 3; int toolX[4] = {}, toolW[4] = {}, toolY = 0, toolH = 0;
                    int kbFor = 0, kbTab = 0, menuTab = 0; std::string kbRom;
                    // (The settings drawn as a page: its title; where its list and preview go, for the menu.)
                    std::string pageTitle; int sX = 0, sY = 0, sW = 0, sH = 0, sListW = 0, sPvX = 0, sListH = 0, sPvY = 0, pdY = 0, dScroll = 0, dMax = 0; };
inline GlassFocus& Focus() { static GlassFocus f; return f; }
inline int PopupCount() { return 7; }
// The game's menu in the order shown: Play, Manage Saves, Add To Home, Rename, Change Picture, Add To
// Tab, Remove Game (row -> what it is, PopupLabel's numbers).
inline int PopupItem(int row) { static const int order[] = { 0, 4, 1, 2, 3, 5, 6 }; return row >= 0 && row < 7 ? order[row] : row; }
inline std::string PopupLabel(const Game& g, int i)
{
    static const char* n[] = { "Play", "", "Rename", "Change Picture", "Manage Saves", "Add To Tab", "Remove Game" };
#ifdef __ANDROID__
    // (A 3DS game inside the app: its game code, from a PC's FullDex Builder or built on the phone.)
    if (g.platform == N3DS && i == 3) return "Use Game Code From PC";
    if (g.platform == N3DS && i == 4) return g.built ? "Game Code Built" : "Build For Best Speed (25 Min)";
#endif
    return i == 1 ? (g.favourite ? "Remove From Home" : "Add To Home") : i >= 0 && i < 7 ? n[i] : "";
}
// The pause menu's keyboard page (Menu::keyGrid) for Rename: BACK, four rows of keys (capitals, small
// letters or signs: layer), then the two layer keys, SPACE, DELETE and done. ids: each item's key.
enum { KB_BACK = -1, KB_KEY0 = 1000, KB_LAYER = 2000, KB_SYMBOLS, KB_SPACE, KB_DEL, KB_DONE };
inline void BuildKeyboard(Menu& m, std::vector<int>& ids, const std::string& text, int layer, const char* prompt, const char* done)
{
    const int keep = m.open ? m.selected : -1;
    m.items.clear();
    ids.clear();
    m.open = true; m.style = 1; m.pageIcon = Glass::I_SPARKLE;
    m.keyGrid = true; m.keySecret = false; m.keyText = text; m.keyPrompt = prompt;
    m.items.push_back({ "BACK", "" });
    m.items.back().kind = Menu::K_BACK;
    ids.push_back(KB_BACK);
    static const char* const layers[3][4] = {
        { "1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM_.!" },
        { "1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm_.!" },
        { "1234567890", "!#$%&*()+=", "[]{}<>/\\|~", ";:'\",?^`.@" } };
    auto key = [&](int id, const std::string& label, int row, int col, int span) {
        m.items.push_back({ label, "" });
        ids.push_back(id);
        MenuItem& it = m.items.back();
        it.kind = Menu::K_KEY; it.keyRow = row; it.keyCol = col; it.keySpan = span;
    };
    const char* const* rows = layers[std::max(0, std::min(2, layer))];
    for (int r = 0; r < 4; r++)
    {
        const int n = (int)strlen(rows[r]);
        for (int i = 0; i < n; i++) key(KB_KEY0 + r * 16 + i, std::string(1, rows[r][i]), r, 10 - n + i * 2, 2);
    }
    key(KB_LAYER, layer == 0 ? "abc" : "ABC", 4, 0, 3);
    key(KB_SYMBOLS, layer == 2 ? "abc" : "#+=", 4, 3, 3);
    key(KB_SPACE, "Space", 4, 6, 6);
    key(KB_DEL, "Delete", 4, 12, 4);
    key(KB_DONE, done, 4, 16, 4);
    m.selected = keep > 0 && keep < (int)m.items.size() ? keep : 1;
}
inline int DialogAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (!f.dialog || f.dW <= 0 || f.dRow <= 0 || x < f.dX || x >= f.dX + f.dW || y < f.dY) return -1;
    const int i = (y - f.dY) / f.dRow;
    return i < f.dRows ? i : -1;
}
// The player's own changes to the list, kept beside the saves: fulldex-names.txt (a ROM's path, a tab,
// the name it shows) and fulldex-hidden.txt (the ROMs taken off the list, the same way).
inline std::map<std::string, std::string> ReadPairs(const std::string& path)
{
    std::map<std::string, std::string> out;
    if (FILE* f = fopen(path.c_str(), "rb"))
    {
        char line[4096];
        while (fgets(line, sizeof line, f))
        {
            std::string s = line;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            const size_t t = s.find('\t');
            if (t != std::string::npos && t > 0) out[s.substr(0, t)] = s.substr(t + 1);
        }
        fclose(f);
    }
    return out;
}
inline bool SetPair(const std::string& path, const std::string& key, const std::string& value)
{
    std::map<std::string, std::string> all = ReadPairs(path);
    if (value.empty()) all.erase(key);
    else all[key] = value;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    for (const auto& kv : all) fprintf(f, "%s\t%s\n", kv.first.c_str(), kv.second.c_str());
    return fclose(f) == 0;
}
inline void ApplyLocal(std::vector<Game>& games, const std::string& savesDir)
{
    const auto names = ReadPairs(savesDir + "/fulldex-names.txt");
    const auto hidden = ReadPairs(savesDir + "/fulldex-hidden.txt");
    for (Game& g : games)
    {
        const auto it = names.find(g.rom);
        g.customName = it == names.end() ? std::string() : it->second;
    }
    games.erase(std::remove_if(games.begin(), games.end(), [&](const Game& g) { return hidden.count(g.rom) > 0; }), games.end());
}
inline int PopupAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (f.popW <= 0 || f.popRow <= 0 || x < f.popX || x >= f.popX + f.popW || y < f.popY) return -1;
    const int i = (y - f.popY) / f.popRow;
    return i < PopupCount() ? i : -1;
}
// What the list found out about the ROMs and patches (a whole DS ROM is read to tell a clean one
// from a hack), kept between looks in a file of the saves folder: BeginScan before reading the
// ROMs, EndScan after. Without BeginScan no ROM is checked (a one-game program's rom.nds).
inline RomHacks::Cache*& ScanCache()
{
    static RomHacks::Cache* cache = nullptr;
    return cache;
}
// (Tests: SSR_ROM_CHECK_WAIT=1 waits for each check while looking, so that a first look, e.g.
// SSR_LIST_SHOT's, shows what was found.)
inline bool ScanWaits()
{
    static const bool wait = getenv("SSR_ROM_CHECK_WAIT") && *getenv("SSR_ROM_CHECK_WAIT") == '1';
    return wait;
}
inline void BeginScan(const std::string& savesFolder)
{
    static RomHacks::Cache cache;
    cache = RomHacks::Cache();
    RomHacks::MakeDir(savesFolder);
    cache.Load(savesFolder + "/rom-check.txt");
    ScanCache() = &cache;
}
inline void EndScan()
{
    if (ScanCache()) ScanCache()->Save();
    ScanCache() = nullptr;
}
// A hack's (or another ROM's) save, from its file name: hack-blaze-black-2-redux.sav (never the
// name of a game's own save).
inline std::string HackSave(const std::string& rom)
{
    std::string k;
    for (char c : RomHacks::Lower(RomHacks::Stem(rom))) k += isalnum((unsigned char)c) ? c : '-';
    while (k.find("--") != std::string::npos) k.erase(k.find("--"), 1);
    if (!k.empty() && k.back() == '-') k.pop_back();
    if (!k.empty() && k.front() == '-') k.erase(0, 1);
    return "hack-" + (k.empty() ? std::string("game") : k) + ".sav";
}
inline std::string HackName(const std::string& rom)
{
    std::string n;
    for (char c : RomHacks::Stem(rom)) n += c == '_' ? ' ' : (char)toupper((unsigned char)c);
    return n.empty() ? "ROM HACK" : n;
}
// A romhack: a DS ROM romhacks.h found is not a clean release (hack), or a GBA game that is not one
// of the official ones by its code (Ultra Violet, ...).
inline bool IsHack(const Game& g)
{
    if (g.hack) return true; // (a DS hack: told apart from its clean game by romhacks.h)
    if (!g.known || g.comingSoon || g.platform != GBA) return false;
    const std::string c = g.code.substr(0, 3);
    return c != "AXV" && c != "AXP" && c != "BPE" && c != "BPR" && c != "BPG";
}
// The search's words in a game's name (its own or the player's), any case.
inline bool NameHas(const Game& g, const std::string& q)
{
    auto low = [](std::string s) { for (char& ch : s) ch = (char)tolower((unsigned char)ch); return s; };
    const std::string k = low(q);
    return low(g.customName).find(k) != std::string::npos || low(g.name).find(k) != std::string::npos;
}
inline bool InTab(const std::vector<Game>& games, int i, int tab)
{
    const Game& g = games[i];
    if (!SearchText().empty() && !NameHas(g, SearchText())) return false;
    if (tab >= TABS)
    {
        const int k = tab - TABS;
        if (k >= (int)CustomTabs().size()) return false;
        for (const std::string& r : CustomTabs()[k].roms) if (r == g.rom) return true;
        return false;
    }
    switch (tab)
    {
    case TAB_HOME:
        for (const Game& o : games)
            if (o.favourite) return g.favourite;
        return !ContinueRom().empty() && g.rom == ContinueRom();
    case TAB_DS: return g.platform == DS;
    case TAB_GBA: return g.platform == GBA;
    case TAB_3DS: return g.platform == N3DS;
    case TAB_HACKS: return IsHack(g);
    default: return true;
    }
}

// The size the larger pictures are kept at (the tiles of the list at 720 lines).
constexpr int Art = 224;

// Games on their way, in their console's section with COMING SOON over the picture: Pokemon
// Omega Ruby on the 3DS (its picture as any game's: icons/ECRA.jpg, or SSR_PC_ART's ECRA.jpg).
inline void AddComingSoon(std::vector<Game>& games)
{
    Game g;
    g.platform = N3DS;
    g.code = "ECRA";
    g.rom = "coming-soon:ECRA"; // (never a file: so never the last game played, nor started)
    g.name = "POKEMON OMEGA RUBY";
    g.comingSoon = true;
    g.soonAt = 1794441600LL; // (12 November 2026, 00:00 UTC: Omega Ruby and Alpha Sapphire come together)
    games.push_back(std::move(g));
    // Alpha Sapphire, the same day.
    Game a;
    a.platform = N3DS;
    a.code = "ECLA";
    a.rom = "coming-soon:ECLA";
    a.name = "POKEMON ALPHA SAPPHIRE";
    a.comingSoon = true;
    a.soonAt = 1794441600LL;
    games.push_back(std::move(a));
}

// The day it comes, as "Release: 12 Nov 2026" ("" without a day, or once it has come).
inline std::string ComingSoonDate(const Game& g)
{
    if (!g.comingSoon || g.soonAt <= 0 || g.soonAt <= (long long)std::time(nullptr)) return "";
    long long z = g.soonAt / 86400 + 719468; // (days to a calendar date, in UTC)
    const long long era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153, day = doy - (153 * mp + 2) / 5 + 1, mon = mp < 10 ? mp + 3 : mp - 9;
    const long long year = yoe + era * 400 + (mon <= 2 ? 1 : 0);
    static const char* const names[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    return "Release: " + std::to_string(day) + " " + names[mon - 1] + " " + std::to_string(year);
}
// What goes over the picture of a game on its way: Coming Soon, or the time left when it has a day
// (soonAt), by the minute: 4d 9h 12m. Once the day has come: Coming Soon again.
inline std::string ComingSoonText(const Game& g)
{
    if (!g.comingSoon) return "";
    const long long left = g.soonAt > 0 ? g.soonAt - (long long)std::time(nullptr) : 0;
    if (left <= 0) return "Coming Soon";
    const long long mins = (left + 59) / 60;
    const long long d = mins / 1440, h = (mins % 1440) / 60, m = mins % 60;
    if (d > 0) return std::to_string(d) + "d " + std::to_string(h) + "h " + std::to_string(m) + "m";
    if (h > 0) return std::to_string(h) + "h " + std::to_string(m) + "m";
    return std::to_string(m) + "m";
}

// The games this program plays, in the order the list shows them.
struct KnownGame
{
    const char* code;
    const char* name;
    const char* save;
};
inline const std::vector<KnownGame>& KnownGames()
{
    static const std::vector<KnownGame> games = [] {
    std::vector<KnownGame> list = {
        { "IRBO", "Pokemon Black", "black.sav" },
        { "IRBS", "Pokemon Black (ES)", "black-es.sav" },
        { "IRBF", "Pokemon Black (FR)", "black-fr.sav" },
        { "IRAO", "Pokemon White", "white.sav" },
        { "IRAS", "Pokemon White (ES)", "white-es.sav" },
        { "IRAF", "Pokemon White (FR)", "white-fr.sav" },
        { "IREO", "Pokemon Black 2", "black2.sav" },
        { "IRES", "Pokemon Black 2 (ES)", "black2-es.sav" },
        { "IREF", "Pokemon Black 2 (FR)", "black2-fr.sav" },
        { "IRDO", "Pokemon White 2", "white2.sav" },
        { "IRDS", "Pokemon White 2 (ES)", "white2-es.sav" },
        { "IRDF", "Pokemon White 2 (FR)", "white2-fr.sav" },
        { "IPKE", "Pokemon HeartGold", "heartgold.sav" },
        { "IPKF", "Pokemon HeartGold (FR)", "heartgold-fr.sav" },
        { "IPGE", "Pokemon SoulSilver", "soulsilver.sav" },
        { "IPGF", "Pokemon SoulSilver (FR)", "soulsilver-fr.sav" },
        { "CPUE", "Pokemon Platinum", "platinum.sav" },
        { "ADAE", "Pokemon Diamond (Experimental)", "diamond.sav" },
        { "APAE", "Pokemon Pearl (Experimental)", "pearl.sav" },
        { "CPUS", "Pokemon Platinum (ES)", "platinum-es.sav" },
        { "IPGS", "Pokemon SoulSilver (ES)", "soulsilver-es.sav" },
        { "IPKS", "Pokemon HeartGold (ES)", "heartgold-es.sav" },
        { "ADAS", "Pokemon Diamond (ES, Experimental)", "diamond-es.sav" },
        { "APAF", "Pokemon Pearl (FR, Experimental)", "pearl-fr.sav" },
        { "APAS", "Pokemon Pearl (ES, Experimental)", "pearl-es.sav" },
        { "APAD", "Pokemon Pearl (DE, Experimental)", "pearl-de.sav" },
        { "ADAD", "Pokemon Diamond (DE, Experimental)", "diamond-de.sav" },
        { "ADAI", "Pokemon Diamond (IT, Experimental)", "diamond-it.sav" },
        { "IPKI", "Pokemon HeartGold (IT)", "heartgold-it.sav" },
        { "CPUF", "Pokemon Platinum (FR)", "platinum-fr.sav" },
    };
    for (const auto& p : RegionalProfiles::profiles) list.push_back({p.code,p.name,p.save});
    return list;
    }();
    return games;
}
inline const KnownGame* Known(const std::string& code)
{
    for (const KnownGame& g : KnownGames())
        if (code == g.code) return &g;
    return nullptr;
}
inline int Order(const std::string& code)
{
    // (DS, then GBA: Emerald, Ultra Violet (UV22: its own code here, as FireRed's is BPRE too),
    // FireRed, LeafGreen.)
    static const char* order[] = { "IRBO", "IRAO", "IREO", "IRDO", "IPKE", "IPGE", "CPUE", "ADAE", "APAE", "BPEE", "UV22", "BPRE", "BPGE" };
    const std::string base = BaseGameCode(code);
    const int n = (int)(sizeof order / sizeof order[0]);
    for (int i = 0; i < n; i++)
        if (base == order[i]) return i;
    return n;
}

// A name as the list has it (POKEMON EMERALD) as a title: Pokemon Emerald (FireRed and LeafGreen
// as they are spelt).
inline std::string TitleCase(const std::string& name)
{
    std::string out;
    bool start = true;
    for (char c : name)
    {
        out += start ? (char)toupper((unsigned char)c) : (char)tolower((unsigned char)c);
        start = c == ' ';
    }
    for (const char* word : { "Firered", "Leafgreen" })
    {
        const size_t at = out.find(word);
        if (at != std::string::npos) out[at + 4] = (char)toupper((unsigned char)out[at + 4]);
    }
    return out;
}

inline bool EndsWithNds(const std::string& name)
{
    if (name.size() < 4) return false;
    std::string ext = name.substr(name.size() - 4);
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    return ext == ".nds";
}

// Reads a ROM's game code, title and icon. False if it is not a DS ROM.
inline bool ReadGame(const std::string& rom, const std::string& folder, Game& g)
{
    FILE* f = fopen(rom.c_str(), "rb");
    if (!f) return false;
    struct stat info;
    if (fstat(fileno(f), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0x200) { fclose(f); return false; }
    uint8_t header[0x70];
    bool ok = fread(header, 1, sizeof header, f) == sizeof header;
    g = Game();
    g.rom = rom;
    g.folder = folder;
    if (ok)
    {
        g.code.assign((const char*)header + 0x0C, 4);
        for (char c : g.code)
            if (c < 0x20 || c > 0x7E) ok = false;
    }
    // The banner: version, CRCs, then the icon's 4-bit pixels (4x4 tiles of 8x8), its 16
    // colours (BGR555, the first transparent) and the title in six languages (UTF-16).
    const uint32_t banner = ok ? header[0x68] | header[0x69] << 8 | header[0x6A] << 16 | (uint32_t)header[0x6B] << 24 : 0;
    uint8_t ban[0x440];
    if (ok && banner >= 0x200 && (uint64_t)banner + sizeof ban <= (uint64_t)info.st_size && fseek(f, (long)banner, SEEK_SET) == 0 && fread(ban, 1, sizeof ban, f) == sizeof ban)
    {
        uint32_t pal[16];
        for (int i = 0; i < 16; i++)
        {
            const uint16_t c = (uint16_t)(ban[0x220 + 2 * i] | ban[0x221 + 2 * i] << 8);
            const uint32_t r = (c & 31) * 255 / 31, gr = (c >> 5 & 31) * 255 / 31, b = (c >> 10 & 31) * 255 / 31;
            pal[i] = (i ? 0xFF000000u : 0u) | r << 16 | gr << 8 | b;
        }
        for (int ty = 0; ty < 4; ty++)
            for (int tx = 0; tx < 4; tx++)
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++)
                    {
                        const uint8_t byte = ban[0x20 + (ty * 4 + tx) * 32 + y * 4 + x / 2];
                        g.icon[(ty * 8 + y) * 32 + tx * 8 + x] = pal[x & 1 ? byte >> 4 : byte & 15];
                    }
        g.hasIcon = true;
        // The English title's first line, for a game this program does not know.
        for (int i = 0; i < 64; i++)
        {
            const uint16_t ch = (uint16_t)(ban[0x340 + 2 * i] | ban[0x341 + 2 * i] << 8);
            if (ch == 0 || ch == '\n') break;
            g.name += ch == 0xE9 ? 'E' : ch < 0x80 ? (char)ch : ' ';
        }
    }
    fclose(f);
    if (!ok) return false;
    if (const KnownGame* k = Known(g.code))
    {
        g.known = true;
        g.name = k->name;
        g.saveName = k->save;
        // A hack keeps its game's code: told by the ROM not being one of the clean releases.
        if (ScanCache() && RomHacks::CheckDs(rom, g.code, *ScanCache(), ScanWaits()) == RomHacks::Kind::Modified)
        {
            g.hack = true;
            g.baseSave = k->save;
            g.name = HackName(rom);
            g.saveName = HackSave(rom);
        }
    }
    else
    {
        if (g.name.empty()) g.name = rom.substr(rom.find_last_of('/') + 1);
        g.saveName = HackSave(rom); // (each its own, not one game.sav for all)
    }
    for (char& c : g.name) c = (char)toupper((unsigned char)c);
    struct stat st;
    g.hasSave = stat((folder + "/" + g.saveName).c_str(), &st) == 0;
    if (!g.hasSave && g.known)
    {
        // A save brought from another emulator or a flashcart is usually named after the ROM
        // (Pokemon - Edicion Negra 2.sav): used when this game's own save is not there yet. Not
        // a name another game's save has (a ROM called black2.nds beside English Black 2's
        // black2.sav), and never black2.sav or game.sav for another language's game: two games
        // writing one file would lose one of them.
        const std::string romBase = rom.substr(rom.find_last_of('/') + 1);
        const size_t dot = romBase.find_last_of('.');
        const std::string stemSave = (dot != std::string::npos ? romBase.substr(0, dot) : romBase) + ".sav";
        bool taken = stemSave == "game.sav";
        for (const KnownGame& k : KnownGames()) taken = taken || stemSave == k.save;
        if (!taken && stemSave != g.saveName && stat((folder + "/" + stemSave).c_str(), &st) == 0)
        {
            g.saveName = stemSave;
            g.hasSave = true;
        }
    }
    return true;
}

// A JPEG or PNG picture, scaled to Art x Art (each pixel the average of the ones it covers).
inline bool ImageAllowed(const unsigned char* data, size_t size)
{
    int w = 0, h = 0, n = 0;
    return data && size > 0 && size <= (16u << 20) && size <= INT_MAX &&
        stbi_info_from_memory(data, (int)size, &w, &h, &n) && w > 0 && h > 0 &&
        w <= 8192 && h <= 8192 && (uint64_t)w * h <= 16000000;
}

inline bool LoadArt(const unsigned char* data, size_t size, std::vector<uint32_t>& out)
{
    if (!ImageAllowed(data, size)) return false;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!px) return false;
    out.assign((size_t)Art * Art, 0);
    for (int y = 0; y < Art; y++)
        for (int x = 0; x < Art; x++)
        {
            const int x0 = x * w / Art, x1 = std::max(x0 + 1, (x + 1) * w / Art);
            const int y0 = y * h / Art, y1 = std::max(y0 + 1, (y + 1) * h / Art);
            uint32_t sum[4] = {}, count = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++)
                {
                    const unsigned char* p = px + ((size_t)yy * w + xx) * 4;
                    for (int c = 0; c < 4; c++) sum[c] += p[c];
                    count++;
                }
            out[(size_t)y * Art + x] = (sum[3] / count) << 24 | (sum[0] / count) << 16 | (sum[1] / count) << 8 | sum[2] / count;
        }
    stbi_image_free(px);
    return true;
}

inline bool ReadFile(const std::string& path, std::vector<unsigned char>& data, long offset = 0, long size = -1)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    if (size < 0)
    {
        fseek(f, 0, SEEK_END);
        size = ftell(f) - offset;
    }
    bool ok = size > 0 && size < (16 << 20) && fseek(f, offset, SEEK_SET) == 0;
    if (ok)
    {
        data.resize((size_t)size);
        ok = fread(data.data(), 1, data.size(), f) == data.size();
    }
    fclose(f);
    return ok;
}

// The icon a Switch homebrew program carries (its Homebrew Menu icon, a JPEG), from a .nro file.
inline bool NroIcon(const std::string& nro, std::vector<unsigned char>& jpeg)
{
    std::vector<unsigned char> head;
    if (!ReadFile(nro, head, 0, 0x20) || memcmp(head.data() + 0x10, "NRO0", 4) != 0) return false;
    const long size = head[0x18] | head[0x19] << 8 | head[0x1A] << 16 | (long)head[0x1B] << 24;
    std::vector<unsigned char> aset;
    if (!ReadFile(nro, aset, size, 0x38) || memcmp(aset.data(), "ASET", 4) != 0) return false;
    uint64_t off = 0, len = 0;
    for (int i = 7; i >= 0; i--)
    {
        off = off << 8 | aset[0x08 + i];
        len = len << 8 | aset[0x10 + i];
    }
    return len > 0 && ReadFile(nro, jpeg, size + (long)off, (long)len);
}

// The game's larger picture, from the first of: <list folder>/icons/<game code>.jpg or .png (e.g.
// icons/IREO.jpg), a picture with the ROM's name (Black 2.nds: Black 2.jpg or .png), icon.jpg in
// the game's folder, or the Homebrew Menu icon of a one-game program (.nro) in its folder (not in
// the list's own folder: that one is the all-games program, whose icon is no game's). The
// downloads hold none: these are the player's own.
// Square ARGB pixels (side n) at side m: smooth both ways (bilinear when larger, the average of
// the pixels covered when smaller), with the colours weighted by their opacity.
inline std::vector<uint32_t> ResizeArt(const std::vector<uint32_t>& src, int n, int m)
{
    std::vector<uint32_t> out((size_t)m * m);
    auto at = [&](int x, int y) { return src[(size_t)std::min(n - 1, std::max(0, y)) * n + std::min(n - 1, std::max(0, x))]; };
    for (int y = 0; y < m; y++)
        for (int x = 0; x < m; x++)
        {
            double acc[4] = {}, wsum = 0;
            auto add = [&](uint32_t p, double w) {
                const double al = (p >> 24) / 255.0;
                acc[0] += w * al * ((p >> 16) & 255);
                acc[1] += w * al * ((p >> 8) & 255);
                acc[2] += w * al * (p & 255);
                acc[3] += w * (p >> 24);
                wsum += w;
            };
            if (m >= n)
            {
                const double fx = (x + 0.5) * n / m - 0.5, fy = (y + 0.5) * n / m - 0.5;
                const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
                const double tx = fx - x0, ty = fy - y0;
                add(at(x0, y0), (1 - tx) * (1 - ty));
                add(at(x0 + 1, y0), tx * (1 - ty));
                add(at(x0, y0 + 1), (1 - tx) * ty);
                add(at(x0 + 1, y0 + 1), tx * ty);
            }
            else
            {
                const int sx0 = x * n / m, sx1 = std::max(sx0 + 1, (x + 1) * n / m);
                const int sy0 = y * n / m, sy1 = std::max(sy0 + 1, (y + 1) * n / m);
                for (int sy = sy0; sy < sy1; sy++)
                    for (int sx = sx0; sx < sx1; sx++) add(at(sx, sy), 1);
            }
            const double alpha = acc[3] / wsum;
            const double k = alpha > 0 ? 255.0 / (alpha * wsum) : 0;
            auto c = [&](double v) { return (uint32_t)std::min(255.0, std::max(0.0, v * k + 0.5)); };
            out[(size_t)y * m + x] = ((uint32_t)std::min(255.0, alpha + 0.5) << 24) | (c(acc[0]) << 16) | (c(acc[1]) << 8) | c(acc[2]);
        }
    return out;
}

// The game's picture at side m, for an icon outside the list (desktop shortcuts, HOME Menu
// entries): its box art or picture, else its DS icon (sharp, as a pixel picture). Empty when it
// has neither.
inline std::vector<uint32_t> GamePicture(const Game& g, int m)
{
    if (!g.art.empty()) return ResizeArt(g.art, Art, m);
    if (!g.hasIcon) return {};
    std::vector<uint32_t> out((size_t)m * m);
    for (int y = 0; y < m; y++)
        for (int x = 0; x < m; x++) out[(size_t)y * m + x] = g.icon[(y * 32 / m) * 32 + x * 32 / m];
    return out;
}

// A picture of the player's own (Change Picture): its middle square, made Art x Art. PNG or JPG.
inline bool LoadSquareArt(const unsigned char* data, size_t size, std::vector<uint32_t>& out)
{
    if (!ImageAllowed(data, size)) return false;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!px) return false;
    const int side = std::min(w, h), ox = (w - side) / 2, oy = (h - side) / 2;
    out.assign((size_t)Art * Art, 0);
    for (int y = 0; y < Art; y++)
        for (int x = 0; x < Art; x++)
        {
            const int x0 = ox + x * side / Art, x1 = std::max(x0 + 1, ox + (x + 1) * side / Art);
            const int y0 = oy + y * side / Art, y1 = std::max(y0 + 1, oy + (y + 1) * side / Art);
            uint32_t sum[4] = {}, count = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++)
                {
                    const unsigned char* p = px + ((size_t)yy * w + xx) * 4;
                    for (int k = 0; k < 4; k++) sum[k] += p[k];
                    count++;
                }
            out[(size_t)y * Art + x] = (sum[3] / count) << 24 | (sum[0] / count) << 16 | (sum[1] / count) << 8 | sum[2] / count;
        }
    stbi_image_free(px);
    return true;
}
// Keep imported pictures by their contents, so ROMs with the same filename in different
// library folders never replace each other's artwork. The original image is only read.
inline bool ImportArt(const std::string& rom, const std::string& savesDir, const std::string& picture, std::string& error)
{
    error.clear();
    std::vector<unsigned char> data;
    std::vector<uint32_t> decoded;
    if (!ReadFile(picture, data) || !LoadSquareArt(data.data(), data.size(), decoded))
    { error = "THAT PICTURE COULD NOT BE READ: PNG OR JPG ONLY"; return false; }
    SHA1_CTX ctx; SHA1Init(&ctx); SHA1Update(&ctx, data.data(), (uint32_t)data.size());
    unsigned char digest[20]; SHA1Final(digest, &ctx);
    char hex[41] = {};
    for (int i = 0; i < 20; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    const std::string stem = std::string("picture-") + hex;
    const std::string ext = data[0] == 0x89 ? ".png" : ".jpg";
    const std::string dir = savesDir + "/artwork";
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
    if (ec) { error = "COULD NOT CREATE THE PICTURE FOLDER"; return false; }
    for (int i = 0; i < 1000; ++i)
    {
        const std::string name = stem + (i ? "-" + std::to_string(i) : "") + ext;
        const std::string dest = dir + "/" + name;
        if (RomHacks::Exists(dest))
        {
            std::vector<unsigned char> existing;
            if (!ReadFile(dest, existing) || existing != data) continue;
        }
        else
        {
            const std::string temp = dest + ".tmp";
            FILE* f = fopen(temp.c_str(), "wb");
            if (!f) { error = "COULD NOT KEEP THE PICTURE"; return false; }
            const bool wrote = fwrite(data.data(), 1, data.size(), f) == data.size();
            const bool closed = fclose(f) == 0;
            if (!wrote || !closed) { remove(temp.c_str()); error = "COULD NOT KEEP THE PICTURE"; return false; }
            std::filesystem::rename(std::filesystem::u8path(temp), std::filesystem::u8path(dest), ec);
            if (ec) { remove(temp.c_str()); error = "COULD NOT KEEP THE PICTURE"; return false; }
        }
        if (SetPair(savesDir + "/fulldex-art.txt", rom, name)) return true;
        error = "COULD NOT SAVE THE PICTURE CHOICE";
        return false;
    }
    error = "COULD NOT KEEP THE PICTURE";
    return false;
}

// The pictures chosen with Change Picture (fulldex-art.txt beside the saves: a ROM's path, a tab, the
// picture's file in saves/artwork, or placeholder for the plain card), over each game's own picture.
inline void ApplyArt(std::vector<Game>& games, const std::string& savesDir)
{
    const auto chosen = ReadPairs(savesDir + "/fulldex-art.txt");
    for (Game& g : games)
    {
        if (!g.ownKept) { g.ownArt = g.art; g.ownIcon = g.hasIcon; g.ownKept = true; }
        g.art = g.ownArt;
        g.hasIcon = g.ownIcon;
        const auto it = chosen.find(g.rom);
        if (it == chosen.end()) continue;
        if (it->second == "placeholder") { g.art.clear(); g.hasIcon = false; continue; }
        std::vector<unsigned char> data;
        std::vector<uint32_t> art;
        if (ReadFile(savesDir + "/artwork/" + it->second, data) && LoadSquareArt(data.data(), data.size(), art)) g.art = art;
    }
}

inline void FindArt(Game& g, const std::string& listFolder)
{
    std::vector<unsigned char> data;
    const std::string base = g.rom.substr(0, g.rom.size() - 4);
    for (const std::string& path : { listFolder + "/icons/" + g.code + ".jpg", listFolder + "/icons/" + g.code + ".png",
                                     base + ".jpg", base + ".png", g.folder + "/icon.jpg" })
        if (ReadFile(path, data) && LoadArt(data.data(), data.size(), g.art)) return;
    if (g.folder == listFolder) return;
    if (DIR* d = opendir(g.folder.c_str()))
    {
        std::vector<std::string> nros;
        while (dirent* e = readdir(d))
        {
            const std::string n = e->d_name;
            if (n.size() > 4 && n.compare(n.size() - 4, 4, ".nro") == 0) nros.push_back(g.folder + "/" + n);
        }
        closedir(d);
        std::sort(nros.begin(), nros.end());
        for (const std::string& nro : nros)
            if (NroIcon(nro, data) && LoadArt(data.data(), data.size(), g.art)) return;
    }
}

// The ROMs in a folder (and in its roms/ folder, with subfolder); their saves go in the folder
// itself.
inline void ScanFolder(const std::string& folder, std::vector<Game>& out, bool subfolder = true)
{
    for (const std::string& dir : { folder, subfolder ? folder + "/roms" : std::string() })
    {
        if (dir.empty()) continue;
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        std::vector<std::string> names;
        while (dirent* e = readdir(d))
            if (EndsWithNds(e->d_name)) names.push_back(e->d_name);
        closedir(d);
        std::sort(names.begin(), names.end());
        for (const std::string& n : names)
        {
            Game g;
            if (ReadGame(dir + "/" + n, folder, g)) out.push_back(g);
        }
    }
}

// The DS hacks made from patch files (romhacks.h) in `folders`, put on the clean DS ROMs already in
// `games`, made into outDir once; added to `games` (saves in savesFolder), and any patch that could
// not be used as a game that cannot be played, saying why. Between BeginScan and EndScan. wait
// false: a patch whose ROM is still being read (in the background) shows up on a later look.
inline void AddPatchedHacks(const std::vector<std::string>& folders, const std::string& outDir, const std::string& savesFolder,
                            std::vector<Game>& games, bool wait, const std::function<void(const std::string&)>& status = {})
{
    if (!ScanCache()) return;
    std::vector<std::string> clean;
    for (const Game& g : games)
        if (g.platform == DS && g.known && !g.hack) clean.push_back(g.rom);
    std::vector<RomHacks::Problem> problems;
    for (const std::string& rom : RomHacks::MakeDsHacks(folders, clean, outDir, *ScanCache(), problems, wait || ScanWaits(), status))
    {
        bool dup = false;
        for (const Game& g : games) dup |= g.rom == rom;
        Game g;
        if (!dup && ReadGame(rom, savesFolder, g)) games.push_back(g);
    }
    for (const RomHacks::Problem& p : problems)
    {
        Game g;
        g.platform = DS;
        g.rom = p.patch;
        g.folder = savesFolder;
        g.code = "PTCH";
        g.name = HackName(p.patch);
        g.problem = p.why;
        games.push_back(std::move(g));
    }
}

// A hack played before this told hacks apart used its game's save: when the game itself (a clean
// ROM of it) isn't in the list, that save is the hack's, and is copied to the hack's own the first
// time (the game's is left as it was). savePath: where a game's save of that name is.
inline void AdoptHackSaves(std::vector<Game>& games, const std::function<std::string(const Game&, const std::string&)>& savePath)
{
    for (Game& h : games)
    {
        if (!h.hack || h.baseSave.empty()) continue;
        bool cleanToo = false;
        for (const Game& g : games) cleanToo |= g.platform == DS && !g.hack && g.known && g.code == h.code && g.folder == h.folder;
        if (cleanToo) continue;
        const std::string own = savePath(h, h.saveName), base = savePath(h, h.baseSave);
        if (RomHacks::Exists(own) || !RomHacks::Exists(base)) continue;
        if (RomHacks::CopyFile(base, own))
        {
            fprintf(stderr, "[launcher] %s: save %s copied to %s (its own from now on)\n", h.rom.c_str(), base.c_str(), own.c_str());
            h.hasSave = true;
        }
    }
}

// Before a DS game starts: its ROM checked for certain (waiting while it is read, the first time),
// so that a hack never plays with its game's save. games[index] is the game; cacheFolder holds
// rom-check.txt (BeginScan's folder).
inline void ConfirmHack(std::vector<Game>& games, size_t index, const std::string& cacheFolder,
                        const std::function<std::string(const Game&, const std::string&)>& savePath)
{
    Game& g = games[index];
    if (g.platform != DS || !g.known || g.hack) return;
    const KnownGame* k = Known(g.code);
    if (!k) return;
    RomHacks::Cache cache;
    cache.Load(cacheFolder + "/rom-check.txt");
    const bool hack = RomHacks::CheckDs(g.rom, g.code, cache, true) == RomHacks::Kind::Modified;
    cache.Save(false);
    if (!hack) return;
    g.hack = true;
    g.baseSave = k->save;
    g.name = HackName(g.rom);
    g.saveName = HackSave(g.rom);
    g.hasSave = RomHacks::Exists(savePath(g, g.saveName));
    AdoptHackSaves(games, savePath);
}

// By console (the list's sections), then known games first, in the list's order, then the rest
// by name.
inline void Sort(std::vector<Game>& games)
{
    std::stable_sort(games.begin(), games.end(), [](const Game& a, const Game& b) {
        if (a.platform != b.platform) return a.platform < b.platform;
        const int oa = Order(a.code), ob = Order(b.code);
        if (oa != ob) return oa < ob;
        if (a.hack != b.hack) return b.hack; // (a game, then its hacks)
        return a.name < b.name;
    });
}

// The list's colours (ARGB): near-black with white text and red details, on the Switch and on a PC.
struct Theme
{
    uint32_t bg = 0xFF0B0B0C, band = 0xFF141416, bandLine = 0xFFE0242C, title = 0xFFFFFFFF;
    uint32_t tile = 0xFF1A1A1D, frame = 0xFFE0242C;
    uint32_t name = 0xFFB8B8BC, nameSel = 0xFFFFFFFF, sub = 0xFF707076, subSel = 0xFFE0242C, bad = 0xFFE0242C;
    uint32_t hint = 0xFF8A8A90;
    int bandLineH = 1;       // the band's line, in font pixels
    int frameW = 2;          // the chosen game's frame, in font pixels
    uint32_t selBar = 0;     // a bar under the chosen game's name (0: none)
    bool pokemon = false;
    struct Background* background = nullptr; // a picture behind the list (BACKGROUND: YOUR PICTURE)
};

// The player's own picture behind the list (background.jpg or .png beside the saves folder; on
// the Switch, with the saves): kept as read, and once fitted to the canvas, darkened so the
// names stay readable.
struct Background
{
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    std::vector<uint32_t> fit;
    int fw = 0, fh = 0;
    // The picture covering a w x h canvas (cut at the sides or top and bottom), or nullptr.
    const uint32_t* Fitted(int cw, int ch)
    {
        if (px.empty() || cw <= 0 || ch <= 0) return nullptr;
        if (fw == cw && fh == ch) return fit.data();
        fit.assign((size_t)cw * ch, 0);
        const double sc = std::max((double)cw / w, (double)ch / h);
        const int ox = (int)((w * sc - cw) / 2), oy = (int)((h * sc - ch) / 2);
        for (int y = 0; y < ch; y++)
        {
            const int sy = std::min(h - 1, (int)((y + oy) / sc));
            for (int x = 0; x < cw; x++)
            {
                const uint32_t p = px[(size_t)sy * w + std::min(w - 1, (int)((x + ox) / sc))];
                // (A third of its brightness: the grey names stayed hard to read on a bright one
                // at two fifths.)
                const uint32_t r = (p >> 16 & 0xFF) / 3, g = (p >> 8 & 0xFF) / 3, b = (p & 0xFF) / 3;
                fit[(size_t)y * cw + x] = 0xFF000000u | r << 16 | g << 8 | b;
            }
        }
        fw = cw;
        fh = ch;
        return fit.data();
    }
};

// The accent colour of the list (COLOUR on the settings page): the band's line, the chosen
// game's frame and its save line.
inline Theme ThemeNamed(const std::string& colour)
{
    Theme t;
    t.bandLine = t.frame = t.subSel = AccentNamed(colour); // (ui.h; red when not set)
    return t;
}

// A setting's value in a settings file ("" when it has none).
inline std::string ConfigValue(const std::string& cfg, const char* key)
{
    std::string out;
    if (FILE* f = fopen(cfg.c_str(), "r"))
    {
        char buf[512];
        const size_t n = strlen(key);
        while (fgets(buf, sizeof buf, f))
            if (!strncmp(buf, key, n) && buf[n] == '=')
            {
                out = buf + n + 1;
                while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
            }
        fclose(f);
    }
    return out;
}

// The GBA games (Pokemon GBA Widescreen, built in) take the DS games' settings where both have
// one: written into its gba-settings.ini in `dataDir` as a GBA game starts, the others there (its
// WIDESCREEN shape, FAST FORWARD SPEED) left as they are. SMOOTH SCALING ON is its SMOOTH; OFF
// keeps its SHARP or WHOLE PIXELS, whichever was chosen there (SHARP to start: it fills the screen;
// WHOLE PIXELS, the crispest, leaves a border, which its WIDESCREEN 18:9 keeps to the top and
// bottom at 720p). FPS COUNTER, VOLUME and, but on the Switch, CONTROLLER BUTTONS as they are.
inline void SyncGbaSettings(const std::string& cfg, const std::string& dataDir)
{
    std::vector<std::pair<std::string, std::string>> set;
    const std::string ini = dataDir + "/gba-settings.ini";
    const std::string had = ConfigValue(ini, "scaling");
    set.push_back({ "scaling", ConfigValue(cfg, "smooth") == "1" ? "smooth" : had == "whole" ? "whole" : "sharp" });
    // ASPECT RATIO, as the DS games have it (their AUTO, 16:9, 16:10, 3:2, 4:3).
    {
        const std::string a = ConfigValue(cfg, "aspect");
        set.push_back({ "widescreen", a == "16:9" || a == "16:10" || a == "3:2" || a == "4:3" ? a : "auto" });
    }
#if !defined(__SWITCH__) && !defined(SSR_PHONE)
    set.push_back({ "fullscreen", ConfigValue(cfg, "fullscreen") == "0" ? "0" : "1" });
#endif
    set.push_back({ "show_fps", ConfigValue(cfg, "show_fps") == "1" ? "1" : "0" });
    const std::string lcd = ConfigValue(cfg, "gba_lcd");
    if (lcd == "0" || lcd == "1") set.push_back({ "lcd", lcd });
    set.push_back({ "fast_button", ConfigValue(cfg, "fast_button") == "toggle" ? "toggle" : "hold" });
    const std::string fast = ConfigValue(cfg, "fast_lock");
    if (fast == "0" || fast == "1") set.push_back({ "fast_lock", fast });
    const std::string vol = ConfigValue(cfg, "volume");
    set.push_back({ "volume", vol.empty() ? "100" : vol });
#ifndef __SWITCH__
    set.push_back({ "buttons", ConfigValue(cfg, "buttons") == "positions" ? "positions" : "labels" });
#endif
    // FAST FORWARD SOUND as the DS games have it, and where the GBA games write back a setting
    // changed in their pause menu that the DS games share (so they, and the next GBA game, have it).
    set.push_back({ "ff_mute", ConfigValue(cfg, "ff_mute") == "1" ? "1" : "0" });
    set.push_back({ "host_settings", cfg });
    std::vector<std::string> lines;
    if (FILE* f = fopen(ini.c_str(), "r"))
    {
        char buf[512];
        while (fgets(buf, sizeof buf, f))
        {
            std::string l = buf;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            bool ours = false;
            for (const auto& kv : set) ours |= !l.compare(0, kv.first.size() + 1, kv.first + "=");
            if (!ours && !l.empty()) lines.push_back(l);
        }
        fclose(f);
    }
    for (const auto& kv : set) lines.push_back(kv.first + "=" + kv.second);
    if (FILE* f = fopen(ini.c_str(), "w"))
    {
        for (const std::string& l : lines) fprintf(f, "%s\n", l.c_str());
        fclose(f);
    }
}

// The list's look from the settings file (COLOUR, BACKGROUND): the picture read from `folder`
// (background.jpg or .png), at most 2048 pixels on a side (larger ones are read at a fraction).
inline void LoadLook(const std::string& cfg, const std::string& folder, Theme& t, Background& b)
{
    const std::string language = ConfigValue(cfg, "list_lang");
    UiLang::Set(language.empty() ? ConfigValue(cfg, "language") : language);
    if (const char* lg = getenv("SSR_LIST_LANG")) UiLang::Set(lg); // (test: the launcher in a language)
    t = ThemeNamed(ConfigValue(cfg, "list_colour"));
    b = Background();
    t.background = nullptr;
    if (ConfigValue(cfg, "list_background") != "picture") return;
    std::vector<unsigned char> data;
    for (const char* name : { "/background.jpg", "/background.png", "/background.jpeg" })
    {
        if (!ReadFile(folder + name, data)) continue;
        if (!ImageAllowed(data.data(), data.size())) continue;
        int w = 0, h = 0, n = 0;
        unsigned char* px = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &n, 4);
        if (!px) continue;
        const int step = std::max(1, (std::max(w, h) + 2047) / 2048);
        b.w = std::max(1, w / step);
        b.h = std::max(1, h / step);
        b.px.resize((size_t)b.w * b.h);
        for (int y = 0; y < b.h; y++)
            for (int x = 0; x < b.w; x++)
            {
                const unsigned char* p = px + ((size_t)y * step * w + (size_t)x * step) * 4;
                b.px[(size_t)y * b.w + x] = 0xFF000000u | p[0] << 16 | p[1] << 8 | p[2];
            }
        stbi_image_free(px);
        t.background = &b;
        return;
    }
}

// Where the list's parts go on a canvas: the title band, the tabs under it, then the games'
// tiles (each the game's picture with its name below), four across at most, in a part of their
// own that scrolls: the content, with y from its top.
struct Grid
{
    int s, gap, labelH, tabsY, tabsH, headH, top, bottom, tile, cols, gx;
    int panelX = 0, panelW = 0;
    int rowH() const { return tile + labelH + gap; }
    // A card's size and the steps between cards (the classic list: a tile with its name below).
    int cw = 0, ch = 0, colStep = 0, rowStep = 0;
    bool horiz = false; // (a phone sideways: one row of cards that scrolls along, not down)
};
// The glass launcher (DrawGlass) on landscape screens with the clean font; the classic list otherwise.
inline bool& GlassOn() { static bool on = true; return on; }
// A phone: its text and buttons bigger (the design 480 high sideways, 405 wide upright), the Tiles layout whichever way it is held.
inline bool& PhoneUi() { static bool p = false; return p; }
inline bool& PortraitNow() { static bool p = false; return p; }
inline bool& ShortNow() { static bool p = false; return p; }
inline void Orient(const Canvas& c) { PortraitNow() = c.h > c.w; ShortNow() = PhoneUi() && c.h <= c.w; }
inline bool PhoneLook() { return PortraitNow() || ShortNow(); } // (a phone, or any upright window: the phone layouts, the Sidebar's own pieces made small)
inline float GU(const Canvas& c) { return c.h > c.w ? c.w / 405.0f : PhoneUi() ? c.h / 480.0f : c.h / 720.0f; } // one design pixel (the designs are 1280 x 720)
inline bool PanelOn(); // (the Panel layout: Launcher > Layout)
inline bool PanelHome(); // (the Panel layout, on the home screen: the sidebar's pages keep their sidebar)
inline bool GridOnly();
inline bool TilesOn(); // (the Tiles layout: Launcher > Layout)
inline bool TilesHome(); // (the Tiles layout, on the home screen: a game carousel over a row of tiles, one per section)
// The Panel home: one glass panel with the sections and the tabs along its top, the game list on its left,
// the chosen game's preview on its right (Draw, the grid, the taps and the buttons all use this).
struct PanelBox
{
    int x, y, w, h, pad, secY, secH, tabY, tabH, contentY, contentB, listX, listW, prevX, prevW;
};
inline PanelBox PanelGeometry(const Canvas& c)
{
    const float u = GU(c);
    PanelBox b;
    b.pad = (int)(16 * u);
    b.x = (int)(28 * u); b.y = (int)(24 * u); b.w = c.w - 2 * b.x; b.h = c.h - (int)(70 * u) - b.y;
    b.secY = b.y + b.pad; b.secH = (int)(44 * u);
    b.tabY = b.secY; b.tabH = b.secH; // (the tabs on the sections' row, between them and the tools)
    b.contentY = b.secY + b.secH + (int)(14 * u);
    b.contentB = b.y + b.h - b.pad;
    b.listX = b.x + b.pad; b.listW = std::min((int)(430 * u), (b.w - 2 * b.pad) * 36 / 100);
    b.prevX = b.listX + b.listW + (int)(16 * u); b.prevW = b.x + b.w - b.pad - b.prevX;
    return b;
}
// The Tiles home: the chosen game's cover large in the middle of a row of covers, its name and the buttons under it,
// a row of tiles (one per section) along the foot.
// The Tiles home's carousel in motion: pos is where it stands among the games (2.5: halfway from
// the third to the fourth), eased towards the chosen one each time it is drawn; drag is how far a
// finger or the mouse holds it from there, in games. (The clock is the computer's own.)
struct CarouselMotion
{
    double pos = -1, drag = 0;
    std::string of;      // the tab and how many games pos is among: a change puts it at the chosen one
    std::chrono::steady_clock::time_point at;
    bool active = false; // still moving: the list draws again without waiting for input
    void Release() { pos += drag; drag = 0; } // (the finger let go: on from where it was held)
    double Step(int chosen, const std::string& among)
    {
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::min(0.1, std::chrono::duration<double>(now - at).count());
        at = now;
        if (pos < 0 || of != among) { pos = chosen; of = among; }
        // (From further than three away the games in between are not run through.)
        if (pos < chosen - 3) pos = chosen - 3;
        if (pos > chosen + 3) pos = chosen + 3;
        pos += (chosen - pos) * (1 - std::exp(-dt * 13));
        if (std::abs(chosen - pos) < 0.004) pos = chosen;
        active = pos != chosen;
        return pos + drag;
    }
};
inline CarouselMotion& Carousel() { static CarouselMotion m; return m; }
struct TilesBox { int coverY, cover, nameY, metaY, btnY, tileY, tileH; int cols = 7, rows = 1, gapT = 14, musicH = 46; };
inline TilesBox TilesGeometry(const Canvas& c)
{
    const float u = GU(c);
    TilesBox b;
    b.coverY = (int)(90 * u); b.cover = (int)(250 * u);
    b.nameY = (int)(378 * u); b.metaY = (int)(408 * u); b.btnY = (int)(432 * u);
    b.tileH = (int)(96 * u); b.tileY = c.h - (int)(70 * u) - b.tileH;
    b.musicH = (int)(46 * u);
    Orient(c);
    if (c.h > c.w)
    {
        // Held upright: the tiles two rows of four.
        b.coverY = (int)(112 * u); b.cover = (int)(190 * u);
        b.nameY = b.coverY + b.cover + (int)(28 * u); b.metaY = b.nameY + (int)(28 * u); b.btnY = b.metaY + (int)(24 * u);
        b.cols = 4; b.rows = 2; b.gapT = (int)(8 * u); b.tileH = (int)(76 * u);
        b.tileY = c.h - (int)(64 * u) - b.rows * b.tileH - (b.rows - 1) * b.gapT;
    }
    else if (ShortNow())
    {
        // A phone sideways: everything smaller down the screen.
        b.coverY = (int)(84 * u); b.cover = (int)(150 * u);
        b.nameY = (int)(258 * u); b.metaY = (int)(279 * u); b.btnY = (int)(296 * u);
        b.tileH = (int)(74 * u); b.tileY = c.h - (int)(20 * u) - b.tileH; b.musicH = (int)(34 * u);
    }
    return b;
}
inline bool Glassy(const Canvas& c) { Orient(c); return GlassOn() && Menu::Clean; } // (Menu::Clean, not UseClean: Japanese and Korean are drawn smooth here too, with the launcher's CJK font)
inline int GlassTabAt(const Canvas& c, int x, int y);
inline Grid Layout(const Canvas& c)
{
    Orient(c);
    Grid g;
    g.s = std::max(1, std::min(c.w, c.h) / 240); // the font's scale: 3 at 720 lines
    if (Glassy(c))
    {
        const float u = GU(c);
        g.gap = (int)(14 * u); g.labelH = 0; g.headH = 0;
        g.tabsY = (int)(28 * u); g.tabsH = (int)(44 * u);
        g.panelX = (int)(204 * u); g.panelW = c.w - g.panelX - (int)(28 * u);
        g.top = (int)(304 * u); g.bottom = c.h - (int)(82 * u);
        if (PhoneLook())
        {
            // A phone. Upright: two columns of cards under the banner; sideways: rows of smaller cards under it. Both scroll down.
            g.labelH = 0; g.headH = 0;
            if (c.h > c.w)
            {
                g.panelX = (int)(12 * u); g.panelW = c.w - 2 * g.panelX; g.gap = (int)(12 * u);
                g.top = (int)((GridOnly() ? 112 : 356) * u); g.bottom = c.h - (int)(12 * u) - (int)(62 * u) - (int)(6 * u);
                g.cols = 2; g.cw = (g.panelW - g.gap) / 2; g.tile = g.cw - (int)(16 * u); g.ch = g.tile + (int)(38 * u);
                g.gx = g.panelX; g.colStep = g.cw + g.gap; g.rowStep = g.ch + (int)(14 * u);
            }
            else
            {
                g.panelX = (int)(74 * u); g.panelW = c.w - g.panelX - (int)(14 * u); g.gap = (int)(14 * u);
                // (Rows that scroll down, as on every other screen: they went along in one row.)
                g.top = (int)(196 * u); g.tile = (int)(150 * u); g.cw = g.tile + (int)(16 * u); g.ch = g.tile + (int)(38 * u); g.bottom = c.h - (int)(10 * u);
                if (GridOnly()) g.top = (int)(66 * u);
                g.cols = std::max(1, (g.panelW + g.gap) / (g.cw + g.gap)); g.gx = g.panelX; g.colStep = g.cw + g.gap; g.rowStep = g.ch + g.gap;
                g.horiz = false;
            }
            return g;
        }
        if (PanelHome())
        {
            // The Panel home: one column of plain rows on the left of its panel, the preview on the right.
            const PanelBox pb = PanelGeometry(c);
            g.top = pb.contentY; g.bottom = pb.contentB;
            g.gap = (int)(4 * u);
            g.cw = pb.listW; g.ch = (int)(54 * u); g.tile = (int)(40 * u);
            g.panelX = pb.x; g.panelW = pb.w;
            g.cols = 1; g.gx = pb.listX; g.colStep = g.cw; g.rowStep = g.ch + g.gap;
            return g;
        }
        if (TilesOn())
        {
            // The Tiles home: one column, one cover in view (the chosen one, in the middle); the arrows step along it.
            const TilesBox tb = TilesGeometry(c);
            g.top = tb.coverY; g.ch = tb.cover; g.bottom = g.top + g.ch; g.cw = tb.cover; g.tile = tb.cover; g.gap = (int)(20 * u);
            g.panelX = 0; g.panelW = c.w; g.cols = 1; g.gx = (c.w - g.cw) / 2; g.colStep = g.cw; g.rowStep = g.ch + g.gap;
            return g;
        }
        // (Box art is square: square pictures, two rows of cards in view, as many across as fit,
        // the room left over shared out between them.)
        const int minGap = (int)(14 * u);
        g.ch = (g.bottom - g.top - minGap) / 2;
        // Keep card sizes when hiding the banner; spend the reclaimed space on more rows.
        if (GridOnly()) g.top = (int)(126 * u);
        g.tile = g.ch - (int)(38 * u); // the picture
        g.cw = g.tile + (int)(16 * u);
        g.cols = std::max(2, std::min(8, (g.panelW + minGap) / std::max(1, g.cw + minGap)));
        g.gap = g.cols > 1 ? (g.panelW - g.cols * g.cw) / (g.cols - 1) : minGap;
        g.gx = g.panelX; g.colStep = g.cw + g.gap; g.rowStep = g.ch + minGap;
        return g;
    }
    g.gap = 8 * g.s;
    g.labelH = (UiLang::Tall() ? 32 : 20) * g.s;
    g.tabsY = (c.h > c.w ? 46 : 25) * g.s;
    g.tabsH = 13 * g.s;
    g.headH = 13 * g.s;
    g.top = g.tabsY + g.tabsH + 5 * g.s;
    g.bottom = c.h - 16 * g.s;
    // Tiles at most Art x Art at 720 lines, small enough for two rows and a heading on a screen.
    g.tile = std::max(32, std::min(Art * c.h / 720, (g.bottom - g.top - g.headH) / 2 - g.labelH - g.gap));
    if (c.h > c.w) g.tile = std::max(32, (c.w - 7 * g.gap) / 2);
    g.cols = std::max(1, std::min(4, (c.w - g.gap) / (g.tile + g.gap * 3)));
    const int gridW = g.cols * g.tile + (g.cols - 1) * g.gap * 3;
    g.gx = (c.w - gridW) / 2;
    g.cw = g.tile; g.ch = g.tile + g.labelH; g.colStep = g.tile + g.gap * 3; g.rowStep = g.rowH();
    return g;
}

// What one tab shows: each game's tile and each console's heading (on ALL) or the line saying it
// has none yet, in content coordinates.
struct View
{
    struct Spot { int game, x, y, section; };
    struct Head { int platform, y; bool empty; };
    Grid g;
    int tab = 0;
    std::vector<Spot> spots;
    std::vector<Head> heads;
    int height = 0;
    // The spot of game i, or -1.
    int SpotOf(int game) const
    {
        for (size_t k = 0; k < spots.size(); k++)
            if (spots[k].game == game) return (int)k;
        return -1;
    }
};
inline View MakeView(const Canvas& c, const std::vector<Game>& games, int tab)
{
    View v;
    v.g = Layout(c);
    v.tab = tab;
    const Grid& g = v.g;
    if (Glassy(c))
    {
        // (One grid of cards, the consoles one after another, without headings.)
        std::vector<int> mine;
        for (int p = 0; p < PLATFORMS; p++)
        {
            for (size_t i = 0; i < games.size(); i++)
                if (games[i].platform == p && InTab(games, (int)i, tab)) mine.push_back((int)i);
        }
        if (mine.empty()) { v.heads.push_back({ 0, 0, true }); return v; }
        if (g.horiz)
        {
            for (size_t k = 0; k < mine.size(); k++) v.spots.push_back({ mine[k], g.gx + (int)k * g.colStep, 0, games[mine[k]].platform });
            v.height = (int)mine.size() * g.colStep;
            return v;
        }
        for (size_t k = 0; k < mine.size(); k++)
            v.spots.push_back({ mine[k], g.gx + (int)(k % g.cols) * g.colStep, (int)(k / g.cols) * g.rowStep, games[mine[k]].platform });
        v.height = (int)((mine.size() + g.cols - 1) / g.cols) * g.rowStep;
        return v;
    }
    const int emptyH = 16 * g.s;
    int y = 0;
    for (int p = 0; p < PLATFORMS; p++)
    {
        if (tab >= TAB_DS && tab <= TAB_3DS && p != tab - TAB_DS) continue;
        std::vector<int> mine;
        for (size_t i = 0; i < games.size(); i++)
            if (games[i].platform == p && InTab(games, (int)i, tab)) mine.push_back((int)i);
        // (HOME and ROMHACKS: only the consoles they have games of, without headings.)
        if ((tab == TAB_HOME || tab == TAB_HACKS) && mine.empty()) continue;
        // (ALL: every console under its heading, also one without games yet.)
        if (tab == TAB_ALL)
        {
            v.heads.push_back({ p, y, false });
            y += g.headH;
        }
        if (mine.empty())
        {
            v.heads.push_back({ p, y, true });
            y += emptyH + g.gap;
            continue;
        }
        for (size_t k = 0; k < mine.size(); k++)
            v.spots.push_back({ mine[k], g.gx + (int)(k % g.cols) * g.colStep, y + (int)(k / g.cols) * g.rowStep, p });
        y += (int)((mine.size() + g.cols - 1) / g.cols) * g.rowStep + g.gap;
    }
    if (v.spots.empty() && v.heads.empty()) v.heads.push_back({ 0, 0, true });
    v.height = y;
    return v;
}

// Keeps the chosen game in view (with its console's heading when it is in the first row under
// it): scroll is how far down the content the screen is. follow false: only kept within the
// content (a finger scrolling the list).
inline void FollowSelection(const View& v, int selected, int& scroll, bool follow = true)
{
    const Grid& g = v.g;
    const int viewH = g.horiz ? g.panelW : g.bottom - g.top;
    const int k = follow ? v.SpotOf(selected) : -1;
    if (k >= 0)
    {
        int top = g.horiz ? v.spots[k].x - g.gx : v.spots[k].y;
        for (const View::Head& h : v.heads)
            if (!h.empty && h.platform == v.spots[k].section && top - h.y <= g.headH) top = h.y;
        const int bottom = g.horiz ? v.spots[k].x - g.gx + g.cw : v.spots[k].y + g.ch;
        if (top < scroll) scroll = top;
        if (bottom > scroll + viewH) scroll = bottom - viewH;
    }
    scroll = std::max(0, std::min(scroll, std::max(0, v.height - viewH)));
}

// The game whose tile (with its name below it) is at x, y on the canvas, or -1.
inline int TileAt(const View& v, int scroll, int x, int y)
{
    const Grid& g = v.g;
    if (y < g.top || y >= g.bottom) return -1;
    if (g.horiz && x < g.panelX) return -1;
    for (const View::Spot& sp : v.spots)
    {
        const int tx = g.horiz ? sp.x - scroll : sp.x, ty = g.horiz ? g.top + sp.y : g.top + sp.y - scroll;
        if (x >= tx && x < tx + g.cw && y >= ty && y < ty + g.ch) return sp.game;
    }
    return -1;
}

// Where the tabs are drawn (TabsDraw) and which one is at x, y, or -1.
inline int TabX(const Canvas& c, const Grid& g, int t)
{
    const int step = 28 * 6 * g.s / 3; // (each tab's room, at the font's scale)
    return c.w / 2 + (t * 2 - (TABS - 1)) * step / 2;
}
inline int TabAt(const Canvas& c, int x, int y)
{
    if (Glassy(c)) return GlassTabAt(c, x, y);
    const Grid g = Layout(c);
    if (y < g.tabsY - 3 * g.s || y >= g.tabsY + g.tabsH + 3 * g.s) return -1;
    const int step = 28 * 6 * g.s / 3;
    for (int t = 0; t < TABS; t++)
        if (std::abs(x - TabX(c, g, t)) < step / 2) return t;
    return -1;
}

// The next game from `selected` in direction dir (1 left, 2 right, 3 up, 4 down): left and right
// go through the tab's games in order, up and down to the nearest tile in the row above or below
// (across the headings). -1 when there is nothing that way (up from the first row: the caller's
// title band; down from the last: stay).
inline int Step(const View& v, int selected, int dir)
{
    const int k = v.SpotOf(selected);
    if (k < 0) return v.spots.empty() ? -1 : v.spots[0].game;
    if (dir == 1) return k > 0 ? v.spots[k - 1].game : selected;
    if (dir == 2) return k + 1 < (int)v.spots.size() ? v.spots[k + 1].game : selected;
    // The row above or below: the nearest y beyond this one, then the nearest x in it.
    const int y = v.spots[k].y, x = v.spots[k].x;
    int rowY = -1;
    for (const View::Spot& sp : v.spots)
        if ((dir == 3 ? sp.y < y : sp.y > y) && (rowY < 0 || (dir == 3 ? sp.y > rowY : sp.y < rowY))) rowY = sp.y;
    if (rowY < 0) return dir == 3 ? -1 : selected;
    int best = -1, bestD = 0;
    for (const View::Spot& sp : v.spots)
        if (sp.y == rowY && (best < 0 || std::abs(sp.x - x) < bestD))
        {
            best = sp.game;
            bestD = std::abs(sp.x - x);
        }
    return best;
}

// A tab's own first game (when the one chosen is not in it), or -1 when it has none.
inline int FirstIn(const View& v) { return v.spots.empty() ? -1 : v.spots[0].game; }

// ---- The glass launcher (the new design: "Option A", the pause menu's frosted glass) ----------
// A sidebar of sections on the left, the tabs as pills along the top, a Continue Playing banner
// and the tab's games as cards below it, all drawn with the pause menu's own glass (glass.h,
// Menu::FrostBox) and clean font over the chosen game's picture, blurred. Used on landscape
// screens with the clean font (Glassy); otherwise the classic list (DrawClassic).
enum Side { SIDE_HOME, SIDE_HUB, SIDE_ROMS, SIDE_SETTINGS, SIDE_LAUNCHER, SIDE_ABOUT, SIDES }; // (Library is Home: its tabs, Sort)
inline const char* SideName(int i)
{
    static const char* n[] = { "Home", "Hub", "ROMs", "Settings", "Launcher", "About" };
    return n[i];
}
inline int SideIcon(int i)
{
    static const int ic[] = { Glass::I_HOME, Glass::I_PEOPLE, Glass::I_CHIP, Glass::I_GEAR, Glass::I_LAYOUT, Glass::I_INFO };
    return ic[i];
}
inline int GPx(float px) { return std::max(6, (int)(px + 0.5f)); }
// (Each in the player's language: LauncherText.)
inline int GW(const std::string& s, float px, bool bold) { return CleanFont::Width(LauncherText::Tr(s), GPx(px), bold); }
// Text with its capitals centred on cy.
inline int& TextLow() { static int v = 0; return v; } // (the lowest line of writing drawn since it was set to 0: how far a panel can scroll)
inline void GT(Canvas& c, int x, float cy, const std::string& s, float px, uint32_t col, bool bold)
{
    TextLow() = std::max(TextLow(), (int)(cy + px));
    CleanFont::Draw(c, x, (int)(cy + CleanFont::CapHeight(GPx(px), bold) * 0.5f), LauncherText::Tr(s), GPx(px), col, bold);
}
// Text kept within w: a little smaller first, then cut with "..." (the longer languages' lines).
// Coming Soon over a picture, centred on cx, cy: with a day, the release date over the countdown.
inline void DrawComingSoon(Canvas& c, const Game& g, int cx, float cy, float px, uint32_t col)
{
    const std::string count = ComingSoonText(g), date = ComingSoonDate(g);
    if (date.empty()) { GT(c, cx - GW(count, px, true) / 2, cy, count, px, col, true); return; }
    const float small = px * 0.8f;
    GT(c, cx - GW(date, small, true) / 2, cy - px * 0.75f, date, small, col, true);
    GT(c, cx - GW(count, px, true) / 2, cy + px * 0.6f, count, px, col, true);
}
inline void GTFit(Canvas& c, int x, float cy, const std::string& s0, float px, int w, uint32_t col, bool bold)
{
    std::string s = LauncherText::Tr(s0);
    float p = px;
    while (p > px * 0.82f && CleanFont::Width(s, GPx(p), bold) > w) p -= 0.5f;
    if (CleanFont::Width(s, GPx(p), bold) > w)
    {
        while (s.size() > 4 && CleanFont::Width(s + "...", GPx(p), bold) > w)
        {
            while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
            if (!s.empty()) s.pop_back();
        }
        s += "...";
    }
    CleanFont::Draw(c, x, (int)(cy + CleanFont::CapHeight(GPx(p), bold) * 0.5f), s, GPx(p), col, bold);
}
// A game's name as the cards show it: HeartGold, Black 2, Ultra Violet.
inline std::string ShortName(const Game& g)
{
    if (!g.customName.empty()) return g.customName;
    std::string n = g.name;
    const KnownGame* k = g.hack ? nullptr : Known(g.code); // (a hack by its own name: Renegade Platinum, not Platinum)
    if (k) n = k->name;
    else n = TitleCase(n);
    for (const char* p : { "Pokemon ", "POKEMON ", "Pokémon " })
        if (n.rfind(p, 0) == 0) { n = n.substr(strlen(p)); break; }
    return n;
}
// Where the sidebar's sections and the tab pills are (Draw and the taps use the same).
struct GlassSpots
{
    int sideX, sideY, sideW, sideH, rowH, rowGap, firstRow;
    int tabsX, tabsY, tabsH;
    float tabFont = 18;
    std::vector<int> tabX, tabW;
    int bannerX, bannerY, bannerW, bannerH;
    int contX, contY, contW, contH; // the Continue button
    int headY;
    int secY = 0, secH = 0;
    std::vector<int> secX, secW; // (the Panel home's sections along the top)
    std::vector<int> secYv; // (the Tiles home's tiles, each its own top: two rows upright)
    int navX = 0, navY = 0, navW = 0, navH = 0; // (a phone held upright: the bar of sections along the foot)
    int winX = 0, winY = 0, winW = 0, winH = 0, pageTop = 0, pageTitleY = 0, pageBottom = 0; // (the Tiles home's pages: a window over the home)
};
inline GlassSpots Spots(const Canvas& c)
{
    Orient(c);
    const float u = GU(c);
    GlassSpots s;
    // (The sidebar flush with the screen's left, top and bottom edges; its rows start level with the tabs.)
    s.sideX = 0; s.sideY = 0; s.sideW = (int)(180 * u); s.sideH = c.h;
    // (text sizes: bigger since 6 November, the sidebar and tabs most)
    s.rowH = (int)(54 * u); s.rowGap = (int)(6 * u); s.firstRow = (int)(12 * u); // (level with the tab pills)
    s.tabsX = (int)(204 * u); s.tabsY = (int)(14 * u); s.tabsH = (int)(46 * u);
    const bool pnl = PanelHome();
    const PanelBox pb = PanelGeometry(c);
    if (pnl)
    {
        // The Panel home: the sections first along the top of its panel, the tabs after them on the same row, the tools at its right.
        s.secY = pb.secY; s.secH = pb.secH;
        s.secX.assign(SIDES, 0); s.secW.assign(SIDES, 0);
        int sx = pb.x + pb.pad + (int)(6 * u);
        for (int i = 0; i < SIDES; i++)
        {
            const int w = (int)(40 * u) + GW(i == SIDE_HOME ? "Games" : SideName(i), 14 * u, true);
            s.secX[i] = sx; s.secW[i] = w;
            sx += w + (int)(2 * u);
        }
        s.tabsX = sx + (int)(18 * u); s.tabsY = pb.tabY; s.tabsH = pb.tabH;
    }
    const bool tls = TilesOn();
    if (tls) { s.sideW = 0; s.tabsX = (int)(28 * u); } // (no sidebar: the tabs start at the left edge)
    if (tls && c.h > c.w) { s.tabsX = (int)(12 * u); s.tabsH = (int)(40 * u); }
    const bool phn = PhoneLook(), phPort = phn && c.h > c.w;
    if (phn)
    {
        // A phone: held upright no sidebar (a bar of sections along the foot); sideways a slim sidebar of icons.
        s.rowH = (int)(50 * u); s.rowGap = (int)(6 * u); s.firstRow = (int)(10 * u);
        if (phPort) { s.sideW = 0; s.tabsX = (int)(12 * u); s.tabsY = (int)(10 * u); s.tabsH = (int)(40 * u); }
        else { s.sideW = (int)(60 * u); s.tabsX = s.sideW + (int)(14 * u); s.tabsY = (int)(8 * u); s.tabsH = (int)(44 * u); }
    }
    // (The tabs smaller as they grow in number, to leave room for + after them and Search and Sort.)
    const int room = pnl ? pb.x + pb.w - pb.pad - (int)(280 * u) - s.tabsX : c.h > c.w ? c.w - (int)(12 * u) - s.tabsX : c.w - (int)(28 * u) - (int)((phn ? 300 : 470) * u) - s.tabsX;
    s.tabFont = 18 * u;
    float pad = 40 * u;
    auto total = [&]() { int w = 0; for (int t = 0; t < TabCount(); t++) w += GW(TabLabel(t), s.tabFont, true) + (int)pad + (int)(2 * u); return w; };
    while (total() > room && s.tabFont > (c.h > c.w ? 11 : 12) * u) { s.tabFont -= u; pad = std::max((c.h > c.w ? 8 : 20) * u, pad - 2 * u); }
    int x = s.tabsX + (int)(4 * u);
    s.tabX.assign(TabCount(), 0); s.tabW.assign(TabCount(), 0);
    for (const int t : TabOrder()) // (in the order shown; each kept at its own number)
    {
        const int w = GW(TabLabel(t), s.tabFont, true) + (int)pad;
        s.tabX[t] = x; s.tabW[t] = w;
        x += w + (int)(2 * u);
    }
    s.bannerX = s.tabsX; s.bannerY = (int)(90 * u); s.bannerW = c.w - s.bannerX - (int)(28 * u); s.bannerH = (int)(160 * u);
    s.contX = s.bannerX + (int)(168 * u); s.contY = s.bannerY + (int)(100 * u); s.contW = (int)(150 * u); s.contH = (int)(44 * u);
    s.headY = (int)(266 * u);
    if (GridOnly()) s.headY = (int)(88 * u);
    if (phn)
    {
        if (phPort)
        {
            s.bannerX = (int)(12 * u); s.bannerY = (int)(112 * u); s.bannerW = c.w - 2 * s.bannerX; s.bannerH = (int)(232 * u);
            s.contW = (int)(140 * u); s.contH = (int)(42 * u);
            s.contX = s.bannerX + s.bannerW - (int)(14 * u) - s.contW; s.contY = s.bannerY + s.bannerH - (int)(14 * u) - s.contH;
            s.navX = (int)(12 * u); s.navW = c.w - 2 * s.navX; s.navH = (int)(62 * u); s.navY = c.h - (int)(12 * u) - s.navH;
            s.secY = s.navY; s.secH = s.navH;
            s.secX.assign(SIDES, 0); s.secW.assign(SIDES, 0); s.secYv.assign(SIDES, s.navY);
            const int tw = (s.navW - (int)(12 * u)) / SIDES;
            for (int i = 0; i < SIDES; i++) { s.secX[i] = s.navX + (int)(6 * u) + i * tw; s.secW[i] = tw; }
        }
        else
        {
            s.bannerX = s.tabsX; s.bannerY = (int)(66 * u); s.bannerW = c.w - s.bannerX - (int)(14 * u); s.bannerH = (int)(120 * u);
            s.contW = (int)(150 * u); s.contH = (int)(44 * u);
            s.contX = s.bannerX + s.bannerW - (int)(16 * u) - s.contW; s.contY = s.bannerY + (s.bannerH - s.contH) / 2;
        }
    }
    if (pnl)
    {
        // The Panel home: the preview's buttons at the foot of its panel.
        s.contH = (int)(44 * u); s.contW = (int)(150 * u);
        s.contX = pb.prevX + (int)(20 * u); s.contY = pb.contentB - (int)(20 * u) - s.contH;
    }
    if (tls)
    {
        // The Tiles home: a tile for each section along the foot, the buttons centred under the chosen game.
        const TilesBox tb = TilesGeometry(c);
        s.secY = tb.tileY; s.secH = tb.tileH;
        s.secX.assign(SIDES, 0); s.secW.assign(SIDES, 0); s.secYv.assign(SIDES, tb.tileY);
        if (tb.rows > 1)
        {
            const int m = (int)(12 * u), gap = tb.gapT, tw = (c.w - 2 * m - (tb.cols - 1) * gap) / tb.cols;
            for (int i = 0; i < SIDES; i++) { s.secX[i] = m + (i % tb.cols) * (tw + gap); s.secW[i] = tw; s.secYv[i] = tb.tileY + (i / tb.cols) * (tb.tileH + gap); }
        }
        else
        {
            const int m = (int)(28 * u), gap = (int)(14 * u), tw = (c.w - 2 * m - (SIDES - 1) * gap) / SIDES;
            for (int i = 0; i < SIDES; i++) { s.secX[i] = m + i * (tw + gap); s.secW[i] = tw; }
        }
        s.contH = (int)(44 * u); s.contW = (int)(150 * u);
        const int ow = GW("Game Options", 18 * u, true) + (int)(36 * u);
        s.contX = (c.w - (s.contW + (int)(10 * u) + ow)) / 2; s.contY = tb.btnY;
        if (Focus().page >= 0)
        {
            // A page of the Tiles home: a window over the dimmed home, its title and rows inside it.
            s.winX = (int)(56 * u); s.winY = (int)(24 * u); s.winW = c.w - 2 * s.winX; s.winH = c.h - (int)(76 * u);
            s.bannerX = s.winX + (int)(24 * u); s.bannerW = s.winW - (int)(48 * u);
            s.tabsX = s.winX + (int)(30 * u);
            s.pageTitleY = s.winY + (int)(30 * u); s.pageTop = s.winY + (int)(66 * u); s.pageBottom = s.winY + s.winH - (int)(22 * u);
        }
    }
    return s;
}
inline bool Glassy(const Canvas& c);
inline int SideAt(const Canvas& c, int x, int y)
{
    if (!Glassy(c)) return -1;
    if (PanelHome() || TilesHome() || (PhoneLook() && c.h > c.w))
    {
        // (The Panel home: the sections along the top of its panel; the Tiles home: its tiles; a phone upright: its bar.)
        const GlassSpots ps = Spots(c);
        for (int i = 0; i < SIDES; i++)
        {
            const int ty = i < (int)ps.secYv.size() ? ps.secYv[i] : ps.secY;
            if (y >= ty && y < ty + ps.secH && x >= ps.secX[i] && x < ps.secX[i] + ps.secW[i]) return i;
        }
        return -1;
    }
    const GlassSpots s = Spots(c);
    if (x < s.sideX || x >= s.sideX + s.sideW) return -1;
    for (int i = 0; i < SIDES; i++)
    {
        const int ry = s.firstRow + i * (s.rowH + s.rowGap);
        if (y >= ry && y < ry + s.rowH) return i;
    }
    return -1;
}
// The banner's Game Options button under a point.
inline bool GameOptionsAt(const Canvas& c, int x, int y);
inline bool ContinueAt(const Canvas& c, int x, int y)
{
    if (!Glassy(c) || GridOnly()) return false;
    const GlassSpots s = Spots(c);
    return x >= s.contX && x < s.contX + s.contW && y >= s.contY && y < s.contY + s.contH;
}
inline bool GameOptionsAt(const Canvas& c, int x, int y)
{
    if (!Glassy(c) || PhoneLook() || GridOnly()) return false; // (a phone: the banner has Continue only)
    const GlassSpots s = Spots(c);
    const float u = GU(c);
    const int ox = s.contX + s.contW + (int)(10 * u), ow = GW("Game Options", 18 * u, true) + (int)(36 * u);
    return x >= ox && x < ox + ow && y >= s.contY && y < s.contY + s.contH;
}
// The game the banner offers: the one played last, if it is still here, else the first that can be played
// (not the chosen one: the banner would change, and be drawn again, with every press).
inline int ContinueGame(const std::vector<Game>& games, int selected)
{
    if (GridOnly()) return -1; // Hidden buttons must not receive controller focus either.
    // (The Panel home: its buttons are for the chosen game, if it can be played.)
    if (PanelHome() || TilesHome()) return selected >= 0 && selected < (int)games.size() && games[selected].known && !games[selected].comingSoon ? selected : -1;
    for (size_t i = 0; i < games.size(); i++)
        if (!ContinueRom().empty() && games[i].rom == ContinueRom()) return (int)i;
    for (size_t i = 0; i < games.size(); i++)
        if (games[i].known && !games[i].comingSoon) return (int)i;
    return selected;
}
// A game's picture (or its DS icon on glass) filling a rounded box, cropped to fill it.
inline void DrawCover(Canvas& c, const Game& g, int x, int y, int w, int h, float r)
{
    if (w <= 0 || h <= 0) return;
    Glass::Mark(c, x, y, x + w, y + h);
    const int ri = (int)r;
    auto inside = [&](int px, int py) {
        const int dx = std::max(0, std::max(ri - px, px - (w - 1 - ri))), dy = std::max(0, std::max(ri - py, py - (h - 1 - ri)));
        return dx * dx + dy * dy <= ri * ri;
    };
    if (!g.art.empty())
    {
        const float sc = std::max(w, h) / (float)Art;
        for (int py = 0; py < h; py++)
        {
            const int Y = y + py;
            if (Y < 0 || Y >= c.h) continue;
            const int sy = std::max(0, std::min(Art - 1, (int)((py - h * 0.5f) / sc + Art * 0.5f)));
            for (int px = 0; px < w; px++)
            {
                const int X = x + px;
                if (X < 0 || X >= c.w || !inside(px, py)) continue;
                const int sx = std::max(0, std::min(Art - 1, (int)((px - w * 0.5f) / sc + Art * 0.5f)));
                const uint32_t p = g.art[(size_t)sy * Art + sx];
                uint32_t& d = c.px[(size_t)Y * c.w + X];
                d = Glass::Over(d, p | 0xFF000000u, (int)(p >> 24));
            }
        }
        return;
    }
    Glass::Round(c, x, y, w, h, r, 0x26FFFFFF);
    if (!g.hasIcon && g.comingSoon) return; // (its card says COMING SOON over it)
    if (!g.hasIcon)
    {
        // (The plain card: the game's name in the middle.)
        const std::string n = ShortName(g);
        float px = h / 7.0f;
        while (px > 8 && GW(n, px, true) > w - h / 8) px -= 1;
        GT(c, x + (w - GW(n, px, true)) / 2, y + h * 0.5f, n, px, 0xE6FFFFFFu, true);
        return;
    }
    if (g.hasIcon)
    {
        const int is = std::max(1, std::min(w, h) / 40), ox = x + (w - 32 * is) / 2, oy = y + (h - 32 * is) / 2;
        for (int py = 0; py < 32; py++)
            for (int px = 0; px < 32; px++)
                if (g.icon[py * 32 + px] >> 24) c.Fill(ox + px * is, oy + py * is, is, is, g.icon[py * 32 + px]);
    }
}
// The Launcher page: the launcher's own look (fulldex-look.txt beside the saves): the accent, what is
// behind it, the cloud on the cards.
inline std::string& HubDir();
struct LookPrefs
{
    // 0 accent, 1 panels, 2 sidebar, 3 top bar, 4 buttons, 5 the background's own: a colour (a name or
    // #RRGGBB), a picture (a file in launcher-pictures; "" none), how solid in percent (-1: as the colour has it).
    std::string col[11] = { "white", "clear", "clear", "clear", "white", "#1E3650", "", "", "", "", "#F2F5F9" }; // (10: the text colour)
    std::string pic[11];
    int op[11] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    std::string backdrop = "game";               // game, custom (the background's own picture or colour), plain
    int blur = 20, dim = 42, grain = 25;         // percent
    std::string font = "noto-sans";              // (launcher_fonts.h)
    int fontSize = 100;                          // percent, 85 to 120
    bool badges = true;
    std::string music = "corner";                 // corner (sidebar foot), top (Launcher > Music Player)
    bool avoidCutout = true;                      // Android: the launcher's screens keep clear of the camera cutout (Launcher > Camera Cutout)
    bool cursorButton = false;                   // the selector outline in the button colour (Theme Editor > Buttons)
    std::string layout = "sidebar";              // sidebar, grid, tiles (panel is kept as a file value only) (Launcher > Layout)
    bool loaded = false;
    std::string from;
    int edit = -1, field = 1;                    // (open: 0 to 5 a colour's wheel, 6 the blur and dim; its row)
    float h = 0, s = 0, v = 1;
    int wx = 0, wy = 0, wr = 0;                  // the wheel on screen (taps)
    int fx[8] = {}, fy[8] = {}, fw = 0, fh = 0;  // its rows
    int px[9] = {}, py = 0, pd = 0, pn = 0;      // its presets
    int rowX = 0, rowW = 0;                      // its rows across
};
inline LookPrefs& Look() { static LookPrefs l; return l; }
struct LayoutPreviewHit { int x, y, w, h; std::string layout; };
inline std::vector<LayoutPreviewHit>& LayoutPreviews() { static std::vector<LayoutPreviewHit> hits; return hits; }
// The Launcher page's sub-pages: 0 its menu, 1 Theme Editor, 2 My Themes, 3 the Theme Library (its grid), 4 the fonts (a grid).
inline int& LauncherMode() { static int m = 0; return m; }
// My Themes: each a .zip in themes/ with the saves: theme.txt (as fulldex-look.txt, its pictures under
// pictures/), the pictures, and preview.jpg (Home wearing it).
struct ThemeState
{
    std::vector<std::string> list;
    bool pending = false;        // (saving: Home drawn once for its picture)
    std::string name, msg;
    GlassFocus kept;
    std::map<std::string, Glass::Picture> previews;
    int gx = 0, gy = 0, cw = 0, ch = 0, gap = 0, cols = 4; // (the grid on screen: taps)
    int perPage = 8, prevX = 0, nextX = 0, pageY = 0, pageW = 0, pageH = 0; // (its pages, and the < > under it)
};
inline ThemeState& Themes() { static ThemeState t; return t; }
// The Theme Gallery (Launcher > Theme Gallery): themes shared through the relay (tools/relay), in a grid
// like the Theme Library's. Its first two cards are Enter A Theme Code and Share A Theme. Fetching runs
// on a background thread; the screen only reads what has finished, under the mutex.
struct GalleryItem
{
    std::string code, name, author;
    int size = 0;
    std::vector<unsigned char> jpg; // (the preview as downloaded, until it is decoded)
    Glass::Picture pic;
    bool decoded = false;
};
struct GalleryState
{
    std::mutex m;
    std::deque<GalleryItem> items;
    std::string cursor, msg, ready; // (the next page's cursor; the line under the grid; a downloaded theme waiting to be tried)
    bool more = false, busy = false, working = false, loaded = false, tried = false;
    bool trying = false;            // (Try It: a gallery theme on, Keep or Go Back to answer)
    std::string tryName;
    LookPrefs before;
    std::set<std::string> picsBefore;
};
inline GalleryState& Gallery() { static GalleryState g; return g; }
inline int GalleryCards() { GalleryState& g = Gallery(); std::lock_guard<std::mutex> l(g.m); return 2 + (int)g.items.size(); }
inline void GalleryEnterCode(const std::string& code);
inline std::string ThemesDir() { return HubDir() + "/themes"; }
inline std::string ThemeSafe(const std::string& raw)
{
    std::string n;
    for (const char ch : raw) if (isalnum((unsigned char)ch) || ch == ' ' || ch == '-' || ch == '_' || (unsigned char)ch >= 128) n += ch;
    while (!n.empty() && n.back() == ' ') n.pop_back();
    while (!n.empty() && n.front() == ' ') n.erase(0, 1);
    if (n.size() > 40) n.resize(40);
    return n;
}
inline void ThemesRefresh()
{
    std::vector<std::string>& l = Themes().list;
    l.clear();
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(ThemesDir(), ec))
    {
        const std::string n = e.path().filename().string();
        if (e.is_regular_file(ec) && n.size() > 4 && (n.compare(n.size() - 4, 4, ".zip") == 0 || n.compare(n.size() - 4, 4, ".ZIP") == 0)) l.push_back(n.substr(0, n.size() - 4));
    }
    std::sort(l.begin(), l.end(), [](const std::string& a, const std::string& b) {
        std::string x = a, y = b;
        for (char& ch : x) ch = (char)tolower((unsigned char)ch);
        for (char& ch : y) ch = (char)tolower((unsigned char)ch);
        return x < y;
    });
}
#ifdef __ANDROID__
static const int kLauncherMenuRows = 6; // (Layout, Theme Editor, My Themes, Theme Gallery, Music Player, Camera Cutout)
#elif defined(__SWITCH__)
static const int kLauncherMenuRows = 4; // (No Music Player on a Switch: there is nothing to play from.)
#else
static const int kLauncherMenuRows = 5;
#endif
inline int LauncherRows() { return LauncherMode() == 5 ? GalleryCards() : LauncherMode() == 4 ? kLauncherFontCount : LauncherMode() == 1 ? 11 : LauncherMode() == 3 ? std::max(1, (int)Themes().list.size()) : LauncherMode() == 2 ? 4 : kLauncherMenuRows; }
// A on a theme: Apply, Show In Folder (a PC), Delete.
inline void OpenThemeMenu(int i)
{
    ThemeState& ts = Themes();
    if (i < 0 || i >= (int)ts.list.size()) return;
    GlassFocus& F = Focus();
    const std::string n = ts.list[(size_t)i];
    F.dialog = 15;
    F.drow = 0;
    F.dTitle = n;
    F.dMsg = "A theme of your own.";
#ifdef __SWITCH__
    F.dItems = { "Apply", "Delete", "Close" };
    F.dPaths = { "#apply:" + n, "#tdelask:" + n, "#close" };
#else
    F.dItems = { "Apply", "Show In Folder", "Delete", "Close" };
    F.dPaths = { "#apply:" + n, "#folder", "#tdelask:" + n, "#close" };
#endif
}
static const char* const kLookAccents[] = { "white", "red", "blue", "green", "purple", "gold", "pink", "teal" };
static const char* const kLookPanels[] = { "clear", "dark", "red", "blue", "green", "purple", "gold", "pink", "teal" };
inline uint32_t Hsv(float h, float s, float v)
{
    h = std::fmod(std::fmod(h, 360.0f) + 360.0f, 360.0f) / 60.0f;
    const int i = (int)h;
    const float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    float r, g, b;
    switch (i) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
                 case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
    return 0xFF000000u | (uint32_t)(r * 255 + 0.5f) << 16 | (uint32_t)(g * 255 + 0.5f) << 8 | (uint32_t)(b * 255 + 0.5f);
}
inline void ToHsv(uint32_t c, float& h, float& s, float& v)
{
    const float r = (c >> 16 & 255) / 255.0f, g = (c >> 8 & 255) / 255.0f, b = (c & 255) / 255.0f;
    const float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b)), d = mx - mn;
    v = mx;
    s = mx > 0 ? d / mx : 0;
    if (d <= 0) h = 0;
    else if (mx == r) h = 60 * std::fmod((g - b) / d + 6, 6.0f);
    else if (mx == g) h = 60 * ((b - r) / d + 2);
    else h = 60 * ((r - g) / d + 4);
}
// A colour as kept: a name, or #RRGGBB (from the wheel).
inline uint32_t SpecRgb(const std::string& sp, uint32_t fallback)
{
    if (sp.size() == 7 && sp[0] == '#') return 0xFF000000u | (uint32_t)strtoul(sp.c_str() + 1, nullptr, 16);
    if (sp == "white") return 0xFFFFFFFFu;
    if (sp == "dark") return 0xFF080C12u;
    if (sp == "clear") return 0xFF101826u;
    for (const char* n : { "red", "blue", "green", "purple", "gold", "pink", "teal" }) if (sp == n) return AccentNamed(sp);
    return fallback;
}
inline bool LookHasOp(int e) { return e >= 1 && e <= 4; }
inline uint32_t LookRgb(int e) { return SpecRgb(Look().col[e], e == 5 ? 0xFF1E3650u : e >= 1 && e <= 3 ? 0xFF101826u : e == 10 ? 0xFFF2F5F9u : 0xFFFFFFFFu); }
inline int LookOpacity(int e)
{
    const LookPrefs& l = Look();
    if (l.op[e] >= 0) return l.op[e];
    if (e == 4 || !l.pic[e].empty()) return e == 4 ? 100 : 90;
    return l.col[e] == "clear" || l.col[e].empty() ? 19 : l.col[e] == "dark" ? 54 : 29;
}
inline uint32_t LookTint(int e) { return (uint32_t)(LookOpacity(e) * 255 / 100) << 24 | (LookRgb(e) & 0xFFFFFFu); }
inline std::string LookPicPath(const std::string& name)
{
    if (name.empty()) return std::string();
    if (name.find(':') != std::string::npos || name[0] == '/') return name; // (a whole path: the tests')
    return HubDir() + "/launcher-pictures/" + name;
}
// A picture read for the launcher, at most 1920 on a side; its average colour too.
inline bool LookDecodePicture(const std::vector<unsigned char>& data, Glass::Picture& b);
inline bool LookLoadPicture(const std::string& path, Glass::Picture& b)
{
    b = Glass::Picture();
    std::vector<unsigned char> data;
    if (path.empty() || !ReadFile(path, data)) return false;
    return LookDecodePicture(data, b);
}
inline bool LookDecodePicture(const std::vector<unsigned char>& data, Glass::Picture& b)
{
    b = Glass::Picture();
    if (data.empty() || !ImageAllowed(data.data(), data.size())) return false;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &n, 4);
    if (!px) return false;
#if defined(__SWITCH__) || defined(__ANDROID__)
    const int most = 1920; // (the Switch's memory; a phone's screen)
#else
    const int most = 3840; // (4K)
#endif
    const int step = std::max(1, (std::max(w, h) + most - 1) / most);
    b.w = std::max(1, w / step);
    b.h = std::max(1, h / step);
    b.px.resize((size_t)b.w * b.h);
    uint64_t sr = 0, sg = 0, sb = 0;
    for (int y = 0; y < b.h; y++)
        for (int x = 0; x < b.w; x++)
        {
            const unsigned char* q = px + ((size_t)y * step * w + (size_t)x * step) * 4;
            b.px[(size_t)y * b.w + x] = 0xFF000000u | q[0] << 16 | q[1] << 8 | q[2];
            sr += q[0]; sg += q[1]; sb += q[2];
        }
    stbi_image_free(px);
    const uint64_t cnt = std::max<uint64_t>(1, b.px.size());
    b.avg = 0xFF000000u | (uint32_t)(sr / cnt) << 16 | (uint32_t)(sg / cnt) << 8 | (uint32_t)(sb / cnt);
    return true;
}
// The launcher's font as CleanFont numbers it (its place in launcher_fonts.h; 0 Noto Sans).
inline int LookFontId() { for (int i = 0; i < kLauncherFontCount; i++) if (Look().font == kLauncherFonts[i].id) return i; return 0; }
inline void LookApply()
{
    LookPrefs& l = Look();
    CleanFont::FontSource() = LauncherFontSource;
    Glass::Accent() = LookRgb(0);
    Glass::Grain() = l.grain;
    for (int i = 0; i < 3; i++) Glass::Tint(i) = LookTint(i + 1);
    // (The pictures read again only when changed.)
    static std::string loaded[4];
    for (int i = 0; i < 4; i++)
    {
        const std::string want = LookPicPath(l.pic[i + 1]);
        if (want != loaded[i]) { loaded[i] = want; if (!LookLoadPicture(want, Glass::SurfacePic(i))) Glass::SurfacePic(i) = Glass::Picture(); }
        Glass::SurfaceAlpha(i) = LookOpacity(i + 1) * 255 / 100;
    }
    const uint32_t btn = Glass::SurfacePic(3).px.empty() ? LookRgb(4) : Glass::SurfacePic(3).avg;
    Glass::Button() = (uint32_t)(LookOpacity(4) * 255 / 100) << 24 | (btn & 0xFFFFFFu);
    if (l.cursorButton) Glass::Accent() = 0xFF000000u | (btn & 0xFFFFFFu); // (the selector in the buttons' colour)
    // The writing: its colour, the hint and the dimmed in proportion (the usual ones while it is the usual colour).
    const uint32_t tx = 0xFF000000u | (LookRgb(10) & 0xFFFFFFu);
    auto scaled = [&](float f) { return 0xFF000000u | (uint32_t)((tx >> 16 & 255) * f) << 16 | (uint32_t)((tx >> 8 & 255) * f) << 8 | (uint32_t)((tx & 255) * f); };
    Menu::GlassText = tx;
    Menu::GlassHint = tx == 0xFFF2F5F9u ? 0xFFB9C3CFu : scaled(0.78f);
    Menu::GlassOff = tx == 0xFFF2F5F9u ? 0xFF7E8894u : scaled(0.5f);
}
// What size of picture suits each surface that can have one. A picture covers each panel, bar or button it is
// on (cropped to fit), so it is the shape that matters most; one over 4K (1080p on the Switch) is made smaller.
inline std::string BestPicture(int e)
{
#ifdef __SWITCH__
    const int m = 1920;
#else
    const int m = 3840;
#endif
    auto n = [&](int v) { return std::to_string(v * m / 3840); };
    switch (e)
    {
    case 1: return "Best picture: " + n(1920) + " x " + n(1080) + " (16:9). It is cropped to fit each panel.";
    case 2: return "Best picture: " + n(540) + " x " + n(2160) + ", tall and narrow. It is cropped to fit the sidebar.";
    case 3: return "Best picture: " + n(3840) + " x " + n(220) + ", a very wide strip. It is cropped to fit the top bar.";
    case 4: return "Best picture: " + n(600) + " x " + n(170) + ", wide and small. It is cropped to fit each button.";
    case 5: return "Best picture: " + n(3840) + " x " + n(2160) + " (16:9). It covers the whole screen, so a wider or taller one is cropped.";
    }
    return std::string();
}
static const char* const kLookKeys[6] = { "accent", "panel", "sidebar", "topbar", "buttons", "bgcolour" };
inline void LookFrom(const std::map<std::string, std::string>& p)
{
    LookPrefs& l = Look();
    const LookPrefs d;
    auto get = [&](const std::string& k, const std::string& def) { const auto it = p.find(k); return it == p.end() ? def : it->second; };
    for (int i = 0; i < 6; i++)
    {
        l.col[i] = get(kLookKeys[i], d.col[i]);
        l.pic[i] = i == 0 ? std::string() : get(std::string(kLookKeys[i]) + "_pic", "");
        l.op[i] = LookHasOp(i) ? std::max(-1, std::min(100, atoi(get(std::string(kLookKeys[i]) + "_op", "-1").c_str()))) : -1;
    }
    l.col[10] = get("text", d.col[10]);
    if (l.col[10].empty()) l.col[10] = d.col[10];
    l.backdrop = get("background", "game");
    if (l.backdrop == "custom") l.backdrop = l.pic[5].empty() ? "colour" : "picture"; // (as it was for a while)
    if (p.count("softness")) l.blur = std::max(0, std::min(100, atoi(p.at("softness").c_str())));
    else { static const int was[5] = { 0, 10, 20, 40, 70 }; l.blur = was[std::max(0, std::min(4, atoi(get("blur", "2").c_str())))]; } // (the five steps before)
    l.dim = std::max(0, std::min(100, atoi(get("dim", "42").c_str())));
    l.grain = std::max(0, std::min(100, atoi(get("grain", "25").c_str())));
    l.font = get("font", "noto-sans");
    l.fontSize = std::max(85, std::min(120, atoi(get("font_size", "100").c_str())));
    l.badges = get("badges", "on") != "off";
    { const std::string lv = get("layout", l.layout); l.layout = lv == "panel" || lv == "tiles" || lv == "grid" ? lv : "sidebar"; }
    if (l.layout == "grid") Focus().banner = 0;
    { const std::string mv = get("music", l.music); l.music = mv == "top" || mv == "compact" || mv == "compactb" || mv == "off" ? mv : "corner"; }
#ifdef __SWITCH__
    l.music = "off"; // (No music player on a Switch.)
#endif
    l.avoidCutout = get("cutout", "on") != "off";
    l.cursorButton = get("cursor_button", "off") == "on";
    LookApply();
}
inline void LookLoad()
{
    LookPrefs& l = Look();
    if (l.loaded && l.from == HubDir()) return;
    l.loaded = true;
    l.from = HubDir();
    LookFrom(ReadPairs(HubDir() + "/fulldex-look.txt"));
    if (const char* e = getenv("SSR_LIST_LAYOUT")) l.layout = !strcmp(e, "panel") ? "panel" : !strcmp(e, "tiles") ? "tiles" : !strcmp(e, "grid") ? "grid" : "sidebar"; // (test: shown, not kept)
    if (const char* e = getenv("SSR_LIST_MUSIC")) l.music = !strcmp(e, "top") || !strcmp(e, "compact") || !strcmp(e, "compactb") || !strcmp(e, "off") ? e : "corner"; // (test)
}
inline bool PanelOn() { LookLoad(); return Look().layout == "panel" && !PhoneLook(); }
inline bool GridOnly() { LookLoad(); return Look().layout == "grid"; }
inline bool PanelHome() { return PanelOn() && Focus().page < 0; }
inline bool TilesOn() { LookLoad(); return Look().layout == "tiles" && !PhoneLook(); }
inline bool TilesHome() { return TilesOn() && Focus().page < 0; }
inline void LookSave()
{
    const LookPrefs& l = Look();
    if (FILE* f = fopen((HubDir() + "/fulldex-look.txt").c_str(), "wb"))
    {
        for (int i = 0; i < 6; i++)
        {
            fprintf(f, "%s\t%s\n", kLookKeys[i], l.col[i].c_str());
            if (i > 0) fprintf(f, "%s_pic\t%s\n", kLookKeys[i], l.pic[i].c_str());
            if (LookHasOp(i)) fprintf(f, "%s_op\t%d\n", kLookKeys[i], l.op[i]);
        }
        fprintf(f, "text\t%s\n", l.col[10].c_str());
        fprintf(f, "background\t%s\nsoftness\t%d\ndim\t%d\ngrain\t%d\nbadges\t%s\nfont\t%s\nfont_size\t%d\nlayout\t%s\nmusic\t%s\n", l.backdrop.c_str(), l.blur, l.dim, l.grain, l.badges ? "on" : "off", l.font.c_str(), l.fontSize, l.layout.c_str(), l.music.c_str());
        fprintf(f, "cutout\t%s\n", l.avoidCutout ? "on" : "off");
        fprintf(f, "cursor_button\t%s\n", l.cursorButton ? "on" : "off");
        fclose(f);
    }
}
// The background: the chosen game's picture, blurred as the pause menu blurs the paused game
// (made again only when the game or the window changes); a calm blue without one.
inline void GlassBackdrop(Canvas& c, const Game* g)
{
    static std::string key;
    static int kw = 0, kh = 0;
    LookLoad();
    const LookPrefs& lp = Look();
    const bool custom = lp.backdrop == "colour", own = lp.backdrop == "picture" && !lp.pic[5].empty(), plain = lp.backdrop == "plain" || (lp.backdrop == "picture" && lp.pic[5].empty());
    const std::string k = !custom && !plain && g && !g->art.empty() ? g->rom + "#" + std::to_string(g->art[(size_t)Art * Art / 2 + Art / 2]) : std::string();
    const std::string kk = k + "|" + lp.backdrop + "|" + std::to_string(lp.blur) + "|" + std::to_string(lp.dim) + "|" + lp.col[5] + "|" + lp.pic[5] + "|" + HubDir();
    if (kk == key && kw == c.w && kh == c.h && c.backdrop.size() == (size_t)c.w * c.h) return;
    key = kk; kw = c.w; kh = c.h;
    // (A colour of the player's own: just that, as chosen.)
    if (custom && !own) { c.backdrop.assign((size_t)c.w * c.h, LookRgb(5)); return; }
    std::vector<uint32_t> src((size_t)c.w * c.h);
    // (Your picture: read once, again when it changes; then only blurred and dimmed as chosen.)
    static Glass::Picture pic;
    static std::string picName = "\x01";
    const std::string want = own ? LookPicPath(lp.pic[5]) : std::string();
    if (own && want != picName) { picName = want; LookLoadPicture(want, pic); }
    if (own && !pic.px.empty()) Glass::FitCover(pic, c.w, c.h, src.data());
    else if (!k.empty())
    {
        const float sc = std::max(c.w, c.h) / (float)Art;
        // (Scaled up smoothly: the picture is small, and with Background Blur off it shows as it is.)
        for (int y = 0; y < c.h; y++)
        {
            const float fy = std::max(0.0f, std::min((float)(Art - 1), (y - c.h * 0.5f) / sc + Art * 0.5f - 0.5f));
            const int y0 = (int)fy, y1 = std::min(Art - 1, y0 + 1);
            const float wy = fy - y0;
            for (int x = 0; x < c.w; x++)
            {
                if (Look().blur > 0)
                {
                    // (Blurred anyway: the nearest pixel, quicker, as the Switch makes this on every move.)
                    const int sx = std::max(0, std::min(Art - 1, (int)((x - c.w * 0.5f) / sc + Art * 0.5f)));
                    const uint32_t p = g->art[(size_t)std::max(0, std::min(Art - 1, (int)((y - c.h * 0.5f) / sc + Art * 0.5f))) * Art + sx];
                    src[(size_t)y * c.w + x] = Glass::Over(0xFF1C2A3Au, p | 0xFF000000u, (int)(p >> 24));
                    continue;
                }
                const float fx = std::max(0.0f, std::min((float)(Art - 1), (x - c.w * 0.5f) / sc + Art * 0.5f - 0.5f));
                const int x0 = (int)fx, x1 = std::min(Art - 1, x0 + 1);
                const float wx = fx - x0;
                const uint32_t q[4] = { g->art[(size_t)y0 * Art + x0], g->art[(size_t)y0 * Art + x1], g->art[(size_t)y1 * Art + x0], g->art[(size_t)y1 * Art + x1] };
                const float k[4] = { (1 - wx) * (1 - wy), wx * (1 - wy), (1 - wx) * wy, wx * wy };
                float ch[4] = {};
                for (int i = 0; i < 4; i++)
                    for (int j = 0; j < 4; j++) ch[j] += k[i] * (float)(q[i] >> (24 - 8 * j) & 255);
                const uint32_t p = (uint32_t)ch[1] << 16 | (uint32_t)ch[2] << 8 | (uint32_t)ch[3];
                src[(size_t)y * c.w + x] = Glass::Over(0xFF1C2A3Au, p | 0xFF000000u, (int)ch[0]);
            }
        }
    }
    else
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++)
            {
                const float t = (float)x / c.w * 0.6f + (float)y / c.h * 0.4f;
                const int r = (int)(0x1E + t * 0x18), gg = (int)(0x36 + t * 0x30), b = (int)(0x5A + t * 0x10);
                src[(size_t)y * c.w + x] = 0xFF000000u | (r << 16) | (gg << 8) | b;
            }
    Glass::MakeBackdrop(c.backdrop, src.data(), c.w, c.h, Look().blur, Look().dim);
}

// The ROMs page's list: every ROM found, those taken off the list too (hidden), with the player's names
// and each file's size; in name order. Made by the platform from its whole list (BuildRomList).
inline std::string& HubDir() { static std::string d; return d; } // (the Hub page's settings folder: BuildRomList sets it)
inline std::vector<Game>& RomList() { static std::vector<Game> r; return r; }
inline void BuildRomList(const std::vector<Game>& all, const std::string& savesDir)
{
    HubDir() = savesDir;
    LauncherText::OverrideDir() = savesDir; // (launcher-xx.txt there corrects the words)
    const auto names = ReadPairs(savesDir + "/fulldex-names.txt");
    const auto hidden = ReadPairs(savesDir + "/fulldex-hidden.txt");
    std::vector<Game>& r = RomList();
    r.clear();
    for (const Game& g : all)
    {
        if (g.comingSoon) continue;
        r.push_back(g);
        Game& x = r.back();
        const auto it = names.find(x.rom);
        if (it != names.end()) x.customName = it->second;
        x.hidden = hidden.count(x.rom) > 0;
        x.fileSize = 0;
        if (FILE* f = fopen(x.rom.c_str(), "rb"))
        {
            if (fseek(f, 0, SEEK_END) == 0) x.fileSize = (long long)ftell(f);
            fclose(f);
        }
    }
    std::stable_sort(r.begin(), r.end(), [](const Game& a, const Game& b) { return ShortName(a) < ShortName(b); });
}
// The ROM folder can be chosen even before the library contains a game.
inline bool CanChooseRomFolder()
{
#if defined(__SWITCH__) || defined(SSR_IOS)
    return false;
#else
    return true;
#endif
}
inline int RomActions()
{
    const int folder = CanChooseRomFolder() ? 1 : 0;
    if (RomList().empty()) return folder;
#if defined(__SWITCH__) || defined(__ANDROID__)
    return folder + 1;
#else
    return folder + 2;
#endif
}
// One line of the settings file set (key=value), the rest kept: written to a .tmp and put in place
// (on the Switch's SD card the old file goes first: it does not rename over one).
inline bool SetConfigLine(const std::string& cfg, const std::string& key, const std::string& value)
{
    std::vector<std::string> lines;
    if (FILE* f = fopen(cfg.c_str(), "r"))
    {
        char buf[1024];
        while (fgets(buf, sizeof buf, f))
        {
            std::string l = buf;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            lines.push_back(l);
        }
        fclose(f);
    }
    bool found = false;
    for (std::string& l : lines)
        if (l.size() > key.size() && !l.compare(0, key.size(), key) && l[key.size()] == '=') { l = key + "=" + value; found = true; }
    if (!found) lines.push_back(key + "=" + value);
    const std::string tmp = cfg + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) return false;
    bool ok = true;
    for (const std::string& l : lines) if (fprintf(f, "%s\n", l.c_str()) < 0) ok = false;
    ok = fclose(f) == 0 && ok;
    if (!ok) return false;
    if (rename(tmp.c_str(), cfg.c_str()) != 0) { remove(cfg.c_str()); if (rename(tmp.c_str(), cfg.c_str()) != 0) return false; }
    return true;
}
// Sort on the tab bar: the next order, and how it is named.
inline const char* NextSort(const std::string& now) { return now == "favourites" ? "recent" : now == "recent" ? "original" : "favourites"; }
inline const char* SortLabel(const std::string& k) { return k == "favourites" ? "Favourites First" : k == "recent" ? "Recently Played" : "Original"; }
// The tab bar's tools under a point (1 +, 2 Search, 3 Sort; 0 none).
inline int ToolAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (f.page >= 0 || y < f.toolY || y >= f.toolY + f.toolH) return 0;
    for (int k = 1; k < 4; k++)
        if (f.toolW[k] > 0 && x >= f.toolX[k] && x < f.toolX[k] + f.toolW[k]) return k;
    return 0;
}
// The keyboard's title, line and done key for what it is for (GlassFocus::kbFor).
inline const char* KbTitle() { const int k = Focus().kbFor; return k == 6 ? "THEME CODE" : k == 5 ? "THEME NAME" : k == 4 ? "TRADE CODE" : k == 1 ? "NEW TAB" : k == 2 ? "SEARCH" : k == 3 ? "RENAME TAB" : "RENAME"; }
inline const char* KbPrompt() { const int k = Focus().kbFor; return k == 6 ? "A Theme Code" : k == 5 ? "Your Theme's Name" : k == 4 ? "The Other Player's Code" : k == 1 || k == 3 ? "Tab Name" : k == 2 ? "Part Of A Game's Name" : "The Game's Own Name"; }
inline const char* KbDone() { const int k = Focus().kbFor; return k == 6 ? "Get It" : k == 4 ? "Join" : k == 1 ? "Make Tab" : k == 2 ? "Search" : "Save"; }
// Add To Tab (the game's menu): the player's tabs, each marked if the game is in it, New Tab and Close.
inline void OpenAddToTab(const std::string& rom)
{
    GlassFocus& F = Focus();
    const std::vector<CustomTab>& tabs = CustomTabs();
    F.dTitle = "Add To Tab";
    F.dMsg = tabs.empty() ? "You have no tabs of your own yet. Make one and this game goes in it."
                          : "Choose a tab to put this game in, or to take it out.";
    F.dItems.clear();
    F.dPaths.clear();
    for (size_t k = 0; k < tabs.size() && k < 6; k++)
    {
        bool in = false;
        for (const std::string& r : tabs[k].roms) in = in || r == rom;
        F.dItems.push_back(tabs[k].name + (in ? "   \xC2\xB7   In This Tab" : ""));
        F.dPaths.push_back("#tab:" + std::to_string(k));
    }
    F.dItems.push_back("New Tab");
    F.dPaths.push_back("#new");
    F.dItems.push_back("Close");
    F.dPaths.push_back("#close");
    F.dialog = 8;
    F.drow = 0;
    F.kbRom = rom;
}
// A on a tab: Move Left, Move Right, Hide Tab; a tab of the player's own also Rename and Delete.
inline void OpenTabMenu(int t)
{
    GlassFocus& F = Focus();
    if (t < 0 || t >= TabCount()) return;
    F.dTitle = TabLabel(t);
    F.menuTab = t;
    F.kbTab = t >= TABS ? t - TABS : -1;
    if (t >= TABS)
    {
        F.dMsg = "A tab of your own. Deleting it keeps its games.";
        F.dItems = { "Rename Tab", "Move Left", "Move Right", "Hide Tab", "Delete Tab", "Close" };
        F.dPaths = { "#rename", "#left", "#right", "#hide", "#delete", "#close" };
    }
    else
    {
        F.dMsg = "A built-in tab: it can be moved or hidden, never deleted. + shows a hidden one again.";
        F.dItems = { "Move Left", "Move Right", "Hide Tab", "Close" };
        F.dPaths = { "#left", "#right", "#hide", "#close" };
    }
    F.dialog = 9;
    F.drow = 0;
}
// + on the tab bar with tabs hidden: a new one, or one shown again (and the list of those hidden).
inline void OpenPlusMenu()
{
    GlassFocus& F = Focus();
    F.dialog = 13;
    F.drow = 0;
    F.dTitle = "Tabs";
    F.dMsg = "Make a new tab, or show one you hid.";
    F.dItems = { "New Tab", "Show A Hidden Tab", "Cancel" };
    F.dPaths = { "#newtab", "#unhidelist", "#close" };
}
inline void OpenUnhideTabs()
{
    GlassFocus& F = Focus();
    F.dialog = 14;
    F.drow = 0;
    F.dTitle = "Show A Hidden Tab";
    F.dMsg = "It goes back where it was.";
    F.dItems.clear();
    F.dPaths.clear();
    for (const int t : TabHidden()) { F.dItems.push_back(TabLabel(t)); F.dPaths.push_back("#unhide:" + std::to_string(t)); }
    F.dItems.push_back("Cancel");
    F.dPaths.push_back("#close");
}
// A row of those two chosen (pick: its path). tab: the tab shown, moved with its tab. Returns 1 when
// the dialog stays (made again), 2 when the keyboard opens (dialog 3, kbFor set), else 0 (closed).
// Sort on the tab bar: a menu of the orders (dialog 16); the chosen is on now.
inline void OpenSortMenu(const std::string& cur)
{
    GlassFocus& F = Focus();
    F.dialog = 16;
    F.dTitle = "Sort Games By";
    F.dMsg = "How the games are put in order, on every tab.";
    F.dItems = { "Original", "Favourites First", "Recently Played", "Close" };
    F.dPaths = { "#sort:", "#sort:favourites", "#sort:recent", "#close" };
    F.drow = cur == "favourites" ? 1 : cur == "recent" ? 2 : 0;
}
inline int TabDialogPick(const std::string& pick, int& tab, std::string& note)
{
    GlassFocus& F = Focus();
    std::vector<CustomTab>& tabs = CustomTabs();
    auto upper = [](std::string s) { for (char& ch : s) ch = (char)toupper((unsigned char)ch); return s; };
    if (pick.rfind("#tab:", 0) == 0)
    {
        const int k = atoi(pick.c_str() + 5);
        if (k < 0 || k >= (int)tabs.size()) { F.dialog = 0; return 0; }
        std::vector<std::string>& roms = tabs[k].roms;
        const auto it = std::find(roms.begin(), roms.end(), F.kbRom);
        const bool was = it != roms.end();
        if (was) roms.erase(it);
        else roms.push_back(F.kbRom);
        note = !SaveTabs() ? "COULD NOT SAVE THE TABS" : was ? "TAKEN OUT OF " + upper(tabs[k].name) : "ADDED TO " + upper(tabs[k].name);
        const int keep = F.drow;
        OpenAddToTab(F.kbRom);
        F.drow = keep;
        return 1;
    }
    if (pick == "#new") { F.kbFor = 1; F.text.clear(); F.dialog = 3; F.drow = 0; return 2; }
    if (pick == "#newtab") { F.kbRom.clear(); F.kbFor = 1; F.text.clear(); F.dialog = 3; F.drow = 0; return 2; }
    if (pick == "#unhidelist") { OpenUnhideTabs(); return 1; }
    if (pick.rfind("#unhide:", 0) == 0)
    {
        const int t = atoi(pick.c_str() + 8);
        std::vector<int>& h = TabHidden();
        h.erase(std::remove(h.begin(), h.end(), t), h.end());
        if (t >= 0 && t < TabCount()) tab = t;
        note = SaveTabOrder() ? "TAB SHOWN AGAIN" : "COULD NOT SAVE THE TABS";
        F.dialog = 0;
        return 0;
    }
    const int t = F.menuTab, k = t - TABS;
    if (t < 0 || t >= TabCount()) { F.dialog = 0; return 0; }
    if (pick == "#rename" && k >= 0) { F.kbTab = k; F.kbFor = 3; F.text = tabs[k].name; F.dialog = 3; F.drow = 0; return 2; }
    if (pick == "#left" || pick == "#right")
    {
        // (Past the next tab shown, any of them; the hidden ones keep their places.)
        const bool left = pick == "#left";
        const std::vector<int> ord = TabOrder();
        const int np = TabPos(t) + (left ? -1 : 1);
        if (np < 0 || np >= (int)ord.size()) { note = left ? "AS FAR LEFT AS IT GOES" : "AS FAR RIGHT AS IT GOES"; return 1; }
        std::vector<int>& all = TabOrderAll();
        const auto a = std::find(all.begin(), all.end(), t), b = std::find(all.begin(), all.end(), ord[(size_t)np]);
        if (a != all.end() && b != all.end()) std::iter_swap(a, b);
        if (!SaveTabOrder()) note = "COULD NOT SAVE THE TABS";
        const int keep = F.drow;
        OpenTabMenu(t);
        F.drow = keep;
        return 1;
    }
    if (pick == "#hide")
    {
        if (TabOrder().size() <= 1) { note = "ONE TAB MUST STAY SHOWN"; return 1; }
        const int next = TabStep(t, 1);
        TabHidden().push_back(t);
        if (tab == t) tab = next;
        note = SaveTabOrder() ? "TAB HIDDEN. + SHOWS IT AGAIN" : "COULD NOT SAVE THE TABS";
        F.dialog = 0;
        return 0;
    }
    if (pick == "#delete" && k >= 0 && k < (int)tabs.size())
    {
        // (Its place in the order gone; the player's own after it one number down.)
        auto renum = [t](std::vector<int>& v) { std::vector<int> o; for (const int x : v) if (x != t) o.push_back(x > t ? x - 1 : x); v = o; };
        renum(TabOrderAll());
        renum(TabHidden());
        tabs.erase(tabs.begin() + k);
        if (tab == t) tab = FirstTab();
        else if (tab > t) tab--;
        note = SaveTabs() ? "TAB DELETED. ITS GAMES ARE KEPT" : "COULD NOT SAVE THE TABS";
        F.dialog = 0;
        return 0;
    }
    F.dialog = 0;
    return 0;
}
// The keyboard's done key for anything but a game's name: a new tab (the game it came from put in it),
// the search ("" clears it), a tab's name. tab: the tab shown after.
inline void TradeEnterCode(const std::string& code); // (the Hub's Trade page, below)
inline void ThemeSaveStart(const std::string& name); // (My Themes, below)
inline void KeyboardDone(int& tab, std::string& note)
{
    GlassFocus& F = Focus();
    std::string t = F.text;
    while (!t.empty() && t.back() == ' ') t.pop_back();
    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    std::vector<CustomTab>& tabs = CustomTabs();
    if (F.kbFor == 1 && !t.empty())
    {
        CustomTab nt;
        nt.name = t;
        if (!F.kbRom.empty()) nt.roms.push_back(F.kbRom);
        tabs.push_back(nt);
        note = SaveTabs() ? "TAB MADE" : "COULD NOT SAVE THE TABS";
        tab = TabCount() - 1;
    }
    else if (F.kbFor == 2)
    {
        SearchText() = t;
        note = t.empty() ? "SEARCH CLEARED" : "";
    }
    else if (F.kbFor == 4) TradeEnterCode(t);
    else if (F.kbFor == 5) ThemeSaveStart(t);
    else if (F.kbFor == 6) GalleryEnterCode(t);
    else if (F.kbFor == 3 && !t.empty() && F.kbTab >= 0 && F.kbTab < (int)tabs.size())
    {
        tabs[F.kbTab].name = t;
        note = SaveTabs() ? "TAB RENAMED" : "COULD NOT SAVE THE TABS";
    }
    F.kbFor = 0;
    F.kbRom.clear();
    F.dialog = 0;
}
inline bool SortAt(int x, int y)
{
    const GlassFocus& f = Focus();
    return f.page < 0 && f.sortW > 0 && x >= f.sortX && x < f.sortX + f.sortW && y >= f.sortY && y < f.sortY + f.sortH;
}
inline const char* PlatformLabel();
// THE HUB PAGE: cloud saves (cloud_saves.h). Row 0 the account; then each game's save, with how it
// last synced. The network is used on a thread of its own, so the page keeps drawing; HubSig()
// changes whenever what the page shows does (the Switch redraws on that).
struct HubState
{
    std::mutex m;
    std::string dir;                        // where cloud.ini and cloud-state.txt are
    bool loaded = false;
    CloudSaves::Settings set;
    int busy = 0;                           // 0 nothing, 1 signing in, 2 syncing
    std::string code, url, msg;             // signing in: the code and where to type it; the last word
    std::map<std::string, CloudSaves::Result> results; // by the save's path
    std::map<std::string, std::string> synced;          // the saves in the cloud (cloud-state.txt): name, as last synced
    std::atomic<int> version{ 0 };
    std::atomic<bool> stop{ false }, cancel{ false };
    std::thread worker;
    ~HubState() { stop = true; if (worker.joinable()) worker.join(); }
};
inline HubState& Hub() { static HubState h; return h; }
struct HubGame { std::string name, path; const Game* g; };
// (A game's other folder as the old list wrote it, "IN BLACK2": just the folder.)
inline std::string WhereName(const std::string& w) { return w.rfind("IN ", 0) == 0 ? w.substr(3) : w; }
// (A platform whose saves are not where the list found the game: its own answer, e.g. the Switch's saves folder.)
inline std::function<std::string(const Game&)>& PlatformSavePath() { static std::function<std::string(const Game&)> f; return f; }
inline std::string HubSavePath(const Game& g)
{
    if (g.platform == GBA) return g.folder + "/" + g.saveName;
    if (PlatformSavePath()) return PlatformSavePath()(g);
#ifdef __SWITCH__
    return g.folder + "/" + g.saveName;
#else
    return HubDir() + "/" + g.saveName;
#endif
}
// (The saves this device last synced, from cloud-state.txt: name, a tab, what it was. Lock held.)
inline void HubReadState(HubState& h)
{
    h.synced.clear();
    if (FILE* f = fopen((h.dir + "/cloud-state.txt").c_str(), "rb"))
    {
        char line[1024];
        while (fgets(line, sizeof line, f))
        {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            const size_t t = l.find('\t');
            if (t != std::string::npos && t > 0) h.synced[l.substr(0, t)] = l.substr(t + 1);
        }
        fclose(f);
    }
}
// The games with a save of their own (the save's path: beside the ROM on the Switch, else the saves folder).
inline std::vector<HubGame> HubGames()
{
    std::vector<HubGame> r;
    for (const Game& g : RomList())
    {
        if (!g.known || g.saveName.empty()) continue;
        const std::string p = HubSavePath(g);
        bool dup = false;
        for (const HubGame& h : r) if (h.path == p) dup = true;
        if (!dup) r.push_back({ ShortName(g), p, &g });
    }
    return r;
}
inline void HubLoad()
{
    HubState& h = Hub();
    std::lock_guard<std::mutex> l(h.m);
    if (h.loaded && h.dir == HubDir()) return;
    h.dir = HubDir();
    h.loaded = true;
    h.set = CloudSaves::LoadSettings(h.dir + "/cloud.ini");
    HubReadState(h);
    h.version++;
}
inline std::string HubDevice()
{
    const std::string p = PlatformLabel();
    return p == "Windows" ? "Windows PC" : p == "Apple" ? "Mac" : p == "Nintendo Switch" ? p : p + " Device";
}
inline bool HubOnLocked(const HubState& h) { return h.set.provider != "off" && !(h.set.provider == "google" && h.set.googleRefreshToken.empty()); }
inline const char* HubProvider(const std::string& p) { return p == "google" ? "Google Drive" : p == "webdav" ? "WebDAV" : p == "folder" ? "A Synced Folder" : "Off"; }
inline void DexPoll();
inline std::string HubSig() { DexPoll(); return std::to_string(Hub().version.load()) + "." + std::to_string(HubLink::Version()); }
inline std::string SettingSentence(const std::string& s); // (below: capitals made a sentence)
// MOVE POKEMON (the Hub's second row): the Pokémon in a box of one of the player's games, and moving
// one to another of them (pokemon_hub.h: the rules, the conversion, both saves backed up first).
// Its rows: 0 the game they come from, 1 its box, 2 the game they go to, then that box's Pokémon.
const int HUB_FIRST = 1; // (Cloud Saves' first save row: after the account)
// (The Hub's page: 0 its first, Cloud Saves and Move Pokémon to open; 1 Cloud Saves. Move Pokémon: Mover().open.)
inline int& HubMode() { static int m = 0; return m; }
struct MoverGame { std::string name, path; bool hack; };
struct MoverState
{
    bool open = false;
    int from = 0, to = 1, box = 0;
    std::string msg;
#ifdef SSR_POKEMON_HUB
    std::unique_ptr<PokemonHub::Save> a, b;
    std::string aPath, bPath, aErr, bErr;
#endif
    // (Where a Pokémon goes in the other game: -1 the first free place, -2 the party, else a box.
    // Swapping: the Pokémon chosen in this game, while one is chosen in the other.)
    int put = -1;
    bool swapping = false;
    int swapBox = 0, swapSlot = 0;
    std::string swapName;
#ifdef SSR_POKEMON_HUB
#endif
};
inline MoverState& Mover() { static MoverState m; return m; }
inline std::vector<MoverGame> MoverGames()
{
    std::vector<MoverGame> r;
    for (const HubGame& g : HubGames()) if (g.g && g.g->hasSave) r.push_back({ g.name + (g.g->where.empty() ? std::string() : "  \xC2\xB7  " + WhereName(g.g->where)), g.path, IsHack(*g.g) });
    return r;
}
#ifdef SSR_POKEMON_HUB
// (The two saves open as chosen: opened again after a move, or when the choice changes.)
inline void MoverOpenSaves(bool again = false)
{
    MoverState& m = Mover();
    const std::vector<MoverGame> gs = MoverGames();
    if (gs.empty()) { m.a.reset(); m.b.reset(); m.aPath.clear(); m.bPath.clear(); return; }
    m.from = std::max(0, std::min(m.from, (int)gs.size() - 1));
    m.to = std::max(0, std::min(m.to, (int)gs.size() - 1));
    if (again || !m.a || m.aPath != gs[m.from].path) { m.aPath = gs[m.from].path; m.aErr.clear(); m.a = PokemonHub::Save::Open(m.aPath, m.aErr, gs[m.from].hack); }
    if (again || !m.b || m.bPath != gs[m.to].path) { m.bPath = gs[m.to].path; m.bErr.clear(); m.b = PokemonHub::Save::Open(m.bPath, m.bErr, gs[m.to].hack); }
    if (m.a && m.box != PokemonHub::PARTY) m.box = std::max(0, std::min(m.box, m.a->Boxes() - 1));
    if (m.b && m.put >= 0) m.put = std::min(m.put, m.b->Boxes() - 1);
}
#endif
#ifdef SSR_POKEMON_HUB
// The Pokémon's box icons, taken from one of the player's own DS ROMs as the companion takes them
// (companion_rom.h): a Black / White (2) ROM has the first five generations' 649, a Gen 4 one 493.
// Only the parts needed are read from the file, never all of it. Later Pokémon: none (a Poké Ball).
inline bool RomPart(FILE* f, uint32_t at, uint32_t n, std::vector<uint8_t>& out)
{
    out.resize(n);
    return n == 0 || (fseek(f, (long)at, SEEK_SET) == 0 && fread(out.data(), 1, n, f) == n);
}
inline std::vector<uint8_t> RomFileFrom(FILE* f, std::string path)
{
    std::vector<uint8_t> h, fnt, fat;
    auto U32 = [](const std::vector<uint8_t>& b, size_t o) { return (uint32_t)b[o] | (uint32_t)b[o + 1] << 8 | (uint32_t)b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; };
    auto U16 = [](const std::vector<uint8_t>& b, size_t o) { return (uint16_t)(b[o] | b[o + 1] << 8); };
    if (!RomPart(f, 0, 0x200, h)) return {};
    if (U32(h, 0x44) > (1u << 20) || U32(h, 0x4C) > (1u << 20)) return {};
    if (!RomPart(f, U32(h, 0x40), U32(h, 0x44), fnt) || !RomPart(f, U32(h, 0x48), U32(h, 0x4C), fat)) return {};
    uint16_t dir = 0xF000;
    for (int depth = 0; depth < 32; depth++)
    {
        const size_t entry = (size_t)(dir & 0xFFF) * 8, slash = path.find('/');
        if (entry + 8 > fnt.size()) return {};
        size_t pos = U32(fnt, entry);
        uint32_t id = U16(fnt, entry + 4);
        bool down = false;
        while (pos < fnt.size())
        {
            const int tag = fnt[pos++];
            if (!tag) break;
            const size_t n = tag & 127;
            if (pos + n > fnt.size()) return {};
            const std::string name((const char*)fnt.data() + pos, n);
            pos += n;
            if (tag & 128)
            {
                if (pos + 2 > fnt.size()) return {};
                const uint16_t sub = U16(fnt, pos);
                pos += 2;
                if (slash != std::string::npos && name == path.substr(0, slash)) { dir = sub; path = path.substr(slash + 1); down = true; break; }
            }
            else
            {
                if (slash == std::string::npos && name == path)
                {
                    if ((size_t)id * 8 + 8 > fat.size()) return {};
                    const uint32_t b = U32(fat, id * 8), e = U32(fat, id * 8 + 4);
                    std::vector<uint8_t> out;
                    if (e < b || e - b > (32u << 20) || !RomPart(f, b, e - b, out)) return {};
                    return out;
                }
                id++;
            }
        }
        if (!down) return {};
    }
    return {};
}
struct MonIconSet { bool tried = false; Companion::Metadata meta; };
inline MonIconSet& MonIcons() { static MonIconSet m; return m; }
inline bool MonIcon(int species, bool egg, std::vector<uint32_t>& out)
{
    MonIconSet& I = MonIcons();
    if (!I.tried)
    {
        I.tried = true;
        const Game* pick = nullptr;
        bool gen5 = false;
        for (const Game& g : RomList())
        {
            if (!g.known || g.platform != DS || g.code.size() < 4) continue;
            const bool five = g.code[0] == 'I' && g.code[1] == 'R';
            const bool four = g.code.rfind("IPK", 0) == 0 || g.code.rfind("IPG", 0) == 0 || g.code.rfind("CPU", 0) == 0;
            if (five && !gen5) { pick = &g; gen5 = true; }
            else if (four && !pick) pick = &g;
        }
        if (pick)
            if (FILE* f = fopen(pick->rom.c_str(), "rb"))
            {
                const bool hg = pick->code.rfind("IPK", 0) == 0 || pick->code.rfind("IPG", 0) == 0;
                Companion::Metadata& m = I.meta;
                m.gen5 = gen5;
                m.monIcons = Companion::Narc(RomFileFrom(f, gen5 ? "a/0/0/7" : hg ? "a/0/2/0" : "poketool/icongra/pl_poke_icon.narc"));
                if (!m.monIcons.empty()) m.iconPalettes = CompanionIcons::Palettes(m.monIcons[0]);
                if (gen5 && !m.monIcons.empty())
                {
                    // (The ARM9, for the table of each Pokémon's palette: the start of the ROM up to its end.)
                    std::vector<uint8_t> h;
                    if (RomPart(f, 0, 0x200, h))
                    {
                        const uint32_t off = h[0x20] | h[0x21] << 8 | h[0x22] << 16 | (uint32_t)h[0x23] << 24;
                        const uint32_t size = h[0x2C] | h[0x2D] << 8 | h[0x2E] << 16 | (uint32_t)h[0x2F] << 24;
                        std::vector<uint8_t> head;
                        if (size <= 0x400000 && off <= 0x100000 && RomPart(f, 0, off + size, head)) m.FindGen5IconPalettes(Companion::RomArm9(head));
                    }
                    for (size_t i = 0; i < m.monIcons.size() && i < 16; i++)
                        if (m.monIcons[i].size() > 4 && !memcmp(m.monIcons[i].data(), "RGCN", 4)) { m.gen5IconStart = (int)i; break; }
                }
                fclose(f);
                fprintf(stderr, "[hub] Pokemon icons from %s: %zu\n", pick->rom.c_str(), m.monIcons.size());
            }
    }
    return I.meta.MonIcon(species, egg, out);
}
inline void DrawMonIcon(Canvas& c, const std::vector<uint32_t>& ic, int x, int y, int size)
{
    if (ic.size() < 32 * 32 || size <= 0) return;
    for (int j = 0; j < size; j++)
    {
        const int yy = y + j;
        if (yy < 0 || yy >= c.h) continue;
        for (int i = 0; i < size; i++)
        {
            const int xx = x + i;
            if (xx < 0 || xx >= c.w) continue;
            const uint32_t p = ic[(size_t)(j * 32 / size) * 32 + (i * 32 / size)];
            if (p >> 24) c.px[(size_t)yy * c.w + xx] = p | 0xFF000000u;
        }
    }
}
// "SWAMPERT" as "Swampert" (and "MR. MIME" as "Mr. Mime", "HO-OH" as "Ho-Oh").
inline std::string MonName(const std::string& up)
{
    std::string s;
    bool start = true;
    for (char ch : up)
    {
        const unsigned char b = (unsigned char)ch;
        s += b < 128 ? (char)(start ? toupper(b) : tolower(b)) : ch;
        start = b == ' ' || b == '-' || b == '.';
    }
    return s;
}
#endif
// (Names from a save as the font can show them: Japanese ones, and empty ones, give way.)
inline bool Drawable(const std::string& s)
{
    bool any = false;
    for (size_t i = 0; i < s.size();)
    {
        const unsigned char b = (unsigned char)s[i];
        uint32_t cp = b;
        int n = 1;
        if (b >= 0xF0) { n = 4; cp = b & 7; } else if (b >= 0xE0) { n = 3; cp = b & 15; } else if (b >= 0xC0) { n = 2; cp = b & 31; }
        for (int k = 1; k < n && i + k < s.size(); k++) cp = cp << 6 | ((unsigned char)s[i + k] & 63);
        if (cp >= 0x2000 || cp < 0x20) return false;
        if (cp != ' ') any = true;
        i += n;
    }
    return any;
}
inline std::string Shown(const std::string& s, const std::string& otherwise) { return Drawable(s) ? s : otherwise; }
// (Mover rows: 0 the game they come from, 1 its box (or party), 2 the game they go to, 3 where they
// go in it (or, swapping, which of its boxes to choose from), then the Pokémon.)
const int MOVER_FIRST = 4;
#ifdef SSR_POKEMON_HUB
inline std::vector<PokemonHub::Mon> MonsIn(const PokemonHub::Save* s, int box)
{
    if (!s) return {};
    return box == PokemonHub::PARTY ? PokemonHub::Party(*s) : s->Box(std::max(0, box));
}
inline std::vector<PokemonHub::Mon> MoverBox()
{
    MoverOpenSaves();
    return MonsIn(Mover().a.get(), Mover().box);
}
inline int MoverTargetBox()
{
    const int p = Mover().put;
    return p == PokemonHub::TO_PARTY ? PokemonHub::PARTY : std::max(0, p);
}
// The Pokémon listed: those in the chosen box, or (swapping) those in the other game's.
inline std::vector<PokemonHub::Mon> MoverList()
{
    MoverOpenSaves();
    MoverState& m = Mover();
    return m.swapping ? MonsIn(m.b.get(), MoverTargetBox()) : MonsIn(m.a.get(), m.box);
}
inline std::string BoxLabel(const PokemonHub::Save* s, int box)
{
    if (box == PokemonHub::PARTY) return "Party";
    return s ? Shown(s->BoxName(box), "Box " + std::to_string(box + 1)) : std::string("-");
}
inline std::string PutLabel()
{
    MoverState& m = Mover();
    if (m.put == PokemonHub::ANY_BOX) return "First Free Place";
    if (m.put == PokemonHub::TO_PARTY) return "Party";
    return BoxLabel(m.b.get(), m.put);
}
// (A box's next, its previous, or the party, in a game of n boxes.)
inline int StepBox(int box, int n, int how)
{
    if (n <= 0) return 0;
    if (how == 2) return PokemonHub::PARTY;
    if (box < 0) return how == 0 ? 0 : n - 1;
    return how == 0 ? (box + 1) % n : (box + n - 1) % n;
}
#endif
inline int MoverRows()
{
#ifdef SSR_POKEMON_HUB
    return MOVER_FIRST + (int)MoverList().size();
#else
    return 3;
#endif
}
inline std::vector<std::string> MoverActions(int row)
{
    std::vector<std::string> r;
    const std::vector<MoverGame> gs = MoverGames();
    if (row == 0 || row == 2)
    {
        for (size_t i = 0; i < gs.size() && i < 7; i++) r.push_back(gs[i].name);
        return r;
    }
#ifdef SSR_POKEMON_HUB
    MoverState& m = Mover();
    if (row == 1) return { "Next Box", "Previous Box", "Party" };
    if (row == 3) return m.swapping ? std::vector<std::string>{ "Next Box", "Previous Box", "Party" }
                                    : std::vector<std::string>{ "First Free Place", "Party", "Next Box", "Previous Box" };
    const std::vector<PokemonHub::Mon> mons = MoverList();
    const int i = row - MOVER_FIRST;
    if (i < 0 || i >= (int)mons.size() || !m.a || !m.b || m.from == m.to) return m.swapping ? std::vector<std::string>{ "Cancel Swap" } : r;
    const std::string toName = gs[std::min(m.to, (int)gs.size() - 1)].name;
    if (m.swapping)
    {
        const PokemonHub::Check ck = PokemonHub::CanSwap(*m.a, m.swapBox, m.swapSlot, *m.b, mons[i].box, mons[i].slot);
        if (ck.verdict == PokemonHub::Verdict::Ok)
        {
            r.push_back("Swap");
            if (ck.evolvesTo) r.push_back("Swap And Evolve");
        }
        r.push_back("Cancel Swap");
        return r;
    }
    const PokemonHub::Check ck = PokemonHub::CanMoveTo(*m.a, mons[i].box, mons[i].slot, *m.b, m.put, -1);
    if (ck.verdict == PokemonHub::Verdict::Ok)
    {
        r.push_back("Move To " + toName);
        if (ck.evolvesTo) r.push_back("Move And Evolve");
    }
    if (m.a->Generation() == m.b->Generation()) r.push_back("Swap With One In " + toName);
#endif
    return r;
}
inline std::string MoverDo(int row, int act)
{
    MoverState& m = Mover();
    const std::vector<std::string> acts = MoverActions(row);
    if (act < 0 || act >= (int)acts.size()) return "";
    Hub().version++;
    if (row == 0) { m.from = act; m.box = 0; m.swapping = false; m.msg.clear(); return ""; }
    if (row == 2) { m.to = act; m.put = -1; m.swapping = false; m.msg.clear(); return ""; }
#ifdef SSR_POKEMON_HUB
    MoverOpenSaves();
    const std::string a = acts[act];
    if (row == 1)
    {
        if (m.a) m.box = StepBox(m.box, m.a->Boxes(), a == "Next Box" ? 0 : a == "Previous Box" ? 1 : 2);
        return "";
    }
    if (row == 3)
    {
        if (a == "First Free Place") m.put = PokemonHub::ANY_BOX;
        else if (a == "Party") m.put = PokemonHub::TO_PARTY;
        else if (m.b) m.put = StepBox(m.put == PokemonHub::TO_PARTY || m.put == PokemonHub::ANY_BOX ? -1 : m.put, m.b->Boxes(), a == "Next Box" ? 0 : 1);
        return "";
    }
    const std::vector<PokemonHub::Mon> mons = MoverList();
    const int i = row - MOVER_FIRST;
    if (a == "Cancel Swap") { m.swapping = false; m.put = -1; m.msg.clear(); Focus().pact = -1; return ""; }
    if (i < 0 || i >= (int)mons.size() || !m.a || !m.b) return "";
    const std::vector<MoverGame> gs = MoverGames();
    const std::string toName = gs[std::min(m.to, (int)gs.size() - 1)].name;
    auto nameOf = [](const PokemonHub::Mon& mon) {
        const std::string n = Shown(mon.nickname, MonName(mon.speciesName));
        return MonName(n) == MonName(mon.speciesName) ? MonName(mon.speciesName) : n + " (" + MonName(mon.speciesName) + ")";
    };
    std::string err;
    std::vector<std::string> backups;
    if (a.rfind("Swap With", 0) == 0)
    {
        m.swapping = true;
        m.swapBox = mons[i].box; m.swapSlot = mons[i].slot; m.swapName = nameOf(mons[i]);
        if (m.put == PokemonHub::ANY_BOX) m.put = 0;
        m.msg = "Now choose the Pok\xC3\xA9mon in " + toName + " to swap for " + m.swapName + ".";
        Focus().prow = MOVER_FIRST; Focus().pact = -1; Focus().rScroll = 0;
        return "";
    }
    if (m.swapping)
    {
        const std::string other = nameOf(mons[i]);
        if (!PokemonHub::Swap(*m.a, m.swapBox, m.swapSlot, *m.b, mons[i].box, mons[i].slot, a == "Swap And Evolve", err)
            || !PokemonHub::Commit(*m.a, *m.b, err, &backups))
            m.msg = "Could not swap them. " + err;
        else
            m.msg = m.swapName + " and " + other + " swapped. Both saves were backed up first.";
        m.swapping = false;
        m.put = -1;
        Focus().pact = -1;
        MoverOpenSaves(true);
        Hub().version++;
        return "";
    }
    const std::string name = nameOf(mons[i]);
    if (!PokemonHub::Move(*m.a, mons[i].box, mons[i].slot, *m.b, a == "Move And Evolve", err, m.put, -1)
        || !PokemonHub::Commit(*m.a, *m.b, err, &backups))
        m.msg = "Could not move it. " + err;
    else
        m.msg = name + " moved to " + toName + " (" + PutLabel() + "). Both saves were backed up first.";
    MoverOpenSaves(true);
    Hub().version++;
#endif
    return "";
}
// TRADE OVER WI-FI (the Hub's third page, hub_link.h): one of the player's games and its box, the
// link to the other device, then the box's Pokémon to offer or send. Rows: 0 the game, 1 the box,
// 2 the link (and what the other side offers), then the Pokémon.
const int TRADE_FIRST = 3;
struct TradeState
{
    int game = 0, box = 0;
    std::string msg;
    bool guide = false;                                // (How Trading Works: shown in place of the link's panel)
    int offerBox = 0, offerSlot = 0, offerSpecies = 0; // (where this side's offer came from)
    std::string checked;                               // (the other side's offer last checked)
#ifdef SSR_POKEMON_HUB
    std::unique_ptr<PokemonHub::Save> save;
    std::string path, err;
#endif
};
inline TradeState& Trade() { static TradeState t; return t; }
#ifdef SSR_POKEMON_HUB
inline void TradeOpenSave(bool again = false)
{
    TradeState& t = Trade();
    const std::vector<MoverGame> gs = MoverGames();
    if (gs.empty()) { t.save.reset(); t.path.clear(); return; }
    t.game = std::max(0, std::min(t.game, (int)gs.size() - 1));
    if (again || !t.save || t.path != gs[t.game].path) { t.path = gs[t.game].path; t.err.clear(); t.save = PokemonHub::Save::Open(t.path, t.err, gs[t.game].hack); }
    if (t.save && t.box != PokemonHub::PARTY) t.box = std::max(0, std::min(t.box, t.save->Boxes() - 1));
}
inline std::vector<PokemonHub::Mon> TradeList() { TradeOpenSave(); return MonsIn(Trade().save.get(), Trade().box); }
inline PokemonHub::Packed TheirPacked(const HubLink::Offer& o)
{
    PokemonHub::Packed p;
    p.generation = o.generation;
    p.data = o.data;
    p.game = o.game;
    return p;
}
// (Each time things change: the other side's offer checked against this game, and, once both have
// said yes, this side's save written.)
inline void TradeTick()
{
    TradeState& t = Trade();
    const HubLink::Status s = HubLink::Snapshot();
    if (s.state != HubLink::State::Linked) return;
    TradeOpenSave();
    if (s.theirs.any)
    {
        const std::string key = std::to_string(s.theirs.data.size()) + "/" + std::to_string(s.theirs.speciesId) + "/" + s.theirs.name + "/" + t.path;
        if (key != t.checked && t.save)
        {
            t.checked = key;
            const PokemonHub::Check ck = PokemonHub::CanReceive(TheirPacked(s.theirs), *t.save, s.mine.any ? t.offerSpecies : 0);
            HubLink::TellCheck(ck.verdict == PokemonHub::Verdict::Ok, ck.reason);
        }
    }
    else t.checked.clear();
    if (!s.finish || !t.save) return;
    std::string err;
    std::vector<std::string> backups;
    bool ok = true;
    if (s.theirs.any) ok = PokemonHub::Receive(*t.save, TheirPacked(s.theirs), true, s.mine.any ? t.offerSpecies : 0, err);
    if (ok && s.mine.any) ok = PokemonHub::Remove(*t.save, t.offerBox, t.offerSlot, err);
    if (ok) ok = PokemonHub::Commit(*t.save, err, &backups);
    const std::string got = s.theirs.any ? s.theirs.name : std::string();
    t.msg = !ok ? "It could not be written: " + err
          : s.mine.any && s.theirs.any ? "Traded! " + got + " arrived. The save was backed up first."
          : s.theirs.any ? got + " arrived. The save was backed up first." : "Sent. The save was backed up first.";
    HubLink::FinishDone(ok, ok ? (s.theirs.any ? "Done: " + got + " arrived." : "Done.") : err);
    TradeOpenSave(true);
    Hub().version++;
}
#endif
// (The relay's address: fulldex-relay.txt beside the settings, for a test, else the one built in.)
inline std::string TradeRelay()
{
    if (FILE* f = fopen((HubDir() + "/fulldex-relay.txt").c_str(), "rb"))
    {
        char b[256] = {};
        if (!fgets(b, sizeof b, f)) b[0] = 0;
        fclose(f);
        std::string r = b;
        while (!r.empty() && isspace((unsigned char)r.back())) r.pop_back();
        if (!r.empty()) return r;
    }
#ifdef FULLDEX_RELAY_URL
    return FULLDEX_RELAY_URL;
#else
    return "";
#endif
}
inline std::string TradeGameName()
{
    const std::vector<MoverGame> gs = MoverGames();
    return gs.empty() ? std::string() : gs[std::max(0, std::min(Trade().game, (int)gs.size() - 1))].name;
}
inline std::string TradeTrainer();
inline void TradeEnterCode(const std::string& code)
{
    if (code.empty()) return;
    HubLink::Online(TradeRelay(), code, TradeTrainer(), TradeGameName());
    Hub().version++;
}
inline int TradeRows()
{
#ifdef SSR_POKEMON_HUB
    return TRADE_FIRST + (int)TradeList().size();
#else
    return TRADE_FIRST;
#endif
}
inline std::string TradeTrainer()
{
#ifdef SSR_POKEMON_HUB
    TradeOpenSave();
    if (Trade().save) return Shown(Trade().save->Trainer(), "A Trainer");
#endif
    return "A Trainer";
}
inline std::vector<std::string> TradeActions(int row)
{
    std::vector<std::string> r;
    const std::vector<MoverGame> gs = MoverGames();
    const HubLink::Status s = HubLink::Snapshot();
    if (row == 0)
    {
        if (s.state == HubLink::State::Linked) return r; // (not while linked: the game is in the trade)
        for (size_t i = 0; i < gs.size() && i < 7; i++) r.push_back(gs[i].name);
        return r;
    }
    if (row == 1) return { "Next Box", "Previous Box", "Party" };
    if (row == 2)
    {
        switch (s.state)
        {
        case HubLink::State::Off:
            if (Trade().guide) return { "Close The Guide" };
            return { "Open A Room", "Join A Room", "Make An Online Code", "Enter An Online Code", "How Trading Works" };
        case HubLink::State::Online: return { "Stop" };
        case HubLink::State::Hosting: return { "Close The Room" };
        case HubLink::State::Browsing:
            for (size_t i = 0; i < s.rooms.size() && i < 5; i++) r.push_back("Join " + s.rooms[i].name + " (" + s.rooms[i].game + ")");
            r.push_back("Stop Looking");
            return r;
        case HubLink::State::Linked:
            if (s.theirs.any && !s.iAccepted)
            {
                if (s.theirs.gift && !s.mine.any) r.push_back("Accept The Gift");
                else if (!s.theirs.gift && s.mine.any && s.theyCanTake) r.push_back("Accept The Trade");
            }
            if (s.iAccepted) r.push_back("Not Yet");
            if (s.mine.any) r.push_back("Take Back My Offer");
            r.push_back("Leave");
            return r;
        }
        return r;
    }
#ifdef SSR_POKEMON_HUB
    if (s.state != HubLink::State::Linked) return r;
    const std::vector<PokemonHub::Mon> mons = TradeList();
    const int i = row - TRADE_FIRST;
    if (i < 0 || i >= (int)mons.size() || !Trade().save) return r;
    if (PokemonHub::CanRemove(*Trade().save, mons[i].box, mons[i].slot).verdict != PokemonHub::Verdict::Ok) return r;
    r.push_back("Offer For A Trade");
    r.push_back("Send As A Gift");
#endif
    return r;
}
inline std::string TradeDo(int row, int act)
{
    TradeState& t = Trade();
    const std::vector<std::string> acts = TradeActions(row);
    if (act < 0 || act >= (int)acts.size()) return "";
    const std::string a = acts[act];
    const std::vector<MoverGame> gs = MoverGames();
    const HubLink::Status s = HubLink::Snapshot();
    Hub().version++;
    if (row == 0) { t.game = act; t.box = 0; return ""; }
    if (row == 2) { const bool was = t.guide; t.guide = a == "How Trading Works"; if (a == "How Trading Works" || (was && a == "Close The Guide")) { Focus().pact = -1; return ""; } }
#ifdef SSR_POKEMON_HUB
    TradeOpenSave();
    if (row == 1) { if (t.save) t.box = StepBox(t.box, t.save->Boxes(), a == "Next Box" ? 0 : a == "Previous Box" ? 1 : 2); return ""; }
    const std::string game = gs.empty() ? std::string() : gs[std::min(t.game, (int)gs.size() - 1)].name;
    if (row == 2)
    {
        if (a == "Open A Room") HubLink::Host(TradeTrainer(), game);
        else if (a == "Join A Room") HubLink::Browse(TradeTrainer(), game);
        else if (a == "Close The Room" || a == "Stop Looking" || a == "Leave" || a == "Stop") HubLink::Leave();
        else if ((a == "Make An Online Code" || a == "Enter An Online Code") && TradeRelay().empty()) t.msg = "Online trades are not set up in this build of Fulldex.";
        else if (a == "Make An Online Code") HubLink::Online(TradeRelay(), HubLink::NewCode(), TradeTrainer(), game);
        else if (a == "Enter An Online Code") { GlassFocus& K = Focus(); K.kbFor = 4; K.text.clear(); K.dialog = 3; K.drow = 0; t.msg.clear(); }
        else if (a.rfind("Join ", 0) == 0 && act < (int)s.rooms.size()) HubLink::Join(s.rooms[act]);
        else if (a == "Accept The Gift" || a == "Accept The Trade") HubLink::Accept(true);
        else if (a == "Not Yet") HubLink::Accept(false);
        else if (a == "Take Back My Offer") HubLink::SetOffer(HubLink::Offer());
        Focus().pact = -1;
        return "";
    }
    const std::vector<PokemonHub::Mon> mons = TradeList();
    const int i = row - TRADE_FIRST;
    if (i < 0 || i >= (int)mons.size() || !t.save) return "";
    PokemonHub::Packed p;
    std::string err;
    if (!PokemonHub::Pack(*t.save, mons[i].box, mons[i].slot, p, err)) { t.msg = "It cannot be sent. " + err; return ""; }
    HubLink::Offer o;
    o.any = true;
    o.gift = a == "Send As A Gift";
    o.generation = p.generation;
    o.data = p.data;
    o.name = Shown(mons[i].nickname, MonName(mons[i].speciesName));
    o.species = MonName(mons[i].speciesName);
    o.game = game;
    o.speciesId = mons[i].species;
    o.level = mons[i].level;
    o.shiny = mons[i].shiny;
    t.offerBox = mons[i].box; t.offerSlot = mons[i].slot; t.offerSpecies = mons[i].species;
    HubLink::SetOffer(o);
    t.msg.clear();
    Focus().prow = 2; Focus().pact = -1;
#endif
    return "";
}
inline void MoverShow(bool open)
{
    MoverState& m = Mover();
    m.open = open;
    m.msg.clear();
    m.swapping = false;
    if (open && m.from == m.to) m.to = m.from == 0 ? 1 : 0;
    Hub().version++;
}
// A game's save and the cloud, for Home: 0 not there (or cloud saves off), 1 kept in the cloud, 2 changed on both.
inline int HubCloudOf(const Game& g)
{
    if (g.saveName.empty() || !g.known) return 0;
    HubLoad();
    HubState& h = Hub();
    std::lock_guard<std::mutex> l(h.m);
    if (!HubOnLocked(h)) return 0;
    const auto it = h.results.find(HubSavePath(g));
    if (it != h.results.end() && it->second.outcome == CloudSaves::Outcome::Conflict) return 2;
    return h.synced.count(g.saveName) ? 1 : 0;
}
// A small cloud, centred on cx, cy; s half its width.
inline void DrawCloud(Canvas& c, float cx, float cy, float s, uint32_t col)
{
    Glass::Round(c, (int)(cx - s), (int)(cy - s * 0.05f), (int)(2 * s), (int)(s * 0.75f), s * 0.37f, col);
    Glass::Round(c, (int)(cx - s * 0.7f), (int)(cy - s * 0.5f), (int)(s * 0.85f), (int)(s * 0.85f), s * 0.42f, col);
    Glass::Round(c, (int)(cx - s * 0.15f), (int)(cy - s * 0.8f), (int)(s * 1.0f), (int)(s * 1.0f), s * 0.5f, col);
}
// A on the Hub's first page: Cloud Saves or Move Pokémon opened. (False: not that page.)
// (Manage Saves, the Hub's page 3: saves_page.h, below.)
inline bool SavesEnter(int row);
inline int SavesRows();
inline std::string SavesSlotDo(int slot);
inline bool SavesDs();
inline bool SavesBack();
inline bool SavesMove(int dir);
inline int SavesSlotAt(int x, int y);
inline std::string SavesTitle();
inline void DexOpen();
inline bool DexEnter(int row);
inline int DexRows();
inline void DexPoll();
inline bool HubEnter(int row)
{
    if (HubMode() == 4 && !Mover().open) return DexEnter(row);
    if (HubMode() == 3 && !Mover().open) return SavesEnter(row);
    if (Mover().open || HubMode() != 0) return false;
    GlassFocus& F = Focus();
    if (row == 0) { HubMode() = 1; F.prow = 0; }
    else if (row == 2) { HubMode() = 2; F.prow = 2; }
    else if (row == 4) { DexOpen(); return true; }
    else if (row == 3) return false; // (Discord: a link, nothing to open here)
    else { MoverShow(true); F.prow = MOVER_FIRST; }
    F.pact = -1; F.rScroll = 0;
    Hub().version++;
    return true;
}
inline bool HubTop() { return !Mover().open && HubMode() == 0; }
// The Launcher page's rows changed (A: the next choice, or one's own page opened), and kept.
inline std::string LookName(const std::string& v)
{
    if (v == "game") return "Game Art";
    if (v == "picture") return "Your Picture";
    if (!v.empty() && v[0] == '#') return "Custom";
    std::string o = v;
    if (!o.empty()) o[0] = (char)toupper((unsigned char)o[0]);
    return o;
}
// What a row's page holds: a picture, presets, the wheel's three bars, how solid; or the blur and dim.
enum { LF_PIC, LF_PRESET, LF_HUE, LF_SAT, LF_VAL, LF_OP, LF_BLUR, LF_DIM, LF_GRAIN, LF_SOURCE, LF_SIZE, LF_CURSOR };
inline std::vector<int> LookFieldList(int e)
{
    if (e == 6) return { LF_GRAIN };
    if (e == 8) return { LF_SIZE }; // (Text Size)
    if (e == 0 || e == 10) return { LF_PRESET, LF_HUE, LF_SAT, LF_VAL };
    if (e == 5) return { LF_SOURCE, LF_PIC, LF_PRESET, LF_HUE, LF_SAT, LF_VAL, LF_BLUR, LF_DIM };
    if (e == 4) return { LF_PIC, LF_PRESET, LF_HUE, LF_SAT, LF_VAL, LF_OP, LF_CURSOR }; // (Buttons: and whether the selector takes their colour)
    return { LF_PIC, LF_PRESET, LF_HUE, LF_SAT, LF_VAL, LF_OP };
}
inline const char* const* LookPresets(int e, int& n) { if (e >= 1 && e <= 3) { n = 9; return kLookPanels; } n = 8; return kLookAccents; }
// The pictures to choose from: PNG and JPG files in launcher-pictures with the saves.
inline std::vector<std::string> LookPictures()
{
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(HubDir() + "/launcher-pictures", ec))
    {
        std::string n = e.path().filename().string(), low = n;
        for (char& ch : low) ch = (char)tolower((unsigned char)ch);
        const auto ends = [&](const char* x) { const size_t k = strlen(x); return low.size() > k && low.compare(low.size() - k, k, x) == 0; };
        if (e.is_regular_file(ec) && (ends(".png") || ends(".jpg") || ends(".jpeg"))) out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
}
inline void LookSetHsv()
{
    LookPrefs& l = Look();
    char hex[8];
    snprintf(hex, sizeof hex, "#%06X", (unsigned)(Hsv(l.h, l.s, l.v) & 0xFFFFFFu));
    l.col[l.edit] = hex;
    l.pic[l.edit].clear(); // (a colour chosen: the colour, not the picture)
    if (l.edit == 5) l.backdrop = "colour";
    LookApply();
}
inline void LookSyncHsv() { LookPrefs& l = Look(); if (l.edit >= 0 && (l.edit < 6 || l.edit == 10)) ToHsv(LookRgb(l.edit), l.h, l.s, l.v); }
inline int LookFieldIndex(int e, int id) { const auto f = LookFieldList(e); for (size_t i = 0; i < f.size(); i++) if (f[i] == id) return (int)i; return 0; }
inline void LookOpen(int e) { LookPrefs& l = Look(); l.edit = e; l.field = e == 6 || e == 5 || e == 8 ? 0 : LookFieldIndex(e, LF_HUE); LookSyncHsv(); }
inline void LookPicked(int e) // (a picture chosen: shown fairly solid; a background one hardly dimmed)
{
    LookPrefs& l = Look();
    if (e == 5 && l.dim == 42) l.dim = 10;
    LookApply();
}
inline void LookNextPicture(int d)
{
    LookPrefs& l = Look();
    const std::vector<std::string> pics = LookPictures();
    int i = -1;
    for (size_t k = 0; k < pics.size(); k++) if (pics[k] == l.pic[l.edit]) i = (int)k;
    const int n = (int)pics.size() + 1; // (and None)
    i = ((i + 1) + d + n) % n;
    l.pic[l.edit] = i == 0 ? std::string() : pics[i - 1];
    if (l.edit == 5 && !l.pic[5].empty()) l.backdrop = "picture";
    if (!l.pic[l.edit].empty()) LookPicked(l.edit); else LookApply();
}
// The background's Show: Game Art, Your Picture, Colour, Plain.
inline void LookCycleSource(int d)
{
    static const char* const b[] = { "game", "picture", "colour", "plain" };
    LookPrefs& l = Look();
    int i = 0;
    for (int k = 0; k < 4; k++) if (l.backdrop == b[k]) i = k;
    l.backdrop = b[(i + d + 4) % 4];
}
inline void GalleryOpen();
inline void GalleryOpenCard(int k);
inline void GalleryShareMenu();
// A on a row: 3 when a picture is wanted from the system's picker (the PC's).
inline int LookDo(int row, int dir)
{
    LookLoad();
    LookPrefs& l = Look();
    int out = 0;
    if (LauncherMode() == 0)
    {
        // (The menu: Layout, Theme Editor, My Themes, Theme Gallery.)
        if (row == 0)
        {
            // (Panel is kept as a file value only. The carousel is for a desktop; a phone has the two grids.)
            static const char* const layouts[] = { "sidebar", "grid", "tiles" };
            int k = 0; for (int i = 0; i < 3; i++) if (l.layout == layouts[i]) k = i;
            l.layout = PhoneLook() ? (l.layout == "grid" ? "sidebar" : "grid") : layouts[(k + (dir < 0 ? 2 : 1)) % 3];
            Focus().banner = 0;
            LookSave(); Hub().version++; return 0;
        }
        if (row == 4) { l.music = l.music == "corner" ? "top" : l.music == "top" ? "compact" : l.music == "compact" ? "compactb" : l.music == "compactb" ? "off" : "corner"; LookSave(); Hub().version++; return 0; }
        if (row == 5) { l.avoidCutout = !l.avoidCutout; LookSave(); Hub().version++; return 0; } // (Android: Camera Cutout)
        if (row == 1) LauncherMode() = 1;
        else if (row == 2) { LauncherMode() = 2; ThemesRefresh(); }
        else { LauncherMode() = 5; Focus().prow = 0; Focus().rScroll = 0; Themes().msg.clear(); GalleryOpen(); } // (the Theme Gallery)
        if (row <= 2) Focus().prow = 0;
        Hub().version++;
        return 0;
    }
    if (LauncherMode() == 2)
    {
        // (My Themes: Save This Look (its name typed), Import A Theme, a theme's menu.)
        GlassFocus& F = Focus();
        Themes().msg.clear();
        if (row == 0) { F.kbFor = 5; F.kbRom.clear(); F.text.clear(); F.dialog = 3; F.drow = 0; return 2; }
        if (row == 1)
        {
#if defined(__SWITCH__) || defined(__ANDROID__)
            ThemesRefresh();
            // (No file picker here: zips dropped in the themes folder show in the Theme Library.)
#ifdef __SWITCH__
            Themes().msg = "PUT .ZIP THEMES IN /SWITCH/POKEMON-DS-WIDE/THEMES/ AND THEY SHOW HERE";
#else
            Themes().msg = "PUT .ZIP THEMES IN " + ThemesDir() + " AND THEY SHOW HERE";
#endif
            Hub().version++;
            return 0;
#else
            return 4;
#endif
        }
        if (row == 2) { LauncherMode() = 3; ThemesRefresh(); F.prow = 0; F.rScroll = 0; Hub().version++; } // (the Theme Library)
        if (row == 3)
        {
            // (Default Look: asked first.)
            F.dialog = 15;
            F.drow = 1;
            F.dTitle = "Default Look";
            F.dMsg = "Go back to frosted glass over the game's art, as Fulldex starts? Your themes stay in the library: save this look first to keep it.";
            F.dItems = { "Use The Default Look", "Cancel" };
            F.dPaths = { "#lookdefault", "#close" };
        }
        return 0;
    }
    if (LauncherMode() == 5)
    {
        // (The Theme Gallery: Enter A Theme Code, Share A Theme, then a shared theme's menu.)
        GlassFocus& GF = Focus();
        Themes().msg.clear();
        if (row == 0) { GF.kbFor = 6; GF.kbRom.clear(); GF.text.clear(); GF.dialog = 3; GF.drow = 0; return 2; }
        if (row == 1) GalleryShareMenu();
        else GalleryOpenCard(row - 2);
        return 0;
    }
    if (LauncherMode() == 3) { OpenThemeMenu(row); return 0; } // (the Theme Library: a theme's menu)
    if (LauncherMode() == 4)
    {
        // (A font chosen: on at once, and kept.)
        if (row >= 0 && row < kLauncherFontCount) { l.font = kLauncherFonts[row].id; LookSave(); Themes().msg = std::string("FONT: ") + kLauncherFonts[row].name; }
        Hub().version++;
        return 0;
    }
    if (l.edit >= 0)
    {
        if (l.edit != 6 && LookFieldList(l.edit)[l.field] == LF_PIC)
        {
#ifdef __SWITCH__
            LookNextPicture(1); LookSave(); Hub().version++; return 0;
#else
            return 3;
#endif
        }
        l.edit = -1; LookSave(); Hub().version++; return 0;
    }
    if (row <= 6) { LookOpen(row); Hub().version++; return 0; } // (each opens its own page)
    if (row == 8) { LauncherMode() = 4; Focus().prow = LookFontId(); Focus().rScroll = 0; Themes().msg.clear(); Hub().version++; return 0; } // (Font: its grid)
    if (row == 9) { LookOpen(8); Hub().version++; return 0; } // (Text Size)
    if (row == 10) { LookOpen(10); Hub().version++; return 0; } // (Text Colour)
    if (row == 7) l.badges = !l.badges;
    (void)dir;
    LookSave();
    Hub().version++; // (drawn again)
    return out;
}
// A picture from the system's picker, copied into launcher-pictures and used for the open row.
// A picture's longest side kept at most this on a small screen (0: as it is, on a PC).
#if defined(__SWITCH__)
static const int kSmallPicture = 1280;
#elif defined(__ANDROID__)
static const int kSmallPicture = 1920;
#else
static const int kSmallPicture = 0;
#endif
// A bigger picture made smaller for a small screen: the longest side at most `most`, the shape kept (a box average, saved
// as a JPEG, its name now .jpg). Left as it is when it already fits, or has see-through parts (a JPEG cannot). True when changed.
inline bool LookShrinkPicture(std::vector<unsigned char>& data, std::string& name, int most)
{
    if (most <= 0 || data.empty()) return false;
    int w = 0, h = 0, n = 0;
    if (!stbi_info_from_memory(data.data(), (int)data.size(), &w, &h, &n) || std::max(w, h) <= most) return false;
    unsigned char* px = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &n, 4);
    if (!px) return false;
    bool see = false;
    for (size_t i = 3; i < (size_t)w * h * 4 && !see; i += 4) see = px[i] != 255;
    if (see) { stbi_image_free(px); return false; }
    const double k = (double)most / std::max(w, h);
    const int tw = std::max(1, (int)(w * k + 0.5)), th = std::max(1, (int)(h * k + 0.5));
    std::vector<uint32_t> out((size_t)tw * th);
    for (int y = 0; y < th; y++)
    {
        const int y0 = (int)((int64_t)y * h / th), y1 = std::max(y0 + 1, (int)((int64_t)(y + 1) * h / th));
        for (int x = 0; x < tw; x++)
        {
            const int x0 = (int)((int64_t)x * w / tw), x1 = std::max(x0 + 1, (int)((int64_t)(x + 1) * w / tw));
            uint32_t r = 0, g = 0, b = 0, c = 0;
            for (int yy = y0; yy < y1 && yy < h; yy++)
                for (int xx = x0; xx < x1 && xx < w; xx++) { const unsigned char* q = px + ((size_t)yy * w + xx) * 4; r += q[0]; g += q[1]; b += q[2]; c++; }
            if (!c) c = 1;
            out[(size_t)y * tw + x] = 0xFF000000u | (r / c) << 16 | (g / c) << 8 | (b / c);
        }
    }
    stbi_image_free(px);
    std::vector<unsigned char> jpg;
    if (!ThemePack::Jpeg(out.data(), tw, th, 88, jpg) || jpg.empty() || jpg.size() >= data.size()) return false;
    data.swap(jpg);
    const size_t dot = name.find_last_of('.');
    name = (dot == std::string::npos ? name : name.substr(0, dot)) + ".jpg";
    return true;
}
inline bool LookAddPicture(const std::string& path)
{
    LookPrefs& l = Look();
    if (l.edit < 1 || l.edit > 5) return false;
    std::vector<unsigned char> data;
    if (!ReadFile(path, data) || !ImageAllowed(data.data(), data.size())) return false;
    std::error_code ec;
    std::filesystem::create_directories(HubDir() + "/launcher-pictures", ec);
    std::string name = std::filesystem::path(path).filename().string();
    LookShrinkPicture(data, name, kSmallPicture);
    FILE* f = fopen((HubDir() + "/launcher-pictures/" + name).c_str(), "wb");
    if (!f) return false;
    const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    if (!ok) return false;
    l.pic[l.edit] = name;
    if (l.edit == 5) l.backdrop = "picture";
    LookPicked(l.edit);
    LookSave();
    Hub().version++;
    return true;
}
// The arrows while a row's page is open: up and down its rows, left and right change that one.
inline void LookMove(int dir)
{
    LookPrefs& l = Look();
    const int e = l.edit;
    const std::vector<int> fl = LookFieldList(e);
    if (dir == 3) l.field = std::max(0, l.field - 1);
    else if (dir == 4) l.field = std::min((int)fl.size() - 1, l.field + 1);
    else
    {
        const int d = dir == 2 ? 1 : -1, id = fl[l.field];
        if (id == LF_BLUR) l.blur = std::max(0, std::min(100, l.blur + d * (l.blur + (d > 0 ? 0 : -1) < 10 ? 1 : 5)));
        else if (id == LF_DIM) l.dim = std::max(0, std::min(100, l.dim + d * 5));
        else if (id == LF_GRAIN) { l.grain = std::max(0, std::min(100, l.grain + d * 5)); Glass::Grain() = l.grain; }
        else if (id == LF_SOURCE) LookCycleSource(d);
        else if (id == LF_SIZE) l.fontSize = std::max(85, std::min(120, l.fontSize + d * 5));
        else if (id == LF_CURSOR) { l.cursorButton = !l.cursorButton; LookApply(); LookSave(); }
        else if (id == LF_PIC) LookNextPicture(d);
        else if (id == LF_PRESET)
        {
            int n = 0;
            const char* const* list = LookPresets(e, n);
            int i = -1;
            for (int k = 0; k < n; k++) if (l.col[e] == list[k]) i = k;
            i = i < 0 ? (d > 0 ? 0 : n - 1) : (i + d + n) % n;
            l.col[e] = list[i];
            l.pic[e].clear();
            if (e == 5) l.backdrop = "colour";
            if (l.col[e] == "clear" || l.col[e] == "dark") l.op[e] = -1;
            LookSyncHsv();
            LookApply();
        }
        else if (id == LF_HUE) { l.h = std::fmod(l.h + d * 6.0f + 360.0f, 360.0f); if (l.s < 0.1f) l.s = 0.75f; if (l.v < 0.2f) l.v = 1; LookSetHsv(); } // (white or black: a colour shown as the hue moves)
        else if (id == LF_SAT) { l.s = std::max(0.0f, std::min(1.0f, l.s + d * 0.05f)); LookSetHsv(); }
        else if (id == LF_VAL) { l.v = std::max(0.0f, std::min(1.0f, l.v + d * 0.05f)); LookSetHsv(); }
        else { l.op[e] = std::max(0, std::min(100, LookOpacity(e) + d * 5)); LookApply(); }
    }
    Hub().version++;
}
// A tap or click on an open row's page: 1 done, 3 a picture wanted from the system's picker, 0 not on it.
inline int LookTap(int x, int y)
{
    LookPrefs& l = Look();
    if (Focus().page == SIDE_LAUNCHER && LauncherMode() == 0 && Focus().prow == 0)
    {
        for (const auto& hit : LayoutPreviews())
            if (x >= hit.x && x < hit.x + hit.w && y >= hit.y && y < hit.y + hit.h)
            {
                l.layout = hit.layout; Focus().banner = 0; Focus().side = -1;
                LookSave(); Hub().version++; return 1;
            }
        return 0;
    }
    if (Focus().page != SIDE_LAUNCHER || l.edit < 0) return 0;
    const int e = l.edit;
    const std::vector<int> fl = LookFieldList(e);
    bool hit = false;
    const float dx = (float)(x - l.wx), dy = (float)(y - l.wy), d = std::sqrt(dx * dx + dy * dy);
    if (e != 6 && l.wr > 0 && d <= l.wr + 4)
    {
        l.h = std::fmod(std::atan2(dy, dx) * 57.29578f + 360.0f, 360.0f);
        l.s = std::max(0.0f, std::min(1.0f, d / l.wr));
        if (l.v < 0.2f) l.v = 1;
        l.field = LookFieldIndex(e, LF_HUE);
        LookSetHsv();
        hit = true;
    }
    for (int k = 0; k < (int)fl.size() && !hit; k++)
    {
        const int id = fl[k];
        if (id == LF_PRESET) continue;
        if (id == LF_SOURCE)
        {
            if (y >= l.fy[k] && y < l.fy[k] + l.fh && x >= l.rowX && x < l.rowX + l.rowW) { l.field = k; LookCycleSource(1); hit = true; }
            continue;
        }
        if (id == LF_CURSOR)
        {
            if (y >= l.fy[k] && y < l.fy[k] + l.fh && x >= l.rowX && x < l.rowX + l.rowW) { l.field = k; LookMove(2); hit = true; }
            continue;
        }
        if (id == LF_PIC)
        {
            if (y >= l.fy[k] && y < l.fy[k] + l.fh && x >= l.rowX && x < l.rowX + l.rowW)
            {
                l.field = k;
#ifdef __SWITCH__
                LookNextPicture(1);
                hit = true;
#else
                Hub().version++;
                return 3;
#endif
            }
            continue;
        }
        if (l.fw > 0 && x >= l.fx[k] - 6 && x < l.fx[k] + l.fw + 6 && y >= l.fy[k] && y < l.fy[k] + l.fh)
        {
            const float t = std::max(0.0f, std::min(1.0f, (float)(x - l.fx[k]) / l.fw));
            l.field = k;
            if (id == LF_BLUR) l.blur = (int)(t * 100 + 0.5f);
            else if (id == LF_DIM) l.dim = (int)(t * 100 + 0.5f);
            else if (id == LF_GRAIN) { l.grain = (int)(t * 100 + 0.5f); Glass::Grain() = l.grain; }
            else if (id == LF_SIZE) l.fontSize = 85 + (int)(t * 35 + 0.5f);
            else if (id == LF_HUE) { l.h = t * 359.0f; if (l.s < 0.1f) l.s = 0.75f; if (l.v < 0.2f) l.v = 1; LookSetHsv(); }
            else if (id == LF_SAT) { l.s = t; LookSetHsv(); }
            else if (id == LF_VAL) { l.v = t; LookSetHsv(); }
            else { l.op[e] = (int)(t * 100 + 0.5f); LookApply(); }
            hit = true;
        }
    }
    for (int i = 0; i < l.pn && !hit; i++)
        if (x >= l.px[i] - 3 && x < l.px[i] + l.pd + 3 && y >= l.py - 3 && y < l.py + l.pd + 3)
        {
            int n = 0;
            l.col[e] = LookPresets(e, n)[i];
            l.pic[e].clear();
            if (e == 5) l.backdrop = "colour";
            if (l.col[e] == "clear" || l.col[e] == "dark") l.op[e] = -1;
            LookSyncHsv();
            LookApply();
            l.field = LookFieldIndex(e, LF_PRESET);
            hit = true;
        }
    if (hit) { LookSave(); Hub().version++; }
    return hit ? 1 : 0;
}
// The wheel and the bars followed by a mouse or a finger held down and dragged: pressed on one (-1 the wheel, k a bar), the value
// follows the pointer along it (up or down off it does not matter) until it is let go.
inline int& LookDragK() { static int k = -2; return k; }
inline void LookDragApply(int x, int y)
{
    LookPrefs& l = Look();
    const int k = LookDragK(), e = l.edit;
    if (k == -2 || e < 0) return;
    if (k == -1)
    {
        const float dx = (float)(x - l.wx), dy = (float)(y - l.wy), d = std::sqrt(dx * dx + dy * dy);
        l.h = std::fmod(std::atan2(dy, dx) * 57.29578f + 360.0f, 360.0f);
        l.s = std::max(0.0f, std::min(1.0f, d / std::max(1, l.wr)));
        if (l.v < 0.2f) l.v = 1;
        l.field = LookFieldIndex(e, LF_HUE);
        LookSetHsv();
        return;
    }
    const std::vector<int> fl = LookFieldList(e);
    if (k >= (int)fl.size() || l.fw <= 0) return;
    const int id = fl[k];
    const float t = std::max(0.0f, std::min(1.0f, (float)(x - l.fx[k]) / l.fw));
    l.field = k;
    if (id == LF_BLUR) l.blur = (int)(t * 100 + 0.5f);
    else if (id == LF_DIM) l.dim = (int)(t * 100 + 0.5f);
    else if (id == LF_GRAIN) { l.grain = (int)(t * 100 + 0.5f); Glass::Grain() = l.grain; }
    else if (id == LF_SIZE) l.fontSize = 85 + (int)(t * 35 + 0.5f);
    else if (id == LF_HUE) { l.h = t * 359.0f; if (l.s < 0.1f) l.s = 0.75f; if (l.v < 0.2f) l.v = 1; LookSetHsv(); }
    else if (id == LF_SAT) { l.s = t; LookSetHsv(); }
    else if (id == LF_VAL) { l.v = t; LookSetHsv(); }
    else if (id == LF_OP) { l.op[e] = (int)(t * 100 + 0.5f); LookApply(); }
}
inline bool LookDragBegin(int x, int y)
{
    LookPrefs& l = Look();
    LookDragK() = -2;
    if (Focus().page != SIDE_LAUNCHER || l.edit < 0) return false;
    const int e = l.edit;
    const float dx = (float)(x - l.wx), dy = (float)(y - l.wy);
    if (e != 6 && l.wr > 0 && std::sqrt(dx * dx + dy * dy) <= l.wr + 4) LookDragK() = -1;
    else
    {
        const std::vector<int> fl = LookFieldList(e);
        for (int k = 0; k < (int)fl.size() && LookDragK() == -2; k++)
        {
            const int id = fl[k];
            if (id == LF_PRESET || id == LF_SOURCE || id == LF_PIC) continue;
            if (l.fw > 0 && x >= l.fx[k] - 6 && x < l.fx[k] + l.fw + 6 && y >= l.fy[k] && y < l.fy[k] + l.fh) LookDragK() = k;
        }
    }
    if (LookDragK() == -2) return false;
    LookDragApply(x, y);
    Hub().version++;
    return true;
}
inline void LookDragMove(int x, int y) { if (LookDragK() != -2) { LookDragApply(x, y); Hub().version++; } }
inline void LookDragEnd() { LookDragK() = -2; LookSave(); Hub().version++; }
// Saving a theme: Home drawn once (ThemeCaptureDone, as the launcher is drawn), its picture taken, then
// the theme written and the page as it was.
inline Glass::Picture* ThemePreview(const std::string& name)
{
    ThemeState& t = Themes();
    auto it = t.previews.find(name);
    if (it == t.previews.end())
    {
        Glass::Picture p;
        std::map<std::string, std::vector<unsigned char>> files;
        if (ThemePack::ZipRead(ThemesDir() + "/" + name + ".zip", files, 4u << 20, "preview.jpg") && files.count("preview.jpg")) LookDecodePicture(files["preview.jpg"], p);
        it = t.previews.emplace(name, p).first;
    }
    return it->second.px.empty() ? nullptr : &it->second;
}
inline void ThemeSaveStart(const std::string& raw)
{
    ThemeState& t = Themes();
    const std::string name = ThemeSafe(raw);
    if (name.empty()) { t.msg = "TYPE A NAME FOR THE THEME (LETTERS AND NUMBERS)"; return; }
    t.pending = true;
    t.name = name;
    t.kept = Focus();
    t.kept.dialog = 0;
    GlassFocus& F = Focus();
    F.page = -1; F.side = -1; F.popup = -1; F.dialog = 0; F.banner = 0;
    t.msg = "SAVING...";
    Hub().version++;
}
inline bool LookWriteTheme(const std::string& name, const std::vector<unsigned char>& preview)
{
    const LookPrefs& l = Look();
    ThemeState& ts = Themes();
    std::vector<std::pair<std::string, std::vector<unsigned char>>> files;
    std::string picName[6];
    size_t total = preview.size();
    for (int i = 1; i < 6; i++)
    {
        if (l.pic[i].empty()) continue;
        std::vector<unsigned char> data;
        if (!ReadFile(LookPicPath(l.pic[i]), data) || !ImageAllowed(data.data(), data.size())) { ts.msg = "COULD NOT READ THE PICTURE " + l.pic[i]; return false; }
        std::string base = std::filesystem::path(l.pic[i]).filename().string();
        int w = 0, h = 0, n = 0;
        if (stbi_info_from_memory(data.data(), (int)data.size(), &w, &h, &n) && std::max(w, h) > 3840)
        {
            // (Over 4K: made smaller, as a JPEG.)
            Glass::Picture p;
            if (!LookDecodePicture(data, p) || !ThemePack::Jpeg(p.px.data(), p.w, p.h, 90, data)) { ts.msg = "COULD NOT MAKE " + base + " SMALLER"; return false; }
            base = base.substr(0, base.find_last_of('.')) + ".jpg";
        }
        picName[i] = "pictures/" + std::string(kLookKeys[i]) + "-" + base;
        total += data.size();
        files.push_back({ picName[i], std::move(data) });
    }
    std::string text = "fulldex_theme\t1\nname\t" + name + "\n";
    for (int i = 0; i < 6; i++)
    {
        text += std::string(kLookKeys[i]) + "\t" + l.col[i] + "\n";
        if (i > 0) text += std::string(kLookKeys[i]) + "_pic\t" + picName[i] + "\n";
        if (LookHasOp(i)) text += std::string(kLookKeys[i]) + "_op\t" + std::to_string(l.op[i]) + "\n";
    }
    text += "text\t" + l.col[10] + "\n";
    text += "background\t" + l.backdrop + "\nsoftness\t" + std::to_string(l.blur) + "\ndim\t" + std::to_string(l.dim) + "\ngrain\t" + std::to_string(l.grain) + "\nbadges\t" + (l.badges ? "on" : "off") + "\n";
    text += "font\t" + l.font + "\nfont_size\t" + std::to_string(l.fontSize) + "\n";
    text += std::string("cursor_button\t") + (l.cursorButton ? "on" : "off") + "\n";
    total += text.size();
    if (total > (20u << 20)) { ts.msg = "OVER 20 MB: USE SMALLER PICTURES"; return false; }
    files.insert(files.begin(), { "theme.txt", std::vector<unsigned char>(text.begin(), text.end()) });
    files.push_back({ "preview.jpg", preview });
    std::error_code ec;
    std::filesystem::create_directories(ThemesDir(), ec);
    if (!ThemePack::ZipWrite(ThemesDir() + "/" + name + ".zip", files)) { ts.msg = "COULD NOT SAVE THE THEME"; return false; }
    ts.previews.erase(name);
    ThemesRefresh();
    return true;
}
// (At the end of drawing: Home just drawn for a theme being saved.)
inline void ThemeCaptureDone(Canvas& c)
{
    ThemeState& t = Themes();
    if (!t.pending || Focus().page >= 0 || c.w <= 0 || c.h <= 0) return;
    const int pw = 960, ph = 540;
    std::vector<uint32_t> px((size_t)pw * ph);
    for (int y = 0; y < ph; y++)
        for (int x = 0; x < pw; x++) px[(size_t)y * pw + x] = c.px[(size_t)(y * c.h / ph) * c.w + x * c.w / pw];
    std::vector<unsigned char> jpg;
    ThemePack::Jpeg(px.data(), pw, ph, 88, jpg);
    t.pending = false;
    Focus() = t.kept;
    LauncherMode() = 2;
    if (LookWriteTheme(t.name, jpg))
    {
        // (Saved: the Theme Library, on it.)
        t.msg = "SAVED: " + t.name;
        LauncherMode() = 3;
        Focus().prow = 0;
        for (int i = 0; i < (int)t.list.size(); i++) if (t.list[(size_t)i] == t.name) Focus().prow = i;
    }
    Hub().version++;
}
inline bool LookApplyTheme(const std::string& name, bool gallery = false)
{
    ThemeState& ts = Themes();
    std::map<std::string, std::vector<unsigned char>> files;
    if (!ThemePack::ZipRead(ThemesDir() + "/" + name + ".zip", files, 48u << 20) || !files.count("theme.txt")) { ts.msg = "COULD NOT READ THAT THEME"; return false; }
    std::map<std::string, std::string> p;
    {
        const std::vector<unsigned char>& t = files["theme.txt"];
        const std::string all(t.begin(), t.end());
        for (size_t at = 0; at < all.size();)
        {
            size_t end = all.find('\n', at);
            if (end == std::string::npos) end = all.size();
            std::string l = all.substr(at, end - at);
            while (!l.empty() && l.back() == '\r') l.pop_back();
            const size_t tb = l.find('\t');
            if (tb != std::string::npos && tb > 0) p[l.substr(0, tb)] = l.substr(tb + 1);
            at = end + 1;
        }
    }
    if (!p.count("fulldex_theme")) { ts.msg = "THAT IS NOT A FULLDEX THEME"; return false; }
    std::error_code ec;
    std::filesystem::create_directories(HubDir() + "/launcher-pictures", ec);
    for (int i = 1; i < 6; i++)
    {
        const auto it = p.find(std::string(kLookKeys[i]) + "_pic");
        if (it == p.end() || it->second.empty()) continue;
        const auto f = files.find(it->second);
        if (f == files.end() || !ImageAllowed(f->second.data(), f->second.size())) { it->second.clear(); continue; }
        std::vector<unsigned char> bytes = f->second;
        std::string fname = std::filesystem::path(it->second).filename().string();
        LookShrinkPicture(bytes, fname, kSmallPicture); // (A phone or a Switch does not need a 4K picture: smaller, the shape kept.)
        const std::string mine = name + " - " + fname;
        FILE* o = fopen((HubDir() + "/launcher-pictures/" + mine).c_str(), "wb");
        if (!o) { it->second.clear(); continue; }
        const bool ok = fwrite(bytes.data(), 1, bytes.size(), o) == bytes.size();
        fclose(o);
        it->second = ok ? mine : std::string();
    }
    if (gallery)
    {
        // (From the Gallery: the built-in fonts only, and not kept until the player says so.)
        bool known = false;
        for (int i = 0; i < kLauncherFontCount; i++) if (p["font"] == kLauncherFonts[i].id) known = true;
        if (!known) p["font"] = "noto-sans";
    }
    LookFrom(p);
    if (!gallery) LookSave();
    Hub().version++;
    ts.msg = "THEME ON: " + name;
    return true;
}
// ---------------------------------------------------------------- the Theme Gallery
inline std::string GalleryEnc(const std::string& s)
{
    static const char* hx = "0123456789ABCDEF";
    std::string o;
    for (const unsigned char ch : s)
    {
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') o += (char)ch;
        else { o += '%'; o += hx[ch >> 4]; o += hx[ch & 15]; }
    }
    return o;
}
// "k7qm-x2rd" is K7QMX2RD; "" when it is not 8 letters and numbers.
inline std::string GalleryCode(const std::string& raw)
{
    std::string c;
    for (const char ch : raw) if (isalnum((unsigned char)ch)) c += (char)toupper((unsigned char)ch);
    return c.size() == 8 ? c : std::string();
}
inline bool GalleryHttp(const std::string& method, const std::string& path, const std::string& body, const std::string& type, int& status, std::string& reply)
{
    std::string base = TradeRelay();
    while (!base.empty() && base.back() == '/') base.pop_back();
    status = 0;
    if (base.empty()) return false;
    Achievements::HttpCall c;
    c.method = method;
    c.url = base + path;
    c.timeout = 30;
    c.limit = 22u << 20;
    if (!type.empty()) c.headers.push_back("Content-Type: " + type);
    c.body = body;
    std::string err;
    return Achievements::HttpRequest(c, status, reply, err);
}
// The relay's answers are small JSON; these read a string or a number of a key between two places.
inline bool GalleryJStr(const std::string& j, size_t from, size_t to, const char* key, std::string& out)
{
    const std::string k = std::string("\"") + key + "\":\"";
    size_t at = j.find(k, from);
    if (at == std::string::npos || at >= to) return false;
    at += k.size();
    out.clear();
    for (; at < to && j[at] != '"'; at++)
    {
        if (j[at] != '\\' || at + 1 >= to) { out += j[at]; continue; }
        const char e = j[++at];
        if (e == 'u' && at + 4 < to)
        {
            const unsigned cp = (unsigned)strtoul(j.substr(at + 1, 4).c_str(), nullptr, 16);
            at += 4;
            if (cp < 0x80) out += (char)cp;
            else if (cp < 0x800) { out += (char)(0xC0 | cp >> 6); out += (char)(0x80 | (cp & 63)); }
            else if (cp >= 0xD800 && cp < 0xE000) out += '?';
            else { out += (char)(0xE0 | cp >> 12); out += (char)(0x80 | (cp >> 6 & 63)); out += (char)(0x80 | (cp & 63)); }
        }
        else out += (e == 'n' || e == 't' || e == 'r') ? ' ' : e;
    }
    return true;
}
inline long GalleryJNum(const std::string& j, size_t from, size_t to, const char* key)
{
    const std::string k = std::string("\"") + key + "\":";
    const size_t at = j.find(k, from);
    return at == std::string::npos || at >= to ? 0 : atol(j.c_str() + at + k.size());
}
inline void GalleryParse(const std::string& j, std::deque<GalleryItem>& out, std::string& next)
{
    next.clear();
    const std::string tag = "{\"code\":\"";
    std::vector<size_t> at;
    for (size_t p = j.find(tag); p != std::string::npos; p = j.find(tag, p + 1)) at.push_back(p);
    for (size_t i = 0; i < at.size(); i++)
    {
        const size_t to = i + 1 < at.size() ? at[i + 1] : j.size();
        GalleryItem it;
        if (!GalleryJStr(j, at[i], to, "code", it.code) || GalleryCode(it.code).empty()) continue;
        GalleryJStr(j, at[i], to, "name", it.name);
        GalleryJStr(j, at[i], to, "author", it.author);
        it.size = (int)GalleryJNum(j, at[i], to, "size");
        out.push_back(std::move(it));
    }
    const size_t cp = j.rfind("\"cursor\":\"");
    if (cp != std::string::npos) GalleryJStr(j, cp, j.size(), "cursor", next);
}
inline std::string GalleryMsg() { GalleryState& g = Gallery(); std::lock_guard<std::mutex> l(g.m); return g.msg; }
// A page of the list (fresh: from the start), then its previews one at a time as they arrive.
// A job on a thread of its own, let go. On the Switch with a 2 MB stack: the default one is too small for the
// secure connection, and the program ended when the gallery asked the relay for its list.
inline void GalleryBg(std::function<void()> job)
{
#ifdef __SWITCH__
    std::function<void()>* p = new std::function<void()>(std::move(job));
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 2u << 20);
    pthread_t t;
    if (pthread_create(&t, &a, [](void* q) -> void* { std::function<void()>* f = (std::function<void()>*)q; (*f)(); delete f; return nullptr; }, p) == 0) pthread_detach(t);
    else { (*p)(); delete p; }
    pthread_attr_destroy(&a);
#else
    std::thread(std::move(job)).detach();
#endif
}
inline void GalleryLoad(bool fresh)
{
    GalleryState& g = Gallery();
    std::string cursor;
    {
        std::lock_guard<std::mutex> l(g.m);
        if (g.busy) return;
        g.busy = true;
        if (fresh) { g.items.clear(); g.cursor.clear(); g.more = false; g.loaded = false; g.tried = true; }
        cursor = g.cursor;
        if (g.msg.empty()) g.msg = "LOADING...";
    }
    Hub().version++;
    GalleryBg([cursor]() {
        GalleryState& g = Gallery();
        int status = 0;
        std::string reply;
        const bool ok = GalleryHttp("GET", "/v2/themes?limit=24" + (cursor.empty() ? std::string() : "&cursor=" + GalleryEnc(cursor)), "", "", status, reply);
        size_t first = 0;
        {
            std::lock_guard<std::mutex> l(g.m);
            first = g.items.size();
            if (!ok || status != 200)
            {
                g.more = false;
                if (g.msg == "LOADING..." || g.msg.empty()) g.msg = TradeRelay().empty() ? "NO GALLERY ADDRESS IS SET UP IN THIS BUILD" : "COULD NOT REACH THE GALLERY";
            }
            else
            {
                std::deque<GalleryItem> got;
                std::string next;
                GalleryParse(reply, got, next);
                for (GalleryItem& it : got) g.items.push_back(std::move(it));
                g.cursor = next;
                g.more = !next.empty();
                g.loaded = true;
                if (g.msg == "LOADING..." || g.msg.empty()) g.msg = g.items.empty() ? "NOTHING SHARED YET. BE THE FIRST: SHARE A THEME" : "";
            }
        }
        Hub().version++;
        for (size_t i = first;; i++)
        {
            std::string code;
            {
                std::lock_guard<std::mutex> l(g.m);
                if (i >= g.items.size()) break;
                code = GalleryCode(g.items[i].code);
            }
            std::string body;
            int st = 0;
            if (GalleryHttp("GET", "/v2/themes/" + code + "/preview", "", "", st, body) && st == 200 && body.size() < (700u << 10))
            {
                std::lock_guard<std::mutex> l(g.m);
                if (i < g.items.size()) g.items[i].jpg.assign(body.begin(), body.end());
            }
            Hub().version++;
        }
        {
            std::lock_guard<std::mutex> l(g.m);
            g.busy = false;
        }
        Hub().version++;
    });
}
inline void GalleryOpen()
{
    GalleryState& g = Gallery();
    std::lock_guard<std::mutex> l(g.m);
    if (g.busy) return;
    g.items.clear(); g.cursor.clear(); g.more = false; g.loaded = false; g.tried = false; g.msg.clear();
}
inline void GalleryCardInfo(int i, std::string& label, std::string& author)
{
    if (i == 0) { label = "Enter A Theme Code"; return; }
    if (i == 1) { label = "Share A Theme"; return; }
    GalleryState& g = Gallery();
    std::lock_guard<std::mutex> l(g.m);
    if (i - 2 < (int)g.items.size()) { label = g.items[(size_t)i - 2].name; author = g.items[(size_t)i - 2].author; }
}
inline const Glass::Picture* GalleryPreview(int k)
{
    GalleryState& g = Gallery();
    std::lock_guard<std::mutex> l(g.m);
    if (k < 0 || k >= (int)g.items.size()) return nullptr;
    GalleryItem& it = g.items[(size_t)k];
    if (!it.decoded && !it.jpg.empty())
    {
        it.decoded = true;
        if (!LookDecodePicture(it.jpg, it.pic)) it.pic = Glass::Picture();
        it.jpg.clear();
        it.jpg.shrink_to_fit();
    }
    return it.pic.px.empty() ? nullptr : &it.pic;
}
// A shared theme chosen: its menu (Try It).
inline void GalleryOpenCard(int k)
{
    GalleryState& g = Gallery();
    GlassFocus& F = Focus();
    std::string name, author, code;
    {
        std::lock_guard<std::mutex> l(g.m);
        if (k < 0 || k >= (int)g.items.size()) return;
        name = g.items[(size_t)k].name; author = g.items[(size_t)k].author; code = GalleryCode(g.items[(size_t)k].code);
    }
    F.dialog = 15;
    F.drow = 0;
    F.dTitle = name;
    F.dMsg = "By " + author + ". Try It puts it on your launcher; then you keep it or go back.";
    F.dItems = { "Try It", "Close" };
    F.dPaths = { "#gtry:" + code, "#close" };
}
// Share A Theme: pick one of My Themes.
inline void GalleryShareMenu()
{
    ThemeState& ts = Themes();
    GlassFocus& F = Focus();
    ThemesRefresh();
    if (ts.list.empty()) { ts.msg = "SAVE A THEME FIRST: LAUNCHER, MY THEMES, SAVE THIS LOOK"; Hub().version++; return; }
    F.dialog = 15;
    F.drow = 0;
    F.dTitle = "Share A Theme";
    F.dMsg = "Pick one of your themes. It is looked at before it shows for others. Share only pictures that are yours.";
    F.dItems.clear();
    F.dPaths.clear();
    for (size_t i = 0; i < ts.list.size() && i < 6; i++) { F.dItems.push_back(ts.list[i]); F.dPaths.push_back("#gshare:" + ts.list[i]); }
    F.dItems.push_back("Close");
    F.dPaths.push_back("#close");
}
inline std::set<std::string> GalleryPicList()
{
    std::set<std::string> s;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(HubDir() + "/launcher-pictures", ec)) s.insert(e.path().filename().string());
    return s;
}
// A theme by its code (from a card or typed): downloaded, checked, kept in the themes folder, and handed to GalleryPoll to try.
inline void GalleryDownload(const std::string& code)
{
    GalleryState& g = Gallery();
    {
        std::lock_guard<std::mutex> l(g.m);
        if (g.working) return;
        g.working = true;
        g.msg = "DOWNLOADING...";
    }
    Hub().version++;
    GalleryBg([code]() {
        GalleryState& g = Gallery();
        int status = 0;
        std::string reply, msg, name;
        std::error_code ec;
        if (!GalleryHttp("GET", "/v2/themes/" + code, "", "", status, reply)) msg = TradeRelay().empty() ? "NO GALLERY ADDRESS IS SET UP IN THIS BUILD" : "COULD NOT REACH THE GALLERY";
        else if (status == 404)
        {
            std::string r2;
            int s2 = 0;
            msg = "THAT CODE DOES NOT WORK";
            if (GalleryHttp("GET", "/v2/themes/" + code + "/status", "", "", s2, r2) && s2 == 200)
            {
                if (r2.find("pending") != std::string::npos) msg = "THAT THEME IS WAITING TO BE APPROVED. TRY AGAIN LATER.";
                else if (r2.find("rejected") != std::string::npos) msg = "THAT THEME WAS NOT APPROVED";
                else if (r2.find("removed") != std::string::npos) msg = "THAT THEME WAS TAKEN DOWN";
            }
        }
        else if (status != 200 || reply.size() < 100 || reply.size() > (20u << 20)) msg = "THE GALLERY GAVE AN ANSWER IT SHOULD NOT";
        else
        {
            std::filesystem::create_directories(ThemesDir(), ec);
            const std::string tmp = ThemesDir() + "/gallery-download.tmp";
            bool wrote = false;
            if (FILE* f = fopen(tmp.c_str(), "wb")) { wrote = fwrite(reply.data(), 1, reply.size(), f) == reply.size(); fclose(f); }
            std::map<std::string, std::vector<unsigned char>> files;
            if (!wrote || !ThemePack::ZipRead(tmp, files, 48u << 20) || !files.count("theme.txt")) msg = "THAT IS NOT A FULLDEX THEME";
            else
            {
                const std::string t(files["theme.txt"].begin(), files["theme.txt"].end());
                std::string raw = "Gallery Theme";
                const size_t at = t.find("\nname\t");
                if (at != std::string::npos)
                {
                    const size_t end = t.find('\n', at + 1);
                    raw = t.substr(at + 6, end == std::string::npos ? std::string::npos : end - (at + 6));
                    while (!raw.empty() && raw.back() == '\r') raw.pop_back();
                }
                std::string nm = ThemeSafe(raw);
                if (nm.empty()) nm = "Gallery Theme";
                std::string to = nm;
                for (int n = 2; std::filesystem::exists(ThemesDir() + "/" + to + ".zip", ec); n++) to = nm + " " + std::to_string(n);
                std::filesystem::rename(tmp, ThemesDir() + "/" + to + ".zip", ec);
                if (ec) msg = "COULD NOT SAVE IT"; else name = to;
            }
            if (name.empty()) std::filesystem::remove(tmp, ec);
        }
        {
            std::lock_guard<std::mutex> l(g.m);
            g.working = false;
            g.msg = name.empty() ? msg : "TRYING: " + name;
            if (!name.empty()) g.ready = name;
        }
        Hub().version++;
    });
}
inline void GalleryEnterCode(const std::string& raw)
{
    const std::string code = GalleryCode(raw);
    if (code.empty()) { Themes().msg = "A CODE IS 8 LETTERS AND NUMBERS, LIKE K7QM-X2RD"; Hub().version++; return; }
    Themes().msg.clear();
    GalleryDownload(code);
}
inline void GalleryTryDialog()
{
    GalleryState& g = Gallery();
    GlassFocus& F = Focus();
    F.dialog = 15;
    F.drow = 0;
    F.dTitle = "Keep This Theme?";
    F.dMsg = g.tryName + " is on. Keep it in My Themes, or go back to your own look.";
    F.dItems = { "Keep", "Go Back" };
    F.dPaths = { "#gkeep", "#gback" };
}
inline void GalleryKeep()
{
    GalleryState& g = Gallery();
    if (!g.trying) return;
    g.trying = false;
    LookSave();
    ThemesRefresh();
    Themes().msg = "KEPT IN MY THEMES: " + g.tryName;
    Hub().version++;
}
// Go Back: the look as it was, and nothing of the theme left behind.
inline void GalleryBack()
{
    GalleryState& g = Gallery();
    ThemeState& ts = Themes();
    if (!g.trying) return;
    g.trying = false;
    Look() = g.before;
    LookApply();
    std::error_code ec;
    std::filesystem::remove(ThemesDir() + "/" + g.tryName + ".zip", ec);
    ts.previews.erase(g.tryName);
    std::vector<std::string> added;
    for (const auto& e : std::filesystem::directory_iterator(HubDir() + "/launcher-pictures", ec))
        if (!g.picsBefore.count(e.path().filename().string())) added.push_back(e.path().string());
    for (const std::string& p : added) std::filesystem::remove(p, ec);
    ThemesRefresh();
    ts.msg = "BACK TO YOUR OWN LOOK";
    Hub().version++;
}
// Each time the Gallery is drawn: first load, the next page near the end, and a downloaded theme to try.
inline void GalleryPoll(int prow)
{
    GalleryState& g = Gallery();
    GlassFocus& F = Focus();
    std::string ready;
    bool load = false, more = false;
    {
        std::lock_guard<std::mutex> l(g.m);
        ready.swap(g.ready);
        load = !g.tried && !g.busy;
        more = g.more && !g.busy;
    }
    if (!ready.empty())
    {
        LookLoad();
        g.before = Look();
        g.picsBefore = GalleryPicList();
        g.tryName = ready;
        if (LookApplyTheme(ready, true)) g.trying = true;
        else
        {
            std::error_code ec;
            std::filesystem::remove(ThemesDir() + "/" + ready + ".zip", ec);
            ThemesRefresh();
        }
    }
    if (g.trying && (F.dialog != 15 || F.dTitle != "Keep This Theme?")) GalleryTryDialog();
    if (load) GalleryLoad(true);
    else if (more && prow + 4 >= GalleryCards()) GalleryLoad(false);
}
// Share: one of My Themes sent up. It waits for approval; the code is shown now and kept in gallery-shared.txt.
inline void GalleryShare(const std::string& name)
{
    GalleryState& g = Gallery();
    ThemeState& ts = Themes();
    std::string body;
    if (FILE* f = fopen((ThemesDir() + "/" + name + ".zip").c_str(), "rb"))
    {
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0 && body.size() <= (20u << 20)) body.append(buf, n);
        fclose(f);
    }
    if (body.empty()) { ts.msg = "COULD NOT READ THAT THEME"; Hub().version++; return; }
    if (body.size() > (20u << 20)) { ts.msg = "OVER 20 MB: USE SMALLER PICTURES"; Hub().version++; return; }
    std::string author;
    if (FILE* f = fopen((HubDir() + "/fulldex-author.txt").c_str(), "rb")) { char b[64] = {}; if (fgets(b, sizeof b, f)) author = b; fclose(f); }
    while (!author.empty() && isspace((unsigned char)author.back())) author.pop_back();
    if (author.empty()) author = "Anonymous";
    {
        std::lock_guard<std::mutex> l(g.m);
        if (g.working) return;
        g.working = true;
        g.msg = "SHARING...";
    }
    Hub().version++;
    GalleryBg([name, body, author]() {
        GalleryState& g = Gallery();
        int status = 0;
        std::string reply, msg, code;
        const bool ok = GalleryHttp("POST", "/v2/themes?author=" + GalleryEnc(author), body, "application/zip", status, reply);
        if (!ok) msg = TradeRelay().empty() ? "NO GALLERY ADDRESS IS SET UP IN THIS BUILD" : "COULD NOT REACH THE GALLERY";
        else if (status == 201 && GalleryJStr(reply, 0, reply.size(), "code", code))
        {
            msg = "SHARED. YOUR CODE: " + code + ". IT SHOWS FOR EVERYONE ONCE APPROVED.";
            if (FILE* f = fopen((HubDir() + "/gallery-shared.txt").c_str(), "ab")) { const std::string line = code + "\t" + name + "\n"; fwrite(line.data(), 1, line.size(), f); fclose(f); }
        }
        else if (status == 429) msg = "TOO MANY SHARES FOR NOW. TRY AGAIN LATER.";
        else if (status == 413) msg = "OVER 20 MB: USE SMALLER PICTURES";
        else if (status == 400)
        {
            std::string why = reply.substr(0, reply.find('\n'));
            for (char& ch : why) ch = (char)toupper((unsigned char)ch);
            msg = "NOT ACCEPTED: " + why;
        }
        else msg = "THE GALLERY DID NOT TAKE IT (" + std::to_string(status) + ")";
        {
            std::lock_guard<std::mutex> l(g.m);
            g.working = false;
            g.msg = msg;
        }
        Hub().version++;
    });
}
// A .zip from the PC's picker, copied into the themes folder (named after its file).
inline bool ThemeImport(const std::string& path)
{
    ThemeState& ts = Themes();
    std::map<std::string, std::vector<unsigned char>> files;
    if (!ThemePack::ZipRead(path, files, 48u << 20, "theme.txt") || !files.count("theme.txt")) { ts.msg = "THAT IS NOT A FULLDEX THEME"; Hub().version++; return false; }
    std::string stem = ThemeSafe(std::filesystem::path(path).stem().string());
    if (stem.empty()) stem = "Theme";
    std::string to = stem;
    std::error_code ec;
    for (int n = 2; std::filesystem::exists(ThemesDir() + "/" + to + ".zip", ec); n++) to = stem + " " + std::to_string(n);
    std::filesystem::create_directories(ThemesDir(), ec);
    std::filesystem::copy_file(path, ThemesDir() + "/" + to + ".zip", ec);
    ThemesRefresh();
    ts.msg = ec ? "COULD NOT COPY THE THEME" : "IMPORTED: " + to;
    if (!ec) { LauncherMode() = 3; Focus().prow = 0; for (int i = 0; i < (int)ts.list.size(); i++) if (ts.list[(size_t)i] == to) Focus().prow = i; } // (the Theme Library, on it)
    Hub().version++;
    return !ec;
}
// A row of a theme's menu chosen: 1 when the menu stays (Delete asked first).
inline int ThemePick(const std::string& pick)
{
    ThemeState& ts = Themes();
    GlassFocus& F = Focus();
    if (pick.rfind("#apply:", 0) == 0) { LookApplyTheme(pick.substr(7)); return 0; }
    if (pick.rfind("#gtry:", 0) == 0) { GalleryDownload(pick.substr(6)); return 0; }
    if (pick == "#gkeep") { GalleryKeep(); return 0; }
    if (pick == "#gback") { GalleryBack(); return 0; }
    if (pick.rfind("#gshare:", 0) == 0) { GalleryShare(pick.substr(8)); return 0; }
    if (pick == "#lookdefault")
    {
        // (Everything as a new install has it; the pictures and themes stay where they are.)
        LookPrefs& l = Look();
        const LookPrefs d;
        for (int i = 0; i < 6; i++) { l.col[i] = d.col[i]; l.pic[i] = d.pic[i]; l.op[i] = d.op[i]; }
        l.col[10] = d.col[10]; l.backdrop = d.backdrop; l.blur = d.blur; l.dim = d.dim; l.grain = d.grain; l.badges = d.badges; l.font = d.font; l.fontSize = d.fontSize;
        LookApply();
        LookSave();
        ts.msg = "THE DEFAULT LOOK IS BACK";
        Hub().version++;
        return 0;
    }
    if (pick.rfind("#tdelask:", 0) == 0)
    {
        const std::string n = pick.substr(9);
        F.dMsg = "Delete " + n + "? Its file goes; the look you have on now stays.";
        F.dItems = { "Delete", "Cancel" };
        F.dPaths = { "#tdelete:" + n, "#close" };
        F.drow = 1;
        return 1;
    }
    if (pick.rfind("#tdelete:", 0) == 0)
    {
        const std::string n = pick.substr(9);
        std::error_code ec;
        std::filesystem::remove(ThemesDir() + "/" + n + ".zip", ec);
        ts.previews.erase(n);
        ThemesRefresh();
        ts.msg = ec ? "COULD NOT DELETE IT" : "DELETED: " + n;
        Hub().version++;
        return 0;
    }
    return 0;
}
// (The Fulldex Discord: people to trade with, help, news.)
static const char* const kDiscordUrl = "https://discord.gg/pnGeMYyJR";
static const char* const kReleasesUrl = "https://github.com/mamesj/pokemon-fulldex/releases";
inline int HubRows() { return Mover().open ? MoverRows() : HubMode() == 0 ? 5 : HubMode() == 4 ? DexRows() : HubMode() == 2 ? TradeRows() : HubMode() == 3 ? SavesRows() : HUB_FIRST + (int)HubGames().size(); }
inline std::vector<std::string> HubActions(int row)
{
    if (Mover().open) return MoverActions(row);
    if (HubMode() == 2) return TradeActions(row);
    if (HubMode() == 3) return SavesDs() ? std::vector<std::string>(10, "Slot") : std::vector<std::string>(); // (Manage Saves: its slots)
    if (HubMode() == 0 || HubMode() == 4) return {};
    HubLoad();
    const std::vector<HubGame> games = HubGames();
    HubState& h = Hub();
    std::lock_guard<std::mutex> l(h.m);
    if (row <= 0)
    {
        if (h.busy == 1) return { "Cancel" };
        if (h.busy) return {};
        if (!HubOnLocked(h)) return { "Sign In With Google" };
        return { "Sync All Now", "Sign Out" };
    }
    if (!HubOnLocked(h) || h.busy || row - HUB_FIRST >= (int)games.size()) return {};
    const auto it = h.results.find(games[row - HUB_FIRST].path);
    if (it != h.results.end() && it->second.outcome == CloudSaves::Outcome::Conflict) return { "Keep This Device's", "Keep The Cloud's" };
    return { "Sync Now" };
}
// (On the worker: each save in turn; resolve -1 a sync, 0 keep this device's, 1 the cloud's.)
inline void HubSyncJob(std::vector<std::string> paths, int resolve)
{
    HubState& h = Hub();
    CloudSaves::Settings s;
    std::string dir;
    { std::lock_guard<std::mutex> l(h.m); s = h.set; dir = h.dir; }
    std::unique_ptr<CloudSaves::Sync> sync = CloudSaves::FromSettings(s, dir + "/cloud-state.txt", HubDevice());
    int up = 0, down = 0, both = 0, failed = 0;
    for (const std::string& p : paths)
    {
        if (h.stop) break;
        CloudSaves::Result r;
        if (!sync) { r.outcome = CloudSaves::Outcome::Off; r.message = "CLOUD SAVES ARE OFF"; }
        else if (resolve >= 0) r = sync->Resolve(p, resolve == 1);
        else
        {
            r = sync->BeforePlay(p);
            if (r.outcome == CloudSaves::Outcome::InSync) r = sync->AfterPlay(p);
        }
        up += r.outcome == CloudSaves::Outcome::Pushed;
        down += r.outcome == CloudSaves::Outcome::Pulled;
        both += r.outcome == CloudSaves::Outcome::Conflict;
        failed += r.outcome == CloudSaves::Outcome::Failed;
        { std::lock_guard<std::mutex> l(h.m); h.results[p] = r; }
        h.version++;
    }
    std::lock_guard<std::mutex> l(h.m);
    h.busy = 0;
    HubReadState(h);
    if (!sync) h.msg = "Cloud saves are not set up.";
    else if (failed) h.msg = "Could not reach " + std::string(HubProvider(s.provider)) + ". The saves here are kept, and sync next time.";
    else
    {
        h.msg = "Synced.";
        if (up) h.msg += " " + std::to_string(up) + " sent up.";
        if (down) h.msg += " " + std::to_string(down) + " brought down.";
        if (both) h.msg += " " + std::to_string(both) + (both == 1 ? " changed on both: choose which to keep." : " changed on both: choose which to keep.");
        if (!up && !down && !both) h.msg += " Everything was already up to date.";
    }
    h.version++;
}
inline void HubSignInJob(std::vector<std::string> paths)
{
    HubState& h = Hub();
    CloudSaves::GoogleClient client;
    { std::lock_guard<std::mutex> l(h.m); client = h.set.google.id.empty() ? CloudSaves::BuiltInGoogleClient() : h.set.google; }
    auto done = [&](const std::string& m) {
        std::lock_guard<std::mutex> l(h.m);
        h.busy = 0; h.code.clear(); h.url.clear(); h.msg = m;
        h.version++;
    };
    if (client.id.empty()) { done("This build of Fulldex has no Google sign-in built in."); return; }
    CloudSaves::SignIn in;
    std::string err, refresh;
    if (!CloudSaves::GoogleSignInStart(client, in, err)) { done("Could not reach Google. Check the internet connection and try again."); return; }
    {
        std::lock_guard<std::mutex> l(h.m);
        h.code = in.userCode; h.url = in.verificationUrl; h.msg.clear();
        h.version++;
    }
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(in.expiresIn);
    while (!h.stop && !h.cancel && std::chrono::steady_clock::now() < end)
    {
        for (int i = 0; i < in.interval * 10 && !h.stop && !h.cancel; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (h.stop || h.cancel) break;
        const CloudSaves::SignInState st = CloudSaves::GoogleSignInPoll(client, in, refresh, err);
        if (st == CloudSaves::SignInState::Done)
        {
            {
                std::lock_guard<std::mutex> l(h.m);
                h.set.provider = "google";
                h.set.googleRefreshToken = refresh;
                if (h.set.deviceName.empty()) h.set.deviceName = HubDevice();
                CloudSaves::SaveSettings(h.dir + "/cloud.ini", h.set);
                h.busy = 2; h.code.clear(); h.url.clear(); h.msg = "Signed in. Syncing your saves...";
                h.version++;
            }
            HubSyncJob(paths, -1);
            return;
        }
        if (st == CloudSaves::SignInState::SlowDown) in.interval += 5;
        else if (st == CloudSaves::SignInState::Denied) { done("Sign-in was turned down on the phone."); return; }
        else if (st != CloudSaves::SignInState::Waiting) { done(st == CloudSaves::SignInState::Expired ? "The code ran out. Choose Sign In again for a new one." : "Sign-in did not work. Try again."); return; }
    }
    done(h.cancel ? "Sign-in cancelled." : "The code ran out. Choose Sign In again for a new one.");
}
inline void HubStart(std::function<void()> job)
{
    HubState& h = Hub();
    if (h.worker.joinable()) h.worker.join(); // (the last one has finished: busy was 0)
    h.worker = std::thread(std::move(job));
}
// A on one of the Hub's actions (row, its action). What to say, if anything ("" for nothing).
inline std::string HubDo(int row, int act)
{
    if (HubMode() == 4) return "";
    if (Mover().open) return MoverDo(row, act);
    if (HubMode() == 2) return TradeDo(row, act);
    if (HubMode() == 3) return SavesSlotDo(act);
    const std::vector<std::string> acts = HubActions(row);
    if (act < 0 || act >= (int)acts.size()) return "";
    const std::string a = acts[act];
    HubState& h = Hub();
    std::vector<std::string> paths;
    for (const HubGame& g : HubGames()) paths.push_back(g.path);
    if (a == "Cancel") { h.cancel = true; return ""; }
    std::lock_guard<std::mutex> l(h.m);
    if (h.busy) return "";
    if (a == "Sign In With Google")
    {
        h.busy = 1; h.cancel = false; h.msg = "Asking Google for a code..."; h.version++;
        HubStart([paths] { HubSignInJob(paths); });
        return "";
    }
    if (a == "Sign Out")
    {
        h.set.provider = "off";
        h.set.googleRefreshToken.clear();
        const bool ok = CloudSaves::SaveSettings(h.dir + "/cloud.ini", h.set);
        h.results.clear();
        h.msg = ok ? "Signed out. Your saves stay here, and in your Drive." : "Could not save the cloud settings.";
        h.version++;
        return "";
    }
    if (row >= HUB_FIRST && row - HUB_FIRST < (int)paths.size()) paths = { paths[row - HUB_FIRST] };
    const int resolve = a == "Keep The Cloud's" ? 1 : a == "Keep This Device's" ? 0 : -1;
    h.busy = 2; h.msg = "Syncing..."; h.version++;
    HubStart([paths, resolve] { HubSyncJob(paths, resolve); });
    return "";
}
// (A save's time as shown: "6 Nov, 17:04".)
inline std::string HubWhen(int64_t t)
{
    if (t <= 0) return "-";
    const time_t tt = (time_t)t;
    const struct tm* tm = std::localtime(&tt);
    if (!tm) return "-";
    char b[32];
    strftime(b, sizeof b, "%d %b, %H:%M", tm);
    return b[0] == '0' ? std::string(b + 1) : std::string(b);
}
inline std::string HubStatus(const CloudSaves::Result* r, bool on, uint32_t& col)
{
    col = 0xFFAEB7C4u;
    if (!r) return on ? "Not Synced Yet" : "Only Here";
    switch (r->outcome)
    {
    case CloudSaves::Outcome::InSync: col = 0xFF8FE3A2u; return "Up To Date";
    case CloudSaves::Outcome::Pushed: col = 0xFF8FE3A2u; return "Sent Up";
    case CloudSaves::Outcome::Pulled: col = 0xFF8CC8FFu; return "Brought Down";
    case CloudSaves::Outcome::NothingYet: return "No Save Yet";
    case CloudSaves::Outcome::Conflict: col = 0xFFF2C46Bu; return "Changed On Both";
    case CloudSaves::Outcome::Failed: col = 0xFFF2A08Au; return "Could Not Reach";
    default: return "Only Here";
    }
}
// The sidebar's pages (About; the others to come): page, the section shown (-1: the games); prow the
// row chosen in its list; pX..pRow where its rows were drawn (for taps).
// A QR code (support_qr.h) on white, with the quiet edge round it that phones need.
inline void DrawQr(Canvas& c, const char* const* rows, int n, int x, int y, int size)
{
#ifdef __ANDROID__
    (void)c; (void)rows; (void)n; (void)x; (void)y; (void)size;
    return; // (no QR codes on Android)
#endif
    const int q = n + 8;
    Glass::Round(c, x, y, size, size, size * 0.05f, 0xFFFFFFFFu);
    const float m = size / (float)q;
    for (int r = 0; r < n; r++)
        for (int k = 0; k < n; k++)
            if (rows[r][k] == '#')
            {
                const int x0 = x + (int)((k + 4) * m), y0 = y + (int)((r + 4) * m), x1 = x + (int)((k + 5) * m), y1 = y + (int)((r + 5) * m);
                for (int yy = std::max(0, y0); yy < std::min(c.h, y1); yy++)
                    for (int xx = std::max(0, x0); xx < std::min(c.w, x1); xx++) c.px[(size_t)yy * c.w + xx] = 0xFF000000u;
            }
}
// The one message after 20 hours of play (PlayTime::AskNow): a beer, if they like.
inline void OpenBeer()
{
    GlassFocus& F = Focus();
    F.dialog = 10;
    F.drow = 0;
    F.dTitle = "Enjoying Fulldex?";
    F.dMsg = "You've played for over 20 hours. Fulldex is free, made by one dude in his spare time: if it's given you some good hours, consider buying me a beer. "
#ifndef __ANDROID__
        "Scan the code with your phone. "
#endif
        "Cheers, James";
#if defined(__SWITCH__)
    F.dItems = { "Done" };
    F.dPaths = { "#close" };
#else
    F.dItems = { "Buy Me A Beer", "Maybe Later" };
    F.dPaths = { "#beer", "#close" };
#endif
}
// B on the launcher's games: asked first (Quit or Cancel), as the list is left from there.
inline void OpenQuitAsk()
{
    GlassFocus& F = Focus();
    F.dialog = 11;
    F.drow = 1;
    F.dTitle = "Quit Fulldex?";
    F.dMsg = "Are you sure you want to quit?";
    F.dItems = { "Quit", "Cancel" };
    F.dPaths = { "#quit", "#close" };
}
// Support on the About page (a PC): which page to open.
inline void OpenSupportAsk()
{
    GlassFocus& F = Focus();
    F.dialog = 12;
    F.drow = 0;
    F.dTitle = "Support Fulldex";
    F.dMsg = "Thank you! Which would you like to open in your browser?";
    F.dItems = { "Buy Me A Coffee", "PayPal", "Close" };
    F.dPaths = { "#open:https://buymeacoffee.com/mamesj", "#open:https://paypal.me/mamesj", "#close" };
}
#include "saves_page.h"
#include "national_dex_page.h"
inline std::string SavesTitle() { return LauncherText::Tr("Manage Saves") + "   \xC2\xB7   " + Saves().name; }
inline int PageRows(int page) { return page == SIDE_LAUNCHER ? LauncherRows() : page == SIDE_ABOUT ? 5 : page == SIDE_ROMS ? std::max(1, (int)RomList().size()) : page == SIDE_HUB ? HubRows() : 0; }
// The cards of the About page that open a page when tapped, as drawn (cleared each frame it is drawn).
struct AboutLink { int x, y, w, h; const char* url; };
inline std::vector<AboutLink>& AboutLinks() { static std::vector<AboutLink> v; return v; }
inline const char* AboutLinkAt(int mx, int my)
{
    for (const AboutLink& l : AboutLinks())
        if (mx >= l.x && mx < l.x + l.w && my >= l.y && my < l.y + l.h) return l.url;
    return nullptr;
}
inline std::string AboutUrl(int row)
{
    return row == 0 ? "https://buymeacoffee.com/mamesj" : row == 2 ? "https://github.com/mamesj/pokemon-fulldex/issues" : row == 4 ? "https://github.com/mamesj/pokemon-fulldex" : row == 3 ? "https://github.com/mamesj/pokemon-fulldex/blob/main/frontend/fonts/FONT-LICENSES.txt" : "";
}
// The page a row of a page opens (none: "").
inline std::string PageUrl(int page, int row)
{
    if (page == SIDE_HUB) return HubTop() && row == 3 ? kDiscordUrl : "";
    return page == SIDE_ABOUT ? AboutUrl(row) : "";
}
inline const char* PlatformLabel()
{
#if defined(__SWITCH__)
    return "Nintendo Switch";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__APPLE__)
    return "Apple";
#elif defined(_WIN32)
    return "Windows";
#else
    return "Linux";
#endif
}
// The arrows while a page is open: up and down in the sidebar or the page's list, right into the
// list, left back to the sidebar. (False: no page open.)
inline bool PageMove(int dir)
{
    GlassFocus& F = Focus();
    if (F.page < 0) return false;
    F.freeScroll = false; // Keyboard/controller navigation follows the chosen item again.
    if (TilesOn() && F.side >= 0) F.side = -1; // (a window over the tiles: no sidebar to move to)
    if (F.side >= 0)
    {
        if (dir == 3) F.side = std::max(0, F.side - 1);
        else if (dir == 4) F.side = std::min((int)SIDES - 1, F.side + 1);
        else if (dir == 2) F.side = -1;
        return true;
    }
    if (F.page == SIDE_LAUNCHER && Look().edit >= 0) { LookMove(dir); return true; } // (the colour wheel)
    if (F.page == SIDE_LAUNCHER && LauncherMode() >= 3)
    {
        // (the Theme Library's and the fonts' grids: four across; left from the first of a row to the sidebar.)
        const int cols = std::max(1, Themes().cols), n = LauncherRows();
        if (dir == 3) { if (F.prow >= cols) F.prow -= cols; }
        else if (dir == 4) { if (F.prow + cols < n) F.prow += cols; else if (F.prow / cols < (n - 1) / cols) F.prow = n - 1; }
        else if (dir == 1) { if (F.prow % cols == 0) F.side = F.page; else F.prow--; }
        else if (dir == 2) { if ((F.prow % cols < cols - 1 || (F.prow + 1) % Themes().perPage == 0) && F.prow + 1 < n) F.prow++; } // (and on to the next page)
        return true;
    }
    if (F.page == SIDE_ABOUT && PortraitNow())
    {
        if (F.pact >= 0)
        {
            if (dir == 1) F.pact = -1;
            else if (dir == 3 || dir == 4)
                F.dScroll = std::clamp(F.dScroll + (dir == 3 ? -1 : 1) * std::max(24, F.abRow), 0, F.dMax);
            return true;
        }
        if (dir == 2 && F.dMax > 0) { F.pact = 0; return true; }
    }
    if (F.page == SIDE_ROMS || F.page == SIDE_HUB)
    {
        // (The list, and right of it the chosen row's actions: a ROM's, or the Hub's.)
        if (F.page == SIDE_HUB && HubMode() == 4 && !Mover().open && DexMove(dir)) return true;
        if (F.page == SIDE_HUB && HubMode() == 3 && !Mover().open && SavesMove(dir)) return true; // (Manage Saves: its slots, a grid)
        const int n = F.page == SIDE_HUB ? HubRows() : (int)RomList().size();
        const int na = F.page == SIDE_HUB ? (int)HubActions(F.prow).size() : RomActions();
        if (F.pact >= 0)
        {
            if (dir == 3) F.pact = std::max(0, F.pact - 1);
            else if (dir == 4) F.pact = std::min(na - 1, F.pact + 1);
            else if (dir == 1) F.pact = -1;
            return true;
        }
        if (dir == 3) F.prow = std::max(0, F.prow - 1);
        else if (dir == 4) F.prow = std::min(std::max(0, n - 1), F.prow + 1);
        else if (dir == 1) F.side = F.page;
        else if (dir == 2 && na > 0) F.pact = 0;
        return true;
    }
    if (dir == 3) F.prow = std::max(0, F.prow - 1);
    else if (dir == 4) F.prow = std::min(PageRows(F.page) - 1, F.prow + 1);
    else if (dir == 1) F.side = F.page;
    return true;
}
// What's New on the Updates page: under a point (for taps and dragging), and the wheel.
inline bool NotesAt(int x, int y)
{
    const GlassFocus& f = Focus();
    (void)f; (void)x; (void)y;
    return false; // (The Updates page is gone: Settings > Updates holds it now.)
}
inline bool NotesScroll(int by)
{
    (void)by;
    return false;
}
// B while a page is open: the sidebar first, then back to the games. (False: no page open.)
// The small back arrow at the bottom right of a page on a phone held sideways (it does what Esc does).
struct BackBtn { int x = 0, y = 0, s = 0; };
inline BackBtn& BackRect() { static BackBtn b; return b; }
inline bool BackAt(int mx, int my)
{
    const BackBtn& b = BackRect();
    const int m = b.s / 4; // (a little more to hit than to see)
    return b.s > 0 && mx >= b.x - m && mx < b.x + b.s + m && my >= b.y - m && my < b.y + b.s + m;
}
inline bool PageBack()
{
    GlassFocus& F = Focus();
    if (F.page < 0) return false;
    if (F.page == SIDE_LAUNCHER && Look().edit >= 0) { Look().edit = -1; LookSave(); Hub().version++; return true; } // (the colour wheel closed)
    if (F.page == SIDE_LAUNCHER && F.side < 0 && LauncherMode() == 4) { LauncherMode() = 1; F.prow = 8; F.rScroll = 0; Themes().msg.clear(); Hub().version++; return true; } // (the fonts: back to the Theme Editor)
    if (F.page == SIDE_LAUNCHER && F.side < 0 && LauncherMode() == 5) { LauncherMode() = 0; F.prow = 3; F.rScroll = 0; Themes().msg.clear(); Hub().version++; return true; } // (the Gallery: back to its menu)
    if (F.page == SIDE_LAUNCHER && F.side < 0 && LauncherMode() == 3) { LauncherMode() = 2; F.prow = 2; F.rScroll = 0; Themes().msg.clear(); Hub().version++; return true; } // (the Library: back to My Themes)
    if (F.page == SIDE_LAUNCHER && F.side < 0 && LauncherMode() != 0) { F.prow = LauncherMode() == 1 ? 1 : 2; LauncherMode() = 0; F.rScroll = 0; Themes().msg.clear(); Hub().version++; return true; } // (back to its menu)
    if (F.page == SIDE_HUB && F.side < 0 && HubMode() == 4 && !Mover().open) return DexBack();
    if (F.page == SIDE_HUB && F.side < 0 && HubMode() == 3 && !Mover().open) return SavesBack(); // (Manage Saves: off a slot, then the games)
    if (F.page == SIDE_HUB && F.side < 0 && F.pact < 0 && Mover().open && Mover().swapping) { Mover().swapping = false; Mover().put = -1; Mover().msg.clear(); Hub().version++; return true; }
    if (F.page == SIDE_HUB && F.side < 0 && F.pact < 0 && Mover().open) { MoverShow(false); HubMode() = 0; F.prow = 1; F.rScroll = 0; return true; }
    if (F.page == SIDE_HUB && F.side < 0 && F.pact < 0 && HubMode() == 2) { HubLink::Leave(); HubMode() = 0; F.prow = 2; F.rScroll = 0; Hub().version++; return true; }
    if (F.page == SIDE_HUB && F.side < 0 && F.pact < 0 && HubMode() != 0) { HubMode() = 0; F.prow = 0; F.rScroll = 0; Hub().version++; return true; }
    if ((F.page == SIDE_ROMS || F.page == SIDE_HUB) && F.side < 0 && F.pact >= 0) { F.pact = -1; return true; }
    if (TilesOn()) { const int pg = F.page; F.page = -1; F.side = pg; return true; } // (the window closed: back on the tile that opened it)
    if (F.side < 0) F.side = F.page;
    else { F.page = -1; F.side = SIDE_HOME; }
    return true;
}
// A on a section of the sidebar: Home back to the games, About its page. (False: not made yet.)
inline bool OpenSide(int side)
{
    GlassFocus& F = Focus();
    if (side == SIDE_HOME) { F.page = -1; F.side = -1; return true; }
    if (side == SIDE_HUB) { HubMode() = 0; Mover().open = false; Hub().version++; }
    if (side == SIDE_LAUNCHER) { LauncherMode() = 0; Look().edit = -1; ThemesRefresh(); Themes().msg.clear(); Hub().version++; }
    if (PageRows(side) > 0) { F.page = side; F.prow = 0; F.side = -1; F.nScroll = 0; F.pact = -1; F.rScroll = 0; return true; }
    return false;
}
inline int PageRowAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (f.page < 0 || f.pW <= 0 || f.pRow <= 0 || x < f.pX || x >= f.pX + f.pW || y < f.pY) return -1;
    if (f.page == SIDE_LAUNCHER && LauncherMode() >= 3)
    {
        // (the Theme Library's and the fonts' grids.)
        const ThemeState& t = Themes();
        // (The < and > under the grid: the first card of the page before or after.)
        if (t.pageW > 0 && y >= t.pageY && y < t.pageY + t.pageH)
        {
            const int page = f.rScroll / std::max(1, f.rVis), pages = (LauncherRows() + t.perPage - 1) / std::max(1, t.perPage);
            if (x >= t.prevX && x < t.prevX + t.pageW && page > 0) return (page - 1) * t.perPage;
            if (x >= t.nextX && x < t.nextX + t.pageW && page + 1 < pages) return (page + 1) * t.perPage;
            return -1;
        }
        if (t.cw <= 0 || x < t.gx || y < t.gy) return -1;
        const int col = (x - t.gx) / (t.cw + t.gap), r = (y - t.gy) / (t.ch + t.gap);
        if (col >= t.cols || (x - t.gx) % (t.cw + t.gap) >= t.cw || (y - t.gy) % (t.ch + t.gap) >= t.ch || r >= f.rVis) return -1;
        const int k = (r + f.rScroll) * t.cols + col;
        return k < LauncherRows() ? k : -1;
    }
    const int i = (y - f.pY) / f.pRow + (f.page == SIDE_LAUNCHER ? f.rScroll : 0); // (My Themes scrolls)
    return i < PageRows(f.page) ? i : -1;
}
inline int RomRowAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if ((f.page != SIDE_ROMS && f.page != SIDE_HUB) || f.pW <= 0 || f.pRow <= 0 || x < f.pX || x >= f.pX + f.pW || y < f.pY) return -1;
    const int k = (y - f.pY) / f.pRow;
    const int i = f.rScroll + k;
    return k < f.rVis && i < (f.page == SIDE_HUB ? HubRows() : (int)RomList().size()) ? i : -1;
}
inline int RomActionAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (f.page == SIDE_HUB && HubMode() == 3 && !Mover().open) return SavesSlotAt(x, y); // (Manage Saves' slots)
    if ((f.page != SIDE_ROMS && f.page != SIDE_HUB) || f.aW <= 0 || f.aRow <= 0 || x < f.aX || x >= f.aX + f.aW || y < f.aY) return -1;
    const int i = (y - f.aY) / f.aRow;
    return i < (f.page == SIDE_HUB ? (int)HubActions(f.prow).size() : RomActions()) ? i : -1;
}
inline std::vector<std::string> WrapLines(const std::string& msg0, float px, int w, bool bold = false)
{
    const std::string msg = LauncherText::Tr(msg0); // (in the player's language before it is wrapped)
    std::vector<std::string> lines;
    std::string line;
    struct Tok { std::string text; bool space; };
    std::vector<Tok> toks;
    std::string word;
    bool pend = false, wordSpace = false;
    auto endWord = [&]() { if (!word.empty()) { toks.push_back({ word, wordSpace }); word.clear(); } };
    for (size_t i = 0; i < msg.size();)
    {
        const unsigned char b = (unsigned char)msg[i];
        const size_t n = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
        const std::string ch = msg.substr(i, n);
        i += n;
        if (ch == " ") { endWord(); pend = true; continue; }
        uint32_t cp = b;
        if (n == 3 && ch.size() == 3) cp = ((b & 0x0F) << 12) | (((unsigned char)ch[1] & 0x3F) << 6) | ((unsigned char)ch[2] & 0x3F);
        const bool cjk = n == 3 && ((cp >= 0x3000 && cp <= 0x9FFF) || (cp >= 0xFF00 && cp <= 0xFFEF)); // (Japanese has no spaces: it may break after any letter)
        if (cjk) { endWord(); toks.push_back({ ch, pend }); pend = false; continue; }
        if (word.empty()) { wordSpace = pend; pend = false; }
        word += ch;
    }
    endWord();
    for (const Tok& k : toks)
    {
        const std::string t = line.empty() ? k.text : line + (k.space ? " " : "") + k.text;
        if (!line.empty() && GW(t, px, bold) > w) { lines.push_back(line); line = k.text; }
        else line = t;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}
// A release's notes (Markdown, as written on GitHub) as plain points: list marks, headings, emphasis,
// code marks, pictures and links' addresses taken out; empty lines and headings left out.
inline std::vector<std::string> ReleasePoints(const std::string& md)
{
    std::vector<std::string> out;
    // (From a heading with "new" in it on, "What's new in this update", when there is one: the
    // download notes above it are not news.)
    size_t i = 0;
    for (size_t at = 0; at < md.size();)
    {
        size_t end = md.find('\n', at);
        if (end == std::string::npos) end = md.size();
        const size_t k = md.find_first_not_of(" \t", at);
        if (k != std::string::npos && k < end && md[k] == '#')
        {
            std::string low = md.substr(k, end - k);
            for (char& ch : low) ch = (char)tolower((unsigned char)ch);
            if (low.find("new") != std::string::npos) { i = end + 1; break; }
        }
        at = end + 1;
    }
    while (i <= md.size())
    {
        size_t j = md.find('\n', i);
        if (j == std::string::npos) j = md.size();
        std::string l = md.substr(i, j - i);
        i = j + 1;
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) l.pop_back();
        size_t k = 0;
        while (k < l.size() && (l[k] == ' ' || l[k] == '\t')) k++;
        l = l.substr(k);
        if (l.empty() || l[0] == '#' || l[0] == '|' || l[0] == '<' || l.compare(0, 3, "---") == 0 || l.compare(0, 3, "***") == 0 || l.compare(0, 2, "![") == 0) continue;
        if (l.size() > 1 && (l[0] == '-' || l[0] == '*' || l[0] == '+') && l[1] == ' ') l = l.substr(2);
        else
        {
            size_t d = 0;
            while (d < l.size() && isdigit((unsigned char)l[d])) d++;
            if (d > 0 && d + 1 < l.size() && (l[d] == '.' || l[d] == ')') && l[d + 1] == ' ') l = l.substr(d + 2);
        }
        if (l.compare(0, 4, "[ ] ") == 0 || l.compare(0, 4, "[x] ") == 0) l = l.substr(4);
        std::string t;
        for (size_t a = 0; a < l.size(); a++)
        {
            const char ch = l[a];
            if (ch == '*' || ch == '`' || (ch == '_' && a + 1 < l.size() && l[a + 1] == '_')) { if (ch == '_') a++; continue; }
            if (ch == '!' && a + 1 < l.size() && l[a + 1] == '[') continue;
            if (ch == '[')
            {
                const size_t close = l.find(']', a);
                if (close != std::string::npos && close + 1 < l.size() && l[close + 1] == '(')
                {
                    const size_t end = l.find(')', close);
                    if (end != std::string::npos) { t += l.substr(a + 1, close - a - 1); a = end; continue; }
                }
            }
            t += ch;
        }
        // (Typographic marks as the font's own: minus and dashes, curly quotes, the ellipsis.)
        static const char* const swaps[][2] = { { "\xE2\x88\x92", "-" }, { "\xE2\x80\x93", "-" }, { "\xE2\x80\x94", "-" },
                                                { "\xE2\x80\x98", "'" }, { "\xE2\x80\x99", "'" }, { "\xE2\x80\x9C", "\"" },
                                                { "\xE2\x80\x9D", "\"" }, { "\xE2\x80\xA6", "..." }, { "\xC2\xA0", " " } };
        for (const auto& sw : swaps)
            for (size_t at = t.find(sw[0]); at != std::string::npos; at = t.find(sw[0], at + strlen(sw[1])))
                t.replace(at, strlen(sw[0]), sw[1]);
        while (!t.empty() && t.back() == ' ') t.pop_back();
        if (!t.empty()) out.push_back(t);
    }
    return out;
}
// A page, right of the sidebar: its title in the bar, its sections in a glass panel on the left
// (what Fulldex is at the bottom), the chosen one in a wider panel on the right.
// (The species under a Pokemon's nickname, only when it differs: MonName is in the Hub builds; elsewhere there is none.)
inline bool SameMonName(const std::string& v, const std::string& spn)
{
#ifdef SSR_POKEMON_HUB
    return MonName(v) == spn;
#else
    return v == spn;
#endif
}
// Cards can cross the scrolling viewport. Preserve only their overflow, including the
// selection glow, so direct pixel writers cannot cover the header or footer.
struct GridCardClip
{
    Canvas& c; const Grid& g; int x0, x1, y0, y1;
    std::vector<uint32_t> saved;
    GridCardClip(Canvas& canvas, const Grid& grid, int x, int y, int w, int h, int margin = 0)
        : c(canvas), g(grid), x0(std::max(0, x - margin)), x1(std::min(c.w, x + w + margin)),
          y0(std::max(0, y - margin)), y1(std::min(c.h, y + h + margin))
    {
        if (g.horiz || x1 <= x0) return;
        for (int yy = y0; yy < y1; yy++)
            if (yy < g.top || yy >= g.bottom)
                saved.insert(saved.end(), c.px.begin() + (size_t)yy * c.w + x0, c.px.begin() + (size_t)yy * c.w + x1);
    }
    ~GridCardClip()
    {
        size_t at = 0;
        if (saved.empty()) return;
        for (int yy = y0; yy < y1; yy++)
            if (yy < g.top || yy >= g.bottom)
            {
                std::copy_n(saved.begin() + at, x1 - x0, c.px.begin() + (size_t)yy * c.w + x0);
                at += x1 - x0;
            }
    }
};

// (A page's detail panel upright: what is drawn in it stays inside it, the pixels above and below put back after.)
struct ClipGuard
{
    Canvas& c; GlassFocus& F; int y0, y1, pad;
    std::decay_t<decltype(std::declval<Canvas&>().px)> a, b;
    ClipGuard(Canvas& cc, GlassFocus& f, int top, int bottom, int p) : c(cc), F(f), y0(top), y1(bottom), pad(p)
    {
        if (y0 > 0) { a.assign(c.px.begin(), c.px.begin() + (size_t)y0 * c.w); b.assign(c.px.begin() + (size_t)y1 * c.w, c.px.end()); }
    }
    ~ClipGuard()
    {
        if (y0 <= 0) return;
        std::copy(a.begin(), a.end(), c.px.begin());
        std::copy(b.begin(), b.end(), c.px.begin() + (size_t)y1 * c.w);
        F.dMax = std::max(0, TextLow() + F.dScroll + pad - y1);
    }
};
// Small diagrams keep the alternatives visible without recursively drawing Home or
// changing its focus, scroll position, cache or selected game.
inline int DrawLayoutPreviews(Canvas& c, int x, int y, int width, float u, int clipTop, int clipBottom)
{
    LayoutPreviews().clear();
    const bool phone = PhoneLook(), portrait = phone && c.h > c.w;
    static const char* const keys[] = { "sidebar", "grid", "tiles" };
    static const char* const labels[] = { "Sidebar", "Grid Only", "Tiles" };
    const int count = phone ? 2 : 3, gap = std::max(4, (int)(10 * u));
    const int cellW = (width - gap) / 2;
    const int thumbH = portrait ? (int)(142 * u) : cellW * 9 / 16;
    const int thumbW = portrait ? thumbH * 9 / 16 : cellW;
    const int rowH = thumbH + (int)(28 * u);
    for (int k = 0; k < count; k++)
    {
        const int cx = x + (k % 2) * (cellW + gap), ty = y + (k / 2) * (rowH + gap);
        const int tx = cx + (cellW - thumbW) / 2;
        const bool selected = Look().layout == keys[k];
        Glass::Round(c, tx, ty, thumbW, thumbH, 6 * u, 0xFF142231u,
                     selected ? Glass::ChosenEdge() : 0x65798A9Au, (selected ? 2.0f : 1.0f) * u);
        auto box = [&](float bx, float by, float bw, float bh, uint32_t colour) {
            Glass::Round(c, tx + (int)(bx * thumbW), ty + (int)(by * thumbH),
                         std::max(1, (int)(bw * thumbW)), std::max(1, (int)(bh * thumbH)), 2 * u, colour);
        };
        const uint32_t bar = 0xFF34485Cu, card = 0xFF789BBBu, ink = 0xFFE3EBF3u;
        if (k <= 1)
        {
            const float left = portrait ? .05f : phone ? .12f : .19f;
            if (!portrait) box(.03f, .04f, phone ? .06f : .12f, .92f, bar);
            box(left, .05f, .95f - left, .07f, bar);
            if (portrait) box(.05f, .90f, .90f, .06f, bar);
            if (k == 0) { box(left, .17f, .95f - left, .22f, bar); box(left + .02f, .19f, .13f, .17f, card); }
            const float top = k == 0 ? .44f : .19f, foot = portrait ? .86f : .95f;
            const int cols = portrait ? 2 : 4, rows = k == 0 ? 2 : 3;
            const float stepX = (.95f - left) / cols, stepY = (foot - top) / rows;
            for (int i = 0; i < cols * rows; i++)
                box(left + (i % cols) * stepX, top + (i / cols) * stepY, stepX - .02f, stepY - .035f, card);
        }
        else
        {
            box(.05f, .07f, .90f, .08f, bar);
            box(.12f, .30f, .18f, .30f, bar); box(.36f, .23f, .28f, .46f, card); box(.70f, .30f, .18f, .30f, bar);
            for (int i = 0; i < 6; i++) box(.08f + i * .145f, .80f, .12f, .13f, bar);
        }
        GT(c, cx + (cellW - GW(labels[k], 12 * u, selected)) / 2, ty + thumbH + 13 * u,
           labels[k], 12 * u, selected ? Menu::GlassText : Menu::GlassHint, selected);
        const int hitY = std::max(ty, clipTop), hitEnd = std::min(ty + rowH, clipBottom);
        if (hitEnd > hitY) LayoutPreviews().push_back({cx, hitY, cellW, hitEnd - hitY, keys[k]});
    }
    return ((count + 1) / 2) * rowH + ((count + 1) / 2 - 1) * gap;
}

inline void DrawPage(Canvas& c, const GlassSpots& sp, bool solid, float u)
{
    LayoutPreviews().clear();
    GlassFocus& F = Focus();
    const uint32_t text = Menu::GlassText, hint = Menu::GlassHint;
    const bool portP = PhoneLook() && c.h > c.w; // (upright: a thin bar over the page)
    const int barH = portP ? (int)(44 * u) : sp.tabsY * 2 + sp.tabsH;
    const int L = sp.bannerX, R = sp.bannerX + sp.bannerW, top = sp.pageTop > 0 ? sp.pageTop : barH + (int)((portP ? 10 : 18) * u), bottom = sp.pageBottom > 0 ? sp.pageBottom : PhoneLook() && c.h > c.w && sp.navY > 0 ? sp.navY - (int)(12 * u) : c.h - (int)((ShortNow() ? 14 : 64) * u); // (a phone sideways: the panels run to the foot of the screen, the back button is in the bar)
    const float titleY = sp.pageTitleY > 0 ? (float)sp.pageTitleY : barH * 0.5f;
    GT(c, sp.tabsX + (int)(6 * u), titleY, F.page == SIDE_ROMS ? "ROMs" : F.page == SIDE_HUB ? (Mover().open ? "Move Pok\xC3\xA9mon" : HubMode() == 1 ? "Cloud Saves" : HubMode() == 2 ? "Trade" : HubMode() == 3 ? SavesTitle() : HubMode() == 4 ? std::string("National Dex") : std::string("Hub")) : F.page == SIDE_SETTINGS ? (F.pageTitle.empty() ? std::string("Settings") : F.pageTitle) : F.page == SIDE_LAUNCHER ? (LauncherMode() == 1 ? "Theme Editor" : LauncherMode() == 2 ? "My Themes" : LauncherMode() == 3 ? "Theme Library" : LauncherMode() == 5 ? "Theme Gallery" : LauncherMode() == 4 ? "Font" : "Launcher") : "About", 26 * u, text, true);
    const std::string where = std::string("Fulldex   \xC2\xB7   ") + PlatformLabel();
    if (!ShortNow() && !portP) GT(c, R - GW(where, 16 * u, true), titleY, where, 16 * u, hint, true);
    if (F.page == SIDE_ROMS)
    {
        const std::vector<Game>& roms = RomList();
        const int n = (int)roms.size();
        const bool stkP = PhoneLook() && c.h > c.w; const int rw = stkP ? R - L : (int)(340 * u), gap = (int)(18 * u), lw2 = stkP ? R - L : R - L - rw - gap, lhP = stkP ? (int)((bottom - top) * 0.38f) : bottom - top, dTopP = stkP ? top + lhP + gap : top, dHP = stkP ? bottom - dTopP : bottom - top, pad = (int)(12 * u);
        const int rowH = (int)(46 * u), rg = (int)(6 * u);
        auto status = [&](const Game& g, uint32_t& col) -> std::string {
            if (g.hidden) { col = 0xFFAEB7C4u; return "Hidden"; }
            if (!g.known) { col = 0xFFF2A08Au; return "Not Playable"; }
            if (IsHack(g)) { col = 0xFFC9B6FFu; return "Romhack"; }
            col = 0xFF8FE3A2u; return "Ready";
        };
        auto fit = [&](std::string s, float px, bool bold, int w) {
            if (GW(s, px, bold) <= w) return s;
            while (!s.empty() && GW(s + "...", px, bold) > w)
            {
                while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
                if (!s.empty()) s.pop_back();
            }
            return s + "...";
        };
        auto fileOf = [](const std::string& p) { const size_t k = p.find_last_of("/\\"); return k == std::string::npos ? p : p.substr(k + 1); };
        auto dirOf = [](std::string p) {
            const size_t k = p.find_last_of("/\\");
            p = k == std::string::npos ? std::string() : p.substr(0, k);
            if (p.rfind("sdmc:", 0) == 0) p = p.substr(5);
            return p;
        };
        // (A file name also in another folder: shown with its folder, the last two parts of it.)
        auto shownFile = [&](const Game& g) {
            const std::string f = fileOf(g.rom);
            int same = 0;
            for (const Game& o : roms) if (fileOf(o.rom) == f) same++;
            if (same < 2) return f;
            std::string d = dirOf(g.rom);
            size_t k = d.find_last_of("/\\");
            if (k != std::string::npos && k > 0) { const size_t k2 = d.find_last_of("/\\", k - 1); if (k2 != std::string::npos) d = d.substr(k2 + 1); }
            return d + "   \xC2\xB7   " + f;
        };
        // The list.
        Menu::FrostBox(c, solid, L, top, lw2, lhP, 18 * u);
        const int vis = std::max(1, ( lhP - 2 * pad + rg) / (rowH + rg));
        F.prow = std::max(0, std::min(F.prow, n - 1));
        if (!F.freeScroll) { if (F.prow < F.rScroll) F.rScroll = F.prow; if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1; } // (a finger scrolling: the list stays where it was put)
        F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        if (n == 0)
        {
            const std::string none = "No ROMs found yet. Choose the folder containing your ROMs.";
            GT(c, L + (lw2 - GW(none, 16 * u, true)) / 2, (top + (top + lhP)) * 0.5f, none, 16 * u, hint, true);
        }
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const Game& g = roms[i];
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && F.pact < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            uint32_t col = 0;
            const std::string st = status(g, col);
            Glass::Round(c, rx + (int)(14 * u), mid - (int)(4 * u), (int)(8 * u), (int)(8 * u), 4 * u, col);
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(10 * u) - chipW;
            Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, chipX + (int)(10 * u), mid, st, 12 * u, col, true);
            GT(c, rx + (int)(32 * u), mid, fit(ShortName(g), 16 * u, true, (int)(200 * u)), 16 * u, g.hidden ? hint : text, true);
            const int fx = rx + (int)(248 * u);
            GT(c, fx, mid, fit(shownFile(g), 13 * u, false, chipX - fx - (int)(12 * u)), 13 * u, hint, false);
        }
        if (n > vis)
        {
            const int tx = L + lw2 - (int)(7 * u), ty = top + (int)(16 * u), th = lhP - (int)(32 * u);
            Glass::Round(c, tx, ty, (int)(3 * u), th, 1.5f * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), th * vis / n), thumbY = ty + (th - thumbH) * F.rScroll / std::max(1, n - vis);
            Glass::Round(c, tx, thumbY, (int)(3 * u), thumbH, 1.5f * u, 0xCCFFFFFFu);
        }
        // The chosen ROM.
        const int dx = stkP ? L : R - rw;
        Menu::FrostBox(c, solid, dx, dTopP, rw, dHP, 18 * u);
        static int dKey = -1; { const int nk = F.page * 100000 + F.prow * 100 + LauncherMode() * 10 + HubMode() + (Look().edit + 1) * 10000000; if (nk != dKey) { dKey = nk; F.dScroll = 0; } }
        if (stkP && F.pact >= 0 && F.side < 0 && !F.freeScroll && F.aW > 0)
        {
            // Use the last picture's button bounds to bring controller focus into view.
            const int actionTop = F.aY + F.pact * F.aRow;
            const int actionBottom = actionTop + F.aRow - (int)(6 * u);
            const int margin = (int)(16 * u);
            if (actionBottom > bottom - margin) F.dScroll += actionBottom - (bottom - margin);
            else if (actionTop < dTopP + margin) F.dScroll -= dTopP + margin - actionTop;
        }
        F.dScroll = std::clamp(F.dScroll, 0, F.dMax); TextLow() = 0; F.pdY = stkP ? dTopP : 0;
        const int dTopC = dTopP - (stkP ? F.dScroll : 0);
        ClipGuard clipG(c, F, stkP ? dTopP : 0, stkP ? bottom : c.h, (int)(16 * u));
        F.aW = 0;
        const int ix = dx + (int)(18 * u), iw = rw - (int)(36 * u), ps = (int)(110 * u);
        int y = dTopC + (int)(18 * u);
        if (n > 0)
        {
            const Game& g = roms[F.prow];
            DrawCover(c, g, ix, y, ps, ps, 12 * u);
            const int tx = ix + ps + (int)(16 * u), tw = iw - ps - (int)(16 * u);
            GT(c, tx, y + 28 * u, fit(ShortName(g), 20 * u, true, tw), 20 * u, text, true);
            GT(c, tx, y + 54 * u, PlatformName(g.platform), 14 * u, hint, true);
            uint32_t col = 0;
            const std::string st = status(g, col);
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u);
            Glass::Round(c, tx, y + (int)(70 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, tx + (int)(10 * u), y + 81 * u, st, 12 * u, col, true);
            y += ps + (int)(22 * u);
            char size[32];
            if (g.fileSize > 0) snprintf(size, sizeof size, "%.1f MB", g.fileSize / 1048576.0);
            else snprintf(size, sizeof size, "-");
            const std::string cells[4][2] = { { "Size", size }, { "Save", g.hasSave ? "Found" : "None Yet" },
                                              { "Game Code", g.code.empty() ? "-" : g.code }, { "Save File", g.saveName.empty() ? "-" : g.saveName } };
            for (int i = 0; i < 4; i++)
            {
                const int cx = ix + (i % 2) * (iw / 2), cy = y + (i / 2) * (int)(48 * u);
                GT(c, cx, cy + 8 * u, cells[i][0], 12 * u, hint, true);
                GT(c, cx, cy + 28 * u, fit(cells[i][1], 15 * u, true, iw / 2 - (int)(8 * u)), 15 * u, text, true);
            }
            y += 2 * (int)(48 * u) + (int)(10 * u);
            const int yLim = bottom - (int)(18 * u) - RomActions() * (int)(44 * u) - (RomActions() - 1) * (int)(6 * u) - (int)(8 * u);
            if (y + (int)(40 * u) < yLim)
            {
                GT(c, ix, y + 8 * u, "File", 12 * u, hint, true);
                y += (int)(26 * u);
                const std::vector<std::string> fl = WrapLines(fileOf(g.rom), 13 * u, iw);
                for (size_t k = 0; k < fl.size() && k < 3 && y + (int)(14 * u) < yLim; k++) { GT(c, ix, y, fl[k], 13 * u, text, false); y += (int)(18 * u); }
                y += (int)(12 * u);
            }
            if (y + (int)(44 * u) < yLim)
            {
                GT(c, ix, y + 8 * u, "Folder", 12 * u, hint, true);
                y += (int)(26 * u);
                const std::vector<std::string> dl = WrapLines(dirOf(g.rom), 13 * u, iw);
                for (size_t k = 0; k < dl.size() && k < 3 && y + (int)(14 * u) < yLim; k++) { GT(c, ix, y, dl[k], 13 * u, text, false); y += (int)(18 * u); }
            }
        }
        {
            // Folder selection is available independently of the chosen ROM.
            const int na = RomActions(), ah = (int)(44 * u), ag = (int)(6 * u);
            const int ay = std::max(bottom - (int)(18 * u) - na * ah - (na - 1) * ag, PhoneLook() && c.h > c.w ? y + (int)(10 * u) : 0); // (upright: under the writing, the panel scrolling to them)
            F.aX = ix; F.aY = ay; F.aW = iw; F.aRow = ah + ag;
            std::vector<std::string> labels;
            std::vector<int> icons;
            if (CanChooseRomFolder()) { labels.push_back("Choose ROMs Folder"); icons.push_back(Glass::I_LOAD); }
            if (n > 0)
            {
                labels.push_back(roms[F.prow].hidden ? "Show In List Again" : "Hide From List");
                icons.push_back(Glass::I_GRID);
#if !defined(__SWITCH__) && !defined(__ANDROID__)
                labels.push_back("Open Folder"); icons.push_back(Glass::I_LOAD);
#endif
            }
            for (int a = 0; a < na; a++)
            {
                const int ry = ay + a * (ah + ag);
                if (a == F.pact)
                {
                    if (F.side < 0) Glass::Glow(c, ix, ry, iw, ah, 12 * u, u);
                    Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
                }
                else Glass::Round(c, ix, ry, iw, ah, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
                Glass::DrawIcon(c, icons[a], ix + 22 * u, ry + ah * 0.5f, 9 * u, text);
                GT(c, ix + (int)(42 * u), ry + ah * 0.5f, labels[a], 15 * u, text, true);
            }
            // The full last button, not just its text baseline, must be reachable by scrolling.
            if (na > 0) TextLow() = std::max(TextLow(), ay + na * ah + (na - 1) * ag);
        }
        return;
    }
    if (F.page == SIDE_SETTINGS)
    {
        // (Two frosted boxes; the settings menu draws its rows and preview in them: DrawSettingsPage.)
        const int gap = (int)(18 * u);
        if (PortraitNow())
        {
            // (A tall window: the rows above, the preview under them.)
            const int lh = (int)((bottom - top) * 0.58f), ph = bottom - top - lh - gap;
            Menu::FrostBox(c, solid, L, top, R - L, lh, 18 * u);
            Menu::FrostBox(c, solid, L, top + lh + gap, R - L, ph, 18 * u);
            F.sX = L; F.sY = top; F.sW = R - L; F.sH = bottom - top; F.sListW = R - L; F.sPvX = L; F.sListH = lh; F.sPvY = top + lh + gap;
            return;
        }
        const int rw = (int)(400 * u), lw2 = R - L - rw - gap;
        Menu::FrostBox(c, solid, L, top, lw2, bottom - top, 18 * u);
        F.sX = L; F.sY = top; F.sW = R - L; F.sH = bottom - top; F.sListW = lw2; F.sPvX = R - rw; F.sListH = 0; F.sPvY = 0;
        return;
    }
    if (F.page == SIDE_HUB && !Mover().open && HubMode() == 2)
    {
        TradeState& t = Trade();
#ifdef SSR_POKEMON_HUB
        TradeTick();
#endif
        const HubLink::Status s = HubLink::Snapshot();
        const std::vector<MoverGame> gs = MoverGames();
        const std::vector<std::string> acts = HubActions(F.prow);
        auto fit = [&](std::string str, float px, bool bold, int w) {
            if (GW(str, px, bold) <= w) return str;
            while (!str.empty() && GW(str + "...", px, bold) > w)
            {
                while (!str.empty() && ((unsigned char)str.back() & 0xC0) == 0x80) str.pop_back();
                if (!str.empty()) str.pop_back();
            }
            return str + "...";
        };
        const uint32_t okCol = 0xFF8FE3A2u, noCol = 0xFFF2C46Bu, linkCol = 0xFF8CC8FFu;
        const std::string game = gs.empty() ? std::string("-") : gs[std::max(0, std::min(t.game, (int)gs.size() - 1))].name;
        std::string linkV, linkChip;
        uint32_t linkCol2 = hint;
        switch (s.state)
        {
        case HubLink::State::Off: linkV = "Not Linked"; linkChip = "Same Wi-Fi"; break;
        case HubLink::State::Hosting: linkV = "Room " + std::to_string(s.code); linkChip = "Waiting"; linkCol2 = noCol; break;
        case HubLink::State::Browsing: linkV = "Looking For Rooms"; linkChip = std::to_string(s.rooms.size()) + " Found"; linkCol2 = linkCol; break;
        case HubLink::State::Linked: linkV = "Linked With " + s.peerName; linkChip = s.onlineCode.empty() ? s.peerGame : "Online"; linkCol2 = okCol; break;
        case HubLink::State::Online: linkV = "Code " + s.onlineCode; linkChip = "Waiting Online"; linkCol2 = noCol; break;
        }
#ifdef SSR_POKEMON_HUB
        const std::vector<PokemonHub::Mon> mons = TradeList();
#else
        const std::vector<int> mons;
#endif
        const int n = TradeRows();
        const bool stkP = PhoneLook() && c.h > c.w; const int rw = stkP ? R - L : (int)(380 * u), gap = (int)(18 * u), lw2 = stkP ? R - L : R - L - rw - gap, lhP = stkP ? (int)((bottom - top) * 0.55f) : bottom - top, dTopP = stkP ? top + lhP + gap : top, dHP = stkP ? bottom - dTopP : bottom - top, pad = (int)(12 * u);
        const int rowH = (int)(46 * u), rg = (int)(6 * u);
        Menu::FrostBox(c, solid, L, top, lw2, lhP, 18 * u);
        const int vis = std::max(1, ( lhP - 2 * pad + rg) / (rowH + rg));
        F.prow = std::max(0, std::min(F.prow, n - 1));
        if (F.pact >= (int)acts.size()) F.pact = acts.empty() ? -1 : (int)acts.size() - 1;
        if (!F.freeScroll) { if (F.prow < F.rScroll) F.rScroll = F.prow; if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1; } // (a finger scrolling: the list stays where it was put)
        F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && F.pact < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            std::string k, v, chip, spn;
            uint32_t chipCol = hint;
            std::vector<uint32_t> ic;
            if (i == 0) { k = "Game"; v = game; }
            else if (i == 1) { k = "Box"; v = "-"; }
            else if (i == 2) { k = "Connect"; v = linkV; chip = linkChip; chipCol = linkCol2; }
#ifdef SSR_POKEMON_HUB
            if (i == 0 && t.save) chip = Shown(t.save->Trainer(), "-");
            if (i == 1 && t.save) { v = BoxLabel(t.save.get(), t.box); chip = std::to_string(mons.size()) + " Pok\xC3\xA9mon"; }
            if (i >= TRADE_FIRST)
            {
                const PokemonHub::Mon& mon = mons[i - TRADE_FIRST];
                k = mon.egg ? std::string("Egg") : "Lv " + std::to_string(mon.level);
                v = Shown(mon.nickname, MonName(mon.speciesName));
                if (!mon.egg) spn = MonName(mon.speciesName);
                if (s.mine.any && t.offerBox == mon.box && t.offerSlot == mon.slot) { chip = s.mine.gift ? "Sending" : "Offered"; chipCol = linkCol; }
                if (MonIcon(mon.species, mon.egg, ic)) DrawMonIcon(c, ic, rx + (int)(2 * u), mid - (int)(21 * u), (int)(40 * u));
                if (mon.shiny) Glass::DrawIcon(c, Glass::I_SPARKLE, rx + 38 * u, mid - 12 * u, 6 * u, 0xFFF2C46Bu);
            }
#endif
            if (i < TRADE_FIRST) Glass::DrawIcon(c, i == 1 ? Glass::I_GRID : i == 2 ? Glass::I_GLOBE : Glass::I_POKEBALL, rx + 18 * u, mid, 9 * u, text);
            if (!chip.empty())
            {
                const int chipW = GW(chip, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(10 * u) - chipW;
                Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
                GT(c, chipX + (int)(10 * u), mid, chip, 12 * u, chipCol, true);
            }
            GT(c, rx + (int)(48 * u), mid, k, 13 * u, hint, true);
            GT(c, rx + (int)(118 * u), mid, fit(v, 16 * u, true, (int)(230 * u)), 16 * u, i >= TRADE_FIRST && s.state != HubLink::State::Linked ? hint : text, true); // (a Pok\xC3\xA9mon dimmed until connected)
            if (!spn.empty() && !SameMonName(v, spn))
                GT(c, rx + (int)(118 * u) + std::min(GW(v, 16 * u, true), (int)(230 * u)) + (int)(12 * u), mid, spn, 14 * u, hint, true);
        }
        if (n > vis)
        {
            const int tx = L + lw2 - (int)(7 * u), ty = top + (int)(16 * u), th = lhP - (int)(32 * u);
            Glass::Round(c, tx, ty, (int)(3 * u), th, 1.5f * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), th * vis / n), thumbY = ty + (th - thumbH) * F.rScroll / std::max(1, n - vis);
            Glass::Round(c, tx, thumbY, (int)(3 * u), thumbH, 1.5f * u, 0xCCFFFFFFu);
        }
        // The panel: the link, both offers, or the chosen row.
        const int dx = stkP ? L : R - rw, ix = dx + (int)(20 * u), iw = rw - (int)(40 * u);
        Menu::FrostBox(c, solid, dx, dTopP, rw, dHP, 18 * u);
        static int dKey = -1; { const int nk = F.page * 100000 + F.prow * 100 + LauncherMode() * 10 + HubMode() + (Look().edit + 1) * 10000000; if (nk != dKey) { dKey = nk; F.dScroll = 0; } }
        F.dScroll = std::min(F.dScroll, F.dMax); TextLow() = 0; F.pdY = stkP ? dTopP : 0;
        const int dTopC = dTopP - (stkP ? F.dScroll : 0);
        ClipGuard clipG(c, F, stkP ? dTopP : 0, stkP ? bottom : c.h, (int)(16 * u));
        int y = dTopC + (int)(20 * u);
        auto para = [&](const std::string& str, uint32_t col, float px) {
            for (const std::string& line : WrapLines(str, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        auto offerCard = [&](const char* who, const HubLink::Offer& o, const std::string& under, uint32_t underCol) {
            const int ch = (int)(78 * u);
            Glass::Round(c, ix, y, iw, ch, 14 * u, 0x1AFFFFFFu, 0x3DFFFFFF, 1.0f * u);
            GT(c, ix + (int)(14 * u), y + 16 * u, who, 12 * u, hint, true);
            std::vector<uint32_t> oi;
#ifdef SSR_POKEMON_HUB
            if (o.any && MonIcon(o.speciesId, false, oi)) DrawMonIcon(c, oi, ix + iw - (int)(70 * u), y + (int)(6 * u), (int)(64 * u));
#endif
            if (o.any)
            {
                GT(c, ix + (int)(14 * u), y + 40 * u, fit(Shown(o.name, o.species) + (o.shiny ? "   \xC2\xB7   Shiny" : ""), 17 * u, true, iw - (int)(90 * u)), 17 * u, text, true);
                GT(c, ix + (int)(14 * u), y + 62 * u, fit(o.species + "   \xC2\xB7   Lv " + std::to_string(o.level) + "   \xC2\xB7   " + o.game, 12 * u, true, iw - (int)(90 * u)), 12 * u, hint, true);
            }
            else GT(c, ix + (int)(14 * u), y + 46 * u, "Nothing yet", 15 * u, hint, true);
            y += ch + (int)(6 * u);
            if (!under.empty()) para(under, underCol, 12 * u);
            else y += (int)(6 * u);
        };
        const bool guideOn = t.guide && F.prow == 2 && s.state == HubLink::State::Off;
        if (!guideOn)
        {
            // How it goes, always at the dTopC: the three steps (the one you are on lit) and what to do now.
            const bool linked = s.state == HubLink::State::Linked;
            const int step = linked ? (s.mine.any || s.theirs.any ? 3 : 2) : 1;
            const std::string peer = s.peerName.empty() ? std::string("the other player") : s.peerName;
            std::string now;
            if (s.state == HubLink::State::Off) now = "Choose the Connect row, then Open A Room on one device and Join A Room on the other (same Wi-Fi), or use an online code.";
            else if (s.state == HubLink::State::Hosting) now = "Your room is open. Waiting for the other device to join it.";
            else if (s.state == HubLink::State::Browsing) now = "Looking for rooms. Choose one on the Connect row to join it.";
            else if (s.state == HubLink::State::Online) now = "Waiting for the other player to enter your code.";
            else if (!s.mine.any && !s.theirs.any) now = "You are linked. Pick one of your Pok\xC3\xA9mon in the list, then choose Offer For A Trade (or Send As A Gift).";
            else if (s.mine.any && !s.theirs.any) now = "Your offer is up. Waiting for " + peer + " to offer one back (or to accept your gift).";
            else if (!s.mine.any && s.theirs.gift) now = peer + " is sending you a gift. Choose the Connect row, then Accept The Gift.";
            else if (!s.mine.any) now = peer + " has offered one. Pick one of yours to offer back.";
            else if (s.iAccepted) now = "You said yes. Waiting for " + peer + "...";
            else now = "Both offers are up. Check them, then choose the Connect row and Accept The Trade.";
            static const char* const lab[3] = { "1  Connect", "2  Offer", "3  Accept" };
            const int cg = (int)(6 * u), cw = (iw - 2 * cg) / 3, chh = (int)(26 * u);
            for (int k = 0; k < 3; k++)
            {
                const int x = ix + k * (cw + cg);
                if (k + 1 == step) Glass::Round(c, x, y, cw, chh, chh * 0.5f, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
                else Glass::Round(c, x, y, cw, chh, chh * 0.5f, 0x1AFFFFFFu, 0x3DFFFFFF, 1.0f * u);
                GT(c, x + (cw - GW(lab[k], 12 * u, true)) / 2, y + chh * 0.5f, lab[k], 12 * u, k + 1 == step ? text : hint, true);
            }
            y += chh + (int)(10 * u);
            para(now, text, 13 * u);
        }
        if (guideOn)
        {
            GT(c, ix, y + 14 * u, "How Trading Works", 20 * u, text, true);
            y += (int)(40 * u);
            const std::string gt[5] = {
                "You both need Fulldex open, on the same Wi-Fi or with an online code. One offer each way, and you both say yes.",
                "1. Connect. On one device choose Open A Room. On the other choose Join A Room, then the room. No shared Wi-Fi? Make An Online Code on one device, Enter An Online Code on the other, and send the code in a message.",
                "2. Offer. Pick a Pok\xC3\xA9mon from your list and choose Offer For A Trade, or Send As A Gift to give it away.",
                "3. Accept. Check both offers: the line under each says whether the game can take it. Then choose Accept The Trade on the Connect row. It happens when you both have said yes.",
                "Each save is backed up before it is written. Trades follow the real games' rules, and Pok\xC3\xA9mon that evolve by trading evolve." };
            const uint32_t gc[5] = { hint, text, text, text, hint };
            float gpx = 13 * u;
            if (ShortNow())
            {
                // (A phone sideways: the guide's writing gets smaller, to 9.5, until it fits above the one button.)
                const int room = bottom - (int)(18 * u) - (int)(34 * u) - (int)(10 * u) - y;
                for (; gpx > 9.5f * u; gpx -= 0.5f * u)
                {
                    int need = 0;
                    for (int k = 0; k < 5; k++) need += (int)WrapLines(gt[k], gpx, iw).size() * (int)(gpx * 1.45f) + (int)(8 * u);
                    if (need <= room) break;
                }
            }
            for (int k = 0; k < 5; k++) para(gt[k], gc[k], gpx);
        }
        else if (F.prow == 2 || (s.state == HubLink::State::Linked && F.prow >= TRADE_FIRST && s.mine.any))
        {
            GT(c, ix, y + 14 * u, s.state == HubLink::State::Linked ? "Linked With " + s.peerName : "Trade", 20 * u, text, true);
            y += (int)(40 * u);
            if (s.state == HubLink::State::Off)
            {
                para("Trade with a friend, or send Pok\xC3\xA9mon to another of your own devices.", hint, 13 * u);
                para("Online instead: one player makes a code, the other enters it.", hint, 13 * u);
            }
            else if (s.state == HubLink::State::Online)
            {
                para("Your code:", hint, 13 * u);
                const int bh = (int)(70 * u);
                Glass::Round(c, ix, y, iw, bh, 14 * u, 0x24FFFFFFu, 0x3DFFFFFF, 1.0f * u);
                GT(c, ix + (iw - GW(s.onlineCode, 34 * u, true)) / 2, y + bh * 0.5f, s.onlineCode, 34 * u, text, true);
                y += bh + (int)(16 * u);
                para("The other player chooses Hub, Trade, Enter An Online Code, and types it. Share it on Discord or in a message. It works for a few minutes.", hint, 13 * u);
            }
            else if (s.state == HubLink::State::Hosting) para("Room " + std::to_string(s.code) + " is open. On the other device: Hub, Trade Over Wi-Fi, Join A Room.", hint, 13 * u);
            if (!s.myAddress.empty() && s.state != HubLink::State::Linked) para("This device: " + s.myAddress, hint, 12 * u);
            else if (s.state == HubLink::State::Browsing) para(s.rooms.empty() ? "Looking for rooms on this network. Open one on the other device." : "Rooms on this network are listed below: choose one to join.", hint, 13 * u);
            else if (s.state == HubLink::State::Linked)
            {
                std::string mineUnder, theirUnder;
                uint32_t mineCol = hint, theirCol = hint;
                if (s.mine.any) { mineUnder = s.theirReason.empty() && !s.theyCanTake ? "Waiting for " + s.peerName + "'s game to check it..." : s.theyCanTake ? s.peerName + "'s game can take it." : "Their game can't take it: " + SettingSentence(s.theirReason); mineCol = s.theyCanTake ? okCol : noCol; }
#ifdef SSR_POKEMON_HUB
                if (s.theirs.any && t.save)
                {
                    const PokemonHub::Check ck = PokemonHub::CanReceive(TheirPacked(s.theirs), *t.save, s.mine.any ? t.offerSpecies : 0);
                    theirUnder = ck.verdict == PokemonHub::Verdict::Ok ? (ck.evolvesTo ? SettingSentence(ck.evolution) + "." : game + " can take it.") : "Your game can't take it: " + SettingSentence(ck.reason);
                    theirCol = ck.verdict == PokemonHub::Verdict::Ok ? okCol : noCol;
                }
#endif
                offerCard(s.mine.any && s.mine.gift ? "You Send" : "You Offer", s.mine, mineUnder, mineCol);
                offerCard(s.theirs.any && s.theirs.gift ? (s.peerName + " Sends").c_str() : (s.peerName + " Offers").c_str(), s.theirs, theirUnder, theirCol);
                if (s.iAccepted || s.theyAccepted)
                    para(std::string(s.iAccepted ? "You said yes. " : "") + (s.theyAccepted ? s.peerName + " said yes." : "Waiting for " + s.peerName + "..."), linkCol, 13 * u);
            }
        }
        else if (F.prow == 0) { GT(c, ix, y + 14 * u, game, 20 * u, text, true); y += (int)(40 * u); para(s.state == HubLink::State::Linked ? "Leave the link to change the game." : "The game to trade from and into.", hint, 13 * u); }
        else if (F.prow == 1) { GT(c, ix, y + 14 * u, "Box", 20 * u, text, true); y += (int)(40 * u); para("Next Box and Previous Box go through its boxes; Party shows the Pok\xC3\xA9mon with you.", hint, 13 * u); }
#ifdef SSR_POKEMON_HUB
        else if (F.prow - TRADE_FIRST < (int)mons.size())
        {
            const PokemonHub::Mon& mon = mons[F.prow - TRADE_FIRST];
            std::vector<uint32_t> big;
            const int bs = (int)(96 * u);
            if (MonIcon(mon.species, mon.egg, big)) DrawMonIcon(c, big, ix + iw - bs, y - (int)(18 * u), bs);
            GT(c, ix, y + 14 * u, fit(Shown(mon.nickname, MonName(mon.speciesName)), 22 * u, true, iw - bs), 22 * u, text, true);
            GT(c, ix, y + 42 * u, (mon.egg ? std::string("Egg") : MonName(mon.speciesName)) + (mon.shiny ? "   \xC2\xB7   Shiny" : ""), 15 * u, mon.shiny ? 0xFFF2C46Bu : hint, true);
            y += (int)(72 * u);
            if (s.state != HubLink::State::Linked) para("Connect to the other device first: the Connect row above.", hint, 13 * u);
            else
            {
                const PokemonHub::Check rm = t.save ? PokemonHub::CanRemove(*t.save, mon.box, mon.slot) : PokemonHub::Check();
                para(rm.verdict == PokemonHub::Verdict::Ok ? "Offer it for a trade (one each way, both say yes), or send it as a gift." : SettingSentence(rm.reason), rm.verdict == PokemonHub::Verdict::Ok ? hint : noCol, 13 * u);
            }
        }
#endif
        const int na = (int)acts.size();
        int ah = (int)(44 * u), ag = (int)(6 * u);
        if (ShortNow() && na > 0)
        {
            // (A phone sideways: the writing above can run long, so the buttons give way, down to a smaller size, instead of lying over it.)
            const int room = bottom - (int)(18 * u) - (y + (int)(8 * u));
            if (na * ah + (na - 1) * ag > room) { ag = (int)(4 * u); ah = std::max((int)(30 * u), (room - (na - 1) * ag) / na); }
        }
        const int ay = std::max(bottom - (int)(18 * u) - na * ah - std::max(0, na - 1) * ag, PhoneLook() && c.h > c.w ? y + (int)(10 * u) : 0); // (upright: under the writing, the panel scrolling to them)
        const std::string word = !t.msg.empty() ? t.msg : s.note;
        if (!word.empty())
        {
            const std::vector<std::string> ml = WrapLines(word, 13 * u, iw);
            int my = ay - (int)(14 * u) - (int)ml.size() * (int)(19 * u);
            if (my > y) for (const std::string& line : ml) { GT(c, ix, my + 8 * u, line, 13 * u, linkCol, true); my += (int)(19 * u); }
        }
        F.aX = ix; F.aY = ay; F.aW = na > 0 ? iw : 0; F.aRow = ah + ag;
        for (int a = 0; a < na; a++)
        {
            const int ry = ay + a * (ah + ag);
            if (a == F.pact)
            {
                if (F.side < 0) Glass::Glow(c, ix, ry, iw, ah, 12 * u, u);
                Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            else Glass::Round(c, ix, ry, iw, ah, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
            const int icon = F.prow == 1 ? Glass::I_GRID : F.prow == 2 ? (acts[a] == "Leave" || acts[a] == "Not Yet" || acts[a] == "Stop Looking" || acts[a] == "Close The Room" || acts[a] == "Take Back My Offer" ? Glass::I_BACK : Glass::I_GLOBE) : F.prow >= TRADE_FIRST ? Glass::I_PEOPLE : Glass::I_POKEBALL;
            Glass::DrawIcon(c, icon, ix + 22 * u, ry + ah * 0.5f, 9 * u, text);
            GT(c, ix + (int)(42 * u), ry + ah * 0.5f, fit(acts[a], 15 * u, true, iw - (int)(60 * u)), 15 * u, text, true);
        }
        return;
    }
    if (F.page == SIDE_LAUNCHER)
    {
        // The Launcher page: the launcher's own look, each row opened with A (its colour, picture, blur).
        LookLoad();
        LookPrefs& lk = Look();
        if (LauncherMode() == 4)
        {
            // The fonts: a grid of cards, each its name and a line in itself; the one in use marked.
            ThemeState& ts = Themes();
            const int pad = (int)(16 * u), gap = (int)(12 * u), cols = 4, n = kLauncherFontCount, W = R - L;
            const int cw = (W - 2 * pad - (cols - 1) * gap) / cols, ch = (int)(96 * u);
            const int visRows = std::max(1, (bottom - top - 2 * pad - (int)(40 * u) + gap) / (ch + gap));
            Menu::FrostBox(c, solid, L, top, W, bottom - top, 18 * u);
            F.prow = std::max(0, std::min(F.prow, n - 1));
            const int perPage = cols * visRows, pages = std::max(1, (n + perPage - 1) / perPage), page = F.prow / perPage;
            F.rScroll = page * visRows;
            ts.perPage = perPage;
            F.pact = -1; F.aW = 0;
            F.pX = L + pad; F.pY = top + pad; F.pW = W - 2 * pad; F.pRow = ch + gap; F.rVis = visRows;
            ts.gx = L + pad; ts.gy = top + pad; ts.cw = cw; ts.ch = ch; ts.gap = gap; ts.cols = cols;
            const int inUse = LookFontId(), was = CleanFont::FontId();
            for (int i = page * perPage; i < std::min(n, (page + 1) * perPage); i++)
            {
                const int r = i / cols - F.rScroll, x = L + pad + (i % cols) * (cw + gap), y = top + pad + r * (ch + gap);
                const bool on = i == F.prow;
                if (on && F.side < 0) Glass::Glow(c, x, y, cw, ch, 14 * u, u);
                if (on) Glass::Round(c, x, y, cw, ch, 14 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
                else Glass::Round(c, x, y, cw, ch, 14 * u, SettingsArt::Row, i == inUse ? Glass::ChosenEdge() & 0x80FFFFFFu : 0x3DFFFFFFu, 1.0f * u);
                GT(c, x + (int)(14 * u), y + (int)(18 * u), kLauncherFonts[i].group, 11 * u, hint, true);
                if (i == inUse)
                {
                    const int iw2 = GW("In Use", 11 * u, true) + (int)(16 * u);
                    Glass::Round(c, x + cw - iw2 - (int)(10 * u), y + (int)(8 * u), iw2, (int)(20 * u), 10 * u, 0x30FFFFFFu);
                    GT(c, x + cw - iw2 - (int)(2 * u), y + (int)(18 * u), "In Use", 11 * u, text, true);
                }
                // (Its name and a line in the font itself.)
                CleanFont::FontId() = i;
                GTFit(c, x + (int)(14 * u), y + (int)(50 * u), kLauncherFonts[i].name, 20 * u, cw - (int)(28 * u), text, true);
                GTFit(c, x + (int)(14 * u), y + (int)(76 * u), "Pok\xC3\xA9mon Fulldex 123", 14 * u, cw - (int)(28 * u), hint, false);
                CleanFont::FontId() = was;
            }
            ts.pageW = 0;
            if (pages > 1)
            {
                const std::string pl = "Page " + std::to_string(page + 1) + " Of " + std::to_string(pages);
                const int bh = (int)(30 * u), bw = (int)(36 * u), ly = bottom - pad - bh, lw = GW(pl, 13 * u, true) + (int)(24 * u);
                const int nx = L + W - pad - bw, lx = nx - lw, px2 = lx - bw;
                Glass::Round(c, px2, ly, bw, bh, bh * 0.5f, page > 0 ? 0x30FFFFFFu : 0x12FFFFFFu, 0x40FFFFFFu, 1.0f * u);
                Glass::Round(c, nx, ly, bw, bh, bh * 0.5f, page + 1 < pages ? 0x30FFFFFFu : 0x12FFFFFFu, 0x40FFFFFFu, 1.0f * u);
                GT(c, px2 + bw / 2 - GW("<", 15 * u, true) / 2, ly + bh / 2, "<", 15 * u, page > 0 ? text : hint, true);
                GT(c, nx + bw / 2 - GW(">", 15 * u, true) / 2, ly + bh / 2, ">", 15 * u, page + 1 < pages ? text : hint, true);
                GT(c, lx + (lw - GW(pl, 13 * u, true)) / 2, ly + bh / 2, pl, 13 * u, text, true);
                ts.prevX = px2; ts.nextX = nx; ts.pageY = ly; ts.pageW = bw; ts.pageH = bh;
            }
#ifdef __SWITCH__
            const std::string line = !ts.msg.empty() ? ts.msg : "Press A to use a font. B goes back to the Theme Editor.";
#else
            const std::string line = !ts.msg.empty() ? ts.msg : "Choose a font to use it. Back goes to the Theme Editor.";
#endif
            GT(c, L + pad, bottom - pad - (int)(4 * u), line, 13 * u, ts.msg.empty() ? hint : 0xFF8CC8FFu, !ts.msg.empty());
            return;
        }
        if (LauncherMode() == 3 || LauncherMode() == 5)
        {
            // The Theme Library: a grid of cards, each a theme's Home picture over its name. One panel
            // across, rows of four, a page at a time.
            ThemeState& ts = Themes();
            const int pad = (int)(16 * u), gap = (int)(14 * u), cols = 4, n = LauncherRows(), W = R - L;
            const int cw = (W - 2 * pad - (cols - 1) * gap) / cols, pw = cw - (int)(16 * u), pph = pw * 9 / 16, ch = pph + (int)(44 * u);
            const int visRows = std::max(1, (bottom - top - 2 * pad - (int)(26 * u) + gap) / (ch + gap));
            Menu::FrostBox(c, solid, L, top, W, bottom - top, 18 * u);
            F.prow = std::max(0, std::min(F.prow, n - 1));
            const bool gal = LauncherMode() == 5; // (the Theme Gallery: shared themes, after two action cards)
            if (gal) GalleryPoll(F.prow);
            // (A page at a time: the chosen card's.)
            const int perPage = cols * visRows, pages = std::max(1, (n + perPage - 1) / perPage), page = F.prow / perPage;
            F.rScroll = page * visRows;
            ts.perPage = perPage;
            F.pact = -1; F.aW = 0;
            F.pX = L + pad; F.pY = top + pad; F.pW = W - 2 * pad; F.pRow = ch + gap; F.rVis = visRows;
            ts.gx = L + pad; ts.gy = top + pad; ts.cw = cw; ts.ch = ch; ts.gap = gap; ts.cols = cols;
            for (int i = 0; i < (gal ? GalleryCards() : (int)ts.list.size()); i++)
            {
                const int r = i / cols - F.rScroll;
                if (r < 0 || r >= visRows) continue;
                const int x = L + pad + (i % cols) * (cw + gap), y = top + pad + r * (ch + gap);
                const bool on = i == F.prow;
                if (on && F.side < 0) Glass::Glow(c, x, y, cw, ch, 14 * u, u);
                if (on) Glass::Round(c, x, y, cw, ch, 14 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
                else Glass::Round(c, x, y, cw, ch, 14 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
                const int px = x + (int)(8 * u), py = y + (int)(8 * u);
                std::string label;
                {
                    std::string gAuthor;
                    const Glass::Picture* pv = nullptr;
                    if (gal) { GalleryCardInfo(i, label, gAuthor); if (i >= 2) pv = GalleryPreview(i - 2); }
                    else { label = ts.list[(size_t)i]; pv = ThemePreview(label); }
                    if (gal && i < 2)
                    {
                        Glass::Round(c, px, py, pw, pph, 10 * u, 0x24FFFFFFu);
                        const std::string big = i == 0 ? "ABCD-1234" : "Share";
                        GT(c, px + (pw - GW(big, 22 * u, true)) / 2, py + pph / 2, big, 22 * u, text, true);
                    }
                    else if (pv) Glass::PictureIn(c, *pv, px, py, pw, pph, 10 * u, 255);
                    else { Glass::Round(c, px, py, pw, pph, 10 * u, 0x24FFFFFFu); GT(c, px + (pw - GW("No Picture", 12 * u, true)) / 2, py + pph / 2, "No Picture", 12 * u, hint, true); }
                }
                float fs = 15 * u;
                std::string shown = label;
                while (fs > 11 * u && GW(shown, fs, true) > pw) fs -= u;
                while (shown.size() > 4 && GW(shown, fs, true) > pw) shown = shown.substr(0, shown.size() - 4) + "...";
                GT(c, px, y + ch - (int)(18 * u), shown, fs, text, true);
            }
            // The pages: < Page 1 Of 3 > at the bottom right (the arrows tapped or clicked).
            ts.pageW = 0;
            if (pages > 1)
            {
                const std::string pl = "Page " + std::to_string(page + 1) + " Of " + std::to_string(pages);
                const int bh = (int)(30 * u), bw = (int)(36 * u), ly = bottom - pad - bh, lw = GW(pl, 13 * u, true) + (int)(24 * u);
                const int nx = L + W - pad - bw, lx = nx - lw, px2 = lx - bw;
                Glass::Round(c, px2, ly, bw, bh, bh * 0.5f, page > 0 ? 0x30FFFFFFu : 0x12FFFFFFu, 0x40FFFFFFu, 1.0f * u);
                Glass::Round(c, nx, ly, bw, bh, bh * 0.5f, page + 1 < pages ? 0x30FFFFFFu : 0x12FFFFFFu, 0x40FFFFFFu, 1.0f * u);
                GT(c, px2 + bw / 2 - GW("<", 15 * u, true) / 2, ly + bh / 2, "<", 15 * u, page > 0 ? text : hint, true);
                GT(c, nx + bw / 2 - GW(">", 15 * u, true) / 2, ly + bh / 2, ">", 15 * u, page + 1 < pages ? text : hint, true);
                GT(c, lx + (lw - GW(pl, 13 * u, true)) / 2, ly + bh / 2, pl, 13 * u, text, true);
                ts.prevX = px2; ts.nextX = nx; ts.pageY = ly; ts.pageW = bw; ts.pageH = bh;
            }
            if (!gal && ts.list.empty()) GT(c, L + (W - GW("No Themes Yet", 20 * u, true)) / 2, (top + bottom) / 2, "No Themes Yet", 20 * u, hint, true);
#ifdef __SWITCH__
            const char* say[2] = { "Save This Look on My Themes makes one.", "Press A to apply it or delete it." };
#else
            const char* say[2] = { "Save This Look on My Themes makes one.", "Choose it to apply it, show its file, or delete it." };
#endif
            const std::string gm = gal ? GalleryMsg() : std::string();
            const std::string line = !ts.msg.empty() ? ts.msg : gal ? (!gm.empty() ? gm : std::string("Choose a theme to try it on. You keep it or go back.")) : std::string(say[ts.list.empty() ? 0 : 1]);
            const bool lit = !ts.msg.empty() || !gm.empty();
            GT(c, L + pad, bottom - pad - (int)(4 * u), line, 13 * u, lit ? 0xFF8CC8FFu : hint, lit);
            return;
        }
        if (LauncherMode() != 1)
        {
            // Its menu, or My Themes: rows, and right of them what the chosen one is (a theme: its picture).
            ThemeState& ts = Themes();
            const bool stkP = PhoneLook() && c.h > c.w; const int rw = stkP ? R - L : (int)(400 * u), gap = (int)(18 * u), lw2 = stkP ? R - L : R - L - rw - gap, lhP = stkP ? (int)((bottom - top) * 0.55f) : bottom - top, dTopP = stkP ? top + lhP + gap : top, dHP = stkP ? bottom - dTopP : bottom - top, pad = (int)(14 * u);
            int rowH = (int)(58 * u), rg = (int)(8 * u);
            const int n = LauncherRows();
            // (The menu's rows all in view when the screen is short, as on a phone held sideways.)
            if (LauncherMode() == 0 && n * (rowH + rg) - rg > lhP - 2 * pad) { rg = (int)(6 * u); rowH = std::max((int)(44 * u), ( lhP - 2 * pad + rg) / n - rg); }
            const int vis = std::max(1, ( lhP - 2 * pad + rg) / (rowH + rg));
            Menu::FrostBox(c, solid, L, top, lw2, lhP, 18 * u);
            F.prow = std::max(0, std::min(F.prow, n - 1));
            if (!F.freeScroll) { if (F.prow < F.rScroll) F.rScroll = F.prow; if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1; } // (a finger scrolling: the list stays where it was put)
            F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
            F.pact = -1;
            F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
            F.aW = 0;
            auto row = [&](int i, int icon, const std::string& name, const std::string& sub, const std::string& chip) {
                const int k = i - F.rScroll;
                if (k < 0 || k >= vis) return;
                const int rx = L + pad, ry = top + pad + k * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
                if (i == F.prow)
                {
                    if (F.side < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                    Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
                }
                else Glass::Round(c, rx, ry, rww, rowH, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
                Glass::DrawIcon(c, icon, rx + 28 * u, mid, 11 * u, text);
                GT(c, rx + (int)(56 * u), mid - 9 * u, name, 17 * u, text, true);
                GTFit(c, rx + (int)(56 * u), mid + 11 * u, sub, 12 * u, rww - (int)(56 * u) - (chip.empty() ? (int)(40 * u) : GW(chip, 12 * u, true) + (int)(80 * u)), hint, false);
                if (!chip.empty())
                {
                    const int chipW = GW(chip, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(42 * u) - chipW;
                    Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
                    GT(c, chipX + (int)(10 * u), mid, chip, 12 * u, text, true);
                }
                GT(c, rx + rww - (int)(24 * u), mid, ">", 15 * u, hint, true);
            };
            // (The rows first: the guard below puts back what was above the detail panel.)
            if (LauncherMode() == 0)
            {
                const std::string count = std::to_string(ts.list.size()) + (ts.list.size() == 1 ? " Theme" : " Themes");
                row(0, Glass::I_LAYOUT, "Layout", "Choose a home layout; Grid Only hides Continue Playing and shows more games", GridOnly() ? "Grid Only" : TilesOn() ? "Tiles" : PanelOn() ? "Panel" : "Sidebar");
                row(1, Glass::I_SPARKLE, "Theme Editor", "Colours, pictures, blur and the rest of the launcher's look", "");
                row(2, Glass::I_LAYERS, "My Themes", "Save this look, switch between your looks, share them as files", count);
                row(3, Glass::I_GLOBE, "Theme Gallery", "Themes other players have shared, ready to try", "Online");
#ifndef __SWITCH__
                row(4, Glass::I_NOTE, "Music Player", "Where the Now Playing player sits: full size, a small pill of buttons, or off", Look().music == "top" ? "Top" : Look().music == "compact" ? "Compact Top" : Look().music == "compactb" ? "Compact Bottom" : Look().music == "off" ? "Off" : "Bottom Left");
#endif
#ifdef __ANDROID__
                row(5, Glass::I_FULLSCREEN, "Camera Cutout", "Keep every launcher screen clear of the camera hole, or fill the whole screen", Look().avoidCutout ? "Avoid" : "Fill");
#endif
            }
            else
            {
                row(0, Glass::I_SAVE, "Save This Look", "Keep the look you have now as a theme", "");
#if defined(__SWITCH__) || defined(__ANDROID__)
                row(1, Glass::I_LOAD, "Import A Theme", "From the themes folder", "");
#else
                row(1, Glass::I_LOAD, "Import A Theme", "A .zip theme someone sent you", "");
#endif
                row(2, Glass::I_GRID, "Theme Library", "All your saved themes, as pictures", std::to_string(ts.list.size()) + (ts.list.size() == 1 ? " Theme" : " Themes"));
                row(3, Glass::I_REWIND, "Default Look", "Frosted glass over the game's art, as Fulldex starts", "");
            }
            const int dx = stkP ? L : R - rw, ix = dx + (int)(22 * u), iw = rw - (int)(44 * u);
            Menu::FrostBox(c, solid, dx, dTopP, rw, dHP, 18 * u);
            static int dKey = -1; { const int nk = F.page * 100000 + F.prow * 100 + LauncherMode() * 10 + HubMode() + (Look().edit + 1) * 10000000; if (nk != dKey) { dKey = nk; F.dScroll = 0; } }
            F.dScroll = std::min(F.dScroll, F.dMax); TextLow() = 0; F.pdY = stkP ? dTopP : 0;
            const int dTopC = dTopP - (stkP ? F.dScroll : 0);
            ClipGuard clipG(c, F, stkP ? dTopP : 0, stkP ? bottom : c.h, (int)(16 * u));
            int y = dTopC + (int)(22 * u);
            auto para = [&](const std::string& s, uint32_t col, float px) {
                for (const std::string& line : WrapLines(s, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
                y += (int)(8 * u);
            };
            if (LauncherMode() == 0)
            {
                static const char* const titles[6] = { "Layout", "Theme Editor", "My Themes", "Theme Gallery", "Music Player", "Camera Cutout" };
                static const char* const about[6] = {
                    "How the home screen is set out. Sidebar has Continue Playing above the games. Grid Only hides that banner and uses the space for more rows. Tiles has a game carousel. Choose it to switch, or use left and right.",
                    "Change the launcher's look: the accent, the panels, the sidebar and the top bar, the buttons, the background, its blur and dim, and the glass's grain. Each can be a colour or a picture.",
                    "Keep the look you have now as a theme, with its pictures, and switch between your themes. Each is one .zip file you can send to a friend.",
                    "Browse themes other players have shared, try one on before you keep it, enter a theme code from a friend, or share one of your own.",
                    "Shows the song playing on your device with previous, play and next. Bottom Left: a square at the foot of the sidebar. Top: a bar along the top. Compact Top and Compact Bottom: just previous, play and next in a small pill, along the top or at the foot of the sidebar. Off: no player. Its buttons use your Buttons look from the Theme Editor.",
                    "Avoid: every launcher screen keeps clear of the phone's camera hole, and the strip beside it stays black. Fill: the launcher runs across the whole screen, behind the camera hole, as the games do. Choose it to switch." };
                GT(c, ix, y + 14 * u, titles[F.prow], 22 * u, text, true);
                y += (int)(44 * u);
                if (F.prow == 0)
                {
                    y += DrawLayoutPreviews(c, ix, y, iw, u, dTopP, bottom) + (int)(12 * u);
                    TextLow() = std::max(TextLow(), y);
                }
                para(about[F.prow], text, 14 * u);
                if (F.prow == 4)
                {
                    // (A small picture of each choice, the chosen one outlined.)
                    static const char* const keys[5] = { "corner", "top", "compact", "compactb", "off" };
                    static const char* const names[5] = { "Bottom Left", "Top", "Compact Top", "Compact Bottom", "Off" };
                    const int gp = (int)(8 * u), tw = (iw - 4 * gp) / 5, th = (int)(tw * 0.64f);
                    for (int k = 0; k < 5; k++)
                    {
                        const int tx = ix + k * (tw + gp), ty = y + (int)(6 * u);
                        const bool on = Look().music == keys[k];
                        Glass::Round(c, tx, ty, tw, th, 8 * u, 0xFF141C28u, on ? 0xFFFFFFFFu : 0x3DFFFFFFu, (on ? 2.0f : 1.0f) * u);
                        const int sx = tx + (int)(5 * u), sy = ty + (int)(5 * u), sw = tw * 22 / 100, sh = th - (int)(10 * u);
                        Glass::Round(c, sx, sy, sw, sh, 4 * u, 0xFF26303Fu, 0, 0);
                        const int gx = sx + sw + (int)(5 * u), gw = tx + tw - (int)(5 * u) - gx, bh = (int)(th * 0.14f);
                        Glass::Round(c, gx, sy, gw, bh, bh * 0.5f, 0xFF26303Fu, 0, 0); // (the dTopC bar)
                        for (int g = 0; g < 6; g++)
                        {
                            const int cw = (gw - 2 * (int)(3 * u)) / 3, ch = (int)((sh - bh - (int)(8 * u)) / 2.0f) - (int)(3 * u);
                            Glass::Round(c, gx + (g % 3) * (cw + (int)(3 * u)), sy + bh + (int)(5 * u) + (g / 3) * (ch + (int)(3 * u)), cw, ch, 3 * u, 0xFF1E2836u, 0, 0);
                        }
                        const uint32_t lit = 0xFF8FB8FFu;
                        if (k == 0) Glass::Round(c, sx + (int)(2 * u), sy + sh - sw + (int)(2 * u), sw - (int)(4 * u), sw - (int)(4 * u), 3 * u, lit, 0, 0);
                        else if (k == 1) Glass::Round(c, gx + gw / 4, sy + (int)(bh * 0.2f), gw / 2, (int)(bh * 0.6f), bh * 0.3f, lit, 0, 0);
                        else if (k == 2) Glass::Round(c, gx + gw - gw / 4 - (int)(2 * u), sy + (int)(bh * 0.2f), gw / 4, (int)(bh * 0.6f), bh * 0.3f, lit, 0, 0);
                        else if (k == 3) { const int pw = sw - (int)(4 * u), ph = std::max(2, (int)(pw * 0.4f)); Glass::Round(c, sx + (int)(2 * u), sy + sh - ph - (int)(2 * u), pw, ph, ph * 0.5f, lit, 0, 0); }
                        const float lp = 11 * u;
                        const std::string nm = names[k];
                        const size_t sp1 = nm.find(' ');
                        const std::string l1 = sp1 == std::string::npos ? nm : nm.substr(0, sp1), l2 = sp1 == std::string::npos ? std::string() : nm.substr(sp1 + 1);
                        GT(c, tx + (tw - GW(l1, lp, on)) / 2, ty + th + (int)(10 * u), l1, lp, on ? text : hint, on);
                        if (!l2.empty()) GT(c, tx + (tw - GW(l2, lp, on)) / 2, ty + th + (int)(24 * u), l2, lp, on ? text : hint, on);
                    }
                    y += (int)(6 * u) + th + (int)(40 * u);
                }
            }
            else
            {
                if (F.prow == 0)
                {
                    GT(c, ix, y + 14 * u, "Save This Look", 22 * u, text, true);
                    y += (int)(44 * u);
                    para("Keeps your accent, panels, bars, buttons, background, blur, dim and grain, with their pictures (up to 4K), as one .zip in the themes folder, with a picture of your Home screen wearing it.", text, 14 * u);
                    para("You name it next. A theme can be up to 20 MB.", hint, 13 * u);
                }
                else if (F.prow == 1)
                {
                    GT(c, ix, y + 14 * u, "Import A Theme", 22 * u, text, true);
                    y += (int)(44 * u);
#ifdef __SWITCH__
                    para("Put .zip themes in /switch/pokemon-ds-wide/themes/ on the SD card and they show in this list.", text, 14 * u);
#else
                    para("Choose a .zip theme someone sent you. It is copied into your themes folder and shows in this list.", text, 14 * u);
#endif
                }
                else if (F.prow == 3)
                {
                    GT(c, ix, y + 14 * u, "Default Look", 22 * u, text, true);
                    y += (int)(44 * u);
                    para("Puts back the look Fulldex starts with: frosted glass over the chosen game's art, white highlights, the usual blur and dim.", text, 14 * u);
                    para("Your saved themes and pictures stay. Save This Look first if you want to come back to the look you have now.", hint, 13 * u);
                }
                else
                {
                    GT(c, ix, y + 14 * u, "Theme Library", 22 * u, text, true);
                    y += (int)(44 * u);
                    para("All your saved themes as pictures of Home wearing them, a page at a time. Choose one to apply it.", text, 14 * u);
                    // (The first four, small.)
                    const int g = (int)(10 * u), tw = (iw - g) / 2, th = tw * 9 / 16;
                    for (int t = 0; t < std::min(4, (int)ts.list.size()); t++)
                    {
                        const int tx = ix + (t % 2) * (tw + g), ty = y + (t / 2) * (th + g);
                        if (const Glass::Picture* pv = ThemePreview(ts.list[(size_t)t])) Glass::PictureIn(c, *pv, tx, ty, tw, th, 8 * u, 255);
                        else Glass::Round(c, tx, ty, tw, th, 8 * u, 0x24FFFFFFu);
                        Glass::Round(c, tx, ty, tw, th, 8 * u, 0, 0x50FFFFFFu, 1.0f * u);
                    }
                    if (ts.list.empty()) para("No themes yet: Save This Look makes one.", hint, 13 * u);
                    else y += 2 * (th + g);
                }
            }
            if (!ts.msg.empty()) para(ts.msg, 0xFF8CC8FFu, 13 * u);
            return;
        }
        const bool stkP = PhoneLook() && c.h > c.w; const int rw = stkP ? R - L : (int)((ShortNow() && LauncherMode() == 1 && Look().edit >= 0 ? 560 : 400) * u), gap = (int)(18 * u), lw2 = stkP ? R - L : R - L - rw - gap, lhP = stkP ? (int)((bottom - top) * (Look().edit >= 0 ? 0.17f : 0.55f)) : bottom - top, dTopP = stkP ? top + lhP + gap : top, dHP = stkP ? bottom - dTopP : bottom - top, pad = (int)(14 * u); // (A phone sideways, a row open: a wider page, so the colour wheel can be big beside the bars)
        const int rowH = (int)(46 * u), rg = (int)(6 * u), n = 11;
        Menu::FrostBox(c, solid, L, top, lw2, lhP, 18 * u);
        if (lk.edit >= 0) F.prow = lk.edit == 8 ? 9 : lk.edit;
        F.prow = std::max(0, std::min(F.prow, n - 1));
        F.pact = -1;
        const int vis = std::max(1, std::min(n, ( lhP - 2 * pad + rg) / (rowH + rg))); // (a short screen: the list scrolls)
        if (!F.freeScroll) { if (F.prow < F.rScroll) F.rScroll = F.prow; if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1; }
        F.rScroll = std::max(0, std::min(F.rScroll, n - vis));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        F.aW = 0;
        static const char* const names[11] = { "Accent", "Panels", "Sidebar", "Top Bar", "Buttons", "Background", "Grain", "Cloud Badges", "Font", "Text Size", "Text Colour" };
        static const char* const subs[11] = { "The frame round what is chosen, and its glow", "The glass panels: a colour or a picture, see-through to solid",
                                             "The bar down the left: a colour or a picture", "The bar along the top: a colour or a picture",
                                             "Play, Continue, the chosen tab and the button marks at the bottom", "Game art, your own picture or a colour; its blur and dim",
                                             "The fine speckle in the glass panels", "A cloud on the cards of games whose saves sync",
                                              "The letters everywhere in the launcher", "How big the writing is", "The colour of the writing" };
        static const int icons[11] = { Glass::I_SPARKLE, Glass::I_LAYERS, Glass::I_LAYOUT, Glass::I_WINDOW, Glass::I_PLAY, Glass::I_SCREENS, Glass::I_GAUGE, Glass::I_GLOBE, Glass::I_NOTE, Glass::I_EXPAND, Glass::I_CHAT };
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && lk.edit < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            else Glass::Round(c, rx, ry, rww, rowH, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
            std::string st;
            if (i == 0) st = LookName(lk.col[0]);
            else if (i <= 4) { st = lk.pic[i].empty() ? LookName(lk.col[i]) : "Picture"; if (lk.op[i] >= 0 || !lk.pic[i].empty()) st += "  " + std::to_string(LookOpacity(i)) + "%"; }
            else if (i == 5) st = LookName(lk.backdrop) + "   Blur " + std::to_string(lk.blur) + "%   Dim " + std::to_string(lk.dim) + "%";
            else if (i == 6) st = std::to_string(lk.grain) + "%";
            else if (i == 7) st = lk.badges ? "On" : "Off";
            else if (i == 8) st = kLauncherFonts[LookFontId()].name;
            else if (i == 9) st = std::to_string(lk.fontSize) + "%";
            else st = LookName(lk.col[10]);
            Glass::DrawIcon(c, icons[i], rx + 28 * u, mid, 11 * u, text);
            GT(c, rx + (int)(56 * u), mid - 8 * u, names[i], 16 * u, text, true);
            GTFit(c, rx + (int)(56 * u), mid + 10 * u, subs[i], 12 * u, rww - (int)(56 * u) - GW(st, 12 * u, true) - (int)(100 * u), hint, false);
            const bool hasDot = i <= 4 || i == 10 || (i == 5 && lk.backdrop == "colour");
            const int dot = hasDot ? (int)(14 * u) : 0;
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u) + (dot ? dot + (int)(6 * u) : 0), chipX = rx + rww - (int)(42 * u) - chipW;
            Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            if (dot)
            {
                const uint32_t dc = i >= 1 && i <= 4 && !lk.pic[i].empty() ? Glass::SurfacePic(i - 1).avg : LookRgb(i == 10 ? 10 : i <= 4 ? i : 5);
                Glass::Round(c, chipX + (int)(8 * u), mid - dot / 2, dot, dot, dot * 0.5f, dc, 0xB0FFFFFFu, 1.0f * u);
            }
            GT(c, chipX + (int)(10 * u) + (dot ? dot + (int)(6 * u) : 0), mid, st, 12 * u, i == 7 && !lk.badges ? 0xFFAEB7C4u : text, true);
            GT(c, rx + rww - (int)(24 * u), mid, ">", 15 * u, hint, true);
        }
        // The chosen one: what it does; its own page when open.
        const int dx = stkP ? L : R - rw, ix = dx + (int)(22 * u), iw = rw - (int)(44 * u);
        Menu::FrostBox(c, solid, dx, dTopP, rw, dHP, 18 * u);
        static int dKey = -1; { const int nk = F.page * 100000 + F.prow * 100 + LauncherMode() * 10 + HubMode() + (Look().edit + 1) * 10000000; if (nk != dKey) { dKey = nk; F.dScroll = 0; } }
        F.dScroll = std::min(F.dScroll, F.dMax); TextLow() = 0; F.pdY = stkP ? dTopP : 0;
        const int dTopC = dTopP - (stkP ? F.dScroll : 0);
        ClipGuard clipG(c, F, stkP ? dTopP : 0, stkP ? bottom : c.h, (int)(16 * u));
        int y = dTopC + (int)(22 * u);
        const bool side = rw > (int)(450 * u); // (the wheel beside the bars, not above them)
        int fieldsW = iw;
        auto para = [&](const std::string& s, uint32_t col, float px) {
            for (const std::string& line : WrapLines(s, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        GT(c, ix, y + 14 * u, names[F.prow], 22 * u, text, true);
        y += (int)(44 * u);
        lk.wr = 0; lk.fw = 0; lk.pn = 0;
        // A bar: its colours along it, a white knob at t.
        auto bar = [&](int k, int fy, int bx, int bw, int fh, float t, auto colourAt) {
            const int fm = fy + fh / 2, bh = (int)(12 * u), by = fm - bh / 2;
            for (int xx = 0; xx < bw; xx++) c.Fill(bx + xx, by, 1, bh, colourAt((float)xx / std::max(1, bw - 1)));
            Glass::Round(c, bx, by, bw, bh, bh * 0.5f, 0, 0x80FFFFFFu, 1.0f * u);
            const int hx = bx + (int)(t * (bw - 1)), hr = (int)(8 * u);
            Glass::Round(c, hx - hr, fm - hr, 2 * hr, 2 * hr, (float)hr, 0xFFFFFFFFu, 0xFF18222Eu, 1.5f * u);
            lk.fx[k] = bx; lk.fy[k] = fy;
        };
        if (lk.edit >= 0)
        {
            const int e = lk.edit;
            const std::vector<int> fl = LookFieldList(e);
            if (e == 6 || e == 8) para(e == 6 ? "The fine speckle in the frosted glass panels: 0% makes them smooth, 25% is as they were, 100% is four times as strong." : "How big the writing is in the launcher, from 85% to 120%. Each font is already sized to fill a line as the others do.", text, 14 * u);
            else
            {
                // The wheel: round the edge the colours, out from the middle stronger; at the Brightness below.
                const float wk = c.h > c.w ? 1.45f : 1.0f; // (upright: the wheel bigger)
                const int fitD = side ? bottom - y - (int)(12 * u) : bottom - y - (int)(12 * u) - (int)fl.size() * (int)(35 * u) - (int)(62 * u), D = std::max((int)(36 * u), std::min(fitD, std::min(side ? (int)(iw * 0.36f) : iw - (int)(40 * u), side ? 99999 : fl.size() >= 8 ? (int)(96 * wk * u) : fl.size() >= 6 ? (int)(128 * wk * u) : fl.size() == 5 ? (int)(146 * wk * u) : (int)(170 * wk * u)))), wr = D / 2, wcx = side ? ix + iw - wr : ix + iw / 2, wcy = y + wr;
                for (int py = -wr - 1; py <= wr + 1; py++)
                {
                    const int yy = wcy + py;
                    if (yy < 0 || yy >= c.h) continue;
                    for (int qx = -wr - 1; qx <= wr + 1; qx++)
                    {
                        const int xx = wcx + qx;
                        if (xx < 0 || xx >= c.w) continue;
                        const float dd = std::sqrt((float)(qx * qx + py * py));
                        const float a = std::max(0.0f, std::min(1.0f, wr - dd + 0.5f));
                        if (a <= 0) continue;
                        uint32_t& dst = c.px[(size_t)yy * c.w + xx];
                        dst = Glass::Over(dst, Hsv(std::atan2((float)py, (float)qx) * 57.29578f, std::min(1.0f, dd / wr), lk.v), (int)(a * 255));
                    }
                }
                const float ang = lk.h / 57.29578f;
                const int mx = wcx + (int)(std::cos(ang) * lk.s * wr), my = wcy + (int)(std::sin(ang) * lk.s * wr), mr = (int)(9 * u);
                Glass::Round(c, mx - mr, my - mr, 2 * mr, 2 * mr, (float)mr, Hsv(lk.h, lk.s, lk.v), 0xFFFFFFFFu, 2.5f * u);
                lk.wx = wcx; lk.wy = wcy; lk.wr = wr;
                if (side) fieldsW = iw - D - (int)(16 * u); else y += D + (int)(12 * u);
            }
            const int fh = (int)(30 * u), bx = ix + (int)(100 * u), bw = fieldsW - (int)(104 * u);
            lk.fw = bw; lk.fh = fh; lk.rowX = ix - (int)(8 * u); lk.rowW = fieldsW + (int)(16 * u);
            for (int k = 0; k < (int)fl.size(); k++)
            {
                const int id = fl[k], fy = y + k * (fh + (int)(5 * u)), fm = fy + fh / 2;
                if (k == lk.field) Glass::Round(c, ix - (int)(8 * u), fy, fieldsW + (int)(16 * u), fh, 10 * u, 0x1FFFFFFFu, Glass::ChosenEdge(), 1.5f * u);
                const std::string label = id == LF_SOURCE ? "Show" : id == LF_PIC ? "Picture" : id == LF_PRESET ? "Preset" : id == LF_HUE ? "Hue" : id == LF_SAT ? "Saturation" : id == LF_VAL ? "Brightness"
                                        : id == LF_OP ? "Opacity " + std::to_string(LookOpacity(e)) + "%" : id == LF_BLUR ? "Blur " + std::to_string(lk.blur) + "%" : id == LF_DIM ? "Dim " + std::to_string(lk.dim) + "%" : id == LF_SIZE ? "Size " + std::to_string(lk.fontSize) + "%" : "Grain " + std::to_string(lk.grain) + "%";
                GT(c, ix, fm, id == LF_CURSOR ? std::string("Selector") : label, 13 * u, k == lk.field ? text : hint, true);
                lk.fx[k] = bx; lk.fy[k] = fy;
                if (id == LF_CURSOR)
                {
                    GT(c, bx, fm, "<", 13 * u, hint, true);
                    GT(c, bx + (int)(16 * u), fm, lk.cursorButton ? "Button Colour" : "Accent Colour", 13 * u, text, true);
                    GT(c, bx + bw - (int)(8 * u), fm, ">", 13 * u, hint, true);
                    continue;
                }
                if (id == LF_SOURCE)
                {
                    GT(c, bx, fm, "<", 13 * u, hint, true);
                    GT(c, bx + (int)(16 * u), fm, LookName(lk.backdrop), 13 * u, text, true);
                    GT(c, bx + bw - (int)(8 * u), fm, ">", 13 * u, hint, true);
                    continue;
                }
                if (id == LF_PIC)
                {
                    std::string nm = lk.pic[e].empty() ? "None" : lk.pic[e];
                    while (nm.size() > 4 && GW(nm, 13 * u, true) > bw - (int)(40 * u)) nm = nm.substr(0, nm.size() - 4) + "...";
                    GT(c, bx, fm, "<", 13 * u, hint, true);
                    GT(c, bx + (int)(16 * u), fm, nm, 13 * u, lk.pic[e].empty() ? hint : text, true);
                    GT(c, bx + bw - (int)(8 * u), fm, ">", 13 * u, hint, true);
                    continue;
                }
                if (id == LF_PRESET)
                {
                    int pn = 0;
                    const char* const* list = LookPresets(e, pn);
                    const int pd = std::min((int)(18 * u), bw / pn - (int)(5 * u)), pg = (bw - pn * pd) / std::max(1, pn - 1);
                    lk.pn = pn; lk.pd = pd; lk.py = fm - pd / 2;
                    for (int i = 0; i < pn; i++)
                    {
                        const int sx = bx + i * (pd + pg);
                        lk.px[i] = sx;
                        const std::string nm = list[i];
                        if (lk.pic[e].empty() && lk.col[e] == nm) Glass::Round(c, sx - (int)(3 * u), lk.py - (int)(3 * u), pd + (int)(6 * u), pd + (int)(6 * u), (pd + 6 * u) * 0.5f, 0, 0xF2FFFFFFu, 1.5f * u);
                        Glass::Round(c, sx, lk.py, pd, pd, pd * 0.5f, nm == "clear" ? 0x30FFFFFFu : SpecRgb(nm, 0xFFFFFFFFu), 0x80FFFFFFu, 1.0f * u);
                    }
                    continue;
                }
                const float hh = lk.h, ss = lk.s, vv = lk.v;
                const uint32_t rgb = e < 6 ? LookRgb(e) : 0;
                if (id == LF_HUE) bar(k, fy, bx, bw, fh, hh / 360.0f, [](float t) { return Hsv(t * 360.0f, 1, 1); });
                else if (id == LF_SAT) bar(k, fy, bx, bw, fh, ss, [hh, vv](float t) { return Hsv(hh, t, vv); });
                else if (id == LF_VAL) bar(k, fy, bx, bw, fh, vv, [hh, ss](float t) { return Hsv(hh, ss, t); });
                else if (id == LF_OP) bar(k, fy, bx, bw, fh, LookOpacity(e) / 100.0f, [rgb](float t) { return Glass::Over(0xFF3A4450u, rgb, (int)(t * 255)); });
                else if (id == LF_BLUR) bar(k, fy, bx, bw, fh, lk.blur / 100.0f, [](float t) { const uint32_t g = (uint32_t)(60 + t * 150); return 0xFF000000u | g << 16 | g << 8 | g; });
                else if (id == LF_DIM) bar(k, fy, bx, bw, fh, lk.dim / 100.0f, [](float t) { const uint32_t g = (uint32_t)(235 - t * 215); return 0xFF000000u | g << 16 | g << 8 | g; });
                else if (id == LF_SIZE) bar(k, fy, bx, bw, fh, (lk.fontSize - 85) / 35.0f, [](float t) { const uint32_t g = (uint32_t)(90 + t * 120); return 0xFF000000u | g << 16 | g << 8 | g; });
                else bar(k, fy, bx, bw, fh, lk.grain / 100.0f, [&](float t) {
                    // (Speckled more along it.)
                    uint32_t hsh = (uint32_t)(t * 9973) * 374761393u; hsh = (hsh ^ (hsh >> 13)) * 1274126177u;
                    const int g = std::max(0, std::min(255, 120 + (int)(((int)((hsh >> 24) & 15) - 8) * t * 12)));
                    return 0xFF000000u | (uint32_t)g << 16 | (uint32_t)g << 8 | (uint32_t)g; });
            }
            y += (int)fl.size() * (fh + (int)(5 * u)) + (int)(8 * u);
            const bool pics = e >= 1 && e <= 5;
            if (pics && (e != 5 || lk.backdrop == "picture")) para(BestPicture(e), text, 12 * u);
#ifdef __SWITCH__
            para(std::string("Up and down choose a row, left and right change it.") + (pics ? " Pictures: put PNG or JPG files in /switch/pokemon-ds-wide/launcher-pictures/." : "") + " A or B: done.", hint, 12 * u);
#else
            para(std::string("Arrows choose and change, or click.") + (pics ? " On Picture, Enter or a click picks a file." : "") + " Esc: done.", hint, 12 * u);
#endif
            return;
        }
        static const char* const about[11] = {
            "The colour of the frame round what is chosen, and its glow, on every page of the launcher.",
            "A colour or a picture over every glass panel. Opacity sets how much: low lets the background through, 100% makes them solid.",
            "A colour or a picture for the sidebar, see-through to solid.",
            "A colour or a picture for the bar along the top, see-through to solid.",
            "Used for Play and Continue, the chosen tab, and the button marks along the bottom (Choose, Back and the rest). Their words and marks turn dark or light to stay readable.",
            "Game Art: the chosen game's picture. Your Picture: one of your own. Colour: one colour. Plain: a calm blue. Its page also sets how blurred and how dim it is.",
            "The fine speckle in the frosted glass panels: 0% makes them smooth, 25% is as they were.",
            "With Cloud Saves on, a cloud shows in the corner of each game whose save syncs. Amber: it changed on two devices and needs a choice.",
            "The letters everywhere in the launcher: 30 to choose from, clean, rounded, bold, retro, fun, handwritten and easy to read. The pause menu keeps its own.",
            "How big the writing is in the launcher, from 85% to 120%.",
            "The colour of the writing everywhere in the launcher. Choose one that reads well on your panels and background." };
        para(about[F.prow], text, 14 * u);
        if (F.prow >= 1 && F.prow <= 5) para(BestPicture(F.prow), text, 13 * u);
        if (F.prow <= 4 || F.prow == 10 || (F.prow == 5 && (lk.backdrop == "colour" || lk.backdrop == "picture")))
        {
            const int e = F.prow, cw = (int)(64 * u);
            if (e >= 1 && e <= 4 && !lk.pic[e].empty()) Glass::PictureIn(c, Glass::SurfacePic(e - 1), ix, y, cw, cw, 14 * u, 255);
            else if (!(e == 5 && lk.backdrop == "picture" && !lk.pic[5].empty()))
                Glass::Round(c, ix, y, cw, cw, 14 * u, LookHasOp(e) ? Glass::Over(0xFF3A4450u, LookRgb(e), LookOpacity(e) * 255 / 100) : LookRgb(e), 0xB0FFFFFFu, 1.0f * u);
            const bool p = e > 0 && !lk.pic[e].empty() && (e != 5 || lk.backdrop == "picture");
            const int tx = e == 5 && p ? ix : ix + cw + (int)(14 * u);
            GT(c, tx, y + cw / 2 - 9 * u, p ? lk.pic[e] : LookName(lk.col[e]), 16 * u, text, true);
            std::string sub2 = !p && lk.col[e][0] == '#' ? lk.col[e] : std::string();
            if (LookHasOp(e)) sub2 += (sub2.empty() ? "" : "   ") + std::string("Opacity ") + std::to_string(LookOpacity(e)) + "%";
            if (!sub2.empty()) GT(c, tx, y + cw / 2 + 11 * u, sub2, 12 * u, hint, false);
            y += cw + (int)(16 * u);
        }
#ifdef __SWITCH__
        para(F.prow == 7 ? "Press A to change it." : "Press A to open its page.", hint, 13 * u);
#else
        para(F.prow == 7 ? "Choose it to change it." : "Choose it to open its page.", hint, 13 * u);
#endif
        return;
    }
    if (F.page == SIDE_HUB && !Mover().open && HubMode() == 4) { DrawDexPage(c, solid, u, L, R, top, bottom); return; }
    if (F.page == SIDE_HUB && !Mover().open && HubMode() == 3) { DrawSavesPage(c, solid, u, L, R, top, bottom); return; } // (Manage Saves)
    if (F.page == SIDE_HUB && !Mover().open && HubMode() == 0)
    {
        // The Hub's first page: what it holds, each opened with A.
        HubLoad();
        const std::vector<HubGame> games = HubGames();
        HubState& h = Hub();
        std::unique_lock<std::mutex> lk(h.m);
        const bool on = HubOnLocked(h);
        const int busy = h.busy;
        const std::string msg = h.msg, prov = h.set.provider;
        const std::string dev = h.set.deviceName.empty() ? HubDevice() : h.set.deviceName;
        int synced = 0;
        for (const HubGame& g : games)
        {
            const auto it = h.results.find(g.path);
            if (h.synced.count(g.g ? g.g->saveName : std::string()) || (it != h.results.end() && (it->second.outcome == CloudSaves::Outcome::InSync || it->second.outcome == CloudSaves::Outcome::Pushed || it->second.outcome == CloudSaves::Outcome::Pulled))) synced++;
        }
        lk.unlock();
        const int moverGames = (int)MoverGames().size();
        const bool stkP = PhoneLook() && c.h > c.w; const int rw = stkP ? R - L : (int)(400 * u), gap = (int)(18 * u), lw2 = stkP ? R - L : R - L - rw - gap, lhP = stkP ? (int)((bottom - top) * 0.55f) : bottom - top, dTopP = stkP ? top + lhP + gap : top, dHP = stkP ? bottom - dTopP : bottom - top, pad = (int)(14 * u);
        const int rg = (int)(6 * u), n = 5;
        const int rowH = std::max(24, std::min((int)(64 * u), (lhP - 2 * pad) / n - rg));
        const HubLink::Status link = HubLink::Snapshot();
        Menu::FrostBox(c, solid, L, top, lw2, lhP, 18 * u);
        F.prow = std::max(0, std::min(F.prow, n - 1));
        F.pact = -1;
        F.rScroll = 0;
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = n;
        F.aW = 0;
        for (int i = 0; i < n; i++)
        {
            const int rx = L + pad, ry = top + pad + i * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0) Glass::Glow(c, rx, ry, rww, rowH, 14 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 14 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            else Glass::Round(c, rx, ry, rww, rowH, 14 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
            std::string name, sub, st;
            uint32_t col = 0xFFAEB7C4u;
            if (i == 0)
            {
                name = "Cloud Saves";
                sub = on ? std::string(HubProvider(prov)) + "   \xC2\xB7   " + std::to_string(synced) + " of " + std::to_string(games.size()) + " up to date"
                         : "Keep your saves the same on every device";
                if (busy == 1) { st = "Signing In"; col = 0xFFF2C46Bu; }
                else if (busy == 2) { st = "Syncing"; col = 0xFF8CC8FFu; }
                else if (on) { st = "On"; col = 0xFF8FE3A2u; }
                else st = "Off";
            }
            else if (i == 2)
            {
                name = "Trade";
                sub = "With a friend online or on the same Wi-Fi, or to another of your devices";
                st = link.state == HubLink::State::Linked ? "Linked" : link.state == HubLink::State::Online ? "Online" : link.state == HubLink::State::Hosting ? "Room Open" : link.state == HubLink::State::Browsing ? "Looking" : "Same Wi-Fi";
                if (link.state != HubLink::State::Off) col = 0xFF8CC8FFu;
            }
            else if (i == 4)
            {
                name = "National Dex"; sub = "Combine Pokédex progress from your selected saves"; st = "Kanto–Kalos";
            }
            else if (i == 3)
            {
                name = "Fulldex Discord";
                sub = "Find people to trade with, get help and hear what's coming";
                st = "Join";
                col = 0xFF8CC8FFu;
            }
            else
            {
                name = "Move Pok\xC3\xA9mon";
                sub = "Between your own games, as a trade would";
                st = std::to_string(moverGames) + (moverGames == 1 ? " Save" : " Saves");
            }
            Glass::DrawIcon(c, i == 0 ? Glass::I_GLOBE : i == 2 ? Glass::I_PEOPLE : i == 3 ? Glass::I_CHAT : Glass::I_POKEBALL, rx + 30 * u, mid, 12 * u, text);
            GT(c, rx + (int)(60 * u), mid - 11 * u, name, 18 * u, text, true);
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(46 * u) - chipW;
            GTFit(c, rx + (int)(60 * u), mid + 13 * u, sub, 13 * u, chipX - (rx + (int)(60 * u)) - (int)(12 * u), hint, false);
            Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, chipX + (int)(10 * u), mid, st, 12 * u, col, true);
            GT(c, rx + rww - (int)(26 * u), mid, ">", 16 * u, hint, true);
        }
        // The chosen one: what it does, how it stands.
        const int dx = stkP ? L : R - rw, ix = dx + (int)(22 * u), iw = rw - (int)(44 * u);
        Menu::FrostBox(c, solid, dx, dTopP, rw, dHP, 18 * u);
        static int dKey = -1; { const int nk = F.page * 100000 + F.prow * 100 + LauncherMode() * 10 + HubMode() + (Look().edit + 1) * 10000000; if (nk != dKey) { dKey = nk; F.dScroll = 0; } }
        F.dScroll = std::min(F.dScroll, F.dMax); TextLow() = 0; F.pdY = stkP ? dTopP : 0;
        const int dTopC = dTopP - (stkP ? F.dScroll : 0);
        ClipGuard clipG(c, F, stkP ? dTopP : 0, stkP ? bottom : c.h, (int)(16 * u));
        int y = dTopC + (int)(22 * u);
        auto para = [&](const std::string& s, uint32_t col, float px) {
            for (const std::string& line : WrapLines(s, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        auto cell = [&](int cx, int cy, int cw, const std::string& k, const std::string& v) {
            GT(c, cx, cy + 8 * u, k, 12 * u, hint, true);
            GT(c, cx, cy + 28 * u, v, 15 * u, text, true);
            (void)cw;
        };
        GT(c, ix, y + 14 * u, F.prow == 4 ? "National Dex" : F.prow == 0 ? "Cloud Saves" : F.prow == 2 ? "Trade" : F.prow == 3 ? "Fulldex Discord" : "Move Pok\xC3\xA9mon", 22 * u, text, true);
        y += (int)(44 * u);
        if (F.prow == 4)
        {
            para("Select your saves and regions to track seen and caught Pokémon together. Saves are only read, never changed.", text, 14 * u);
        }
        else if (F.prow == 0)
        {
            if (on)
            {
                cell(ix, y, iw / 2, "Saved To", HubProvider(prov));
                cell(ix + iw / 2, y, iw / 2, "This Device", dev);
                y += (int)(52 * u);
                cell(ix, y, iw / 2, "Up To Date", std::to_string(synced) + " of " + std::to_string(games.size()));
                y += (int)(60 * u);
                para("Saves sync when a game starts and when it closes, and the DS games after each save too. Open it to sync now, see each game, or choose which save to keep.", hint, 13 * u);
            }
            else
            {
                para("Keep your saves the same on every device: play on your PC, then carry on on the Switch.", text, 14 * u);
                para("Open it to sign in with Google. Your saves go to your own Drive, in a folder called Fulldex Cloud Saves.", hint, 13 * u);
            }
            if (!msg.empty()) para(msg, busy ? 0xFF8CC8FFu : hint, 13 * u);
        }
        else if (F.prow == 2)
        {
            para("Trade with a friend, online or on the same Wi-Fi, or send Pok\xC3\xA9mon to another of your own devices: a PC and a Switch, say.", text, 14 * u);
            para("Trades follow the real games' rules: one each way, both say yes, and those that evolve by trading evolve, Karrablast and Shelmet too. Each save is backed up before it is written.", hint, 13 * u);
        }
        else if (F.prow == 3)
        {
            para("Join the Fulldex Discord: find someone to trade with, get help with a game, report a bug and see what's coming next.", text, 14 * u);
            GT(c, ix, y + 10 * u, "discord.gg/pnGeMYyJR", 16 * u, 0xFF8CC8FFu, true);
            y += (int)(34 * u);
#ifdef __ANDROID__
            const int qs = 0; // (no QR codes on Android: the links are there to choose)
#else
            const int qs = std::max((int)(90 * u), std::min(iw - (int)(80 * u), bottom - y - (int)(70 * u)));
#endif
            DrawQr(c, kDiscordQr, kDiscordQrSize, ix + (iw - qs) / 2, y, qs);
            y += qs + (int)(18 * u);
#ifdef __SWITCH__
            para("Scan the code with your phone's camera.", hint, 13 * u);
#else
            para(
#ifdef __ANDROID__
                 "Choose it to open Discord in your browser."
#else
                 "Choose it to open Discord in your browser, or scan the code with your phone."
#endif
                 , hint, 13 * u);
#endif
        }
        else
        {
            para("Move a Pok\xC3\xA9mon from one of your games to another, as a trade between them would.", text, 14 * u);
            para("They can go to a newer game, never an older one. Fulldex says first what one would lose on the way, and backs up both saves before it writes them.", hint, 13 * u);
            cell(ix, y, iw, "Games With A Save", std::to_string(moverGames));
            y += (int)(56 * u);
        }
        return;
    }
    if (F.page == SIDE_HUB && Mover().open)
    {
        MoverState& m = Mover();
        const std::vector<MoverGame> gs = MoverGames();
        const std::vector<std::string> acts = HubActions(F.prow);
        auto fit = [&](std::string s, float px, bool bold, int w) {
            if (GW(s, px, bold) <= w) return s;
            while (!s.empty() && GW(s + "...", px, bold) > w)
            {
                while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
                if (!s.empty()) s.pop_back();
            }
            return s + "...";
        };
        const uint32_t okCol = 0xFF8FE3A2u, noCol = 0xFFF2C46Bu, greyCol = 0xFFAEB7C4u, swapCol = 0xFF8CC8FFu;
        const std::string fromName = gs.empty() ? std::string("-") : gs[std::max(0, std::min(m.from, (int)gs.size() - 1))].name;
        const std::string toName = gs.empty() ? std::string("-") : gs[std::max(0, std::min(m.to, (int)gs.size() - 1))].name;
#ifdef SSR_POKEMON_HUB
        const std::vector<PokemonHub::Mon> mons = MoverList();
        auto check = [&](const PokemonHub::Mon& mon) {
            PokemonHub::Check ck;
            if (!m.a || !m.b) { ck.verdict = PokemonHub::Verdict::Empty; ck.reason = "That game's save could not be read."; }
            else if (m.from == m.to) { ck.verdict = PokemonHub::Verdict::Empty; ck.reason = "Choose two different games."; }
            else if (m.swapping) ck = PokemonHub::CanSwap(*m.a, m.swapBox, m.swapSlot, *m.b, mon.box, mon.slot);
            else ck = PokemonHub::CanMoveTo(*m.a, mon.box, mon.slot, *m.b, m.put, -1);
            return ck;
        };
#else
        const std::vector<int> mons;
#endif
        const int n = MoverRows();
        const bool stkP = PhoneLook() && c.h > c.w; const int rw = stkP ? R - L : (int)(360 * u), gap = (int)(18 * u), lw2 = stkP ? R - L : R - L - rw - gap, lhP = stkP ? (int)((bottom - top) * 0.55f) : bottom - top, dTopP = stkP ? top + lhP + gap : top, dHP = stkP ? bottom - dTopP : bottom - top, pad = (int)(12 * u);
        const int rowH = (int)(46 * u), rg = (int)(6 * u);
        Menu::FrostBox(c, solid, L, top, lw2, lhP, 18 * u);
        const int vis = std::max(1, ( lhP - 2 * pad + rg) / (rowH + rg));
        F.prow = std::max(0, std::min(F.prow, n - 1));
        if (F.pact >= (int)acts.size()) F.pact = acts.empty() ? -1 : (int)acts.size() - 1;
        if (!F.freeScroll) { if (F.prow < F.rScroll) F.rScroll = F.prow; if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1; } // (a finger scrolling: the list stays where it was put)
        F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && F.pact < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            else if (m.swapping && i == 3) Glass::Round(c, rx, ry, rww, rowH, 12 * u, 0x1A8CC8FFu);
            std::string k, v, chip, spn;
            std::vector<uint32_t> ic;
            uint32_t col = greyCol, chipCol = hint;
            int icon = Glass::I_POKEBALL;
            if (i == 0) { k = "From"; v = fromName; }
            else if (i == 1) { k = "Box"; v = "-"; icon = Glass::I_GRID; }
            else if (i == 2) { k = "To"; v = toName; }
            else if (i == 3) { k = m.swapping ? "Swap In" : "Put In"; v = "-"; icon = m.swapping ? Glass::I_GRID : Glass::I_LOAD; }
#ifdef SSR_POKEMON_HUB
            if (i == 0 && m.a) chip = Shown(m.a->Trainer(), "-");
            if (i == 2 && m.b) chip = Shown(m.b->Trainer(), "-");
            if (i == 1 && m.a)
            {
                v = BoxLabel(m.a.get(), m.box);
                const size_t cnt = MonsIn(m.a.get(), m.box).size();
                chip = std::to_string(cnt) + " Pok\xC3\xA9mon";
            }
            if (i == 3)
            {
                v = m.swapping ? BoxLabel(m.b.get(), MoverTargetBox()) : PutLabel();
                if (m.swapping) { chip = "Swapping " + m.swapName; chipCol = swapCol; }
                else if (m.b && m.put >= 0) chip = std::to_string(30 - (int)MonsIn(m.b.get(), m.put).size()) + " Free";
                else if (m.b && m.put == PokemonHub::TO_PARTY) chip = std::to_string(6 - (int)PokemonHub::Party(*m.b).size()) + " Free";
            }
            if (i >= MOVER_FIRST)
            {
                const PokemonHub::Mon& mon = mons[i - MOVER_FIRST];
                const PokemonHub::Check ck = check(mon);
                col = ck.verdict == PokemonHub::Verdict::Ok ? okCol : noCol;
                k = mon.egg ? std::string("Egg") : "Lv " + std::to_string(mon.level);
                v = Shown(mon.nickname, MonName(mon.speciesName));
                if (!mon.egg && !mon.speciesName.empty()) spn = MonName(mon.speciesName);
                chip = ck.verdict == PokemonHub::Verdict::Ok ? (m.swapping ? (ck.evolvesTo ? "Can Swap, Evolves" : "Can Swap") : (ck.evolvesTo ? "Can Move, Evolves" : "Can Move"))
                                                             : (m.swapping ? "Can't Swap" : "Can't Move");
                chipCol = col;
                if (MonIcon(mon.species, mon.egg, ic)) DrawMonIcon(c, ic, rx + (int)(2 * u), mid - (int)(21 * u), (int)(40 * u));
                else Glass::Round(c, rx + (int)(14 * u), mid - (int)(4 * u), (int)(8 * u), (int)(8 * u), 4 * u, col);
                if (mon.shiny) Glass::DrawIcon(c, Glass::I_SPARKLE, rx + 38 * u, mid - 12 * u, 6 * u, 0xFFF2C46Bu);
            }
#endif
            if (i < MOVER_FIRST) Glass::DrawIcon(c, icon, rx + 18 * u, mid, 9 * u, text);
            if (!chip.empty())
            {
                const int chipW = GW(chip, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(10 * u) - chipW;
                Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
                GT(c, chipX + (int)(10 * u), mid, chip, 12 * u, chipCol, true);
            }
            GT(c, rx + (int)(48 * u), mid, k, 13 * u, hint, true);
            GT(c, rx + (int)(118 * u), mid, fit(v, 16 * u, true, (int)(210 * u)), 16 * u, text, true);
            if (!spn.empty() && !SameMonName(v, spn))
            {
                const int sx = rx + (int)(118 * u) + std::min(GW(v, 16 * u, true), (int)(210 * u)) + (int)(12 * u);
                GT(c, sx, mid, spn, 14 * u, hint, true);
            }
        }
        if (n > vis)
        {
            const int tx = L + lw2 - (int)(7 * u), ty = top + (int)(16 * u), th = lhP - (int)(32 * u);
            Glass::Round(c, tx, ty, (int)(3 * u), th, 1.5f * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), th * vis / n), thumbY = ty + (th - thumbH) * F.rScroll / std::max(1, n - vis);
            Glass::Round(c, tx, thumbY, (int)(3 * u), thumbH, 1.5f * u, 0xCCFFFFFFu);
        }
        // The chosen row.
        const int dx = stkP ? L : R - rw, ix = dx + (int)(20 * u), iw = rw - (int)(40 * u);
        Menu::FrostBox(c, solid, dx, dTopP, rw, dHP, 18 * u);
        static int dKey = -1; { const int nk = F.page * 100000 + F.prow * 100 + LauncherMode() * 10 + HubMode() + (Look().edit + 1) * 10000000; if (nk != dKey) { dKey = nk; F.dScroll = 0; } }
        F.dScroll = std::min(F.dScroll, F.dMax); TextLow() = 0; F.pdY = stkP ? dTopP : 0;
        const int dTopC = dTopP - (stkP ? F.dScroll : 0);
        ClipGuard clipG(c, F, stkP ? dTopP : 0, stkP ? bottom : c.h, (int)(16 * u));
        int y = dTopC + (int)(20 * u);
        auto para = [&](const std::string& s, uint32_t col, float px) {
            for (const std::string& line : WrapLines(s, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        auto cell = [&](int cx, int cy, int cw, const std::string& k, const std::string& v) {
            GT(c, cx, cy + 8 * u, k, 12 * u, hint, true);
            GT(c, cx, cy + 28 * u, fit(v, 15 * u, true, cw - (int)(8 * u)), 15 * u, text, true);
        };
#ifdef SSR_POKEMON_HUB
        if (F.prow == 0 || F.prow == 2)
        {
            const PokemonHub::Save* s = F.prow == 0 ? m.a.get() : m.b.get();
            GT(c, ix, y + 14 * u, F.prow == 0 ? "Take From" : "Send To", 13 * u, hint, true);
            GT(c, ix, y + 40 * u, fit(F.prow == 0 ? fromName : toName, 22 * u, true, iw), 22 * u, text, true);
            y += (int)(64 * u);
            if (!s) para("This save could not be read. " + (F.prow == 0 ? m.aErr : m.bErr), noCol, 13 * u);
            else
            {
                cell(ix, y, iw / 2, "Trainer", Shown(s->Trainer(), "-"));
                cell(ix + iw / 2, y, iw / 2, "Generation", std::to_string(s->Generation()));
                y += (int)(56 * u);
                if (s->ReadOnly()) para("Pok\xC3\xA9mon can be looked at in this game but not moved in or out yet.", noCol, 13 * u);
                para(F.prow == 0 ? "Choose the game to take Pok\xC3\xA9mon from." : "Choose the game to send them to. They can go to a newer game, never an older one, as in the real games.", hint, 13 * u);
            }
        }
        else if (F.prow == 1)
        {
            GT(c, ix, y + 14 * u, "Box", 13 * u, hint, true);
            GT(c, ix, y + 40 * u, fit(BoxLabel(m.a.get(), m.box), 22 * u, true, iw), 22 * u, text, true);
            y += (int)(64 * u);
            if (m.a) cell(ix, y, iw / 2, "Box", m.box == PokemonHub::PARTY ? std::string("Party") : std::to_string(m.box + 1) + " of " + std::to_string(m.a->Boxes()));
            cell(ix + iw / 2, y, iw / 2, "Pok\xC3\xA9mon", std::to_string(MonsIn(m.a.get(), m.box).size()));
            y += (int)(56 * u);
            para("Next Box and Previous Box go through its boxes; Party shows the Pok\xC3\xA9mon with you.", hint, 13 * u);
        }
        else if (F.prow == 3)
        {
            GT(c, ix, y + 14 * u, m.swapping ? "Swap In" : "Put In", 13 * u, hint, true);
            GT(c, ix, y + 40 * u, fit(m.swapping ? BoxLabel(m.b.get(), MoverTargetBox()) : PutLabel(), 22 * u, true, iw), 22 * u, text, true);
            y += (int)(64 * u);
            if (m.swapping) para("Choose the box in " + toName + " to pick the Pok\xC3\xA9mon from, to swap for " + m.swapName + ". B: no swap.", hint, 13 * u);
            else para("Where it goes in " + toName + ": the first free place in its boxes, the party, or a box of your choosing.", hint, 13 * u);
        }
        else if (F.prow - MOVER_FIRST < (int)mons.size())
        {
            const PokemonHub::Mon& mon = mons[F.prow - MOVER_FIRST];
            const PokemonHub::Check ck = check(mon);
            std::vector<uint32_t> big;
            const int bs = (int)(96 * u);
            if (MonIcon(mon.species, mon.egg, big)) DrawMonIcon(c, big, ix + iw - bs, y - (int)(18 * u), bs);
            GT(c, ix, y + 14 * u, fit(Shown(mon.nickname, MonName(mon.speciesName)), 22 * u, true, iw - bs), 22 * u, text, true);
            GT(c, ix, y + 42 * u, (mon.egg ? std::string("Egg") : MonName(mon.speciesName)) + (mon.shiny ? "   \xC2\xB7   Shiny" : ""), 15 * u, mon.shiny ? 0xFFF2C46Bu : hint, true);
            y += (int)(66 * u);
            cell(ix, y, iw / 2, "Level", mon.egg ? std::string("Egg") : std::to_string(mon.level));
            cell(ix + iw / 2, y, iw / 2, "Trainer", Shown(mon.trainer, "-"));
            y += (int)(58 * u);
            GT(c, ix, y + 8 * u, m.swapping ? "Swap For " + m.swapName : "To " + toName + "   \xC2\xB7   " + PutLabel(), 12 * u, m.swapping ? swapCol : hint, true);
            y += (int)(24 * u);
            if (ck.verdict == PokemonHub::Verdict::Ok)
            {
                para(ck.losses.empty() ? (m.swapping ? "They can swap, and keep everything." : "It can move, and keeps everything.") : (m.swapping ? "They can swap, but on the way:" : "It can move, but on the way:"), okCol, 14 * u);
                for (const std::string& l : ck.losses) para("\xC2\xB7  " + SettingSentence(l), text, 13 * u);
                if (ck.evolvesTo) para(SettingSentence(ck.evolution) + (m.swapping ? ", if you choose Swap And Evolve." : ", if you choose Move And Evolve."), text, 13 * u);
            }
            else para(ck.reason.empty() ? std::string("It can't go to that game.") : SettingSentence(ck.reason), noCol, 14 * u);
        }
#else
        para("Moving Pok\xC3\xA9mon is not in this build of Fulldex.", hint, 14 * u);
#endif
        const int na = (int)acts.size();
        int ah = (int)(44 * u), ag = (int)(6 * u);
        if (na > 0)
        {
            // (A phone sideways: the writing above can run long, so the buttons give way, down to a smaller size, instead of lying over it.)
            const int room = bottom - (int)(18 * u) - (y + (int)(8 * u));
            if (na * ah + (na - 1) * ag > room) { ag = (int)(4 * u); ah = std::max((int)(30 * u), (room - (na - 1) * ag) / na); }
        }
        const int ay = std::max(bottom - (int)(18 * u) - na * ah - std::max(0, na - 1) * ag, PhoneLook() && c.h > c.w ? y + (int)(10 * u) : 0); // (upright: under the writing, the panel scrolling to them)
        if (!m.msg.empty())
        {
            const std::vector<std::string> ml = WrapLines(m.msg, 13 * u, iw);
            int my = ay - (int)(14 * u) - (int)ml.size() * (int)(19 * u);
            if (my > y) for (const std::string& line : ml) { GT(c, ix, my + 8 * u, line, 13 * u, swapCol, true); my += (int)(19 * u); }
        }
        F.aX = ix; F.aY = ay; F.aW = na > 0 ? iw : 0; F.aRow = ah + ag;
        const int cur = F.prow == 0 ? m.from : F.prow == 2 ? m.to : -1;
        for (int a = 0; a < na; a++)
        {
            const int ry = ay + a * (ah + ag);
            if (a == F.pact)
            {
                if (F.side < 0) Glass::Glow(c, ix, ry, iw, ah, 12 * u, u);
                Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            else Glass::Round(c, ix, ry, iw, ah, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
            const int icon = F.prow == 1 || F.prow == 3 ? Glass::I_GRID : F.prow >= MOVER_FIRST ? (acts[a].rfind("Swap", 0) == 0 ? Glass::I_PEOPLE : acts[a] == "Cancel Swap" ? Glass::I_BACK : Glass::I_LOAD) : Glass::I_POKEBALL;
            Glass::DrawIcon(c, icon, ix + 22 * u, ry + ah * 0.5f, 9 * u, text);
            GT(c, ix + (int)(42 * u), ry + ah * 0.5f, fit(acts[a], 15 * u, true, iw - (int)(60 * u)), 15 * u, text, true);
            if (a == cur) GT(c, ix + iw - (int)(14 * u) - GW("Chosen", 12 * u, true), ry + ah * 0.5f, "Chosen", 12 * u, okCol, true);
        }
        return;
    }
    if (F.page == SIDE_HUB)
    {
        HubLoad();
        const std::vector<HubGame> games = HubGames();
        const std::vector<std::string> acts = HubActions(F.prow);
        HubState& h = Hub();
        std::unique_lock<std::mutex> lk(h.m);
        const bool on = HubOnLocked(h);
        const int busy = h.busy;
        const std::string code = h.code, url = h.url, msg = h.msg, prov = h.set.provider;
        const std::string dev = h.set.deviceName.empty() ? HubDevice() : h.set.deviceName;
        const std::map<std::string, CloudSaves::Result> results = h.results;
        lk.unlock();
        auto resultOf = [&](const std::string& p) -> const CloudSaves::Result* { const auto it = results.find(p); return it == results.end() ? nullptr : &it->second; };
        auto fit = [&](std::string s, float px, bool bold, int w) {
            if (GW(s, px, bold) <= w) return s;
            while (!s.empty() && GW(s + "...", px, bold) > w)
            {
                while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
                if (!s.empty()) s.pop_back();
            }
            return s + "...";
        };
        auto fileOf = [](const std::string& p) { const size_t k = p.find_last_of("/\\"); return k == std::string::npos ? p : p.substr(k + 1); };
        const int n = HUB_FIRST + (int)games.size();
        const bool stkP = PhoneLook() && c.h > c.w; const int rw = stkP ? R - L : (int)(360 * u), gap = (int)(18 * u), lw2 = stkP ? R - L : R - L - rw - gap, lhP = stkP ? (int)((bottom - top) * 0.55f) : bottom - top, dTopP = stkP ? top + lhP + gap : top, dHP = stkP ? bottom - dTopP : bottom - top, pad = (int)(12 * u);
        const int rowH = (int)(46 * u), rg = (int)(6 * u);
        // The list: the account, then each game's save.
        Menu::FrostBox(c, solid, L, top, lw2, lhP, 18 * u);
        const int vis = std::max(1, ( lhP - 2 * pad + rg) / (rowH + rg));
        F.prow = std::max(0, std::min(F.prow, n - 1));
        if (F.pact >= (int)acts.size()) F.pact = acts.empty() ? -1 : (int)acts.size() - 1;
        if (!F.freeScroll) { if (F.prow < F.rScroll) F.rScroll = F.prow; if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1; } // (a finger scrolling: the list stays where it was put)
        F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && F.pact < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            uint32_t col = 0xFFAEB7C4u;
            std::string st, name, sub;
            if (i == 0)
            {
                name = "Account";
                sub = on ? std::string(HubProvider(prov)) + "   \xC2\xB7   " + dev : "Keep your saves the same on every device";
                if (busy == 1) { st = "Signing In"; col = 0xFFF2C46Bu; }
                else if (busy == 2) { st = "Syncing"; col = 0xFF8CC8FFu; }
                else if (on) { st = "On"; col = 0xFF8FE3A2u; }
                else st = "Off";
                Glass::DrawIcon(c, Glass::I_GLOBE, rx + 18 * u, mid, 9 * u, text);
            }
            else
            {
                const HubGame& g = games[i - HUB_FIRST];
                name = g.name;
                sub = (g.g && !g.g->where.empty() ? WhereName(g.g->where) + "   \xC2\xB7   " : std::string()) + fileOf(g.path) + (g.g && !g.g->hasSave ? "   \xC2\xB7   none here yet" : "");
                st = HubStatus(resultOf(g.path), on, col);
                Glass::Round(c, rx + (int)(14 * u), mid - (int)(4 * u), (int)(8 * u), (int)(8 * u), 4 * u, col);
            }
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(10 * u) - chipW;
            Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, chipX + (int)(10 * u), mid, st, 12 * u, col, true);
            GT(c, rx + (int)(36 * u), mid, fit(name, 16 * u, true, (int)(200 * u)), 16 * u, text, true);
            const int fx = rx + (int)(248 * u);
            GT(c, fx, mid, fit(sub, 13 * u, false, chipX - fx - (int)(12 * u)), 13 * u, hint, false);
        }
        if (n > vis)
        {
            const int tx = L + lw2 - (int)(7 * u), ty = top + (int)(16 * u), th = lhP - (int)(32 * u);
            Glass::Round(c, tx, ty, (int)(3 * u), th, 1.5f * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), th * vis / n), thumbY = ty + (th - thumbH) * F.rScroll / std::max(1, n - vis);
            Glass::Round(c, tx, thumbY, (int)(3 * u), thumbH, 1.5f * u, 0xCCFFFFFFu);
        }
        // The chosen row.
        const int dx = stkP ? L : R - rw, ix = dx + (int)(20 * u), iw = rw - (int)(40 * u);
        Menu::FrostBox(c, solid, dx, dTopP, rw, dHP, 18 * u);
        static int dKey = -1; { const int nk = F.page * 100000 + F.prow * 100 + LauncherMode() * 10 + HubMode() + (Look().edit + 1) * 10000000; if (nk != dKey) { dKey = nk; F.dScroll = 0; } }
        F.dScroll = std::min(F.dScroll, F.dMax); TextLow() = 0; F.pdY = stkP ? dTopP : 0;
        const int dTopC = dTopP - (stkP ? F.dScroll : 0);
        ClipGuard clipG(c, F, stkP ? dTopP : 0, stkP ? bottom : c.h, (int)(16 * u));
        int y = dTopC + (int)(20 * u);
        auto para = [&](const std::string& s, uint32_t col, float px) {
            for (const std::string& line : WrapLines(s, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        auto cell = [&](int cx, int cy, int cw, const std::string& k, const std::string& v) {
            GT(c, cx, cy + 8 * u, k, 12 * u, hint, true);
            GT(c, cx, cy + 28 * u, fit(v, 15 * u, true, cw - (int)(8 * u)), 15 * u, text, true);
        };
        if (F.prow == 0)
        {
            GT(c, ix, y + 14 * u, "Account", 22 * u, text, true);
            y += (int)(40 * u);
            if (busy == 1 && !code.empty())
            {
                para("On your phone or computer, go to", hint, 14 * u);
                GT(c, ix, y + 8 * u, fit(url.empty() ? std::string("google.com/device") : url, 16 * u, true, iw), 16 * u, text, true);
                y += (int)(30 * u);
                para("and enter this code:", hint, 14 * u);
                const int bh = (int)(70 * u);
                Glass::Round(c, ix, y, iw, bh, 14 * u, 0x24FFFFFFu, 0x3DFFFFFF, 1.0f * u);
                GT(c, ix + (iw - GW(code, 34 * u, true)) / 2, y + bh * 0.5f, code, 34 * u, text, true);
                y += bh + (int)(16 * u);
                para("Fulldex is waiting for you to say yes. It only sees the saves it puts in your Drive.", hint, 13 * u);
            }
            else if (!on)
            {
                para("Keep your saves the same on every device: play on your PC, then carry on on the Switch.", text, 14 * u);
                para("Saves go to your own Google Drive, in a folder called Fulldex Cloud Saves. Fulldex sees only the files it made there.", hint, 13 * u);
                para("If a save changed on two devices, Fulldex asks which one to keep, and puts the other in the backups.", hint, 13 * u);
            }
            else
            {
                cell(ix, y, iw / 2, "Saved To", HubProvider(prov));
                cell(ix + iw / 2, y, iw / 2, "This Device", dev);
                y += (int)(48 * u);
                int synced = 0;
                for (const HubGame& g : games) { const CloudSaves::Result* r = resultOf(g.path); if (r && (r->outcome == CloudSaves::Outcome::InSync || r->outcome == CloudSaves::Outcome::Pushed || r->outcome == CloudSaves::Outcome::Pulled)) synced++; }
                cell(ix, y, iw / 2, "Folder", prov == "google" ? "Fulldex Cloud Saves" : prov == "webdav" ? "WebDAV" : "Synced Folder");
                cell(ix + iw / 2, y, iw / 2, "Up To Date", std::to_string(synced) + " of " + std::to_string(games.size()));
                y += (int)(56 * u);
                para("Choose a game in the list to sync it on its own, or to choose which save to keep.", hint, 13 * u);
            }
        }
        else
        {
            const HubGame& hg = games[F.prow - HUB_FIRST];
            const CloudSaves::Result* r = resultOf(hg.path);
            const int ps = (int)(84 * u);
            if (hg.g) DrawCover(c, *hg.g, ix, y, ps, ps, 12 * u);
            const int tx = ix + ps + (int)(14 * u), tw = iw - ps - (int)(14 * u);
            GT(c, tx, y + 22 * u, fit(hg.name, 19 * u, true, tw), 19 * u, text, true);
            uint32_t col = 0;
            const std::string st = HubStatus(r, on, col);
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u);
            Glass::Round(c, tx, y + (int)(42 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, tx + (int)(10 * u), y + 53 * u, st, 12 * u, col, true);
            y += ps + (int)(18 * u);
            std::string cloudDev = r && !r->cloud.device.empty() ? r->cloud.device : std::string("another device");
            if (r && r->outcome == CloudSaves::Outcome::Conflict)
            {
                cell(ix, y, iw / 2, "This Device", HubWhen(r->local.savedAt));
                cell(ix + iw / 2, y, iw / 2, "Cloud (" + cloudDev + ")", HubWhen(r->cloud.savedAt));
                y += (int)(56 * u);
                para("This save changed here and on " + cloudDev + " since they last matched. Choose which to keep: the other goes into the backups.", text, 14 * u);
            }
            else if (!on) para("Cloud saves are off. Sign in on the Cloud Saves row first.", hint, 14 * u);
            else if (!r) para("Not synced yet. Choose Sync Now to send it up, or bring down a newer one.", hint, 14 * u);
            else switch (r->outcome)
            {
            case CloudSaves::Outcome::InSync: para("This save is the same here and in the cloud.", text, 14 * u); break;
            case CloudSaves::Outcome::Pushed: para("This save was sent up to the cloud.", text, 14 * u); break;
            case CloudSaves::Outcome::Pulled: para("A newer save from " + cloudDev + " was brought down. The one that was here is in the backups.", text, 14 * u); break;
            case CloudSaves::Outcome::NothingYet: para("There is no save for this game yet, here or in the cloud.", hint, 14 * u); break;
            default: para("Could not reach the cloud. The save here is kept, and syncs next time.", hint, 14 * u); break;
            }
            GT(c, ix, y + 8 * u, "Save File", 12 * u, hint, true);
            GT(c, ix, y + 28 * u, fit(fileOf(hg.path), 14 * u, false, iw), 14 * u, text, false);
            y += (int)(48 * u);
        }
        // What can be done (and the last word, above it).
        const int na = (int)acts.size();
        int ah = (int)(44 * u), ag = (int)(6 * u);
        if (ShortNow() && na > 0)
        {
            // (A phone sideways: the writing above can run long, so the buttons give way, down to a smaller size, instead of lying over it.)
            const int room = bottom - (int)(18 * u) - (y + (int)(8 * u));
            if (na * ah + (na - 1) * ag > room) { ag = (int)(4 * u); ah = std::max((int)(30 * u), (room - (na - 1) * ag) / na); }
        }
        const int ay = std::max(bottom - (int)(18 * u) - na * ah - std::max(0, na - 1) * ag, PhoneLook() && c.h > c.w ? y + (int)(10 * u) : 0); // (upright: under the writing, the panel scrolling to them)
        if (!msg.empty())
        {
            const std::vector<std::string> ml = WrapLines(msg, 13 * u, iw);
            int my = ay - (int)(14 * u) - (int)ml.size() * (int)(19 * u);
            if (my > y) for (const std::string& line : ml) { GT(c, ix, my + 8 * u, line, 13 * u, busy ? 0xFF8CC8FFu : hint, true); my += (int)(19 * u); }
        }
        F.aX = ix; F.aY = ay; F.aW = na > 0 ? iw : 0; F.aRow = ah + ag;
        for (int a = 0; a < na; a++)
        {
            const int ry = ay + a * (ah + ag);
            if (a == F.pact)
            {
                if (F.side < 0) Glass::Glow(c, ix, ry, iw, ah, 12 * u, u);
                Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            else Glass::Round(c, ix, ry, iw, ah, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
            const int icon = acts[a] == "Sign In With Google" ? Glass::I_GLOBE : acts[a] == "Sign Out" || acts[a] == "Cancel" ? Glass::I_BACK : Glass::I_LOAD;
            Glass::DrawIcon(c, icon, ix + 22 * u, ry + ah * 0.5f, 9 * u, text);
            GT(c, ix + (int)(42 * u), ry + ah * 0.5f, acts[a], 15 * u, text, true);
        }
        return;
    }
    // The sections.
    const bool stkA = PhoneLook() && c.h > c.w;
    const int lw = stkA ? R - L : (int)(300 * u), pad = (int)(12 * u), rowH = (int)((stkA ? 40 : 50) * u), rg = (int)((stkA ? 4 : 6) * u);
    const int lhA = stkA ? 5 * (rowH + rg) + 2 * pad : bottom - top, pTop = stkA ? top + lhA + (int)(18 * u) : top;
    Menu::FrostBox(c, solid, L, top, lw, lhA, 18 * u);
    static const char* names[] = { "Support", "Credits", "Report A Bug", "Licences", "Website" };
    static const int icons[] = { Glass::I_SPARKLE, Glass::I_PEOPLE, Glass::I_INFO, Glass::I_LAYERS, Glass::I_GLOBE };
    F.pX = L + pad; F.pY = top + pad; F.pW = lw - 2 * pad; F.pRow = rowH + rg;
    for (int i = 0; i < 5; i++)
    {
        const int rx = L + pad, ry = top + pad + i * (rowH + rg), rw = lw - 2 * pad;
        if (i == F.prow)
        {
            if (F.side < 0) Glass::Glow(c, rx, ry, rw, rowH, 12 * u, u);
            Glass::Round(c, rx, ry, rw, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
        }
        Glass::DrawIcon(c, icons[i], rx + 24 * u, ry + rowH * 0.5f, 10 * u, text);
        GT(c, rx + (int)(48 * u), ry + rowH * 0.5f, names[i], 17 * u, text, true);
    }
    {
        const std::vector<std::string> lines = WrapLines("Fulldex doesn't include any games. Play with ROMs from games you own.", 13 * u, lw - 2 * pad - (int)(28 * u));
        const int ch = (int)(66 * u) + (int)lines.size() * (int)(18 * u);
        const int cx = L + pad, cy = top + lhA - pad - ch, cw = lw - 2 * pad;
        if (cy >= top + pad + 5 * (rowH + rg)) { // (not on a short screen: it would be drawn over the rows)
        Glass::Round(c, cx, cy, cw, ch, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
        GT(c, cx + (int)(14 * u), cy + 24 * u, "Fulldex", 20 * u, text, true);
        GT(c, cx + (int)(14 * u), cy + 48 * u, PlatformLabel(), 14 * u, hint, true);
        int ly = cy + (int)(70 * u);
        for (const std::string& l : lines) { GT(c, cx + (int)(14 * u), ly, l, 13 * u, hint, false); ly += (int)(18 * u); }
        }
    }
    // The chosen section.
    const int px = stkA ? L : L + lw + (int)(18 * u), pw = R - px, ip = (int)(22 * u);
    Menu::FrostBox(c, solid, px, pTop, pw, bottom - pTop, 18 * u);
    if (F.abKey != F.prow) { F.abKey = F.prow; F.rScroll = 0; F.dScroll = 0; }
    F.dScroll = std::clamp(F.dScroll, 0, F.dMax);
    TextLow() = 0; F.pdY = stkA ? pTop : 0;
    ClipGuard aboutClip(c, F, stkA ? pTop : 0, stkA ? bottom : c.h, ip);
    int y = pTop + ip - (stkA ? F.dScroll : 0);
    auto heading = [&](const char* h) { GT(c, px + ip, y + 14 * u, h, 22 * u, text, true); y += (int)(46 * u); };
    auto para = [&](const std::string& s) {
        for (const std::string& l : WrapLines(s, 16 * u, pw - 2 * ip)) { GT(c, px + ip, y + 11 * u, l, 16 * u, text, false); y += (int)(24 * u); }
        y += (int)(10 * u);
    };
    auto card = [&](int x, int cy, int w, int h, const std::string& a, const std::string& b) {
        TextLow() = std::max(TextLow(), cy + h);
        Glass::Round(c, x, cy, w, h, 12 * u, 0x14FFFFFFu);
        float fa = 16 * u, fb = 13 * u;
        while (fa > 10 * u && GW(a, fa, true) > w - (int)(28 * u)) fa -= u;
        while (fb > 9 * u && GW(b, fb, true) > w - (int)(28 * u)) fb -= u;
        GT(c, x + (int)(14 * u), cy + h * 0.36f, a, fa, text, true);
        GT(c, x + (int)(14 * u), cy + h * 0.70f, b, fb, hint, true);
    };
#ifdef __SWITCH__
    const char* open = "Open it on your phone or PC";
#else
    const char* open = "Choose it to open it in your browser";
#endif
    F.rVis = 0; F.abX = px; F.abW = pw; F.abRow = (int)(60 * u);

    struct Blk { int h; std::function<void()> draw; };
    // (A section too long for the screen: its blocks drawn from the one scrolled to, a finger dragging moves along them.)
    auto runBlocks = [&](std::vector<Blk>& bl, int step) {
        if (stkA) { for (auto& block : bl) block.draw(); TextLow() = std::max(TextLow(), y); return; }
        const int avail = bottom - ip - y, total = (int)bl.size();
        int maxStart = total, acc = 0;
        while (maxStart > 0 && acc + bl[maxStart - 1].h <= avail) { acc += bl[maxStart - 1].h; maxStart--; }
        const int start = std::max(0, std::min(F.rScroll, maxStart));
        F.rScroll = start; F.rVis = maxStart > 0 ? 1 : 0; F.abRow = step;
        const int y0 = y;
        int used = 0;
        for (int i = start; i < total && used + bl[i].h <= avail; i++) { bl[i].draw(); used += bl[i].h; }
        if (maxStart > 0)
        {
            const int th = std::max((int)(24 * u), avail * (total - maxStart) / std::max(1, total));
            Glass::Round(c, px + pw - (int)(10 * u), y0 + (avail - th) * start / maxStart, (int)(4 * u), th, 2 * u, 0x80FFFFFFu);
        }
    };
    AboutLinks().clear();
    auto link = [&](int lx, int ly, int lw, int lh, const char* url) {
        const int linkTop = stkA ? std::max(ly, pTop) : ly;
        const int linkBottom = stkA ? std::min(ly + lh, bottom) : ly + lh;
        if (linkBottom > linkTop) AboutLinks().push_back({ lx, linkTop, lw, linkBottom - linkTop, url });
        Glass::DrawIcon(c, Glass::I_CHEVRON_R, lx + lw - (int)(18 * u), ly + lh * 0.5f, 7 * u, hint);
    };
    if (F.prow == 0)
    {
        heading("Support Fulldex");
        para("Hi, I'm James, AKA Mamesj, the mind behind Fulldex. I'm just a dude who wanted to play some Pok\xC3\xA9mon and got a bit carried away.");
        para("Fulldex is free and always will be. If it's given you some good hours, a coffee or a beer is always appreciated, never expected.");
        const int gap = (int)(16 * u), cw = (pw - 2 * ip - gap) / 2, ch = (int)(64 * u);
#ifdef __ANDROID__
        const int qs = 0; // (no QR codes on Android: the links are there to choose)
#else
        const int qs = std::max((int)(80 * u), std::min(cw - (int)(60 * u), bottom - ip - (y + (stkA ? F.dScroll : 0) + ch + (int)(14 * u)) - (int)(30 * u)));
#endif
        const char* names2[2] = { "Buy Me A Coffee", "PayPal" };
        const char* where[2] = { "buymeacoffee.com/mamesj", "paypal.me/mamesj" };
        for (int i = 0; i < 2; i++)
        {
            const int cx = px + ip + i * (cw + gap);
            card(cx, y, cw, ch, names2[i], where[i]);
            link(cx, y, cw, ch, i == 0 ? "https://buymeacoffee.com/mamesj" : "https://paypal.me/mamesj");
            if (i == 0) DrawQr(c, kBeerQr, kBeerQrSize, cx + (cw - qs) / 2, y + ch + (int)(14 * u), qs);
            else DrawQr(c, kPaypalQr, kPaypalQrSize, cx + (cw - qs) / 2, y + ch + (int)(14 * u), qs);
        }
        y += ch + (int)(14 * u) + qs + (int)(14 * u);
        TextLow() = std::max(TextLow(), y);
#ifdef __SWITCH__
        GT(c, px + ip, y + 8 * u, "Scan a code with your phone's camera.", 13 * u, hint, true);
#else
        { int hy2 = y + (int)(8 * u); for (const std::string& hl : WrapLines(
#ifdef __ANDROID__
            "Choose Support to open Buy Me A Coffee or PayPal."
#else
            "Choose Support to open Buy Me A Coffee, or scan a code with your phone."
#endif
            , 13 * u, pw - 2 * ip)) { GT(c, px + ip, hy2, hl, 13 * u, hint, true); hy2 += (int)(18 * u); } }
#endif
    }
    else if (F.prow == 1)
    {
        heading("Credits And Acknowledgements");
        static const char* const creditUrl[10] = { "https://github.com/ClickingMaster/ssrecomp", "https://github.com/melonDS-emu/melonDS", "https://mgba.io",
            "https://github.com/lidor30/pokedaisy", "https://github.com/fuddlesworth/pokebw2", "https://gbatemp.net/threads/deadskullzjrs-nds-cheat-databases.488711/",
            "https://github.com/ITotalJustice/sphaira", "", "", "" };
        static const char* credits[][2] = {
            { "ssrecomp by ClickingMaster", "The single-screen and widescreen view Fulldex grew from" },
            { "melonDS", "The DS emulation" },
            { "mGBA", "The GBA games, through Pok\xC3\xA9mon GBA Widescreen" },
            { "Pok\xC3\xA9" "Daisy by Lidor Itzhari", "Companion readers and text decoders" },
            { "pokebw2, aletheia, pret/pokeplatinum", "Function names from their decompilations" },
            { "DeadSkullzJr's cheat database", "The built-in DS cheats, via libretro-database" },
            { "sphaira", "How the HOME Menu icons are made" },
            { "WeeabooGod", "Cheats menu ideas (PR #16)" },
            { "Nils Menn", "Fucking legend" },
            { "Everyone who reported bugs and helped test", "Thank you" } };
        const int gap = (int)(12 * u), cw = (pw - 2 * ip - gap) / 2, ch = (int)(62 * u);
        std::vector<Blk> bl;
        for (int r = 0; r < 5; r++)
            bl.push_back({ ch + (int)(10 * u), [&, r]() { for (int k = 0; k < 2; k++) { card(px + ip + k * (cw + gap), y, cw, ch, credits[r * 2 + k][0], credits[r * 2 + k][1]); if (*creditUrl[r * 2 + k]) link(px + ip + k * (cw + gap), y, cw, ch, creditUrl[r * 2 + k]); } y += ch + (int)(10 * u); } });
        runBlocks(bl, ch + (int)(10 * u));
    }
    else if (F.prow == 2)
    {
        heading("Report A Bug");
        para("Found something wrong? Open an issue on the GitHub page: say which game, which device, and what happened just before.");
        para("A screenshot helps, and so does game.log from the folder Fulldex runs from.");
        card(px + ip, y, pw - 2 * ip, (int)(64 * u), "github.com/mamesj/pokemon-fulldex/issues", open);
        link(px + ip, y, pw - 2 * ip, (int)(64 * u), "https://github.com/mamesj/pokemon-fulldex/issues");
    }
    else if (F.prow == 3)
    {
        heading("Licences");
        static const char* const licUrl[6] = { "https://github.com/mamesj/pokemon-fulldex", "https://github.com/melonDS-emu/melonDS", "https://github.com/lidor30/pokedaisy",
            "https://github.com/switchbrew/nx-hbloader", "https://mgba.io", "https://github.com/mamesj/pokemon-fulldex/blob/main/frontend/fonts/FONT-LICENSES.txt" };
        static const char* lic[][2] = {
            { "Fulldex", "GPL-3.0-or-later" }, { "melonDS", "GPL-3.0" }, { "Pok\xC3\xA9" "Daisy readers", "GPL-3.0" },
            { "HOME Menu icon loader (nx-hbloader)", "GPL-3.0" }, { "mGBA", "MPL-2.0" }, { "The launcher's fonts", "SIL Open Font License" } };
        const int rh = (int)(44 * u);
        const std::string p1 = "Each font's copyright notice and the full licence text are in FONT-LICENSES.txt, in the fonts folder of the download and on the GitHub page.";
        const std::string p2 = "Not affiliated with or endorsed by Nintendo, Game Freak, The Pok\xC3\xA9mon Company or the melonDS project. Pok\xC3\xA9mon is a trademark of its owners. The full licence texts are on the GitHub page.";
        auto paraH = [&](const std::string& s) { return (int)WrapLines(s, 16 * u, pw - 2 * ip).size() * (int)(24 * u) + (int)(10 * u); };
        std::vector<Blk> bl;
        for (int i = 0; i < 6; i++)
            bl.push_back({ rh + (int)(6 * u), [&, i]() {
                Glass::Round(c, px + ip, y, pw - 2 * ip, rh, 12 * u, 0x14FFFFFFu);
                GT(c, px + ip + (int)(14 * u), y + rh * 0.5f, lic[i][0], 16 * u, text, true);
                GT(c, px + pw - ip - (int)(40 * u) - GW(lic[i][1], 15 * u, true), y + rh * 0.5f, lic[i][1], 15 * u, hint, true);
                link(px + ip, y, pw - 2 * ip, rh, licUrl[i]);
                y += rh + (int)(6 * u); } });
        bl.push_back({ (int)(4 * u) + paraH(p1), [&]() { y += (int)(4 * u); para(p1); } });
        bl.push_back({ (int)(78 * u), [&]() { card(px + ip, y, pw - 2 * ip, (int)(64 * u), "FONT-LICENSES.txt", open); link(px + ip, y, pw - 2 * ip, (int)(64 * u), "https://github.com/mamesj/pokemon-fulldex/blob/main/frontend/fonts/FONT-LICENSES.txt"); y += (int)(78 * u); } });
        bl.push_back({ paraH(p2), [&]() { para(p2); } });
        runBlocks(bl, rh + (int)(6 * u));
    }
    else
    {
        heading("Website");
        para("Downloads, the latest changes and the full instructions are on the GitHub page.");
        card(px + ip, y, pw - 2 * ip, (int)(64 * u), "github.com/mamesj/pokemon-fulldex", open);
        link(px + ip, y, pw - 2 * ip, (int)(64 * u), "https://github.com/mamesj/pokemon-fulldex");
        y += (int)(84 * u);
        para("Tips are welcome but never expected: see Support, at the top of this page.");
    }
}

// NOW PLAYING on the home screen: what the player's own music app is playing (frontend/now_playing.h; pc_launcher.cpp
// fills this in every frame), a slim bar with its album art and previous / play-pause / next. Nothing while no music
// app is playing, and none where the system cannot tell (the Switch).
struct MusicBar
{
    bool shown = false, idle = true, blind = false, playing = false, canPrev = true, canNext = true;
    std::string title, artist, app;
    std::vector<uint32_t> art; int artW = 0, artH = 0;
    int bx[3] = {}, by = 0, bs = 0; // the buttons as drawn (centres, half size), for taps; bs 0: not drawn
    int action = -1;                // a button pressed (0 previous, 1 play / pause, 2 next), for the caller
};
inline MusicBar& Music() { static MusicBar m; return m; }
inline int MusicAt(int x, int y)
{
    const MusicBar& m = Music();
    if (!m.shown || m.idle || m.bs <= 0) return -1;
    for (int i = 0; i < 3; i++)
        if (x >= m.bx[i] - m.bs && x < m.bx[i] + m.bs && y >= m.by - m.bs && y < m.by + m.bs) return i;
    return -1;
}
inline void DrawMusicBar(Canvas& c, int x, int y, int w, int h, float u)
{
    MusicBar& m = Music();
    const uint32_t text = Menu::GlassText, hint = Menu::GlassHint;
    Glass::Round(c, x, y, w, h, h * 0.32f, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
    const int pad = std::max(3, h / 8), a = h - 2 * pad, ax = x + pad, ay = y + pad;
    Glass::Round(c, ax, ay, a, a, a * 0.22f, 0x400A111C, 0x2EFFFFFF, 1.0f);
    if (!m.blind && !m.art.empty() && m.artW > 0 && m.artH > 0)
    {
        const float rr = a * 0.22f;
        for (int py = 0; py < a; py++)
            for (int px = 0; px < a; px++)
            {
                const int X = ax + px, Y = ay + py;
                if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
                const float qx = std::max(0.0f, std::fabs(px + 0.5f - a * 0.5f) - (a * 0.5f - rr)), qy = std::max(0.0f, std::fabs(py + 0.5f - a * 0.5f) - (a * 0.5f - rr));
                const float cover = std::clamp(rr + 0.5f - Glass::Hyp(qx, qy), 0.0f, 1.0f);
                if (cover <= 0) continue;
                const uint32_t sp = ArtSmooth::At(m.art, m.artW, m.artH, px, py, a);
                c.px[(size_t)Y * c.w + X] = Glass::Over(c.px[(size_t)Y * c.w + X], sp | 0xFF000000u, (int)(cover * 255));
            }
        Glass::Mark(c, ax, ay, ax + a, ay + a);
    }
    else Glass::DrawIcon(c, Glass::I_NOTE, ax + a * 0.5f, ay + a * 0.5f, a * 0.3f, text);
    // (Controls at the right: previous, play / pause, next.)
    const int gap = (int)(h * 0.9f), cy = y + h / 2, cxN = x + w - pad - gap / 2, cxP = cxN - gap, cxB = cxP - gap;
    m.bx[0] = cxB; m.bx[1] = cxP; m.bx[2] = cxN; m.by = cy; m.bs = gap / 2;
    const int pr = (int)(h * 0.36f);
    const int sr = (int)(h * 0.27f);
    const uint32_t ink = Glass::ButtonInk(), inkOff = (ink & 0x00FFFFFFu) | 0x66000000u;
    Glass::Surface(c, 3, cxB - sr, cy - sr, 2 * sr, 2 * sr, (float)sr, Glass::Button());
    Glass::Surface(c, 3, cxN - sr, cy - sr, 2 * sr, 2 * sr, (float)sr, Glass::Button());
    Glass::Surface(c, 3, cxP - pr, cy - pr, 2 * pr, 2 * pr, (float)pr, Glass::Button());
    Glass::DrawIcon(c, m.playing ? Glass::I_PAUSE : Glass::I_PLAY, cxP + (m.playing ? 0 : h * 0.03f), cy, h * 0.16f, ink);
    Glass::DrawIcon(c, Glass::I_PREV, cxB, cy, h * 0.15f, m.canPrev ? ink : inkOff);
    Glass::DrawIcon(c, Glass::I_NEXT, cxN, cy, h * 0.15f, m.canNext ? ink : inkOff);
    // (The song, and who it is by with the app after it, cut short to fit before the controls.)
    const int tx = ax + a + pad * 2, tw = cxB - gap / 2 - (int)(6 * u) - tx;
    if (tw > (int)(40 * u))
    {
        const std::string l1 = m.blind ? std::string("Your music") : m.title;
        const std::string l2 = m.blind ? std::string("Allow song names in Settings") : m.artist.empty() ? m.app : m.artist + "  \xC2\xB7  " + m.app;
        GTFit(c, tx, y + h * 0.36f, l1, h * 0.34f, tw, text, true);
        GTFit(c, tx, y + h * 0.72f, l2, h * 0.27f, tw, hint, true);
    }
}
// A phone sideways: just the buttons (previous, play / pause, next) in a slim pill, to leave the room to the tabs.
inline void DrawMusicButtons(Canvas& c, int x, int y, int w, int h, float u)
{
    MusicBar& m = Music();
    Glass::Round(c, x, y, w, h, h * 0.5f, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
    const int gap = w / 3, cy = y + h / 2, cxB = x + gap / 2, cxP = x + gap * 3 / 2, cxN = x + gap * 5 / 2;
    m.bx[0] = cxB; m.bx[1] = cxP; m.bx[2] = cxN; m.by = cy; m.bs = gap / 2;
    const int pr = (int)(h * 0.34f), sr = (int)(h * 0.27f);
    const uint32_t ink = Glass::ButtonInk(), inkOff = (ink & 0x00FFFFFFu) | 0x66000000u;
    Glass::Surface(c, 3, cxB - sr, cy - sr, 2 * sr, 2 * sr, (float)sr, Glass::Button());
    Glass::Surface(c, 3, cxN - sr, cy - sr, 2 * sr, 2 * sr, (float)sr, Glass::Button());
    Glass::Surface(c, 3, cxP - pr, cy - pr, 2 * pr, 2 * pr, (float)pr, Glass::Button());
    Glass::DrawIcon(c, m.playing ? Glass::I_PAUSE : Glass::I_PLAY, cxP + (m.playing ? 0 : h * 0.03f), cy, h * 0.16f, ink);
    Glass::DrawIcon(c, Glass::I_PREV, cxB, cy, h * 0.15f, m.canPrev ? ink : inkOff);
    Glass::DrawIcon(c, Glass::I_NEXT, cxN, cy, h * 0.15f, m.canNext ? ink : inkOff);
}
// The sidebar layout's music player: a square at the foot of the sidebar, the album art filling it, the song over the
// art's darkened foot and the controls under that.
inline void DrawMusicSquare(Canvas& c, int x, int y, int sz, float u)
{
    MusicBar& m = Music();
    const uint32_t text = 0xFFFFFFFFu;
    const float rr = 16 * u;
    Glass::Shadow(c, x, y, sz, sz, rr, 14 * u, 0x50);
    Glass::Round(c, x, y, sz, sz, rr, 0xFF1A2430u, 0x3DFFFFFF, 1.0f * u);
    const bool art = !m.blind && !m.art.empty() && m.artW > 0 && m.artH > 0;
    for (int py = 0; py < sz; py++)
        for (int px = 0; px < sz; px++)
        {
            const int X = x + px, Y = y + py;
            if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
            const float qx = std::max(0.0f, std::fabs(px + 0.5f - sz * 0.5f) - (sz * 0.5f - rr)), qy = std::max(0.0f, std::fabs(py + 0.5f - sz * 0.5f) - (sz * 0.5f - rr));
            const float cover = std::clamp(rr + 0.5f - Glass::Hyp(qx, qy), 0.0f, 1.0f);
            if (cover <= 0) continue;
            uint32_t col = art ? ArtSmooth::At(m.art, m.artW, m.artH, px, py, sz) : 0xFF1A2430u;
            const float dark = std::clamp((py - 0.34f * sz) / (0.56f * sz), 0.0f, 1.0f); // (the foot darker, for the text)
            col = Glass::Over(col, 0xFF000000u, (int)(dark * 0.82f * 255));
            c.px[(size_t)Y * c.w + X] = Glass::Over(c.px[(size_t)Y * c.w + X], col, (int)(cover * 255));
        }
    Glass::Mark(c, x, y, x + sz, y + sz);
    if (!art) Glass::DrawIcon(c, Glass::I_NOTE, x + sz * 0.5f, y + sz * 0.3f, sz * 0.14f, 0xCCFFFFFFu);
    const int pad = (int)(12 * u), tw = sz - 2 * pad;
    const std::string l1 = m.blind ? std::string("Your music") : m.title;
    const std::string l2 = m.blind ? std::string("Allow song names in Settings") : m.artist.empty() ? m.app : m.artist;
    GTFit(c, x + pad, y + sz * 0.58f, l1, 14 * u, tw, text, true);
    GTFit(c, x + pad, y + sz * 0.58f + 18 * u, l2, 11 * u, tw, 0xCCFFFFFFu, true);
    const int cy = y + sz - (int)(26 * u), gap = (int)(42 * u), cx = x + sz / 2;
    m.bx[0] = cx - gap; m.bx[1] = cx; m.bx[2] = cx + gap; m.by = cy; m.bs = gap / 2;
    const int pr = (int)(16 * u);
    const int sr = (int)(12 * u);
    const uint32_t ink = Glass::ButtonInk(), inkOff = (ink & 0x00FFFFFFu) | 0x66000000u;
    Glass::Surface(c, 3, cx - gap - sr, cy - sr, 2 * sr, 2 * sr, (float)sr, Glass::Button());
    Glass::Surface(c, 3, cx + gap - sr, cy - sr, 2 * sr, 2 * sr, (float)sr, Glass::Button());
    Glass::Surface(c, 3, cx - pr, cy - pr, 2 * pr, 2 * pr, (float)pr, Glass::Button());
    Glass::DrawIcon(c, m.playing ? Glass::I_PAUSE : Glass::I_PLAY, cx + (m.playing ? 0 : 1.5f * u), cy, 7 * u, ink);
    Glass::DrawIcon(c, Glass::I_PREV, cx - gap, cy, 6.5f * u, m.canPrev ? ink : inkOff);
    Glass::DrawIcon(c, Glass::I_NEXT, cx + gap, cy, 6.5f * u, m.canNext ? ink : inkOff);
}
inline void DrawGlass(Canvas& c, const std::vector<Game>& games, const View& v, int tab, int selected, int& scroll,
                      const std::string& note, const std::string& hintText, bool follow, bool onTabs, bool settingsChosen)
{
    // (The launcher's own font and Text Size while it draws; CleanFont's own after.)
    LookLoad();
    struct FontUse { FontUse() { CleanFont::FontId() = LookFontId(); CleanFont::SizeScale() = Look().fontSize / 100.0f; CleanFont::Cjk() = true; } ~FontUse() { CleanFont::FontId() = 0; CleanFont::SizeScale() = 1.0f; CleanFont::Cjk() = false; } } fontUse;
    const float u = GU(c);
    const Grid& grid = v.g;
    const GlassSpots sp = Spots(c);
    const int cont = ContinueGame(games, selected);
    const Game* shown = selected >= 0 && selected < (int)games.size() ? &games[selected]
                      : cont >= 0 && cont < (int)games.size() ? &games[cont] : nullptr;
    const Game* bg = shown;
    if (!BackdropRom().empty())
        for (const Game& g : games)
            if (g.rom == BackdropRom()) { bg = &g; break; }
    GlassBackdrop(c, bg);
    const uint32_t text = Menu::GlassText, hint = Menu::GlassHint, dark = 0xFF18222Eu;
    FollowSelection(v, selected, scroll, follow);
    Music().bs = 0; // (its buttons: set again where the bar is drawn)
    // (Everything but the chosen card, the game's menu, the dialogs and the hints is kept as drawn and
    // copied back while nothing on it changes; a press then redraws only the chosen card. The Switch
    // draws in software about 20 times slower than a PC.)
    auto sig = [](const Game& g) { return g.art.empty() ? std::string("-") : std::to_string(g.art[(size_t)Art * Art / 2 + Art / 2]); };
    std::string key = std::string(TilesOn() ? "T" + std::to_string(selected) : PanelOn() ? "P" + std::to_string(selected) : "S") + std::to_string(c.w) + "x" + std::to_string(c.h) + "/" + std::to_string(tab) + "/" + std::to_string(scroll) + "/"
                    + (bg ? bg->rom + "#" + sig(*bg) : std::string()) + "/"
                    + std::to_string(onTabs) + std::to_string(settingsChosen) + "/" + std::to_string(cont) + "/" + ContinueRom()
                    + "/t" + Focus().pageTitle + "/h" + HubSig() + (Focus().page == SIDE_HUB ? std::to_string(Mover().open) + std::to_string(HubMode()) + "," + std::to_string(Focus().pact) + "," + std::to_string(Focus().rScroll) : std::string()) + "/p" + std::to_string(Focus().page) + "," + std::to_string(Focus().page >= 0 ? Focus().prow : 0) + std::to_string(Focus().page >= 0 && Focus().side < 0) + "/s" + std::to_string(Focus().onSort) + SortKey() + "/q" + SearchText() + "/t" + TabsSig()
                    + (Focus().page == SIDE_ROMS ? "/r" + std::to_string(Focus().pact) + "," + std::to_string(RomList().size()) + ","
                       + std::to_string(std::count_if(RomList().begin(), RomList().end(), [](const Game& g) { return g.hidden; })) : std::string());
    if (Focus().page >= 0) key += "/scroll" + std::to_string(Focus().dScroll) + "," + std::to_string(Focus().rScroll) + "," + std::to_string(Focus().nScroll);
    key += GridOnly() ? "/grid-only" : "/banner";
    for (const Game& g : games) key += "|" + g.rom + g.customName + (g.favourite ? "*" : "") + sig(g) + (g.soonAt ? ComingSoonText(g) : std::string());
    static std::vector<uint32_t> base;
    static std::string baseKey;
    bool solid = c.backdrop.size() == c.px.size();
    auto visible = [&](const View::Spot& s) {
        if (grid.horiz) { const int x = s.x - scroll; return x >= grid.panelX - 2 && x + grid.cw <= c.w; }
        const int y = grid.top + s.y - scroll; return y < grid.bottom && y + grid.ch > grid.top; };
    auto drawCard = [&](const View::Spot& s, bool sel) {
        const int x = grid.horiz ? s.x - scroll : s.x, y = grid.horiz ? grid.top + s.y : grid.top + s.y - scroll;
        const int clipMargin = (int)std::ceil(std::max(8 * u, std::max(6.0f, grid.ch * 0.06f) * 1.35f)) + 2;
        GridCardClip clip(c, grid, x, y, grid.cw, grid.ch, clipMargin);
        const Game& g = games[s.game];
        if (PanelHome())
        {
            // (The Panel home: a plain row, the picture small on the left, the name and the console right.)
            const int rh = grid.ch, pr = (int)(12 * u), t = grid.tile, tx0 = x + (int)(10 * u), ty0 = y + (rh - t) / 2;
            if (sel) { Glass::Glow(c, x, y, grid.cw, rh, pr, u); Menu::FrostBox(c, solid, x, y, grid.cw, rh, pr, true, 1.5f * u); }
            DrawCover(c, g, tx0, ty0, t, t, 8 * u);
            if (g.comingSoon) Glass::Round(c, tx0, ty0, t, t, 8 * u, 0x99000000u);
            const int nx = tx0 + t + (int)(14 * u);
            const std::string sub = g.comingSoon ? std::string("Coming Soon") : !g.known ? std::string("Not Playable") : g.platform == N3DS ? std::string(g.built ? "3DS Beta \xC2\xB7 Built" : "3DS Beta") : std::string(PlatformName(g.platform));
            const int sw = GW(sub, 13 * u, true);
            GTFit(c, nx, y + rh * 0.5f, ShortName(g), 17 * u, x + grid.cw - nx - sw - (int)(30 * u), text, true);
            GT(c, x + grid.cw - (int)(16 * u) - sw, y + rh * 0.5f, sub, 13 * u, !g.known && !g.comingSoon ? 0xFFF2A08Au : hint, true);
            if (g.favourite) Glass::DrawIcon(c, Glass::I_SPARKLE, tx0 + t - 6 * u, ty0 + 6 * u, 6 * u, 0xFFFFFFFFu);
            if (const int cl = Look().badges ? HubCloudOf(g) : 0) DrawCloud(c, tx0 + t - 8 * u, ty0 + t - 7 * u, 6 * u, cl == 2 ? 0xFFF2C46Bu : 0xFFFFFFFFu);
            return;
        }
        if (sel) Glass::Glow(c, x, y, grid.cw, grid.ch, 16 * u, u);
        Menu::FrostBox(c, solid, x, y, grid.cw, grid.ch, 16 * u, sel, sel ? 1.5f * u : 1.0f);
        const int ix = x + (int)(8 * u), iy = y + (int)(8 * u), iw = grid.tile, ih = grid.tile;
        DrawCover(c, g, ix, iy, iw, ih, 10 * u);
        if (g.comingSoon)
        {
            Glass::Round(c, ix, iy, iw, ih, 10 * u, 0x99000000u);
            DrawComingSoon(c, g, ix + iw / 2, iy + ih * 0.5f, 14 * u, text);
        }
        const float cy = iy + ih + 16 * u;
        std::string name = ShortName(g);
        float npx = 17 * u;
        // (A 3DS game whose code is built says so beside 3DS.)
        const std::string tag = g.platform == N3DS ? (g.built ? "3DS Beta \xC2\xB7 Built" : "3DS Beta") : PlatformName(g.platform);
        const int room = iw - GW(tag, 13 * u, true) - (int)(14 * u);
        while (npx > 10 * u && GW(name, npx, true) > room) npx -= u;
        GT(c, ix + (int)(4 * u), cy, name, npx, text, true);
        const std::string sub = !g.known && !g.comingSoon ? "Not Playable" : tag;
        GT(c, ix + iw - (int)(4 * u) - GW(sub, 13 * u, true), cy, sub, 13 * u, !g.known && !g.comingSoon ? 0xFFF2A08Au : hint, true);
        if (g.favourite) Glass::DrawIcon(c, Glass::I_SPARKLE, ix + iw - 14 * u, iy + 14 * u, 7 * u, 0xFFFFFFFFu);
        // (Its save kept in the cloud: a cloud in the corner; amber when it changed on both.)
        if (const int cl = Look().badges ? HubCloudOf(g) : 0)
        {
            Glass::Round(c, ix + (int)(6 * u), iy + (int)(6 * u), (int)(32 * u), (int)(22 * u), 11 * u, 0xA0000000u);
            DrawCloud(c, ix + 22 * u, iy + 18 * u, 8 * u, cl == 2 ? 0xFFF2C46Bu : 0xFFFFFFFFu);
        }
    };
    // The sidebar's rows (the chosen one framed) and the banner's buttons: drawn over the kept picture
    // each time, as they change with the arrows.
    auto drawSideRows = [&]() {
        for (int i = 0; i < SIDES; i++)
        {
            const bool cmp = sp.sideW < (int)(100 * u); // (a phone sideways: icons only)
            const int mg = cmp ? (int)(7 * u) : (int)(16 * u);
            const int ry = sp.firstRow + i * (sp.rowH + sp.rowGap), rx = sp.sideX + mg, rw = sp.sideW - 2 * mg;
            const int fs = Focus().side;
            const bool on = fs >= 0 ? i == fs : settingsChosen ? i == SIDE_SETTINGS : i == (Focus().page >= 0 && !(Focus().page == SIDE_HUB && HubMode() == 3) ? Focus().page : (int)SIDE_HOME);
            const bool lit = fs >= 0 ? i == fs : settingsChosen && i == SIDE_SETTINGS; // (chosen with the arrows)
            if (on)
            {
                if (lit) Glass::Glow(c, rx, ry, rw, sp.rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rw, sp.rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            Glass::DrawIcon(c, SideIcon(i), cmp ? rx + rw * 0.5f : rx + 27 * u, ry + sp.rowH * 0.5f, 12 * u, text);
            if (!cmp) GT(c, rx + (int)(52 * u), ry + sp.rowH * 0.5f, SideName(i), 19 * u, text, true);
        }
    };
    auto drawSections = [&]() {
        // (The Panel home's sections along the top of its panel; the one chosen with the arrows framed.)
        const int fs = Focus().side, n = SIDES;
        const int cx0 = sp.secX[0] - (int)(6 * u), cx1 = sp.secX[n - 1] + sp.secW[n - 1] + (int)(6 * u);
        Glass::Round(c, cx0, sp.secY, cx1 - cx0, sp.secH, sp.secH * 0.5f, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
        for (int i = 0; i < n; i++)
        {
            const int x = sp.secX[i], w = sp.secW[i], py = sp.secY + (int)(4 * u), ph = sp.secH - (int)(8 * u);
            const bool cur = i == SIDE_HOME, foc = fs == i;
            if (foc) { Glass::Glow(c, x, py, w, ph, ph * 0.5f, u); Glass::Round(c, x, py, w, ph, ph * 0.5f, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u); }
            else if (cur) Glass::Surface(c, 3, x, py, w, ph, ph * 0.5f, Glass::Button());
            const uint32_t ink = cur && !foc ? Glass::ButtonInk() : text;
            Glass::DrawIcon(c, SideIcon(i), x + 18 * u, py + ph * 0.5f, 8 * u, ink);
            GT(c, x + (int)(32 * u), py + ph * 0.5f, i == SIDE_HOME ? "Games" : SideName(i), 14 * u, ink, true);
        }
    };
    auto drawCarousel = [&](bool animate) {
        // (The Tiles home: the chosen game's cover large in the middle, the ones either side smaller and dimmer; its name and details under it.)
        const TilesBox tb = TilesGeometry(c);
        const int k = v.SpotOf(selected);
        if (k < 0) return;
        const int S = tb.cover, cx = c.w / 2, cyc = tb.coverY + S / 2;
        const float ks = std::min(1.0f, S / (250.0f * u)); // (the covers either side smaller with the middle one)
        const int nearW = (int)(168 * u * ks), farW = (int)(112 * u * ks), sp1 = (int)(24 * u * ks), sp2 = (int)(16 * u * ks);
        // Where each cover is and how large, a whole number of games from the middle; in between,
        // part of the way from one place to the next, so the covers slide and grow as the
        // carousel moves (Carousel: eased to the chosen game, or held by a finger).
        const double at = animate ? Carousel().Step(k, std::to_string(tab) + "/" + std::to_string(v.spots.size())) : k;
        const float widths[4] = { (float)S, (float)nearW, (float)farW, farW * 0.6f };
        const float offs[4] = { 0, S / 2.0f + sp1 + nearW / 2.0f, S / 2.0f + sp1 + nearW + sp2 + farW / 2.0f, S / 2.0f + sp1 + nearW + sp2 + farW + sp2 + farW * 0.3f };
        const float dims[4] = { 0, 0x70, 0xA0, 0xFF }, radii[4] = { 20, 14, 14, 14 };
        const int mid = (int)std::floor(at + 0.5);
        for (int pass = 0; pass < 2; pass++) // (the middle one last, over its neighbours as they pass)
        for (int j = mid - 3; j <= mid + 3; j++)
        {
            if (j < 0 || j >= (int)v.spots.size() || (pass == 1) != (j == mid)) continue;
            const float t = (float)(j - at), a = std::min(2.999f, std::fabs(t));
            if (std::fabs(t) >= 2.95f) continue;
            const int lo = (int)a;
            const float f = a - lo;
            auto mix = [&](const float* q) { return q[lo] + (q[lo + 1] - q[lo]) * f; };
            const int w = std::max(2, (int)std::lround(mix(widths)));
            const int x = cx + (int)std::lround((t < 0 ? -1 : 1) * mix(offs)) - w / 2, y = cyc - w / 2;
            const float r = mix(radii) * u;
            const Game& g = games[v.spots[j].game];
            const bool front = a < 0.5f;
            if (front) Glass::Glow(c, x, y, w, w, r, u);
            DrawCover(c, g, x, y, w, w, r);
            const int dim = (int)mix(dims);
            if (g.comingSoon) Glass::Round(c, x, y, w, w, r, 0x99000000u);
            else if (dim > 0) Glass::Round(c, x, y, w, w, r, (uint32_t)std::min(255, dim) << 24);
            if (front && g.comingSoon) DrawComingSoon(c, g, x + w / 2, y + w * 0.5f, 16 * u, text);
            if (front && g.favourite) Glass::DrawIcon(c, Glass::I_SPARKLE, x + w - 18 * u, y + 18 * u, 8 * u, 0xFFFFFFFFu);
        }
        const Game& g = games[v.spots[k].game];
        const std::string nm = ShortName(g);
        const int nw = std::min(GW(nm, 34 * u, true), (int)(c.w * 0.7f));
        GTFit(c, cx - nw / 2, (float)tb.nameY, nm, 34 * u, nw, text, true);
        const std::string saveS = HubCloudOf(g) == 2 ? "Cloud Save: Changed On Both" : HubCloudOf(g) ? "Cloud Save" : g.hasSave ? "Save Found" : "New Game";
        const std::string meta = g.comingSoon ? std::string("Coming Soon") : !g.known ? std::string("Not Playable") : std::string(PlatformName(g.platform)) + "   \xC2\xB7   " + saveS;
        GT(c, cx - GW(meta, 16 * u, true) / 2, (float)tb.metaY, meta, 16 * u, hint, true);
    };
    auto drawTiles = [&]() {
        // (The Tiles home's row of tiles, the chosen one with the arrows framed; its glass is in the kept picture.)
        for (int i = 0; i < SIDES; i++)
        {
            const int x = sp.secX[i], y = sp.secYv[i], w = sp.secW[i], h = sp.secH, r = (int)(16 * u);
            const bool foc = Focus().page < 0 && Focus().side == i, cur = i == (Focus().page >= 0 && !(Focus().page == SIDE_HUB && HubMode() == 3) ? Focus().page : (int)SIDE_HOME);
            if (foc) { Glass::Glow(c, x, y, w, h, r, u); Glass::Round(c, x, y, w, h, r, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u); }
            else if (cur) Glass::Surface(c, 3, x, y, w, h, r, Glass::Button());
            const uint32_t ink = cur && !foc ? Glass::ButtonInk() : text;
            Glass::DrawIcon(c, SideIcon(i), x + w * 0.5f, y + h * 0.375f, 13 * u, ink);
            const std::string lbl = i == SIDE_HOME ? "Library" : SideName(i);
            GT(c, x + (w - GW(lbl, 15 * u, true)) / 2, y + h * 0.73f, lbl, 15 * u, ink, true);
        }
    };
    auto drawNavBar = [&]() {
        // (A phone upright: the sections along the foot, icon over name; the shown one in the Buttons' colour, the one chosen with the arrows framed.)
        for (int i = 0; i < SIDES; i++)
        {
            const int x = sp.secX[i], w = sp.secW[i], py = sp.navY + (int)(5 * u), ph = sp.navH - (int)(10 * u);
            const bool foc = Focus().page < 0 && Focus().side == i, cur = i == (Focus().page >= 0 && !(Focus().page == SIDE_HUB && HubMode() == 3) ? Focus().page : (int)SIDE_HOME);
            if (foc) { Glass::Glow(c, x, py, w, ph, 14 * u, u); Glass::Round(c, x, py, w, ph, 14 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u); }
            else if (cur) Glass::Surface(c, 3, x, py, w, ph, 14 * u, Glass::Button());
            const uint32_t ink = cur && !foc ? Glass::ButtonInk() : text;
            Glass::DrawIcon(c, SideIcon(i), x + w * 0.5f, py + ph * 0.36f, 9 * u, ink);
            const std::string lbl = SideName(i);
            GT(c, x + (w - GW(lbl, 10 * u, true)) / 2, py + ph * 0.78f, lbl, 10 * u, ink, true);
        }
    };
    auto drawBannerButtons = [&]() {
        if (cont < 0 || cont >= (int)games.size() || Focus().page >= 0) return;
        const bool played = !ContinueRom().empty() && games[cont].rom == ContinueRom();
        if (Focus().banner == 1) Glass::Glow(c, sp.contX, sp.contY, sp.contW, sp.contH, 12 * u, u);
        Glass::Surface(c, 3, sp.contX, sp.contY, sp.contW, sp.contH, 12 * u, Glass::Button()); // (the Buttons: a colour or a picture)
        Glass::DrawIcon(c, Glass::I_PLAY, sp.contX + 30 * u, sp.contY + sp.contH * 0.5f, 8 * u, Glass::ButtonInk());
        GT(c, sp.contX + (int)(48 * u), sp.contY + sp.contH * 0.5f, played ? "Continue" : "Play", 18 * u, Glass::ButtonInk(), true);
        if (PhoneLook()) return; // (a phone: Continue only; a game's menu is on its card)
        const int ox = sp.contX + sp.contW + (int)(10 * u), ow = GW("Game Options", 18 * u, true) + (int)(36 * u);
        if (Focus().banner == 2)
        {
            Glass::Glow(c, ox, sp.contY, ow, sp.contH, 12 * u, u);
            Glass::Round(c, ox, sp.contY, ow, sp.contH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
        }
        else Glass::Round(c, ox, sp.contY, ow, sp.contH, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
        GT(c, ox + (int)(18 * u), sp.contY + sp.contH * 0.5f, "Game Options", 18 * u, text, true);
    };
    auto moreBelow = [&]() {
        if (TilesHome() || grid.horiz) return;
        if (scroll + (grid.bottom - grid.top) < v.height - grid.gap)
            Glass::Round(c, (PanelHome() ? grid.gx : sp.tabsX) + grid.cols * grid.colStep / 2 - (int)(16 * u), grid.bottom + (int)(4 * u), (int)(32 * u), (int)(4 * u), 2 * u, 0x80FFFFFFu);
    };
    if (key == baseKey && base.size() == c.px.size())
    {
        std::copy(base.begin(), base.end(), c.px.begin());
        Glass::Mark(c, 0, 0, c.w, c.h);
    }
    else
    {
    solid = Glass::Backdrop(c);

    // The sidebar: the sections, Home chosen (Settings while it is chosen with the arrows).
    // (Frosted, square, with a soft shadow and a faint line where it meets the rest of the screen.)
    if (PanelHome())
    {
        // The Panel home: one glass panel holds everything (no sidebar: its sections run along the top).
        const PanelBox pb = PanelGeometry(c);
        Menu::FrostBox(c, solid, pb.x, pb.y, pb.w, pb.h, 22 * u);
    }
    else if (PhoneLook() && c.h > c.w)
    {
        // A phone upright: one frosted bar along the foot holds the sections.
        Menu::FrostBox(c, solid, sp.navX, sp.navY, sp.navW, sp.navH, 26 * u); // (on every page: the way to the others)
    }
    else if (TilesOn())
    {
        // The Tiles home: no sidebar; a frosted tile for each section along the foot.
        for (int i = 0; i < SIDES; i++) Menu::FrostBox(c, solid, sp.secX[i], sp.secYv[i], sp.secW[i], sp.secH, 16 * u);
    }
    else
    {
    Glass::Shadow(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, 18 * u, 0x60);
    if (solid) Glass::Frost(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0);
    else Glass::Round(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, Menu::GlassShade);
    Glass::Surface(c, 1, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, Glass::Tint(1)); // (the Sidebar: a colour or a picture)
    Glass::Round(c, sp.sideX + sp.sideW - std::max(1, (int)u), sp.sideY, std::max(1, (int)u), sp.sideH, 0, 0x3DFFFFFF);
    }
    // (Its rows: drawSideRows, over the kept picture.)

    // The tabs, as pills: the shown one white.
    {
        // (A bar along the top, flush with the screen's top and right edges, meeting the sidebar.)
        const int barX = sp.sideX + sp.sideW, barW = c.w - barX, barH = c.h > c.w && Focus().page >= 0 ? (int)(44 * u) : sp.tabsY * 2 + sp.tabsH + (c.h > c.w ? (int)(44 * u) : 0), line = std::max(1, (int)u);
        if (!PanelHome())
        {
        if (solid) Glass::Frost(c, barX, 0, barW, barH, 0);
        else Glass::Round(c, barX, 0, barW, barH, 0, Menu::GlassShade);
        Glass::Surface(c, 2, barX, 0, barW, barH, 0, Glass::Tint(2)); // (the Top Bar: a colour or a picture)
        Glass::Round(c, barX, barH - line, barW, line, 0, 0x3DFFFFFF);
        }
        if (Focus().page < 0) for (int t = 0; t < TabCount(); t++)
        {
            if (TabIsHidden(t)) continue; // (hidden: not drawn)
            const int px = sp.tabX[t], py = sp.tabsY + (int)(4 * u), pw = sp.tabW[t], ph = sp.tabsH - (int)(8 * u);
            const std::string name = TabLabel(t);
            if (t == tab)
            {
                if (onTabs && !Focus().onSort) Glass::Glow(c, px, py, pw, ph, ph * 0.5f, u);
                Glass::Surface(c, 3, px, py, pw, ph, ph * 0.5f, Glass::Button());
            }
            GT(c, px + (pw - GW(name, sp.tabFont, true)) / 2, py + ph * 0.5f, name, sp.tabFont, t == tab ? Glass::ButtonInk() : text, true);
        }
    }

    // A page of the sidebar (About) in place of the games.
    if (Focus().page >= 0 && !TilesOn()) DrawPage(c, sp, solid, ShortNow() ? u * 1.0f : u); // (a phone sideways: the pages are made for a tall screen, so smaller than the home)
    BackRect().s = 0;
    if (PhoneLook() && Focus().page >= 0)
    {
        const int bs = (int)(34 * u), bx = c.w - (int)((c.h > c.w ? 12 : 16) * u) - bs;
        const int by = (int)((c.h > c.w ? 22.0f * u : sp.pageTitleY > 0 ? (float)sp.pageTitleY : (sp.tabsY * 2 + sp.tabsH) * 0.5f) - bs * 0.5f); // (in the top bar, right)
        Glass::Round(c, bx, by, bs, bs, bs * 0.5f, 0x40000000u, 0x80FFFFFFu, 1.5f * u);
        Glass::DrawIcon(c, Glass::I_BACK, bx + bs * 0.5f, by + bs * 0.5f, 9 * u, text);
        BackRect() = { bx, by, bs };
    }

    // The banner: the game played last, ready to carry on.
    if (Focus().page < 0 && cont >= 0 && cont < (int)games.size() && !PanelOn() && !TilesOn())
    {
        const Game& g = games[cont];
        Menu::FrostBox(c, solid, sp.bannerX, sp.bannerY, sp.bannerW, sp.bannerH, 18 * u);
        const bool phP = PhoneLook() && c.h > c.w, phL = PhoneLook() && !phP; // (a phone: upright the cover wide over the name; sideways the cover wide beside it)
        const int ax = sp.bannerX + (int)((phP ? 14 : 16) * u), ay = sp.bannerY + (int)((phP ? 14 : 16) * u);
        const int ah = phP ? (int)(130 * u) : sp.bannerH - (int)(32 * u), aw = phP ? sp.bannerW - (int)(28 * u) : phL ? ah * 16 / 9 : ah;
        DrawCover(c, g, ax, ay, aw, ah, 12 * u);
        const int tx = phP ? ax : ax + aw + (int)(24 * u);
        const bool played = !ContinueRom().empty() && g.rom == ContinueRom();
        const std::string meta = std::string(played ? "Continue Playing" : "Ready To Play") + "   \xC2\xB7   " + PlatformName(g.platform)
                               + "   \xC2\xB7   " + (HubCloudOf(g) == 2 ? "Cloud Save: Changed On Both" : HubCloudOf(g) ? "Cloud Save" : g.hasSave ? "Save Found" : "New Game");
        if (phP)
        {
            const std::string sv = HubCloudOf(g) == 2 ? "Cloud Save: Changed On Both" : HubCloudOf(g) ? "Cloud Save" : g.hasSave ? "Save Found" : "New Game";
            GT(c, tx, ay + ah + 18 * u, played ? "Continue Playing" : "Ready To Play", 13 * u, hint, true);
            GTFit(c, tx, ay + ah + 46 * u, ShortName(g), 28 * u, sp.contX - tx - (int)(10 * u), text, true);
            GT(c, tx, ay + ah + 74 * u, std::string(PlatformName(g.platform)) + "   \xC2\xB7   " + sv, 13 * u, hint, true);
        }
        else if (phL)
        {
            GT(c, tx, sp.bannerY + 36 * u, meta, 14 * u, hint, true);
            GTFit(c, tx, sp.bannerY + 70 * u, ShortName(g), 30 * u, sp.contX - tx - (int)(16 * u), text, true);
        }
        else
        {
        GT(c, tx, sp.bannerY + 30 * u, meta, 16 * u, hint, true);
        GT(c, tx, sp.bannerY + 66 * u, ShortName(g), 36 * u, text, true);
        }
        // (Continue and Game Options: drawBannerButtons, over the kept picture.)
    }

    if (Focus().page < 0)
    {
    // "Your Games" and how many.
    if (!PanelOn() && !TilesOn() && !PhoneLook()) GT(c, sp.tabsX, sp.headY + 14 * u, SearchText().empty() ? std::string("Your Games") : "Results For \"" + SearchText() + "\"", 24 * u, text, true);
    if (!PanelOn() && !TilesOn() && !PhoneLook())
    {
        const std::string n = std::to_string(v.spots.size()) + (v.spots.size() == 1 ? " Game" : " Games");
        GT(c, sp.bannerX + sp.bannerW - GW(n, 16 * u, true), sp.headY + 14 * u, n, 16 * u, hint, true);
    }

    // The cards (none chosen: the chosen one is drawn over them below).
    for (const View::Head& h : v.heads)
        if (h.empty)
        {
            // (No games at all: where they go, whichever tab is open.)
            bool any = false;
            for (const Game& g : games) any |= !g.comingSoon;
            const std::string none = !SearchText().empty() ? "No Games Match \"" + SearchText() + "\""
                                   : !any ? std::string("No Games Yet: Add Your ROMs On The ROMs Page")
                                   : tab >= TABS ? std::string("Add Games To This Tab From A Game's Menu (A On A Game)")
                                   : tab == TAB_HOME ? "Add Games To Home From A Game's Menu (A On A Game)"
                                   : tab == TAB_HACKS ? "No Romhacks Yet" : "No " + std::string(TabLabel(tab)) + " Games Yet";
            GT(c, (TilesHome() ? grid.gx : sp.tabsX) + (grid.cols * grid.colStep - GW(none, 16 * u, true)) / 2, (grid.top + grid.bottom) * 0.5f, none, 16 * u, hint, true);
        }
    if (TilesHome()) { /* (the carousel: over the kept picture, below, as it moves) */ }
    else for (const View::Spot& s : v.spots)
        if (visible(s) && !(PanelHome() && s.game == selected)) drawCard(s, false);
    moreBelow();
    }
    if (TilesOn() && Focus().page >= 0)
    {
        // The Tiles home's pages: over the home, dimmed, in a window.
        drawCarousel(false); drawTiles();
        // (The window is the launcher's own glass: inside it the theme's Background, so its boxes and rows take the Panels, Buttons and
        // Accent as on the other homes; outside it the home dimmed by the Background's Dim.)
        const int dimA = std::min(255, (int)(0xB0 * (0.5f + Look().dim / 20.0f)));
        Glass::Round(c, 0, 0, c.w, c.h, 0, (uint32_t)dimA << 24);
        Glass::Shadow(c, sp.winX, sp.winY, sp.winW, sp.winH, 22 * u, 24 * u, 0x80);
        {
            const std::vector<uint32_t> keep = c.px;
            solid = Glass::Backdrop(c);
            const int wr = (int)(22 * u), wx0 = sp.winX, wx1 = sp.winX + sp.winW - 1, wy0 = sp.winY, wy1 = sp.winY + sp.winH - 1;
            for (int py = 0; py < c.h; py++)
                for (int px = 0; px < c.w; px++)
                {
                    bool in = px >= wx0 && px <= wx1 && py >= wy0 && py <= wy1;
                    if (in)
                    {
                        const int dx = std::max(0, std::max(wx0 + wr - px, px - (wx1 - wr))), dy = std::max(0, std::max(wy0 + wr - py, py - (wy1 - wr)));
                        in = dx * dx + dy * dy <= wr * wr;
                    }
                    if (!in) c.px[(size_t)py * c.w + px] = keep[(size_t)py * c.w + px];
                }
            Glass::Round(c, sp.winX, sp.winY, sp.winW, sp.winH, 22 * u, 0x28000000u, 0x4DFFFFFFu, 1.5f * u);
        }
        DrawPage(c, sp, solid, u);
    }
    base = c.px;
    baseKey = key;
    }
    if (PanelHome()) drawSections(); else if (TilesOn())
    {
        if (Focus().page < 0)
        {
            drawCarousel(true); drawTiles(); drawBannerButtons();
            // NOW PLAYING: centred above the tile row, under the buttons.
            if (Music().shown && !Music().idle && Look().music != "off" && (Look().music != "top" || c.h > c.w))
            {
                const TilesBox tb = TilesGeometry(c);
                const int bh2 = tb.musicH, bw2 = std::min((int)(480 * u), c.w - (int)((c.h > c.w ? 24 : 120) * u));
                const int by2 = (tb.btnY + (int)(44 * u) + tb.tileY) / 2 - bh2 / 2;
                if (Look().music == "compact" || Look().music == "compactb") { const int mw2 = 3 * (int)(bh2 * 0.9f); DrawMusicButtons(c, (c.w - mw2) / 2, by2, mw2, bh2, u); }
                else DrawMusicBar(c, (c.w - bw2) / 2, by2, bw2, bh2, u);
            }
        }
    } else if (PhoneLook() && c.h > c.w)
    {
        drawNavBar();
        if (Focus().page < 0)
        {
            drawBannerButtons();
            // NOW PLAYING: a strip just over the bar of sections.
            if (Music().shown && !Music().idle && Look().music != "off")
            {
                const int mh = (int)(36 * u);
                if (Look().music == "compact" || Look().music == "compactb") { const int mw = 3 * (int)(mh * 0.9f); DrawMusicButtons(c, sp.navX + (sp.navW - mw) / 2, sp.navY - (int)(8 * u) - mh, mw, mh, u); }
                else DrawMusicBar(c, sp.navX, sp.navY - (int)(8 * u) - mh, sp.navW, mh, u);
            }
        }
    } else
    {
        drawSideRows(); drawBannerButtons();
        // NOW PLAYING: a square at the foot of the sidebar, almost to its edges.
        if (Music().shown && !Music().idle && Look().music == "corner" && !ShortNow())
        {
            const int m0 = (int)(10 * u), sz = sp.sideW - 2 * m0;
            DrawMusicSquare(c, sp.sideX + m0, c.h - m0 - sz, sz, u);
        }
        else if (Music().shown && !Music().idle && Look().music == "compactb" && !ShortNow())
        {
            const int m0 = (int)(10 * u), pw = sp.sideW - 2 * m0, ph = std::min((int)(44 * u), pw * 10 / 27);
            DrawMusicButtons(c, sp.sideX + m0, c.h - m0 - ph, pw, ph, u);
        }
    }
    // The Panel home's preview: the chosen game beside the list (its buttons: drawBannerButtons).
    if (PanelHome())
    {
        const PanelBox pb = PanelGeometry(c);
        const int by = pb.contentY, bh = pb.contentB - pb.contentY;
        Menu::FrostBox(c, solid, pb.prevX, by, pb.prevW, bh, 18 * u);
        if (shown)
        {
            const Game& g = *shown;
            const int ix = pb.prevX + (int)(20 * u), iw = pb.prevW - (int)(40 * u);
            const int ph = bh * 40 / 100, py0 = by + (int)(20 * u);
            Glass::Round(c, ix, py0, iw, ph, 14 * u, 0x26FFFFFFu, 0x3DFFFFFF, 1.0f * u);
            const int cov = ph - (int)(20 * u);
            DrawCover(c, g, ix + (iw - cov) / 2, py0 + (ph - cov) / 2, cov, cov, 12 * u);
            if (g.comingSoon)
            {
                Glass::Round(c, ix + (iw - cov) / 2, py0 + (ph - cov) / 2, cov, cov, 12 * u, 0x99000000u);
                DrawComingSoon(c, g, ix + iw / 2, py0 + ph * 0.5f, 16 * u, text);
            }
            float ty = (float)(py0 + ph + 34 * u);
            GTFit(c, ix, ty, ShortName(g), 34 * u, iw, text, true);
            ty += 46 * u;
            const std::string saveS = HubCloudOf(g) == 2 ? "Cloud: Changed On Both" : HubCloudOf(g) ? "Cloud Save" : g.hasSave ? "Save Found" : "New Game";
            std::string last = "Never";
            if (g.lastPlayed > 0)
            {
                const long long days = ((long long)std::time(nullptr) - g.lastPlayed) / 86400;
                last = days <= 0 ? std::string("Today") : days == 1 ? std::string("Yesterday") : std::to_string(days) + " Days Ago";
            }
            const std::string lab[3] = { "Platform", "Save", "Last Played" };
            const std::string val[3] = { std::string(PlatformName(g.platform)), saveS, last };
            const int colW = iw / 3;
            for (int k = 0; k < 3; k++)
            {
                GT(c, ix + k * colW, ty, lab[k], 12 * u, hint, true);
                GTFit(c, ix + k * colW, ty + 24 * u, val[k], 18 * u, colW - (int)(10 * u), text, true);
            }
        }
        drawBannerButtons();
    }
    // The tab bar's tools: + (a tab of your own) after the tabs, Search and Sort at its right end
    // (chosen: right past the last tab, then along them).
    for (int k = 1; k < 4; k++) Focus().toolW[k] = 0;
    if (Focus().page < 0 && !sp.tabX.empty())
    {
        const bool pnl = PanelHome();
        const PanelBox pb = PanelGeometry(c);
        const bool prt = c.h > c.w; // (upright: Search and Sort on a row of their own under the tabs)
        const int barH = sp.tabsY * 2 + sp.tabsH, bh = prt ? (int)(36 * u) : (int)(40 * u), by = prt ? sp.tabsY + sp.tabsH + (int)(8 * u) : pnl ? sp.tabsY + (sp.tabsH - bh) / 2 : (barH - bh) / 2;
        const int toolR = pnl ? pb.x + pb.w - pb.pad : c.w - (int)((prt ? 12 : 28) * u);
        const bool pnlTool = pnl;
        auto tool = [&](int k, int bx, int bw, const std::string& label, int icon) {
            GlassFocus& F = Focus();
            F.toolX[k] = bx; F.toolW[k] = bw; F.toolY = by; F.toolH = bh;
            if (onTabs && F.onSort && F.tool == k)
            {
                Glass::Glow(c, bx, by, bw, bh, 12 * u, u);
                Glass::Round(c, bx, by, bw, bh, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            else Glass::Round(c, bx, by, bw, bh, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
            if (icon >= 0 && label.empty()) Glass::DrawIcon(c, icon, bx + bw * 0.5f, by + bh * 0.5f, 9 * u, text);
            else if (icon >= 0)
            {
                Glass::DrawIcon(c, icon, bx + 22 * u, by + bh * 0.5f, 9 * u, text);
                GT(c, bx + (int)(40 * u), by + bh * 0.5f, label, pnlTool ? 13 * u : 15 * u, text, true);
            }
            else
            {
                const float px = label == "+" ? 24 * u : pnlTool ? 13 * u : 15 * u;
                GT(c, bx + (bw - GW(label, px, true)) / 2, by + bh * 0.5f, label, px, text, true);
            }
        };
        const float tpx = pnl ? 13 * u : 15 * u;
        const bool icn = true; // (Sort and Search are small icon buttons: Sort opens a menu of orders; Search shows what is searched for)
        const std::string sortL = std::string("Sort: ") + SortLabel(SortKey());
        const int sortW = icn ? bh : GW(sortL, tpx, true) + (int)(pnl ? 46 : 52) * u, sortX = toolR - sortW;
        std::string q = SearchText();
        if (q.size() > 14) q = q.substr(0, 13) + "...";
        const std::string searchL = q.empty() ? (icn ? std::string() : std::string("Search")) : "\"" + q + "\"";
        const int searchW = q.empty() && icn ? bh : GW(searchL, tpx, true) + (int)(q.empty() ? (pnl ? 26 : 36) : (pnl ? 46 : 52)) * u, searchX = sortX - (int)(10 * u) - searchW;
        const int last = LastTab();
        tool(1, prt ? (int)(12 * u) : sp.tabX[last] + sp.tabW[last] + (int)(10 * u), bh, "+", -1);
        tool(2, searchX, searchW, searchL, icn ? Glass::I_SEARCH : -1);
        tool(3, sortX, sortW, icn ? std::string() : sortL, icn ? Glass::I_SORT : Glass::I_LAYERS);
        // NOW PLAYING along the top (Launcher > Music Player > Top): between the tabs and Search / Sort.
        if (Look().music == "off") { }
        else if ((ShortNow() || (Look().music == "compact" && !TilesOn() && !pnl)) && Music().shown && !Music().idle) { const int mw = 3 * (int)(bh * 0.9f); DrawMusicButtons(c, searchX - (int)(10 * u) - mw, by, mw, bh, u); }
        else if ((Look().music == "top" || ShortNow()) && !pnl && Music().shown && !Music().idle)
        {
            const int mx = sp.tabX[last] + sp.tabW[last] + (int)(10 * u) + bh + (int)(14 * u), avail = searchX - (int)(14 * u) - mx;
            if (avail >= (int)(200 * u)) { const int mw = std::min(avail, (int)(400 * u)); DrawMusicBar(c, mx + (avail - mw) / 2, by, mw, bh, u); }
        }
    }
    // The chosen card, over the kept picture: the blurred background put back round it first.
    {
        const int k = v.SpotOf(selected);
        if (k >= 0 && visible(v.spots[k]) && Focus().page < 0 && !TilesOn())
        {
            const View::Spot& s = v.spots[k];
            const int sx = grid.horiz ? s.x - scroll : s.x;
            const int m = (int)(7 * u), x0 = std::max(0, sx - m), x1 = std::min(c.w, sx + grid.cw + m);
            const int y = grid.horiz ? grid.top + s.y : grid.top + s.y - scroll;
            const int y0 = std::max(grid.horiz ? 0 : grid.top, y - m), y1 = std::min(grid.horiz ? c.h : grid.bottom, y + grid.ch + m);
            if (solid && !PanelHome())
                for (int yy = y0; yy < y1; yy++)
                    std::copy(c.backdrop.begin() + (size_t)yy * c.w + x0, c.backdrop.begin() + (size_t)yy * c.w + x1, c.px.begin() + (size_t)yy * c.w + x0);
            drawCard(s, true);
            moreBelow();
        }
    }

    // The game's menu (A on a game), beside its card: a glass panel of rows as the sidebar's.
    Focus().popW = 0;
    if (Focus().popup >= 0 && selected >= 0 && selected < (int)games.size())
    {
        const int k = v.SpotOf(selected);
        if (k >= 0)
        {
            static const int icons[] = { Glass::I_PLAY, Glass::I_POKEBALL, Glass::I_SPARKLE, Glass::I_WINDOW, Glass::I_SAVE, Glass::I_LAYERS, Glass::I_BACK };
            const Game& g = games[selected];
            const int cx = grid.horiz ? v.spots[k].x - scroll : v.spots[k].x, cyy = grid.horiz ? grid.top + v.spots[k].y : grid.top + v.spots[k].y - scroll;
            const int n = PopupCount(), rowH = (int)(44 * u), rg = (int)(4 * u), pad = (int)(8 * u), pw = (int)(250 * u);
            const int ph = n * rowH + (n - 1) * rg + 2 * pad;
            int px = cx + grid.cw + (int)(12 * u);
            if (px + pw > c.w - (int)(28 * u)) px = cx - (int)(12 * u) - pw;
            px = std::max((int)(12 * u), std::min(px, c.w - (int)(12 * u) - pw)); // (upright: no room beside the card, so over it, never off the screen)
            const int py = std::max((int)(28 * u), std::min(cyy, c.h - (int)(82 * u) - ph));
            Focus().popX = px + pad; Focus().popY = py + pad; Focus().popW = pw - 2 * pad; Focus().popRow = rowH + rg;
            // (The rest of the screen dimmed, and the blurred background put back under the menu so
            // its glass frosts that, not the cards behind it.)
            Glass::Round(c, 0, 0, c.w, c.h, 0, 0x59000000u);
            if (solid)
            {
                const int r = (int)(18 * u);
                for (int yy = std::max(0, py); yy < std::min(c.h, py + ph); yy++)
                    for (int xx = std::max(0, px); xx < std::min(c.w, px + pw); xx++)
                    {
                        const int ex = std::max(0, std::max(px + r - xx, xx - (px + pw - 1 - r))), ey = std::max(0, std::max(py + r - yy, yy - (py + ph - 1 - r)));
                        if (ex * ex + ey * ey <= r * r) c.px[(size_t)yy * c.w + xx] = c.backdrop[(size_t)yy * c.w + xx];
                    }
            }
            Menu::FrostBox(c, solid, px, py, pw, ph, 18 * u);
            for (int i = 0; i < n; i++)
            {
                const int ry = py + pad + i * (rowH + rg), rx = px + pad, rw = pw - 2 * pad;
                const uint32_t ink = PopupItem(i) == 6 ? 0xFFF2A08Au : text;
                if (i == Focus().popup)
                {
                    Glass::Glow(c, rx, ry, rw, rowH, 12 * u, u);
                    Glass::Round(c, rx, ry, rw, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
                }
                Glass::DrawIcon(c, icons[PopupItem(i)], rx + 22 * u, ry + rowH * 0.5f, 9 * u, ink);
                GT(c, rx + (int)(44 * u), ry + rowH * 0.5f, PopupLabel(g, PopupItem(i)), 17 * u, ink, true);
            }
        }
    }

    // A dialog from the game's menu (Remove Game, Delete ROM File, Rename) in the middle, the rest dimmed.
    Focus().dW = 0;
    // (Rename: the keyboard page. A tab of your own's menu: drawn with no game chosen too, from the tab bar.)
    if (Focus().dialog && Focus().dialog != 3 && ((selected >= 0 && selected < (int)games.size()) || Focus().dialog >= 8))
    {
        GlassFocus& F = Focus();
        static const Game none;
        const Game& g = selected >= 0 && selected < (int)games.size() ? games[selected] : none;
        Game own = g;
        own.customName.clear();
        std::string title, msg;
        std::vector<std::string> rows;
        int red = -1;
        if (F.dialog == 1) { title = "Remove " + ShortName(g) + "?"; msg = "Remove From List hides it here and keeps the ROM in your folder. Your saves are never deleted."; rows = { "Remove From List", "Delete ROM File", "Cancel" }; red = 1; }
        else if (F.dialog == 2)
        {
            const size_t sl = g.rom.find_last_of("/\\");
            title = "Delete The ROM File?";
            msg = "This deletes " + (sl == std::string::npos ? g.rom : g.rom.substr(sl + 1)) + " from your folder. Your saves are kept.";
            rows = { "Delete ROM File", "Cancel" }; red = 0;
        }
        else if (F.dialog >= 5) { title = F.dTitle; msg = F.dMsg; rows = F.dItems; red = F.dialog == 6 || F.dialog == 18 ? 0 : -1; }
        else if (F.dialog == 4) { title = "Change Picture"; msg = "Pictures stay on this device. Only use pictures you have permission to use."; rows = { "Import A Picture", "Find Artwork Online", "Plain Card", "Restore Original", "Cancel" }; }
        else { title = "Rename " + ShortName(own); msg = "Type a new name. Leave it empty to go back to the game's own name."; rows = { "Save", "Cancel" }; }
        const int pw = std::min((int)(560 * u), c.w - (int)(24 * u)), pad = (int)(24 * u), rowH = (int)(48 * u), rg = (int)(8 * u), fieldH = (int)(52 * u);
        std::vector<std::string> lines;
        {
            std::string line, word;
            const int avail = pw - 2 * pad;
            auto flush = [&]() { if (!word.empty()) { const std::string t = line.empty() ? word : line + " " + word; if (!line.empty() && GW(t, 15 * u, false) > avail) { lines.push_back(line); line = word; } else line = t; word.clear();
                while (GW(line, 15 * u, false) > avail) { size_t cut = 0, i = 0; while (i < line.size()) { size_t j = i + 1; while (j < line.size() && ((unsigned char)line[j] & 0xC0) == 0x80) j++; if (cut > 0 && GW(line.substr(0, j), 15 * u, false) > avail) break; cut = j; i = j; } if (cut == 0 || cut >= line.size()) break; lines.push_back(line.substr(0, cut)); line = line.substr(cut); } } };
            const std::string tmsg = LauncherText::Tr(msg); // (the player's language first: then the lines are cut where its words are)
            for (char ch : tmsg) { if (ch == ' ') flush(); else word += ch; }
            flush();
            if (!line.empty()) lines.push_back(line);
        }
        const int ph = pad + (int)(36 * u) + (int)lines.size() * (int)(22 * u) + (int)(16 * u) + (F.dialog == 3 ? fieldH + rg : 0) + (F.dialog == 4 ? (int)(150 * u) + 2 * rg : 0) + (F.dialog == 10 ? (int)(200 * u) + 2 * rg : 0)
                     + (int)rows.size() * (rowH + rg) - rg + pad;
        const int px = (c.w - pw) / 2, py = (c.h - ph) / 2;
        Glass::Round(c, 0, 0, c.w, c.h, 0, 0x73000000u);
        if (solid)
        {
            const int r = (int)(20 * u);
            for (int yy = std::max(0, py); yy < std::min(c.h, py + ph); yy++)
                for (int xx = std::max(0, px); xx < std::min(c.w, px + pw); xx++)
                {
                    const int ex = std::max(0, std::max(px + r - xx, xx - (px + pw - 1 - r))), ey = std::max(0, std::max(py + r - yy, yy - (py + ph - 1 - r)));
                    if (ex * ex + ey * ey <= r * r) c.px[(size_t)yy * c.w + xx] = c.backdrop[(size_t)yy * c.w + xx];
                }
        }
        Menu::FrostBox(c, solid, px, py, pw, ph, 20 * u);
        int y = py + pad;
        GT(c, px + pad, y + 14 * u, title, 22 * u, text, true);
        y += (int)(36 * u);
        if (F.dialog == 4)
        {
            // (The picture it has now.)
            const int ps = (int)(150 * u);
            DrawCover(c, g, px + (pw - ps) / 2, y, ps, ps, 14 * u);
            y += ps + 2 * rg;
        }
        for (const std::string& l : lines) { GT(c, px + pad, y + 10 * u, l, 15 * u, hint, false); y += (int)(22 * u); }
        y += (int)(16 * u);
        if (F.dialog == 10)
        {
            // (The beer message: its code, to scan with a phone.)
#ifdef __ANDROID__
            const int qs = 0; // (no QR codes on Android: the links are there to choose)
#else
            const int qs = (int)(200 * u);
#endif
            DrawQr(c, kBeerQr, kBeerQrSize, px + (pw - qs) / 2, y, qs);
            y += qs + 2 * rg;
        }
        if (F.dialog == 3)
        {
            Glass::Round(c, px + pad, y, pw - 2 * pad, fieldH, 12 * u, 0x24FFFFFFu, 0xF2FFFFFFu, 1.5f * u);
            GT(c, px + pad + (int)(16 * u), y + fieldH * 0.5f, F.text + "|", 18 * u, text, true);
            y += fieldH + rg;
        }
        F.dX = px + pad; F.dY = y; F.dW = pw - 2 * pad; F.dRow = rowH + rg; F.dRows = (int)rows.size();
        for (int i = 0; i < (int)rows.size(); i++)
        {
            const int ry = y + i * (rowH + rg), rx = px + pad, rw = pw - 2 * pad;
            const uint32_t ink = i == red ? 0xFFF2A08Au : text;
            if (i == F.drow)
            {
                Glass::Glow(c, rx, ry, rw, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rw, rowH, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
            }
            else Glass::Round(c, rx, ry, rw, rowH, 12 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
            GT(c, rx + (rw - GW(rows[i], 16 * u, true)) / 2, ry + rowH * 0.5f, rows[i], 16 * u, ink, true);
        }
    }

    // The hints in the bottom right corner, lined up with the cards' right edge: each button a
    // white badge before what it does (a message, when there is one, in their place).
    {
        const int bx = sp.tabsX, bw = c.w - sp.tabsX - (int)(28 * u), bh = (int)(44 * u), by = c.h - (int)(24 * u) - bh;
        const float cy = by + bh * 0.5f;
        if (!note.empty())
        {
            const std::string n = CleanFont::Title(UiLang::Tr(note));
            const bool up = c.h > c.w && sp.navY > 0; // (upright: its own bar over the sections, not on them)
            const int room = up ? c.w - 2 * sp.navX - (int)(66 * u) : bw - (int)(26 * u);
            float fp = 16 * u;
            while (fp > 11 * u && GW(n, fp, true) > room) fp -= 0.5f * u;
            const int tw = std::min(room, GW(n, fp, true)), w = (int)(26 * u) + tw;
            int x0 = bx + bw - w;
            float ny = cy;
            if (up)
            {
                const int ph = (int)(40 * u), py = sp.navY - (int)(10 * u) - ph;
                x0 = (c.w - w) / 2; ny = py + ph * 0.5f;
                Glass::Round(c, x0 - (int)(16 * u), py, w + (int)(32 * u), ph, ph * 0.5f, 0xE6101418u, 0x3DFFFFFFu, 1.0f * u);
            }
            Glass::DrawIcon(c, Glass::I_INFO, x0 + 9 * u, ny, 9 * u, text);
            GTFit(c, x0 + (int)(26 * u), ny, n, fp, tw + 1, text, true);
        }
        else if (c.h <= c.w && !ShortNow()) // (not upright or a phone sideways: it is a touch screen)
        {
            const std::string all = !hintText.empty() ? hintText : "A: PLAY    Y: SETTINGS    L / R: TABS    +: QUIT";
            struct Item { std::string key, act; };
            std::vector<Item> items;
            for (size_t i = 0; i < all.size();)
            {
                size_t j = all.find("    ", i);
                if (j == std::string::npos) j = all.size();
                std::string part = all.substr(i, j - i);
                while (!part.empty() && part.front() == ' ') part.erase(0, 1);
                while (!part.empty() && part.back() == ' ') part.pop_back();
                // (SETTINGS is not shown: it is in the sidebar, one press of left away. Its button still works.)
                if (!part.empty() && part.find(": SETTINGS") == std::string::npos)
                {
                    const size_t k = part.find(": ");
                    if (k == std::string::npos) items.push_back({ "", CleanFont::Title(UiLang::Tr(part)) });
                    else items.push_back({ CleanFont::Title(part.substr(0, k)), CleanFont::Title(UiLang::Tr(part.substr(k + 2))) });
                }
                i = j + 4;
            }
            auto keyW = [&](const Item& it, float f) { return std::max((int)(f * 1.7f), GW(it.key, f * 0.9f, true) + (int)(f * 1.1f)); };
            auto widthOf = [&](float f) {
                int w = 0;
                for (const Item& it : items) w += (it.key.empty() ? 0 : keyW(it, f) + (int)(f * 0.6f)) + GW(it.act, f, true) + (int)(f * 2.4f);
                return w - (int)(f * 2.4f);
            };
            float fp = 16 * u;
            while (fp > 9 * u && widthOf(fp) > bw - 40 * u) fp -= u;
            int x = bx + bw - widthOf(fp);
            for (const Item& it : items)
            {
                if (!it.key.empty())
                {
                    const int kh = (int)(fp * 1.7f), kw = keyW(it, fp);
                    Glass::Surface(c, 3, x, (int)(cy - kh * 0.5f), kw, kh, kh * 0.5f, Glass::Button()); // (the Buttons)
                    GT(c, x + (kw - GW(it.key, fp * 0.9f, true)) / 2, cy, it.key, fp * 0.9f, Glass::ButtonInk(), true);
                    x += kw + (int)(fp * 0.6f);
                }
                GT(c, x, cy, it.act, fp, text, true);
                x += GW(it.act, fp, true) + (int)(fp * 2.4f);
            }
        }
    }
    if (solid) Glass::Opaque(c);
    ThemeCaptureDone(c); // (a theme being saved: Home just drawn)
}

inline int GlassTabAt(const Canvas& c, int x, int y)
{
    const GlassSpots s = Spots(c);
    if (y < s.tabsY || y >= s.tabsY + s.tabsH) return -1;
    for (int t = 0; t < TabCount() && t < (int)s.tabX.size(); t++)
        if (x >= s.tabX[t] && x < s.tabX[t] + s.tabW[t]) return t;
    return -1;
}

// The classic list, drawn over the whole canvas (1280x720 on the Switch): the title band, the tabs, then
// the tab's games as tiles, each the game's picture (or its DS icon, enlarged) with its name and
// whether it has a save, under each console's heading on ALL. selected: -1 for none. scroll is
// kept so the chosen game is in view (follow false: as it is, a finger having scrolled the list).
// hint: the line along the bottom (the Switch's buttons when empty). onTabs: the tabs are chosen
// (up from the first row of games; left and right then change the tab), the shown one framed.
inline void DrawClassic(Canvas& c, const std::vector<Game>& games, int tab, int selected, int& scroll, const std::string& note,
                 const std::string& hintText = std::string(), const Theme& th = Theme(), bool follow = true, bool onTabs = false)
{
    const View v = MakeView(c, games, tab);
    const Grid& grid = v.g;
    const int s = grid.s;
    c.Fill(0, 0, c.w, c.h, th.bg);
    if (th.background)
        if (const uint32_t* bgPx = th.background->Fitted(c.w, c.h)) std::copy(bgPx, bgPx + (size_t)c.w * c.h, c.px.begin());
    // A band along the top with the title.
    c.Fill(0, 0, c.w, (c.h > c.w ? 43 : 22) * s, th.band);
    c.Fill(0, (c.h > c.w ? 43 : 22) * s, c.w, th.bandLineH * s, th.bandLine);
    const std::string title = "POK\x05MON FULLDEX"; // (Pokémon Fulldex: \x05 is the font's É, ui.h)
    c.Text((c.w - Canvas::TextWidth(title, s + 1)) / 2, 6 * s, title, s + 1, th.title);
    // The tabs: the chosen one bright and underlined.
    for (int t = 0; t < TABS; t++)
    {
        const std::string name = TabName(t);
        const int x = TabX(c, grid, t) - Canvas::TextWidth(name, s) / 2, y = grid.tabsY + 3 * s;
        if (onTabs && t == tab) c.Fill(x - 3 * s, y - 3 * s, Canvas::TextWidth(name, s) + 5 * s, 13 * s, th.frame);
        c.Text(x, y, name, s, t == tab ? th.title : th.hint);
        if (t == tab) c.Fill(x - 2 * s, grid.tabsY + grid.tabsH - s, Canvas::TextWidth(name, s) + 3 * s, s, th.frame);
    }

    FollowSelection(v, selected, scroll, follow);
    const int top = grid.top, bottom = grid.bottom, tile = grid.tile, labelH = grid.labelH;
    // The headings (on ALL): the console's name and a line to the right edge of the tiles.
    for (const View::Head& h : v.heads)
    {
        const int y = top + h.y - scroll;
        if (h.empty)
        {
            const std::string none = UiLang::Tr("NO GAMES FOUND") + " (" + PlatformName(h.platform) + ")";
            // (On its own tab: in the middle of the page.)
            if (tab != TAB_ALL) c.Text((c.w - Canvas::TextWidth(none, s)) / 2, (top + bottom) / 2 - 4 * s, none, s, th.hint);
            else if (y >= top && y + 10 * s <= bottom) c.Text(grid.gx, y + 2 * s, none, s, th.hint);
            continue;
        }
        if (y < top || y + 10 * s > bottom) continue;
        const std::string name = PlatformName(h.platform);
        c.Text(grid.gx, y + s, name, s + 1, th.frame);
        const int lx = grid.gx + Canvas::TextWidth(name, s + 1) + 4 * s;
        const int rx = grid.gx + grid.cols * tile + (grid.cols - 1) * grid.gap * 3;
        if (rx > lx) c.Fill(lx, y + 6 * s, rx - lx, std::max(1, s / 2), th.tile);
    }
    // Show every intersecting tile, clipped to the scrolling viewport.
    for (const View::Spot& sp : v.spots)
    {
        const int x = sp.x, y = top + sp.y - scroll;
        if (y >= bottom || y + tile + labelH <= top) continue;
        GridCardClip clip(c, grid, x, y, tile, tile + labelH, th.frameW * s);
        const Game& g = games[sp.game];
        const bool sel = sp.game == selected;
        // A frame round the chosen one.
        if (sel) c.Fill(x - th.frameW * s, y - th.frameW * s, tile + 2 * th.frameW * s, tile + 2 * th.frameW * s, th.frame);
        c.Fill(x, y, tile, tile, th.tile);
        if (!g.art.empty())
        {
            for (int py = std::max(0, -y); py < tile && y + py < c.h; py++)
            {
                const uint32_t* row = &g.art[(size_t)(py * Art / tile) * Art];
                for (int px = 0; px < tile; px++)
                {
                    const uint32_t p = row[px * Art / tile];
                    if (p >> 24 >= 0x80) c.px[(size_t)(y + py) * c.w + x + px] = p | 0xFF000000u;
                }
            }
            c.Fill(x, y, 1, 1, th.tile); // (so the canvas knows the tile was drawn)
        }
        else if (g.hasIcon)
        {
            // The DS icon, enlarged by whole pixels.
            const int is = std::max(1, tile / 40), o = (tile - 32 * is) / 2;
            for (int py = 0; py < 32; py++)
                for (int px = 0; px < 32; px++)
                    if (g.icon[py * 32 + px] >> 24) c.Fill(x + o + px * is, y + o + py * is, is, is, g.icon[py * 32 + px]);
        }
        // COMING SOON across the picture of a game on its way, the picture darkened.
        if (g.comingSoon)
        {
            for (int py = std::max(0, -y); py < tile && y + py < c.h; py++)
                for (int px = 0; px < tile; px++)
                {
                    uint32_t& p = c.px[(size_t)(y + py) * c.w + x + px];
                    p = 0xFF000000u | ((p >> 1) & 0x7F7F7F);
                }
            std::string soon = ComingSoonText(g);
            for (char& ch : soon) ch = (char)toupper((unsigned char)ch);
            const int bs = Canvas::TextWidth(soon, s) + 8 * s <= tile ? s : std::max(1, s - 1);
            c.Fill(x, y + tile / 2 - 8 * bs, tile, 16 * bs, th.band);
            c.Text(x + (tile - Canvas::TextWidth(soon, bs)) / 2, y + tile / 2 - 4 * bs, soon, bs, th.title);
        }
        // The name (shortened: POKEMON is the same for all) and the save.
        std::string name = g.name;
        if (name.rfind("POKEMON ", 0) == 0) name = name.substr(8);
        int ns = Canvas::TextWidth(name, s) <= tile ? s : std::max(1, s - 1);
        // (A hack's name is its file's, which can be long: cut to fit.)
        if (Canvas::TextWidth(name, ns) > tile && ns > 1 && (g.hack || !g.problem.empty())) ns--;
        while ((g.hack || !g.problem.empty()) && name.size() > 4 && Canvas::TextWidth(name, ns) > tile) name = name.substr(0, name.size() - 3) + "..";
        c.Text(x + (tile - Canvas::TextWidth(name, ns)) / 2, y + tile + 3 * s, name, ns, sel ? th.nameSel : th.name);
        if (sel && th.selBar) c.Fill(x + tile / 4, y + tile + (UiLang::Tall() ? 31 : 19) * s, tile / 2, s, th.selBar);
        std::string sub = g.comingSoon ? "COMING SOON" : !g.problem.empty() ? "PATCH PROBLEM" : !g.known ? "NOT PLAYABLE"
                        : g.hack ? (g.hasSave ? "HACK: SAVE FOUND" : "HACK: NEW GAME") : g.hasSave ? "SAVE FOUND" : "NEW GAME";
        if (!g.where.empty() && g.known) sub = (g.hasSave ? "SAVE " : "NEW ") + g.where; // e.g. SAVE IN BLACK2
        if(g.favourite)c.Text(x+tile-24*s,y+4*s,"FAV",s,th.nameSel);
        const int ss = std::max(1, s * 2 / 3);
        c.Text(x + (tile - Canvas::TextWidth(sub, ss)) / 2, y + tile + (UiLang::Tall() ? 19 : 12) * s, sub, ss, !g.known && !g.comingSoon ? th.bad : sel ? th.subSel : th.sub);
    }
    // More games below the ones shown. (None drawn for above: under the tabs it looked like one
    // of their underlines, and the cut tiles at the top say it already.)
    if (scroll + (bottom - top) < v.height - grid.gap) c.Fill(c.w / 2 - 8 * s, bottom - 2 * s, 16 * s, s, th.hint);
    const std::string hint = !note.empty() ? note : !hintText.empty() ? hintText : "A: PLAY    Y: SETTINGS    L / R: TABS    +: QUIT";
    int hintScale=s;
        while(hintScale>1 && Canvas::TextWidth(hint,hintScale)>c.w-8)--hintScale;
    c.Text((c.w - Canvas::TextWidth(hint, hintScale)) / 2, c.h - 12 * s, hint, hintScale, th.hint);
}

// The list: the glass launcher where it can be drawn (Glassy), else the classic one. settingsChosen:
// the sidebar's Settings chosen with the arrows (the classic list draws its own SETTINGS button).
inline void Draw(Canvas& c, const std::vector<Game>& games, int tab, int selected, int& scroll, const std::string& note,
                 const std::string& hintText = std::string(), const Theme& th = Theme(), bool follow = true, bool onTabs = false,
                 bool settingsChosen = false)
{
    if (Glassy(c))
    {
        const View v = MakeView(c, games, tab);
        DrawGlass(c, games, v, tab, selected, scroll, note, hintText, follow, onTabs, settingsChosen);
        return;
    }
    DrawClassic(c, games, tab, selected, scroll, note, hintText, th, follow, onTabs);
}

// The settings page (Y on the Switch's list, H or a controller's Y on a PC's): the pause menu's
// options with what each does, set here before playing. They are written to the same settings
// file the games read (main.cpp, Settings: keys, values and defaults must stay in step with it),
// a line at a time, leaving the others as they are. Entries without a key are what can only be
// done in the pause menu. The list's font has capitals, digits and : - + . / < > ! ( ) only.
struct Setting
{
    const char* key;  // nullptr: told about, not set here; "!...": something to do, not stored
    const char* name;
    std::vector<std::pair<std::string, std::string>> values; // stored, shown
    std::string def;  // the stored value when the file has none ("": the game decides, AUTO)
    const char* text;
};
inline std::vector<Setting> SettingsList()
{
    const std::vector<std::pair<std::string, std::string>> onOff = { { "1", "ON" }, { "0", "OFF" } };
    std::vector<Setting> l;
#ifdef __ANDROID__
    l.push_back({ "!updates", "CHECK FOR UPDATES", { { "", "CHECK" } }, "",
                  "CHECKS GITHUB FOR A NEW VERSION. INSTALL DOWNLOADS IT: IT IS CHECKED AGAINST THE RELEASE SHA256SUMS.TXT AND ANDROID ASKS "
                  "BEFORE INSTALLING. THE OLD VERSION IS KEPT. YOUR ROMS / SAVES AND SETTINGS STAY AS THEY ARE." });
#elif defined(__APPLE__)
    l.push_back({ "!updates", "CHECK FOR UPDATES", { { "", "CHECK" } }, "",
                  "CHECKS GITHUB FOR A NEW VERSION. WHEN THERE IS ONE: CHOOSE AGAIN TO OPEN ITS PAGE AND DOWNLOAD IT THERE." });
#elif defined(__SWITCH__) && defined(SSR_ALL_GAMES)
    l.push_back({ "!updates", "CHECK FOR UPDATES", { { "", "CHECK" } }, "",
                  "CHECKS GITHUB FOR A NEW VERSION. CHOOSE AGAIN TO INSTALL IT: IT IS CHECKED AGAINST THE RELEASE SHA256SUMS.TXT "
                  "AND THE OLD .NRO IS KEPT IN THE UPDATE FOLDER. YOUR ROMS / SAVES AND SETTINGS STAY AS THEY ARE." });
#elif !defined(__SWITCH__)
    l.push_back({ "!updates", "CHECK FOR UPDATES", { { "", "CHECK" } }, "",
                  "CHECKS GITHUB FOR A NEW VERSION. CHOOSE AGAIN TO INSTALL IT: IT IS CHECKED AGAINST THE RELEASE SHA256SUMS.TXT "
                  "AND THE OLD VERSION IS KEPT IN THE UPDATE FOLDER. YOUR ROMS / SAVES AND SETTINGS STAY AS THEY ARE." });
#endif
#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(SSR_IOS)
    l.push_back({ "!saves_folder", "SAVES FOLDER", { { "", "BESIDE THE ROMS" } }, "",
                  "WHERE YOUR SAVES ARE KEPT. CHOOSE A FOLDER OF YOUR OWN: YOUR SAVES AND FULLDEX FILES ARE COPIED THERE (NOTHING THERE IS REPLACED / THE OLD FOLDER STAYS AS IT IS) "
                  "AND FULLDEX USES IT FROM ITS NEXT START. A FOLDER DROPBOX OR ONEDRIVE OR SYNCTHING KEEPS IN SYNC WORKS TOO. CHOOSE AGAIN TO GO BACK TO THE FOLDER BESIDE THE ROMS." });
#endif
    l.push_back({ "!releases", "RELEASE PAGE", { { "", "OPEN" } }, "",
                  "OPENS THE LIST OF RELEASES ON GITHUB: THE CHANGES IN EACH VERSION, AND THE DOWNLOADS." });
    l.push_back({ nullptr, "RELEASE NOTES", {}, "",
                  "THE CHANGES IN EACH VERSION ARE LISTED ON ITS RELEASE PAGE ON GITHUB. YOUR ROMS / SAVES AND SETTINGS STAY WHEN FULLDEX UPDATES. "
                  "THE VERSION BEFORE IS KEPT, IN CASE YOU NEED TO GO BACK." });

#ifdef __SWITCH__
    // (The first start's 60 FPS and WORLD DETAIL depend on the game: SoulSilver and HeartGold
    // start lighter. Until either is set, AUTO.)
    l.push_back({ "fps60", "60 FPS", onOff, "", "ON: THE 3D WORLD GETS IN-BETWEEN FRAMES AND MOVES AT 60 FRAMES A SECOND (THE GAMES DRAW IT AT 30). OFF: 30 AS ON A DS." });
#else
    // (A PC, and the Android app: no BORDERLESS FULLSCREEN, WORLD ON GPU or STICK TOUCH CURSOR
    // there, which is always full screen, draws the world with the processor and has a touch
    // screen.)
    l.push_back({ "view", "SCREEN LAYOUT", { { "merged", "MERGED" }, { "hybrid", "HYBRID" }, { "side", "SIDE" }, { "vertical", "VERTICAL" }, { "top", "TOP" }, { "bottom", "BOTTOM" } }, "merged",
                  "MERGED: THE ONE WIDESCREEN PICTURE. THE OTHERS SHOW THE TWO DS SCREENS LAID OUT AS ON OTHER EMULATORS." });
    l.push_back({ "large_screen", "LARGE SCREEN", { { "auto", "AUTO" }, { "top", "TOP" }, { "bottom", "BOTTOM" } }, "auto",
                  "FOR THE LAYOUTS WITH A LARGE AND A SMALL SCREEN: WHICH ONE IS LARGE. AUTO FOLLOWS WHAT THE GAME IS DOING." });
    l.push_back({ "fps60", "60 FPS", onOff, "1", "ON: THE 3D WORLD GETS IN-BETWEEN FRAMES AND MOVES AT 60 FRAMES A SECOND (THE GAMES DRAW IT AT 30). OFF: 30 AS ON A DS." });
#ifndef SSR_PHONE
    l.push_back({ "fullscreen", "BORDERLESS FULLSCREEN", onOff, "1", "ON: FILLS YOUR SCREEN AS A BORDERLESS WINDOW AT ITS OWN RESOLUTION. SWITCHING TO OTHER WINDOWS IS INSTANT. F11 DOES THE SAME." });
#endif
#endif
    l.push_back({ "fast_button", "FAST FORWARD BUTTON", { { "hold", "HOLD" }, { "toggle", "TOGGLE" } }, "hold",
                  "HOLD: RELEASE TO STOP. TOGGLE: PRESS ONCE TO START AND AGAIN TO STOP. APPLIES TO KEYBOARD, CONTROLLER AND ON-SCREEN FAST FORWARD." });
    l.push_back({ "smooth", "SMOOTH SCALING", onOff, "0", "ON: THE PICTURE IS SOFTENED AS IT IS SCALED UP TO YOUR SCREEN. OFF: SHARP PIXELS." });
    l.push_back({ "screen_filter", "DS SCREEN FILTER", { { "0", "OFF" }, { "1", "SCANLINES" }, { "2", "LCD GRID" } }, "0",
                  "DS GAMES: A RETRO LOOK OVER THE PICTURE. LCD GRID ADDS A FAINT PIXEL GRID. THE SAME CHOICE IS AVAILABLE IN THE GAME'S DISPLAY MENU." });
    l.push_back({ "gba_lcd", "GBA LCD EFFECT", { { "auto", "AUTO" }, { "0", "OFF" }, { "1", "ON" } }, "auto",
                  "GBA GAMES: A FAINT GRID BETWEEN THE PIXELS. AUTO KEEPS YOUR EXISTING GBA SETTING. CHANGES IN THE GAME'S DISPLAY MENU ARE REMEMBERED HERE." });
    l.push_back({ "wide", "WIDESCREEN WORLD", onOff, "1", "ON: THE 3D WORLD IS DRAWN OUT TO THE SIDES OF THE 16:9 SCREEN. OFF: ONLY AS WIDE AS A DS SCREEN." });
    l.push_back({ "wide_map", "WIDE MAP LOADING", onOff, "1", "BLACK / WHITE / BLACK 2 / WHITE 2: LOADS MORE OF THE MAP AROUND YOU SO THE WIDE VIEW HAS LAND AT ITS EDGES." });
    l.push_back({ "battle_buttons", "BATTLE BUTTONS", { { "corner", "CORNER" }, { "panel", "PANEL" }, { "strip", "STRIP" } }, "strip",
                  "WHERE THE BATTLE MENU GOES. CORNER: SMALL BUTTONS IN A CORNER. PANEL: THE FIGHT / BAG / POKEMON / RUN PANEL FROM THE GAME BESIDE THE BATTLE. "
                  "STRIP: LARGE BUTTONS IN THE MESSAGE BOX IN BLACK AND WHITE (CORNER IN THE OTHERS). SOULSILVER / HEARTGOLD / PLATINUM HAVE THEIR OWN." });
#ifdef __SWITCH__
    l.push_back({ "wide_lines", "WORLD DETAIL", { { "192", "192" }, { "288", "288" }, { "384", "384" }, { "540", "540" }, { "720", "720" } }, "",
                  "HOW MANY LINES THE WIDE 3D WORLD IS DRAWN WITH. 720 IS FULL SHARPNESS ON THE SWITCH. LOWER IS LIGHTER ON THE CONSOLE." });
    l.push_back({ "busy_detail", "LOWER DETAIL WHEN BUSY", onOff, "0", "ON: DROPS THE WORLD DETAIL FOR A MOMENT WHEN DRAWING CANNOT KEEP UP (A MOVE FULL OF EFFECTS) INSTEAD OF SLOWING THE GAME DOWN." });
#else
#ifdef SSR_PHONE
    l.push_back({ "wide_lines", "3D RESOLUTION", { { "192", "192P (DS)" }, { "288", "288P" }, { "384", "384P" }, { "540", "540P" }, { "720", "720P" }, { "1080", "1080P" } }, "720",
                  "HOW SHARP THE 3D WORLD IS. 720P SUITS MOST PHONES. LOWER IF YOURS CANNOT KEEP UP OR TO SAVE BATTERY. SPRITES AND TEXT KEEP THEIR DS PIXELS." });
    l.push_back({ "busy_detail", "LOWER RES WHEN BUSY", onOff, "0", "ON: DROPS THE 3D RESOLUTION FOR A MOMENT WHEN THE PHONE CANNOT KEEP UP (A MOVE FULL OF EFFECTS) INSTEAD OF SLOWING THE GAME DOWN." });
#else
    l.push_back({ "wide_lines", "3D RESOLUTION", { { "192", "192P (DS)" }, { "288", "288P" }, { "384", "384P" }, { "540", "540P" }, { "720", "720P" }, { "1080", "1080P" }, { "1440", "1440P (2K)" }, { "2160", "2160P (4K)" } },
#ifdef __APPLE__
                  "720", // (drawn by the processor on a Mac)
#else
                  "1080",
#endif

                  "HOW SHARP THE 3D WORLD IS. PICK YOUR SCREEN RESOLUTION OR LOWER IF YOUR PC CANNOT KEEP UP. SPRITES AND TEXT KEEP THEIR DS PIXELS." });
    l.push_back({ "busy_detail", "LOWER RES WHEN BUSY", onOff, "0", "ON: DROPS THE 3D RESOLUTION FOR A MOMENT WHEN YOUR PC CANNOT KEEP UP (A MOVE FULL OF EFFECTS) INSTEAD OF SLOWING THE GAME DOWN." });
    l.push_back({ "ff_low_res", "FAST FORWARD LOW RES", onOff, "0", "ON: THE 3D WORLD DROPS TO 360P WHILE YOU FAST FORWARD FOR MORE FPS (A FASTER FAST FORWARD IF YOUR PC CANNOT KEEP UP). OFF: YOUR 3D RESOLUTION STAYS." });
#ifndef __APPLE__ // (a Mac's OpenGL stops at 4.1: the processor draws the world)
    l.push_back({ "wide_gpu", "WORLD ON GPU", onOff, "1", "ON: THE WIDE 3D WORLD IS DRAWN BY YOUR GRAPHICS CARD (OPENGL 4.3). TURN IT OFF IF THE WORLD LOOKS WRONG OR THE GAME WILL NOT START: THE PROCESSOR THEN DRAWS IT (THAT WANTS A FAST PC)." });
#endif
#endif
#endif
    l.push_back({ "show_fps", "FPS COUNTER", onOff, "0", "SHOWS THE FRAMES A SECOND IN A CORNER." });
    l.push_back({ "volume", "VOLUME", { { "0", "OFF" }, { "10", "10%" }, { "20", "20%" }, { "30", "30%" }, { "40", "40%" }, { "50", "50%" }, { "60", "60%" }, { "70", "70%" }, { "80", "80%" }, { "90", "90%" }, { "100", "100%" } }, "100",
                  "THE GAME'S SOUND AND MUSIC. OFF FOR NONE. ALSO IN THE PAUSE MENU WHILE YOU PLAY." });
#ifdef __SWITCH__
    l.push_back({ "start_button", "START BUTTON", { { "menu", "PAUSE MENU" }, { "ds", "DS START" } }, "menu",
                  "PAUSE MENU: + OPENS THE PAUSE MENU WHILE PLAYING. DS START: + IS THE DS START BUTTON (THE GAMES HARDLY USE IT) AND + AND - TOGETHER OPEN THE MENU." });
#elif defined(SSR_PHONE)
    l.push_back({ "start_button", "START BUTTON", { { "menu", "PAUSE MENU" }, { "ds", "DS START" } }, "menu",
                  "PAUSE MENU: START ON A CONTROLLER OPENS THE PAUSE MENU WHILE PLAYING. DS START: IT IS THE DS START BUTTON (THE GAMES HARDLY USE IT). THE START ON THE SCREEN IS ALWAYS THE DS START." });
#else
    l.push_back({ "start_button", "START BUTTON", { { "menu", "PAUSE MENU" }, { "ds", "DS START" } }, "ds",
                  "DS START: START ON A CONTROLLER IS THE DS START; THE XBOX / PS BUTTON, OR START AND BACK TOGETHER, OPEN THE PAUSE MENU. PAUSE MENU: START (MENU ON XBOX / OPTIONS ON PS) OPENS IT. ENTER IS ALWAYS THE DS START." });
#endif
#ifndef __SWITCH__
    l.push_back({ "buttons", "BUTTONS", { { "labels", "BY LABEL" }, { "positions", "BY POSITION" } }, "labels",
                  "BY LABEL: THE BUTTON MARKED A IS THE DS A ON ANY CONTROLLER. BY POSITION: THE BUTTONS WHERE A DS HAS THEM (XBOX A IS DS B)." });
#ifndef SSR_PHONE
    l.push_back({ "pad_touch", "STICK TOUCH CURSOR", onOff, "0", "ON: THE RIGHT STICK MOVES A CURSOR ON THE TOUCH SCREEN AND THE LEFT TRIGGER PRESSES IT." });
    // (frontend/shortcuts.h, made by the game list: pc_launcher.cpp. Not on a Mac: its Dock and
    // Launchpad start the app itself.)
#if defined(_WIN32)
    l.push_back({ "!shortcuts", "DESKTOP SHORTCUTS", { { "", "CREATE" } }, "",
                  "PUTS A SHORTCUT FOR EACH GAME IN THIS LIST ON YOUR DESKTOP WITH ITS PICTURE. IT STARTS THAT GAME STRAIGHT AWAY (QUIT IN THE PAUSE MENU STILL COMES BACK TO THIS LIST). CREATE AGAIN AFTER ADDING GAMES OR MOVING THIS FOLDER." });
#elif !defined(__APPLE__)
    l.push_back({ "!shortcuts", "DESKTOP SHORTCUTS", { { "", "CREATE" } }, "",
                  "PUTS A SHORTCUT FOR EACH GAME IN THIS LIST ON YOUR DESKTOP AND IN YOUR APPS MENU WITH ITS PICTURE. IT STARTS THAT GAME STRAIGHT AWAY. ON THE STEAM DECK THEY ALSO SHOW UP IN ADD A NON-STEAM GAME. CREATE AGAIN AFTER ADDING GAMES OR MOVING THIS FOLDER." });
#endif
#endif
#endif
#if defined(__SWITCH__) && defined(SSR_ALL_GAMES)
    // (frontend/switch_forwarder.cpp, made by the game list: switch_platform.cpp.)
    l.push_back({ "!forwarders", "HOME MENU ICONS", { { "", "INSTALL" } }, "",
                  "PUTS AN ICON FOR EACH GAME IN THIS LIST ON THE HOME MENU WITH ITS PICTURE. IT STARTS THAT GAME STRAIGHT AWAY. MADE ON THIS CONSOLE: NO PC OR KEYS FILE NEEDED. LIKE ANY FORWARDER THE ICONS NEED SIGPATCHES TO START. INSTALL AGAIN AFTER ADDING GAMES. REMOVE ONE IN THE HOME MENU (OPTIONS / DELETE SOFTWARE)." });
#endif
    // The interface's language (UiLang, ui_lang.h), the first group in the settings, LANGUAGE, on its own.
    l.insert(l.begin(), { "list_lang", "LANGUAGE", UiLang::Choices(), "auto",
                          "THE LANGUAGE OF THE GAME LIST, THE SETTINGS AND THE PAUSE MENU (NOT OF THE GAMES)." });
    size_t extraCount;const auto* extras=OptionalFeatures::Options(extraCount);
    for(size_t i=0;i<extraCount;i++)l.push_back({extras[i].key,extras[i].name,onOff,"0",extras[i].help});
#if !defined(__SWITCH__)
    // RetroAchievements from the game list too (pc_launcher.cpp: the login, as the pause menu's).
    l.push_back({ "ra", "RETROACHIEVEMENTS", onOff, "0",
                  "ON: YOUR RETROACHIEVEMENTS ACCOUNT FOLLOWS THE GAMES YOU PLAY AND UNLOCKS POP UP AS YOU PLAY (CASUAL MODE: SAVE STATES AND FAST FORWARD STILL WORK; PAUSED WHILE A CHEAT IS ON)." });
    l.push_back({ "!ra_login", "ACCOUNT", { { "", "LOG IN" } }, "",
                  "LOG IN WITH YOUR RETROACHIEVEMENTS USER NAME AND PASSWORD (A FREE ACCOUNT FROM RETROACHIEVEMENTS.ORG). THE PASSWORD IS SENT ONCE AND NOT KEPT. CHOSEN AGAIN WHEN LOGGED IN: LOG OUT." });
#endif
    // Only in the pause menu.
#ifdef __SWITCH__
    l.push_back({ nullptr, "THE PAUSE MENU", {}, "", "PRESS + WHILE PLAYING (+ AND - TOGETHER WITH START BUTTON ON DS START). EVERYTHING ABOVE IS THERE TOO AND CAN BE CHANGED AS YOU PLAY." });
    l.push_back({ nullptr, "FAST FORWARD", {}, "", "THE FAST FORWARD BUTTON USES YOUR HOLD / TOGGLE SETTING. THE PAUSE MENU CAN ALSO KEEP FAST FORWARD ON. THE BUTTON IS CHANGED IN KEYS AND BUTTONS." });
#elif defined(SSR_PHONE)
    l.push_back({ nullptr, "THE PAUSE MENU", {}, "", "THE MENU BUTTON AT THE TOP OF THE SCREEN WHILE PLAYING OR START OR GUIDE ON A CONTROLLER. EVERYTHING ABOVE IS THERE TOO." });
    l.push_back({ nullptr, "FAST FORWARD", {}, "", "THE FAST FORWARD BUTTON USES YOUR HOLD / TOGGLE SETTING. THE PAUSE MENU CAN ALSO KEEP FAST FORWARD ON. THE BUTTON IS CHANGED IN KEYS AND BUTTONS." });
#else
    l.push_back({ nullptr, "THE PAUSE MENU", {}, "", "ESC ON THE KEYBOARD. ON A CONTROLLER: START (MENU ON XBOX / OPTIONS ON PS) OR ITS GUIDE BUTTON. EVERYTHING ABOVE IS THERE TOO." });
    l.push_back({ nullptr, "FAST FORWARD", {}, "", "THE FAST FORWARD BUTTON USES YOUR HOLD / TOGGLE SETTING. THE PAUSE MENU CAN ALSO KEEP FAST FORWARD ON. THE KEY AND BUTTON ARE CHANGED IN KEYS AND BUTTONS." });
#endif
#ifdef __SWITCH__
    l.push_back({ "!keys", "KEYS AND BUTTONS", { { "", "CHANGE" } }, "",
                  "GIVE ANY DS BUTTON / THE PAUSE MENU / FAST FORWARD / SWAP SCREENS / SAVE STATE / LOAD STATE ANOTHER BUTTON (ANY CONTROLLER: PRO / JOY-CON OR ONE ADDED WITH SYS-CON OR MISSIONCONTROL). GIVING ONE A BUTTON ANOTHER ONE HAD SWAPS THE TWO. SAVE STATE AND LOAD STATE START NOT SET. ALSO IN THE PAUSE MENU (CONTROLS). THIS LIST FOLLOWS THE ONES YOU CHANGE." });
#else
    l.push_back({ "!keys", "KEYS AND BUTTONS", { { "", "CHANGE" } }, "",
                  "GIVE ANY DS BUTTON / THE PAUSE MENU / FAST FORWARD / SWAP SCREENS / SAVE STATE / LOAD STATE ANOTHER KEY OR CONTROLLER BUTTON. GIVING ONE A KEY OR BUTTON ANOTHER ONE HAD SWAPS THE TWO. SAVE STATE STARTS AS F5 AND LOAD STATE AS F8 (NOT SET ON A CONTROLLER). ALSO IN THE PAUSE MENU (CONTROLS). THIS LIST FOLLOWS THE ONES YOU CHANGE (A PLAYS / B GOES BACK / Y OPENS THESE SETTINGS)." });
#endif
#ifdef __SWITCH__
    l.push_back({ nullptr, "SAVE STATE / LOAD STATE", {}, "", "ONE QUICK SLOT PER GAME, FROM THE PAUSE MENU. CHOOSE A BUTTON FOR EACH IN KEYS AND BUTTONS (THEY START NOT SET). YOUR SAVE IN THE GAME IS SEPARATE AND STILL WORKS AS NORMAL." });
#else
    l.push_back({ nullptr, "SAVE STATE / LOAD STATE", {}, "", "ONE QUICK SLOT PER GAME. F5 SAVES AND F8 LOADS UNTIL YOU CHANGE THEM IN KEYS AND BUTTONS. A CONTROLLER BUTTON IS NOT SET UNTIL YOU CHOOSE ONE. YOUR SAVE IN THE GAME IS SEPARATE AND STILL WORKS AS NORMAL." });
#endif
    l.push_back({ nullptr, "RESET GAME / QUIT", {}, "", "IN THE PAUSE MENU: START THE GAME AGAIN FROM ITS TITLE SCREEN / BACK TO THIS GAME LIST." });
    return l;
}

// Words to lines of at most `chars` characters.
inline std::vector<std::string> Wrap(const std::string& text, int chars)
{
    return UiLang::Wrap(text, chars * 6);
}

struct SettingsGroup
{
    const char* key;  // its row in the list of groups (a "!" key: chosen, not stored)
    const char* name;
    const char* text;
};
inline const std::vector<SettingsGroup>& SettingsGroups()
{
    static const std::vector<SettingsGroup> g = {
        // (What each holds is listed under it: its options' names, ShowGroups.)
        { "!group0", "LANGUAGE", "" },
        { "!group1", "DISPLAY", "" },
        { "!group2", "SOUND", "" },
        { "!group3", "GRAPHICS", "" },
        { "!group4", "CONTROLS", "" },
        { "!group5", "SHORTCUTS", "" },
        { "!group6", "GAME LIST", "" },
        { "!group7", "RETROACHIEVEMENTS", "" },
        { "!group8", "UPDATES", "" },
        { "!group9", "OPTIONAL FEATURES", "" },
    };
    return g;
}
// Which group an option is in (SettingsGroups).
inline int GroupOf(const Setting& s)
{
    if (s.key && (!strcmp(s.key, "!updates") || !strcmp(s.key, "!releases"))) return 8;
    if (s.key && !strcmp(s.key,"!favourite")) return 6;
    if (s.key && !strcmp(s.key, "!saves_folder")) return 6; // (GAME LIST)
    if (!s.key) return !strcmp(s.name, "KEYS AND BUTTONS") ? 3 : !strcmp(s.name, "RELEASE NOTES") ? 8 : 7;
    if (!strncmp(s.key,"extra_",6)) return 9;
    if (!strcmp(s.key, "list_lang")) return 0;
    if (!strncmp(s.key, "list_", 5)) return 6;
    if (!strcmp(s.key, "ra") || !strcmp(s.key, "!ra_login")) return 7;
    static const char* display[] = { "view", "large_screen", "fullscreen", "smooth", "screen_filter", "gba_lcd", "battle_buttons", "show_fps" };
    static const char* controls[] = { "start_button", "buttons", "pad_touch", "fast_button" };
    for (const char* k : display)
        if (!strcmp(s.key, k)) return 1;
    if (!strcmp(s.key, "volume")) return 2;
    for (const char* k : controls)
        if (!strcmp(s.key, k)) return 4;
    if (!strcmp(s.key, "!keys")) return 4;
    if (s.key[0] == '!') return 5; // (DESKTOP SHORTCUTS, HOME MENU ICONS)
    return 3;
}

struct SettingsPage
{
    std::string path;
    std::vector<Setting> all = SettingsList();
    std::vector<Setting> items; // what is shown: the groups, or one group's options
    int group = -1;             // the group shown, -1: the list of groups
    std::string groupText[10];   // (the groups' lines: their options' names)
    std::vector<std::string> lines; // the file, as it is
    int selected = 0, scroll = 0;
    // Draw keeps the chosen option in view while follow is set (the keys and the D-pad); a
    // finger scrolling the page clears it. Draw also notes, for taps (ItemAt), where each
    // option's name line is and how tall a line is.
    bool follow = true;
    int lineH = 20;
    std::vector<std::pair<int, int>> rows; // (option, the top of its name line)

    // The option whose part of the page (its name and the lines below it) is at y, or -1.
    int ItemAt(int y) const
    {
        int found = -1;
        for (const auto& r : rows)
            if (r.second - lineH / 4 <= y) found = r.first;
        if (found >= 0 && !rows.empty() && y > rows.back().second + 6 * lineH) return -1;
        return found;
    }

    void Open(const std::string& cfgPath, bool keepChoice = false)
    {
        path = cfgPath;
        lines.clear();
        if (FILE* f = fopen(path.c_str(), "r"))
        {
            char buf[1024];
            while (fgets(buf, sizeof buf, f))
            {
                std::string l = buf;
                while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
                lines.push_back(l);
            }
            fclose(f);
        }
        if (keepChoice) return;
        ShowGroups(0);
    }
    // The list of groups, with group `sel` chosen.
    void ShowGroups(int sel)
    {
        group = -1;
        items.clear();
        int row = 0;
        for (int g = 0; g < (int)SettingsGroups().size(); g++)
        {
            bool any = false;
            for (const Setting& s : all) any |= GroupOf(s) == g;
            if (!any) continue; // (a platform without any of it: no SHORTCUTS on a Mac)
            if (g == sel) row = (int)items.size();
            const SettingsGroup& sg = SettingsGroups()[g];
            std::string names;
            for (const Setting& s : all)
                if (GroupOf(s) == g) names += (names.empty() ? "" : " / ") + UiLang::Tr(s.name);
            groupText[g] = names + ".";
            items.push_back({ sg.key, sg.name, { { "", ">" } }, "", groupText[g].c_str() });
        }
        selected = row;
        scroll = 0;
        follow = true;
    }
    // Group g's options.
    void Enter(int g)
    {
        group = g;
        items.clear();
        for (const Setting& s : all)
            if (GroupOf(s) == g) items.push_back(s);
        selected = 0;
        scroll = 0;
        follow = true;
    }
    // B: from a group back to the list of groups; false when already there (the page closes).
    bool Back()
    {
        if (group < 0) return false;
        ShowGroups(group);
        return true;
    }
    std::string Stored(const std::string& key) const
    {
        for (const std::string& l : lines)
            if (l.size() > key.size() && !l.compare(0, key.size(), key) && l[key.size()] == '=') return l.substr(key.size() + 1);
        return "";
    }
    // The index of the shown value (-1: AUTO).
    int ValueIndex(const Setting& s) const
    {
        if (s.key[0] == '!') return 0; // (something to do: its one value is what it says)
        std::string v = Stored(s.key);
        if (v.empty()) v = s.def;
        if (v.empty()) return -1;
        for (size_t i = 0; i < s.values.size(); i++)
            if (s.values[i].first == v) return (int)i;
        // Values the file may hold in other spellings: numbers between the choices (the
        // nearest below), ON/OFF as anything but "0" or "1".
        if (!strcmp(s.key, "wide_lines"))
        {
            int best = 0;
            for (size_t i = 0; i < s.values.size(); i++)
                if (atoi(s.values[i].first.c_str()) <= atoi(v.c_str())) best = (int)i;
            return best;
        }
        for (size_t i = 0; i < s.values.size(); i++)
            if (s.values[i].first == s.def) return (int)i;
        return 0;
    }
    std::string Shown(const Setting& s) const
    {
        const int i = ValueIndex(s);
        if (i >= (int)s.values.size()) return std::string(); // (TOGGLE FAVOURITE has no values: it is something to do, and read past its empty list this crashed the Game List page on Android)
        return i < 0 ? "AUTO" : s.values[i].second;
    }
    void Move(int d)
    {
        follow = true;
        if (selected + d >= 0 && selected + d < (int)items.size()) selected += d;
    }
    // The chosen option's next (d 1) or previous (d -1) value, written to the file. Returns its
    // key, or nullptr.
    const char* Change(int d)
    {
        if (selected < 0 || selected >= (int)items.size() || !items[selected].key) return nullptr;
        const Setting& s = items[selected];
        if (!strncmp(s.key, "!group", 6))
        {
            if (d > 0) Enter(atoi(s.key + 6));
            return nullptr;
        }
        if (s.key[0] == '!') return s.key; // (done by the caller)
        const int n = (int)s.values.size();
        const int i = ValueIndex(s);
        const int next = i < 0 ? (d > 0 ? 0 : n - 1) : (i + d + n) % n;
        const std::string line = std::string(s.key) + "=" + s.values[next].first;
        bool found = false;
        for (std::string& l : lines)
            if (l.size() > strlen(s.key) && !l.compare(0, strlen(s.key), s.key) && l[strlen(s.key)] == '=')
            {
                l = line;
                found = true;
            }
        if (!found) lines.push_back(line);
        if (!strcmp(s.key, "wide_lines"))
        {
            // Match the in-game menu: choosing a manual resolution disables automatic
            // lowering, so starting the game honours that choice.
            bool haveBusy = false;
            for (std::string& l : lines)
                if (l.rfind("busy_detail=", 0) == 0) { l = "busy_detail=0"; haveBusy = true; }
            if (!haveBusy) lines.push_back("busy_detail=0");
        }
        const std::string tmp = path + ".tmp";
        if (FILE* f = fopen(tmp.c_str(), "w"))
        {
            bool ok = true;
            for (const std::string& l : lines) if (fprintf(f, "%s\n", l.c_str()) < 0) ok = false;
            ok = fclose(f) == 0 && ok;
            std::error_code ec;
            if (ok) std::filesystem::rename(tmp, path, ec);
            if (ok && ec)
            {
                // (The Switch's SD card does not rename over a file that is there: the old one goes first.)
                std::error_code gone;
                std::filesystem::remove(path, gone);
                ec.clear();
                std::filesystem::rename(tmp, path, ec);
            }
            if (!ok || ec) fprintf(stderr, "[launcher] cannot save settings: %s\n", path.c_str());
        }
        return s.key;
    }

    // Over the whole canvas: the options with their values (the chosen one marked), each with
    // what it does below it, kept in view as the choice moves.
    void Draw(Canvas& c, const std::string& hint, const Theme& th = Theme())
    {
        const int s = std::max(1, std::min(c.w, c.h) / 240);
#ifdef __ANDROID__
        // Phone settings use readable touch targets in either orientation.
        const int ts = std::max(2, s);
#else
        const int ts = std::max(1, (s * 2 + 1) / 3); // the text's scale: 2 at 720 lines, 3 at 1080
#endif
        c.Fill(0, 0, c.w, c.h, th.bg);
        c.Fill(0, 0, c.w, (c.h > c.w ? 43 : 22) * s, th.band);
        c.Fill(0, (c.h > c.w ? 43 : 22) * s, c.w, th.bandLineH * s, th.bandLine);
        const std::string title = group < 0 ? "SETTINGS" : std::string("SETTINGS: ") + UiLang::Tr(SettingsGroups()[group].name);
        int titleScale=s+1;
        while(titleScale>1 && Canvas::TextWidth(title,titleScale)>c.w-8)--titleScale;
        c.Text((c.w-Canvas::TextWidth(title,titleScale))/2,6*s,title,titleScale,th.title);
        const int width = std::min(c.w - 24 * s, 100 * 6 * ts);
        const int x = (c.w - width) / 2;
        struct Line { std::string text, value; int item; bool name; };
        std::vector<Line> out;
        int selFirst = 0, selLast = 0;
        bool infoHeading = false;
        for (int i = 0; i < (int)items.size(); i++)
        {
            const Setting& e = items[i];
            if (!e.key && !infoHeading)
            {
                out.push_back({ "", "", -1, false });
                infoHeading = true;
            }
            if (i == selected) selFirst = (int)out.size();
            const std::string value = e.key ? Shown(e) : "";
            const bool arrows = !value.empty() && i == selected && e.key && strncmp(e.key, "!group", 6);
            const std::string drawnValue = arrows ? "< " + UiLang::Tr(value) + " >" : UiLang::Tr(value);
            const bool stacked = Canvas::TextWidth(e.name, ts) + Canvas::TextWidth(drawnValue, ts) + 8 * ts > width;
            out.push_back({ e.name, stacked ? "" : value, i, true });
            if (stacked && !value.empty())
                for (const std::string& l : Wrap(UiLang::Tr(value), width / (6 * ts) - 2)) out.push_back({ l, "", i, false });
            for (const std::string& l : Wrap(UiLang::Tr(e.text), width / (6 * ts) - 2)) out.push_back({ l, "", i, false });
            if (i == selected) selLast = (int)out.size() - 1;
            out.push_back({ "", "", -1, false });
        }
        const int top = (c.h > c.w ? 48 : 30) * s, bottom = c.h - 18 * s;
        lineH = (UiLang::Tall() ? 16 : 10) * ts;
        const int fit = std::max(1, (bottom - top) / lineH);
        if (follow)
        {
            if (selFirst < scroll) scroll = selFirst;
            if (selLast >= scroll + fit) scroll = selLast - fit + 1;
        }
        scroll = std::max(0, std::min(scroll, (int)out.size() - fit));
        rows.clear();
        int y = top;
        for (int i = scroll; i < (int)out.size() && y + lineH <= bottom; i++, y += lineH)
        {
            const Line& l = out[i];
            if (l.name) rows.push_back({ l.item, y });
            const bool sel = l.item == selected && l.item >= 0;
            if (l.name)
            {
                if (sel) c.Fill(x - 4 * ts, y - 2 * ts, width + 8 * ts, lineH, th.tile);
                const bool info = !items[l.item].key;
                c.Text(x, y, l.text, ts, info ? th.hint : sel ? th.nameSel : th.subSel);
                if (!l.value.empty())
                {
                    const bool group = !strncmp(items[l.item].key, "!group", 6);
                    const std::string v = sel && !group ? "< " + UiLang::Tr(l.value) + " >" : l.value;
                    c.Text(x + width - Canvas::TextWidth(v, ts), y, v, ts, sel ? th.nameSel : th.name);
                }
            }
            else if (!l.text.empty())
                c.TextRaw(x + 2 * 6 * ts, y, l.text, ts, sel ? th.name : th.sub);
        }
        if (scroll > 0) c.Fill(c.w / 2 - 8 * s, top - 4 * s, 16 * s, s, th.hint);
        if (scroll + fit < (int)out.size()) c.Fill(c.w / 2 - 8 * s, bottom + s, 16 * s, s, th.hint);
        int hintScale=s;
        while(hintScale>1 && Canvas::TextWidth(hint,hintScale)>c.w-8)--hintScale;
    c.Text((c.w - Canvas::TextWidth(hint, hintScale)) / 2, c.h - 12 * s, hint, hintScale, th.pokemon ? 0xFFDBE9FAu : th.hint);
    }
};

// The settings page as the pause menu's glass settings pages draw it (Menu::style 1, ui_settings.h):
// a back row, then the groups (each opens its options) or one group's options (On / Off, a choice
// of values, something to do, or something to read), each with what it does beside the rows.
// map: each row's place in p.items (-1: the back row). onBack: the back row chosen.
// A settings description (written in capitals) in sentence case for the glass page: lower case but for
// each sentence's first letter and the names kept as they are written (ROM, SD, HOME, GitHub...). A
// lone A stays a capital only as the button (A:, A PLAYS, A / B, THE A BUTTON); else it is "a".
inline std::string SettingSentence(const std::string& s)
{
    for (char ch : s) if (ch >= 'a' && ch <= 'z') return s;
    static const char* const keep[] = { "FPS", "HUD", "GBA", "DS", "3D", "2D", "PC", "OK", "B", "X", "Y", "L", "R", "ZL", "ZR", "MB", "GPU",
                                        "CPU", "JIT", "LCD", "ROM", "ROMS", "SD", "HOME", "USB", "AUTO", "PS", "NDS", "UI", "4X", "2X", "16:9" };
    static const std::pair<const char*, const char*> spelt[] = { { "POKEMON", "Pok\xC3\xA9mon" }, { "POKEDEX", "Pok\xC3\xA9" "dex" },
        { "GITHUB", "GitHub" }, { "RETROACHIEVEMENTS", "RetroAchievements" }, { "XBOX", "Xbox" }, { "JOY", "Joy" }, { "CON", "Con" },
        { "MISSIONCONTROL", "MissionControl" }, { "SYS", "sys" }, { "FULLDEX", "Fulldex" }, { "WINDOWS", "Windows" }, { "MAC", "Mac" },
        { "LINUX", "Linux" }, { "ANDROID", "Android" }, { "IPHONE", "iPhone" }, { "SWITCH", "Switch" }, { "STEAM", "Steam" } };
    std::string out;
    bool start = true;
    size_t i = 0;
    while (i < s.size())
    {
        size_t j = i;
        while (j < s.size() && ((s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= '0' && s[j] <= '9') || (unsigned char)s[j] >= 0x80)) j++;
        if (j == i) { out += s[i]; if (s[i] == '.' || s[i] == '!' || s[i] == '?') start = true; i++; continue; }
        std::string w = s.substr(i, j - i);
        bool kept = false;
        for (const char* k : keep) if (w == k) kept = true;
        for (const auto& sp : spelt) if (w == sp.first) { w = sp.second; kept = true; }
        if (w == "A")
        {
            // (The button: before ":" or "/", before PLAYS / BUTTON, after "/" or THE.)
            size_t k = j;
            while (k < s.size() && s[k] == ' ') k++;
            const std::string next = s.substr(k, 7);
            const std::string before = out.size() >= 4 ? out.substr(out.size() - 4) : out;
            kept = (k < s.size() && (s[k] == ':' || s[k] == '/')) || next.compare(0, 5, "PLAYS") == 0 || next.compare(0, 6, "BUTTON") == 0 ||
                   (!out.empty() && out.find_last_not_of(' ') != std::string::npos && out[out.find_last_not_of(' ')] == '/') || before == "the ";
        }
        if (!kept)
            for (size_t k = start ? 1 : 0; k < w.size(); k++)
                if (w[k] >= 'A' && w[k] <= 'Z') w[k] += 32;
        out += w;
        start = false;
        i = j;
    }
    return out;
}
// The icon a settings row has on the glass page: its group's for a group, else its own (or its group's).
inline int SettingIcon(const Setting& s, int group)
{
    static const int groupIcon[] = { Glass::I_GLOBE, Glass::I_MONITOR, Glass::I_SPEAKER, Glass::I_LAYERS, Glass::I_PAD, Glass::I_WINDOW,
                                     Glass::I_GRID, Glass::I_TROPHY, Glass::I_INFO, Glass::I_SPARKLE };
    if (s.key && !strncmp(s.key, "!group", 6)) { const int g = atoi(s.key + 6); return g >= 0 && g < 10 ? groupIcon[g] : Glass::I_GEAR; }
    if (!s.key) return Glass::I_INFO;
    static const std::pair<const char*, int> own[] = {
        { "view", Glass::I_LAYOUT }, { "large_screen", Glass::I_EXPAND }, { "fullscreen", Glass::I_FULLSCREEN }, { "smooth", Glass::I_SCREENS },
        { "screen_filter", Glass::I_SPARKLE }, { "gba_lcd", Glass::I_SPARKLE }, { "battle_buttons", Glass::I_PAD }, { "show_fps", Glass::I_GAUGE }, { "fps60", Glass::I_FAST }, { "wide", Glass::I_RATIO },
        { "wide_map", Glass::I_GLOBE }, { "wide_lines", Glass::I_LAYERS }, { "busy_detail", Glass::I_GAUGE }, { "ff_low_res", Glass::I_FAST },
        { "wide_gpu", Glass::I_CHIP }, { "volume", Glass::I_SPEAKER }, { "start_button", Glass::I_PAD }, { "buttons", Glass::I_PAD },
        { "pad_touch", Glass::I_PAD }, { "!keys", Glass::I_PAD }, { "!shortcuts", Glass::I_WINDOW }, { "!forwarders", Glass::I_WINDOW },
        { "list_lang", Glass::I_GLOBE }, { "ra", Glass::I_TROPHY }, { "!ra_login", Glass::I_PEOPLE }, { "!updates", Glass::I_LOAD }, { "!releases", Glass::I_GLOBE } };
    for (const auto& o : own)
        if (!strcmp(s.key, o.first)) return o.second;
    if (!strncmp(s.key, "list_", 5)) return Glass::I_GRID;
    if (!strncmp(s.key, "extra_", 6)) return Glass::I_SPARKLE;
    return group >= 0 && group < 9 ? groupIcon[group] : Glass::I_GEAR;
}
// The settings as a page of the launcher: the page drawn (sidebar, title, buttons at the bottom), then
// the menu's rows and preview in it. The launcher's focus is as it was afterwards.
inline void DrawSettingsPage(Canvas& c, Menu& m, const std::vector<Game>& games, int tab, int scroll, const std::string& title,
                             const std::string& hint, const Theme& theme)
{
    const GlassFocus kept = Focus();
    GlassFocus& F = Focus();
    // ("GAME LIST" as "Game List", the names written as they are: RetroAchievements, FPS, ROMs.)
    std::string shown;
    bool start = true;
    for (char ch : title)
    {
        const unsigned char b = (unsigned char)ch;
        shown += b < 128 ? (char)(start ? toupper(b) : tolower(b)) : ch;
        start = b == ' ' || b == '/' || b == '-';
    }
    for (const auto& r : std::vector<std::pair<std::string, std::string>>{ { "Retroachievements", "RetroAchievements" }, { "Fps", "FPS" }, { "Roms", "ROMs" }, { "Rom ", "ROM " }, { "Gba", "GBA" }, { "Ds ", "DS " } })
        for (size_t at = shown.find(r.first); at != std::string::npos; at = shown.find(r.first, at + r.second.size())) shown.replace(at, r.first.size(), r.second);
    F.page = SIDE_SETTINGS; F.side = -1; F.popup = -1; F.dialog = 0; F.onSort = false; F.pageTitle = shown;
    F.sW = 0;
    Draw(c, games, tab, -1, scroll, std::string(), hint, theme, true, false, false);
    // (Where the page put them, kept: when nothing on the page changed it is copied, not drawn, and says nothing.)
    static int lane[8] = {};
    if (F.sW > 0) { lane[0] = F.sX; lane[1] = F.sY; lane[2] = F.sW; lane[3] = F.sH; lane[4] = F.sListW; lane[5] = F.sPvX; lane[6] = F.sListH; lane[7] = F.sPvY; }
    m.laneX = lane[0]; m.laneY = lane[1]; m.laneW = lane[2]; m.laneH = lane[3]; m.laneListW = lane[4]; m.lanePvX = lane[5]; m.laneListH = lane[6]; m.lanePvY = lane[7];
    Focus() = kept;
    m.Draw(c, title, std::string());
    m.laneX = -1;
}
// The settings previews draw their pictures from a game's two screens (Canvas::dsTop, dsBottom); the launcher has no
// game running, so: the top is the game's own picture (the one played last, else the first with art), and the bottom
// a plain stand-in drawn here (never bundled art). With no picture at all, both are stand-ins.
inline void PreviewFrames(Canvas& c, const std::vector<Game>& games)
{
    int gi = -1;
    for (size_t i = 0; i < games.size() && gi < 0; i++)
        if (!games[i].art.empty() && !ContinueRom().empty() && games[i].rom == ContinueRom()) gi = (int)i;
    for (size_t i = 0; i < games.size() && gi < 0; i++)
        if (!games[i].art.empty()) gi = (int)i;
    static std::string key;
    const std::string k = gi >= 0 ? games[gi].rom : std::string("-");
    if (c.dsTop.size() == 256 * 192 && c.dsBottom.size() == 256 * 192 && key == k) return;
    key = k;
    c.dsTop.assign(256 * 192, 0xFF000000u);
    c.dsBottom.assign(256 * 192, 0xFF000000u);
    auto mix = [](uint32_t a, uint32_t b, float t) {
        uint32_t o = 0xFF000000u;
        for (int s = 0; s <= 16; s += 8) o |= (uint32_t)((float)(a >> s & 255) * (1 - t) + (float)(b >> s & 255) * t) << s;
        return o;
    };
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++)
        {
            // (The top: sky, hills, a path.)
            uint32_t p = mix(0xFF4F9AD8u, 0xFFBFE4F4u, y / 110.0f);
            if (y >= 110)
            {
                p = mix(0xFF4E9B48u, 0xFF2F6B35u, (y - 110) / 82.0f);
                if (std::abs(x - 128 - (y - 110) / 3) < 10 + (y - 110) / 5) p = 0xFFC9A66Bu;
                if ((x / 24 + y / 16) % 7 == 0 && y > 150) p = mix(p, 0xFF000000u, 0.12f);
            }
            if ((x - 200) * (x - 200) + (y - 40) * (y - 40) < 18 * 18) p = 0xFFFFF2B8u;
            if (y >= 92 && y < 110 && (x * 7 + 13) % 64 < 30) p = 0xFF2F7A3Au;
            c.dsTop[(size_t)y * 256 + x] = p;
            // (The bottom: a dark screen of rounded tiles and a bar.)
            uint32_t q = mix(0xFF1B2A46u, 0xFF2C4A78u, y / 191.0f);
            const int tx = (x - 16) % 76, ty = (y - 20) % 66;
            if (x >= 16 && x < 240 && y >= 20 && y < 152 && tx < 68 && ty < 54) q = mix(q, 0xFFFFFFFFu, 0.22f);
            if (y >= 168 && y < 184 && x >= 16 && x < 240) q = mix(q, 0xFFFFFFFFu, 0.30f);
            c.dsBottom[(size_t)y * 256 + x] = q;
        }
    if (gi >= 0)
    {
        const Game& g = games[gi];
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 256; x++)
            {
                const int sx = std::min(Art - 1, x * Art / 256), sy = std::min(Art - 1, (Art - Art * 3 / 4) / 2 + y * (Art * 3 / 4) / 192);
                const uint32_t px = g.art[(size_t)sy * Art + sx];
                c.dsTop[(size_t)y * 256 + x] = mix(0xFF000000u, px | 0xFF000000u, (float)(px >> 24) / 255.0f);
            }
    }
}

inline void BuildSettingsMenu(Menu& m, std::vector<int>& map, const SettingsPage& p, bool onBack)
{
    m.items.clear();
    map.clear();
    m.open = true; m.style = 1; m.keyGrid = false; m.columns = 0; m.pageIcon = Glass::I_GEAR;
    m.items.push_back({ "BACK", "" });
    m.items.back().kind = Menu::K_BACK;
    map.push_back(-1);
    for (size_t i = 0; i < p.items.size(); i++)
    {
        const Setting& s = p.items[i];
        MenuItem it;
        it.label = UiLang::Tr(s.name);
        it.desc = SettingSentence(UiLang::Tr(s.text ? s.text : ""));
        it.icon = SettingIcon(s, p.group);
        if (p.group < 0) it.kind = Menu::K_LINK;
        else if (!s.key) it.kind = Menu::K_PLAIN;
        else if (s.key[0] == '!') { it.kind = Menu::K_LINK; it.value = p.Shown(s); }
        else
        {
            const int vi = p.ValueIndex(s);
            bool onOff = s.values.size() == 2;
            for (const auto& v : s.values) onOff = onOff && (v.second == "ON" || v.second == "OFF");
            if (onOff)
            {
                it.kind = Menu::K_TOGGLE;
                it.choices = { "OFF", "ON" };
                it.choice = vi >= 0 && s.values[vi].second == "ON" ? 1 : 0;
            }
            else
            {
                it.kind = Menu::K_PICKER;
                it.value = p.Shown(s);
                for (const auto& v : s.values) it.choices.push_back(v.second);
                it.choice = vi;
            }
        }
        if (s.key && s.key[0] != '!' && p.group >= 0)
        {
            const std::string kk = s.key; // (the picture drawn for each choice: the choice's own number)
            it.preview = kk == "view" ? Menu::P_LAYOUT : kk == "large_screen" ? Menu::P_LARGE : kk == "aspect" ? Menu::P_ASPECT : kk == "fullscreen" ? Menu::P_FULLSCREEN
                       : kk == "screen_filter" ? Menu::P_FILTER : kk == "smooth" ? Menu::P_SMOOTH : kk == "battle_buttons" ? Menu::P_BATTLE : kk == "show_fps" ? Menu::P_FPS : kk == "wide" ? Menu::P_WIDE
                       : kk == "fps60" ? Menu::P_MOTION : -1;
        }
        m.items.push_back(it);
        map.push_back((int)i);
    }
    m.selected = onBack ? 0 : std::max(0, std::min((int)m.items.size() - 1, p.selected + 1));
}

}

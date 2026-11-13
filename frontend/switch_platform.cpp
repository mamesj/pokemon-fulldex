// Nintendo Switch (homebrew, libnx) parts of the frontend. See switch_platform.h.
#ifdef __SWITCH__
#include "switch_platform.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <switch.h>

#include "testrun.h"
// (stb_image's code here, for the game list's pictures: only JPEG and PNG.)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "thirdparty/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "launcher.h"
#include "n3ds.h" // (Omega Ruby and Alpha Sapphire: listed and started)
#include "cloud_autosync.h"
#include "ui_lang_sdl.h"
#include "controls.h"
#ifdef SSR_GBA
#include <map>
#include "gba_games.h" // Pokemon GBA Widescreen, built in: its GBA games (CMakeLists.txt, SSR_GBA_DIR)
#include "gba_ra.h"    // RetroAchievements for those games, the same account as the DS games
#endif
#ifdef SSR_ALL_GAMES
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "thirdparty/stb_image_write.h"
#include "switch_forwarder.h"
#ifdef SSR_UPDATER
#include "updater.h"
#endif
#endif
#ifdef SSR_PC_ART
// The game list's pictures built into the program (pc_art.cpp, CMakeLists.txt: SSR_PC_ART).
namespace PcLauncher { const unsigned char* BuiltInArt(const char* code, size_t& size); }
#endif

// How many chunks the program's SoulSilver tables have: none in a build for the Black games only.
extern const uint32_t rc_num_chunks;
bool rc_portable_knows(const char code[4]); // (runtime/recomp_api.h)

namespace SwitchPlatform
{
void ExitTrace(const char* step);
void ExitTraceIn(const std::string& dir);
namespace
{
std::vector<std::string> g_args;
static std::string SavePath(const Launcher::Game& g); // (below: a DS game's save in its folder's saves folder)
std::vector<char*> g_argv;
// A test run from test.txt (testrun.h), and the .nro to start again for the next one.
TestRun::State g_test;
std::string g_self;

// Shows a message on the console's screen until + is pressed. Used only before SDL starts.
void ShowMessage(const std::string& text)
{
    fprintf(stderr, "%s\n", text.c_str());
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    printf("\n  Pokemon DS widescreen\n\n%s\n\n  Press + to return to the Homebrew Menu.\n", text.c_str());
    while (appletMainLoop())
    {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
}

// The program's romfs (the Black games' function tables), mounted once.
bool RomfsReady()
{
    static const bool ready = R_SUCCEEDED(romfsInit());
    return ready;
}

// Whether this build plays a game: SoulSilver and HeartGold need the program's own tables, the
// Black games and Platinum a function table (the player's own funcs.txt beside the game, or the
// build's).
bool Playable(const Launcher::Game& g)
{
    if (g.platform == Launcher::GBA) return true; // (any GBA game plays, with widescreen or without)
    if (g.platform == Launcher::N3DS) return g.known && !g.comingSoon; // (its own program: n3ds.h)
    if (!g.known || !g.problem.empty()) return false;
    if (IsGame(g.code, "IPGE") || IsGame(g.code, "IPKE")) return rc_num_chunks > 0 && rc_portable_knows(g.code.c_str());
    struct stat st;
    if (stat((g.folder + "/funcs.txt").c_str(), &st) == 0) return true;
    return RomfsReady() && stat(("romfs:/funcs/" + g.code + ".txt").c_str(), &st) == 0;
}

// The settings: settings.cfg in the program's folder (ssrecomp.cfg before; an old one is renamed).
std::string SettingsPath(const std::string& dir)
{
    const std::string cfgPath = dir + "/settings.cfg", oldCfg = dir + "/ssrecomp.cfg";
    struct stat cst;
    if (stat(cfgPath.c_str(), &cst) != 0 && stat(oldCfg.c_str(), &cst) == 0) rename(oldCfg.c_str(), cfgPath.c_str());
    return cfgPath;
}

#ifdef SSR_ALL_GAMES
// The game's picture as the HOME Menu's icon: a 256x256 JPEG (over the list's dark grey where
// the picture is see-through, as a DS icon is). Empty without a picture.
std::vector<uint8_t> IconJpeg(const Launcher::Game& g)
{
    const std::vector<uint32_t> px = Launcher::GamePicture(g, 256);
    if (px.empty()) return {};
    std::vector<uint8_t> rgb((size_t)256 * 256 * 3);
    for (size_t i = 0; i < px.size(); i++)
    {
        const uint32_t p = px[i], a = p >> 24;
        for (int c = 0; c < 3; c++)
        {
            const uint32_t v = (p >> (16 - 8 * c)) & 255;
            rgb[i * 3 + c] = (uint8_t)((v * a + 0x1c * (255 - a)) / 255);
        }
    }
    std::vector<uint8_t> jpeg;
    stbi_write_jpg_to_func([](void* ctx, void* data, int size) {
        auto* out = (std::vector<uint8_t>*)ctx;
        out->insert(out->end(), (uint8_t*)data, (uint8_t*)data + size);
    }, &jpeg, 256, 256, 3, rgb.data(), 92);
    return jpeg;
}

// HOME MENU ICONS (the settings page): one HOME Menu entry per game in the list, each starting
// this program with "--game CODE" (switch_forwarder.cpp). What came of it, for the page.
std::string InstallHomeIcons(const std::vector<Launcher::Game>& games)
{
    if (g_self.empty()) return "FAILED: NO PROGRAM PATH";
    std::vector<SwitchForwarder::Entry> entries;
    std::vector<std::string> done;
    for (const Launcher::Game& g : games)
    {
        if (!Playable(g) || g.hack || g.platform == Launcher::N3DS || std::find(done.begin(), done.end(), g.code) != done.end()) continue;
        done.push_back(g.code);
        SwitchForwarder::Entry e;
        const Launcher::KnownGame* k = Launcher::Known(g.code);
        e.name = k ? k->name : Launcher::TitleCase(g.name);
        e.args = "--game " + g.code;
        // The icon: the player's own picture in icons/ if there is one, else the box art built
        // into the program, else the list's picture (a one-game program's icon can be a placeholder).
        Launcher::Game pic = g;
        const std::string dir = g_self.substr(0, g_self.find_last_of('/'));
        struct stat is;
        const bool own = stat((dir + "/icons/" + g.code + ".jpg").c_str(), &is) == 0 || stat((dir + "/icons/" + g.code + ".png").c_str(), &is) == 0;
#ifdef SSR_PC_ART
        size_t size = 0;
        std::vector<uint32_t> art;
        if (!own)
            if (const unsigned char* data = PcLauncher::BuiltInArt(g.code.c_str(), size))
                if (Launcher::LoadArt(data, size, art)) pic.art = art;
#else
        (void)own;
#endif
        e.icon = IconJpeg(pic);
        if (e.icon.empty()) continue;
        // (For checking: the icons also written to a forwarder-debug folder beside the program, if there.)
        const std::string debugDir = g_self.substr(0, g_self.find_last_of('/')) + "/forwarder-debug";
        struct stat ds;
        if (stat(debugDir.c_str(), &ds) == 0)
            if (FILE* f = fopen((debugDir + "/" + g.code + ".jpg").c_str(), "wb"))
            {
                fwrite(e.icon.data(), 1, e.icon.size(), f);
                fclose(f);
            }
        entries.push_back(std::move(e));
    }
    if (entries.empty()) return "NO GAMES TO ADD";
    {
        const std::string debugDir = g_self.substr(0, g_self.find_last_of('/')) + "/forwarder-debug";
        struct stat ds;
        SwitchForwarder::SetDebugFolder(stat(debugDir.c_str(), &ds) == 0 ? debugDir : "");
    }
    // The forwarder's parts are in the romfs, mounted the first time something needs it: the list
    // mounts it only to look for a game's function table, which it did not need with only
    // SoulSilver and HeartGold, or games with their own funcs.txt (then: THIS BUILD HAS NO
    // FORWARDER).
    if (!RomfsReady()) fprintf(stderr, "[switch] the program's romfs could not be mounted\n");
    std::string error;
    const int made = SwitchForwarder::Install(g_self, entries, error);
    fprintf(stderr, "[switch] HOME Menu icons: %d of %d%s%s\n", made, (int)entries.size(), error.empty() ? "" : ", ", error.c_str());
    if (made == (int)entries.size()) return std::to_string(made) + " ON THE HOME MENU";
    if (made > 0) return std::to_string(made) + " OF " + std::to_string(entries.size()) + " (" + error + ")";
    return "FAILED: " + error;
}
#endif

// The game list (launcher.h) on the screen until a game is chosen: its index, or -1 for +.
// Shown with libnx's framebuffer, before the game's own window exists.
// autoFrames > 0: the list is shown that many frames and the selected game played (test runs:
// "arg --show-list" in test.txt, to time a game started after the list).
// cfgPath: the settings file, for the settings page (Y).
#include "library_history.h"
#include "save_backups.h"

// The Updates page: the updater's state, as the list draws it (working: CHECKING... until the work runs).
static void FillUpdateStatus(bool working)
{
#ifdef SSR_UPDATER
    const Updater::Status st = Updater::Get();
    Launcher::GlassFocus& F = Launcher::Focus();
    F.upState = working ? (st.state == Updater::State::Available ? 4 : 1) : (int)st.state; F.upCan = st.canInstall; F.upText = st.text; F.upNotes = st.notes;
    F.upHave = st.currentBuild ? Updater::BuildLabel(st.currentBuild) : std::string();
    F.upNew = st.availableBuild ? Updater::BuildLabel(st.availableBuild) : std::string();
#else
    (void)working;
#endif
}

int PickGame(const std::vector<Launcher::Game>& gamesIn, int selected, const std::string& cfgPath, int autoFrames = 0)
{
    bool pageUpdateWork = false; // (Check Now / Install Now on the Updates page: done at the next pass)
    Framebuffer fb;
    if (R_FAILED(framebufferCreate(&fb, nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 2))) return selected;
    framebufferMakeLinear(&fb);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    Canvas canvas;
    canvas.Resize(1280, 720);
    Launcher::PhoneUi() = true; // (the same bigger text and buttons as a phone, so the menus read in handheld)
    // The list as shown (the glass launcher's, as the PC's): the games with the player's names,
    // pictures and Home (favourites), those taken off the list left out; kept beside the settings.
    // The chosen game is handed back as its place in gamesIn.
    const std::string home = cfgPath.substr(0, cfgPath.find_last_of('/'));
    ExitTraceIn(home);
    std::vector<Launcher::Game> games;
    LibraryHistory library;
    auto refresh = [&](int& sel) {
        const std::string keep = sel >= 0 && sel < (int)games.size() ? games[sel].rom : std::string();
        games = gamesIn;
        Launcher::BuildRomList(games, home); // (the ROMs page: before those taken off the list are left out)
        if (PlayTime::AskNow(home)) Launcher::OpenBeer(); // (twenty hours played: the one Support message)
        library.Load(home + "/fulldex-library.txt");
        Launcher::ApplyLocal(games, home);
        Launcher::ApplyArt(games, home);
        Launcher::SortKey() = Launcher::ConfigValue(cfgPath, "list_sort");
        Launcher::LoadTabs(home);
        library.Apply(games, Launcher::ConfigValue(cfgPath, "list_sort"));
        sel = -1;
        for (size_t i = 0; i < games.size(); i++)
            if (games[i].rom == keep) sel = (int)i;
    };
    {
        const std::string keep = selected >= 0 && selected < (int)gamesIn.size() ? gamesIn[selected].rom : std::string();
        int s = -1;
        refresh(s);
        selected = -1;
        for (size_t i = 0; i < games.size(); i++)
            if (games[i].rom == keep) selected = (int)i;
        if (selected < 0 && !games.empty()) selected = 0;
    }
    // (Rename: the pause menu's keyboard page; kbLayer capitals, small letters or signs.)
    Menu kb;
    std::vector<int> kbIds;
    int kbLayer = 0;
    auto kbBuild = [&]() { Launcher::BuildKeyboard(kb, kbIds, Launcher::Focus().text, kbLayer, Launcher::KbPrompt(), Launcher::KbDone()); };
    // The touch screen (handheld): SETTINGS at the right of the title band, BACK at its left in
    // the settings, as on a PC or a phone (pc_launcher.cpp). A tap counts as the finger lifts.
    hidInitializeTouchScreen();
    bool touching = false;
    int touchX = 0, touchY = 0;
    // SETTINGS or the tabs chosen with the D-pad: up from the first row of games the tabs (left
    // and right change the tab), up again SETTINGS.
    bool onSettings = false, onTabs = false;
    // The tab shown (ALL first, the list opens on it), how far down it is scrolled; selected: a
    // game in that tab, or -1 when it has none (Launcher::MakeView, Step).
    int tab = 0, scroll = 0, held = 0, result = -1;
    auto view = [&]() { return Launcher::MakeView(canvas, games, tab); };
    bool guide = false; // the settings page (Launcher::SettingsPage)
    Launcher::SettingsPage page;
    Menu setMenu;             // (the glass launcher: the page as the pause menu's settings pages)
    std::vector<int> setMap;
    // The buttons changed in KEYS AND BUTTONS (controls.h): the list follows them (A plays, Y the
    // settings, B leaves), and the settings page's CONTROLS changes them here too. A controller's
    // buttons by their labels, as main.cpp reads them on the Switch (Controls::IN_*).
    Controls::Bindings ctl;
    ctl.Load(cfgPath);
    auto mask = [](int in) -> u64 {
        static const u64 m[Controls::IN_COUNT] = { HidNpadButton_A, HidNpadButton_B, HidNpadButton_X, HidNpadButton_Y,
            HidNpadButton_Minus, 0, HidNpadButton_Plus, HidNpadButton_StickL, HidNpadButton_StickR, HidNpadButton_L,
            HidNpadButton_R, HidNpadButton_Up, HidNpadButton_Down, HidNpadButton_Left, HidNpadButton_Right,
            HidNpadButton_ZL, HidNpadButton_ZR };
        return in >= 0 && in < Controls::IN_COUNT ? m[in] : 0;
    };
    auto action = [&](int a) { return mask(ctl.Pad(a, true)); };
    bool keysOpen = false;
    int keysCapture = -1;
    u64 keysUntil = 0;
    Menu keysMenu;
    auto keysBuild = [&]() {
        keysMenu.items.clear();
        for (int a = 0; a < Controls::COUNT; a++)
            keysMenu.items.push_back({ Controls::ActionName(a), keysCapture == a ? "PRESS A BUTTON" : Controls::InputName(ctl.Pad(a, true)) });
        keysMenu.items.push_back({ "RESET ALL", "" });
        keysMenu.items.push_back({ "BACK", "" });
    };
    std::string note;
    int shown = 0;
#ifdef SSR_ALL_GAMES
    bool installIcons = false; // (HOME MENU ICONS chosen: done after INSTALLING is on the screen)
#ifdef SSR_UPDATER
    bool updateWork = false; // (CHECK FOR UPDATES: done after CHECKING... is on the screen)
    bool launchCheck = false; // (Once, after the list's first picture is on the screen)
    bool updateAsked = false;
    unsigned updateSeen = 0;
    Menu updateMenu;
#endif
#endif
    // The list's look (GAME LIST on the settings page: COLOUR, and BACKGROUND, a picture in this
    // folder), read again when one of them changes.
    const std::string listDir = cfgPath.substr(0, cfgPath.find_last_of('/'));
    Launcher::Theme theme;
    Launcher::Background background;
    UiLang::DetectSystem();
    Launcher::LoadLook(cfgPath, listDir, theme, background);
    auto change = [&](int d) {
        const char* key = page.Change(d);
        if (key && !strncmp(key, "list_", 5)) Launcher::LoadLook(cfgPath, listDir, theme, background);
        if (key && !strcmp(key, "!releases")) { page.items[page.selected].values[0].second = "PHONE OR PC"; return; } // (No browser here: the release page is for another device.)
        if (key && !strcmp(key, "!keys"))
        {
            ctl.Load(cfgPath);
            keysOpen = true;
            keysCapture = -1;
            keysMenu.selected = 0;
            keysMenu.open = true;
            keysBuild();
            return;
        }
#ifdef SSR_ALL_GAMES
        if (key && !strcmp(key, "!forwarders") && !installIcons)
        {
            page.items[page.selected].values[0].second = "INSTALLING...";
            installIcons = true;
        }
#ifdef SSR_UPDATER
        // CHECK FOR UPDATES: the work runs on this thread after CHECKING... is on the screen
        // (a second thread here stops the program).
        if (key && !strcmp(key, "!updates") && !updateWork)
        {
            page.items[page.selected].values[0].second =
                Updater::Get().state == Updater::State::Available ? "DOWNLOADING..." : "CHECKING...";
            updateWork = true;
        }
#endif
#else
        (void)key;
#endif
    };
    while (appletMainLoop())
    {
#ifdef SSR_ALL_GAMES
        // (Set during the last pass, which then showed INSTALLING... on the screen.)
        if (installIcons)
        {
            page.items[page.selected].values[0].second = InstallHomeIcons(games);
            installIcons = false;
        }
#ifdef SSR_UPDATER
        if (updateWork)
        {
            const std::string url = Updater::Choose();
            if (!url.empty()) note = "NEW VERSION: GITHUB.COM/MAMESJ/POKEMON-FULLDEX/RELEASES";
            const Updater::Status u = Updater::Get();
            if (page.selected >= 0 && page.selected < (int)page.items.size() && !page.items[page.selected].values.empty())
                page.items[page.selected].values[0].second = u.text;
            updateSeen = u.serial;
            updateWork = false;
            if (u.state == Updater::State::Installed && Updater::RestartPending()) break;
        }
#endif
#endif
#ifdef SSR_UPDATER
        // (The Updates page's Check Now / Install Now, after the pass that showed it working.)
        if (pageUpdateWork)
        {
            const std::string url = Updater::Choose();
            if (!url.empty()) note = "NEW VERSION: GITHUB.COM/MAMESJ/POKEMON-FULLDEX/RELEASES";
            pageUpdateWork = false;
            if (Updater::Get().state == Updater::State::Installed && Updater::RestartPending()) break;
        }
#endif
        if (autoFrames > 0 && ++shown > autoFrames)
        {
            result = selected;
            break;
        }
#ifdef SSR_UPDATER
        // The update check and install (on this thread): shown on CHECK FOR UPDATES, a new
        // version said under the list too, and the list closed for the new program to start.
        {
            const Updater::Status u = Updater::Get();
            if (u.serial != updateSeen)
            {
                updateSeen = u.serial;
                for (auto* list : { &page.items, &page.all })
                    for (Launcher::Setting& st : *list)
                        if (st.key && !strcmp(st.key, "!updates") && !st.values.empty()) st.values[0].second = u.text;
                if (u.state == Updater::State::Available && !updateAsked)
                {
                    updateAsked = true;
                    updateMenu.items = { { u.canInstall ? Updater::PopupInstall() : Updater::PopupOpen(), "" },
                                         { Updater::PopupLater(), "" } };
                    updateMenu.selected = 0;
                    updateMenu.open = true;
                }
                if (u.state == Updater::State::Downloading || u.state == Updater::State::Installed) updateMenu.open = false;
                if (u.state == Updater::State::Installed && Updater::RestartPending()) break;
            }
        }
#endif
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad), now = padGetButtons(&pad);
        int tapX = -1, tapY = -1;
        {
            // (A finger dragging What's New on the Updates page scrolls it, and is not a tap.)
            static bool dragNotes = false, dragMoved = false;
            static int dragY0 = 0, dragS0 = 0;
            HidTouchScreenState ts = {};
            if (hidGetTouchScreenStates(&ts, 1) && ts.count > 0)
            {
                const bool fresh = !touching;
                touching = true;
                touchX = (int)ts.touches[0].x;
                touchY = (int)ts.touches[0].y;
                Launcher::GlassFocus& NF = Launcher::Focus();
                if (fresh) { dragNotes = !guide && Launcher::NotesAt(touchX, touchY); dragMoved = false; dragY0 = touchY; dragS0 = NF.nScroll; }
                if (dragNotes && NF.nLine > 0)
                {
                    if (std::abs(touchY - dragY0) > 12) dragMoved = true;
                    if (dragMoved) NF.nScroll = std::max(0, std::min(NF.nMax, dragS0 - (touchY - dragY0) / NF.nLine));
                }
            }
            else if (touching)
            {
                touching = false;
                if (!(dragNotes && dragMoved)) { tapX = touchX; tapY = touchY; }
                dragNotes = false;
            }
        }
        const int bandH = 22 * std::max(1, canvas.h / 240);
        const bool tapSettings = tapY >= 0 && tapY < bandH && tapX > canvas.w * 3 / 4;
        const bool tapBack = tapY >= 0 && tapY < bandH && tapX < canvas.w / 4;
        // The arrows (or a stick) move through the grid, and keep moving while held.
        const bool up = now & HidNpadButton_AnyUp, dn = now & HidNpadButton_AnyDown;
        const bool lf = now & HidNpadButton_AnyLeft, rt = now & HidNpadButton_AnyRight;
        held = (up || dn || lf || rt) ? held + 1 : 0;
#ifdef SSR_UPDATER
        if (updateMenu.open)
        {
            const bool step = held == 1 || (held > 20 && held % 6 == 0);
            MenuInput nav;
            nav.up = step && up;
            nav.down = step && dn;
            nav.accept = (down & HidNpadButton_A) != 0;
            if ((down & HidNpadButton_B) || tapBack) updateMenu.open = false;
            if (tapX >= 0)
            {
                const int row = updateMenu.ItemAt(tapX, tapY);
                if (row >= 0) { updateMenu.selected = row; nav.accept = true; }
                else if (row == -2) updateMenu.open = false;
            }
            int dir = 0;
            const int hit = updateMenu.Update(nav, dir);
            if (hit == 0)
            {
                updateMenu.open = false;
                if (!Updater::Choose().empty()) note = "NEW VERSION: GITHUB.COM/MAMESJ/POKEMON-FULLDEX/RELEASES";
            }
            else if (hit == 1) updateMenu.open = false;
            if (down & HidNpadButton_Plus) break;
        }
        else
#endif
        // KEYS AND BUTTONS: choose one with the D-pad and A, then press the button it should be
        // (5 seconds without one: as it was); B goes back to the settings page.
        if (guide && keysOpen)
        {
            if (keysCapture >= 0)
            {
                for (int in = 0; in < Controls::IN_COUNT; in++)
                    if (mask(in) && (down & mask(in)))
                    {
                        ctl.SetPad(keysCapture, in, true);
                        ctl.SaveTo(cfgPath);
                        page.Open(cfgPath, true);
                        keysCapture = -1;
                        break;
                    }
                if (keysCapture >= 0 && armGetSystemTick() > keysUntil) keysCapture = -1;
                keysBuild();
            }
            else
            {
                MenuInput nav;
                const bool step = held == 1 || (held > 20 && held % 6 == 0);
                nav.up = step && up;
                nav.down = step && dn;
                nav.accept = (down & HidNpadButton_A) != 0;
                if (down & HidNpadButton_B) keysOpen = false;
                int dir = 0;
                const int hit = keysMenu.Update(nav, dir);
                if (hit >= 0 && hit < Controls::COUNT)
                {
                    keysCapture = hit;
                    keysUntil = armGetSystemTick() + armNsToTicks(5000000000ULL);
                }
                else if (hit == Controls::COUNT)
                {
                    ctl.Reset();
                    ctl.SaveTo(cfgPath);
                    page.Open(cfgPath, true);
                }
                else if (hit == Controls::COUNT + 1) keysOpen = false;
                keysBuild();
            }
        }
        // Y: the settings page: up and down choose, left, right and A change, B or Y go back.
        else if (guide)
        {
            const bool step = held == 1 || (held > 20 && held % 6 == 0);
            if (step && (up || dn)) page.Move(up ? -1 : 1);
            if (step && (lf || rt)) change(lf ? -1 : 1);
            if (down & HidNpadButton_A) change(1);
            // (B: from a group back to the groups first; Y closes the page.)
            if (((down & HidNpadButton_B) || (tapBack && !Launcher::Glassy(canvas))) && !page.Back()) guide = false;
            // (The glass settings page: a tap chooses a row, again changes it; BACK goes back.)
            if (tapX >= 0 && Launcher::Glassy(canvas))
            {
                // (A picture card of the preview tapped: that choice, stepping the row on until it shows.)
                bool cardHit = false;
                if (page.selected >= 0 && page.selected < (int)page.items.size() && page.items[page.selected].key && page.items[page.selected].key[0] != '!')
                    for (const auto& r : setMenu.pickRects)
                        if (tapX >= r[0] && tapX < r[0] + r[2] && tapY >= r[1] && tapY < r[1] + r[3])
                        {
                            for (size_t k = 0; k < page.items[page.selected].values.size() && page.ValueIndex(page.items[page.selected]) != r[4]; k++) change(1);
                            cardHit = true;
                            break;
                        }
                const int hit = cardHit ? -1 : setMenu.SettingsItemAt(tapX, tapY);
                if (hit >= 0 && hit < (int)setMap.size())
                {
                    const int item = setMap[hit];
                    if (item < 0) { if (!page.Back()) guide = false; }
                    else if (page.items[item].key) { if (item == page.selected) change(1); else page.selected = item; }
                }
            }
            if (down & HidNpadButton_Y) guide = false;
            if (down & HidNpadButton_Plus) break;
        }
        else if ((down & (ctl.PadCustom() ? action(Controls::Y) : (u64)HidNpadButton_Y)) || (tapSettings && !Launcher::Glassy(canvas)) ||
                 (onSettings && (down & (ctl.PadCustom() ? action(Controls::A) : (u64)HidNpadButton_A))))
        {
            guide = true;
            onSettings = false;
            page.Open(cfgPath);
            held = 0;
        }
        else if (Launcher::Glassy(canvas))
        {
            // The glass launcher, as the PC's (pc_launcher.cpp): the sidebar (left from the first column,
            // the first tab or Continue), the banner's Continue and Game Options, the tabs and the games;
            // A on a game opens its menu, whose rows open the dialogs and the keyboard.
            Launcher::GlassFocus& F = Launcher::Focus();
            const u64 bA = ctl.PadCustom() ? action(Controls::A) : (u64)HidNpadButton_A;
            const u64 bB = ctl.PadCustom() ? action(Controls::B) : (u64)HidNpadButton_B;
            const u64 tl = ctl.PadCustom() ? action(Controls::L) : (u64)HidNpadButton_L;
            const u64 tr = ctl.PadCustom() ? action(Controls::R) : (u64)HidNpadButton_R;
            const bool step = held == 1 || (held > 20 && held % 6 == 0);
            const int dir = !step ? 0 : lf ? 1 : rt ? 2 : up ? 3 : dn ? 4 : 0;
            bool aDown = (down & bA) != 0;
            const bool bDown = (down & bB) != 0;
            const bool valid = selected >= 0 && selected < (int)games.size();
            const bool banner = Launcher::ContinueGame(games, selected) >= 0;
            const bool panel = Launcher::PanelHome(); const bool tiles = Launcher::TilesHome(); const bool alt = panel || tiles;
            bool play = false, quitList = false;
            auto setTab = [&](int by) {
                tab = Launcher::TabStep(tab, by); // (in the order shown)
                scroll = 0;
                const Launcher::View v = view();
                if (v.SpotOf(selected) < 0) selected = Launcher::FirstIn(v);
                if (Launcher::FirstIn(v) < 0) onTabs = true;
                note.clear();
            };
            auto intoGames = [&]() {
                const Launcher::View v = view();
                if (Launcher::FirstIn(v) < 0) { if (banner) F.banner = 1; else onTabs = true; return; }
                if (v.SpotOf(selected) < 0) selected = Launcher::FirstIn(v);
            };
            auto settle = [&]() { const Launcher::View v = view(); if (v.SpotOf(selected) < 0) selected = Launcher::FirstIn(v); };
            auto closeDialog = [&]() { F.dialog = 0; kb.open = false; };
            // The glass launcher by touch: what a tap lands on is chosen and used, as A would (a tap on a
            // game that is not the chosen one only chooses it); a tap beside the menu or a dialog closes it.
            if (tapX >= 0)
            {
                if (F.dialog == 3 && kb.open) { const int hit = kb.ItemAt(tapX, tapY); if (hit >= 0) { kb.selected = hit; aDown = true; } }
                else if (F.dialog) { const int row = Launcher::DialogAt(tapX, tapY); if (row >= 0) { F.drow = row; aDown = true; } else closeDialog(); }
                else if (F.popup >= 0) { const int row = Launcher::PopupAt(tapX, tapY); if (row >= 0) { F.popup = row; aDown = true; } else F.popup = -1; }
                else
                {
                    const int side = Launcher::SideAt(canvas, tapX, tapY), t = Launcher::TabAt(canvas, tapX, tapY);
                    const int tile = Launcher::TileAt(view(), scroll, tapX, tapY);
                    if (side >= 0) { F.side = side; F.banner = 0; onTabs = false; aDown = true; }
                    else if (F.page == Launcher::SIDE_ROMS || F.page == Launcher::SIDE_HUB)
                    {
                        const int a = Launcher::RomActionAt(tapX, tapY), r = Launcher::RomRowAt(tapX, tapY);
                        if (a >= 0) { F.side = -1; F.pact = a; aDown = true; }
                        else if (r >= 0) { if (F.page == Launcher::SIDE_HUB && (Launcher::HubTop() || Launcher::HubMode() == 3 || Launcher::HubMode() == 4) && F.prow == r && F.side < 0) aDown = true; F.side = -1; F.pact = -1; F.prow = r; }
                    }
                    else if (F.page == Launcher::SIDE_LAUNCHER && Launcher::LookTap(tapX, tapY)) {} // (the colour wheel)
                    else if (F.page >= 0 && Launcher::NotesAt(tapX, tapY)) { if (F.prow == 2 && F.side < 0) aDown = true; F.side = -1; F.prow = 2; }
                    else if (F.page >= 0)
                    {
                        const int r = Launcher::PageRowAt(tapX, tapY);
                        if (r >= 0) { if (F.prow == r && F.side < 0) aDown = true; F.side = -1; F.prow = r; }
                    }
                    else if (const int tl = Launcher::ToolAt(tapX, tapY)) { F.side = -1; F.banner = 0; onTabs = true; F.onSort = true; F.tool = tl; aDown = true; }
                    else if (Launcher::GameOptionsAt(canvas, tapX, tapY)) { F.side = -1; onTabs = false; F.banner = 2; aDown = true; }
                    else if (Launcher::ContinueAt(canvas, tapX, tapY)) { F.side = -1; onTabs = false; F.banner = 1; aDown = true; }
                    else if (t >= 0)
                    {
                        F.side = -1; F.banner = 0; onTabs = false;
                        if (t != tab) setTab(Launcher::TabDistance(tab, t));
                        else { onTabs = true; F.onSort = false; aDown = true; } // (the tab shown: its menu)
                    }
                    else if (tile >= 0)
                    {
                        F.side = -1; F.banner = 0; onTabs = false;
                        if (tile == selected) aDown = true;
                        else { selected = tile; note.clear(); }
                    }
                }
            }
            if (F.dialog == 3)
            {
                // Rename: the keyboard. A types, X deletes, + saves, B cancels.
                if (!kb.open) { kbLayer = 0; kbBuild(); }
                auto erase = [&]() {
                    while (!F.text.empty() && ((unsigned char)F.text.back() & 0xC0) == 0x80) F.text.pop_back();
                    if (!F.text.empty()) F.text.pop_back();
                };
                auto save = [&]() {
                    std::string name = F.text;
                    closeDialog();
                    if (F.kbFor) { Launcher::KeyboardDone(tab, note); refresh(selected); scroll = 0; settle(); return; } // (a new tab, the search, a tab's name)
                    if (!valid) return;
                    while (!name.empty() && name.back() == ' ') name.pop_back();
                    while (!name.empty() && name.front() == ' ') name.erase(0, 1);
                    Launcher::Game own = games[selected];
                    own.customName.clear();
                    if (name == Launcher::ShortName(own)) name.clear();
                    const bool ok = Launcher::SetPair(home + "/fulldex-names.txt", games[selected].rom, name);
                    refresh(selected);
                    note = !ok ? "COULD NOT SAVE THE NAME" : name.empty() ? "NAME RESET" : "RENAMED";
                };
                if (bDown) closeDialog();
                else if (down & HidNpadButton_X) { erase(); kbBuild(); }
                else if (down & HidNpadButton_Plus) save();
                else if (dir || aDown)
                {
                    MenuInput nav;
                    nav.up = dir == 3; nav.down = dir == 4; nav.left = dir == 1; nav.right = dir == 2; nav.accept = aDown;
                    int d = 0;
                    const int hit = kb.Update(nav, d);
                    const int id = hit >= 0 && hit < (int)kbIds.size() ? kbIds[hit] : -2;
                    if (id == Launcher::KB_BACK) closeDialog();
                    else if (id == Launcher::KB_DONE) save();
                    else if (id != -2)
                    {
                        if (id == Launcher::KB_LAYER) kbLayer = kbLayer == 0 ? 1 : 0;
                        else if (id == Launcher::KB_SYMBOLS) kbLayer = kbLayer == 2 ? 1 : 2;
                        else if (id == Launcher::KB_DEL) erase();
                        else if (id == Launcher::KB_SPACE) { if (F.text.size() < 48) F.text += ' '; }
                        else if (id >= Launcher::KB_KEY0 && id < Launcher::KB_LAYER)
                        {
                            if (F.text.size() < 48) F.text += kb.items[hit].label;
                            if (kbLayer == 0 && isupper((unsigned char)kb.items[hit].label[0])) kbLayer = 1;
                        }
                        kbBuild();
                    }
                }
            }
            else if (F.dialog)
            {
                // The dialogs' rows: up and down, A does it, B closes.
                const int rows = F.dialog >= 5 ? std::max(1, (int)F.dItems.size()) : F.dialog == 1 ? 3 : F.dialog == 4 ? 5 : 2;
                if (dir == 3 || dir == 4) F.drow = (F.drow + (dir == 3 ? rows - 1 : 1)) % rows;
                if (bDown) closeDialog();
                else if (aDown && valid)
                {
                    const std::string rom = games[selected].rom;
                    if (F.dialog == 1)
                    {
                        if (F.drow == 0)
                        {
                            closeDialog();
                            const bool ok = Launcher::SetPair(home + "/fulldex-hidden.txt", rom, "1");
                            refresh(selected);
                            settle();
                            note = ok ? "REMOVED FROM THE LIST. THE ROM IS STILL ON THE SD CARD" : "COULD NOT SAVE THE LIST";
                        }
                        else if (F.drow == 1) { F.dialog = 2; F.drow = 1; }
                        else closeDialog();
                    }
                    else if (F.dialog == 2)
                    {
                        closeDialog();
                        if (F.drow == 0)
                        {
                            std::error_code ec;
                            std::filesystem::remove(std::filesystem::u8path(rom), ec);
                            fprintf(stderr, "[launcher] delete %s: %s\n", rom.c_str(), ec ? ec.message().c_str() : "deleted");
                            if (ec) note = "COULD NOT DELETE THE ROM FILE";
                            else
                            {
                                Launcher::SetPair(home + "/fulldex-hidden.txt", rom, "1"); // (gone from this list until the next start too)
                                refresh(selected);
                                settle();
                                note = "ROM FILE DELETED. ITS SAVES ARE KEPT";
                            }
                        }
                    }
                    else if (F.dialog == 4)
                    {
                        if (F.drow == 0)
                        {
                            // Import: the pictures in the artwork folder beside the settings (PNG or JPG,
                            // put there from a PC or phone), in name order; up to 7 shown.
                            std::vector<std::string> pics;
                            std::error_code ec;
                            for (const auto& e : std::filesystem::directory_iterator(std::filesystem::u8path(home + "/artwork"), ec))
                            {
                                std::string n = e.path().filename().string(), low = n;
                                for (char& ch : low) ch = (char)tolower((unsigned char)ch);
                                if (e.is_regular_file(ec) && low.size() > 4 && (low.compare(low.size() - 4, 4, ".png") == 0 || low.compare(low.size() - 4, 4, ".jpg") == 0 || low.compare(low.size() - 5, 5, ".jpeg") == 0)) pics.push_back(n);
                            }
                            std::sort(pics.begin(), pics.end());
                            if (pics.size() > 7) pics.resize(7);
                            std::string where = home + "/artwork";
                            if (where.rfind("sdmc:", 0) == 0) where = where.substr(5);
                            F.dTitle = "Import A Picture";
                            F.dMsg = pics.empty() ? "No pictures yet. Put PNG or JPG files in " + where + " on the SD card, then come back here."
                                                  : "Pictures in " + where + " on the SD card.";
                            F.dItems = pics;
                            F.dPaths = pics;
                            F.dItems.push_back("Cancel");
                            F.dPaths.push_back("#close");
                            F.dialog = 7;
                            F.drow = 0;
                        }
                        else if (F.drow == 1) note = "FIND ARTWORK ON YOUR PHONE OR PC";
                        else if (F.drow == 2 || F.drow == 3)
                        {
                            const bool ok = Launcher::SetPair(home + "/fulldex-art.txt", rom, F.drow == 2 ? "placeholder" : "");
                            closeDialog();
                            refresh(selected);
                            note = !ok ? "COULD NOT SAVE THE PICTURE" : F.drow == 2 ? "PLAIN CARD" : "ORIGINAL PICTURE BACK";
                        }
                        else closeDialog();
                    }
                    else if (F.dialog == 15) { const std::string pick = F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : std::string("#close"); if (Launcher::ThemePick(pick) != 1) closeDialog(); } // (a theme: Apply, Delete)
                    else if (F.dialog >= 10 && F.dialog <= 12) { const bool yes = F.dialog == 11 && F.drow == 0; closeDialog(); if (yes) quitList = true; } // (the beer message: its code is scanned; Quit Fulldex?: Quit)
                    else if (F.dialog == 17 || F.dialog == 18) Launcher::SavesPick(F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : std::string("#close")); // (Manage Saves' dialogs)
                    else if (F.dialog >= 5)
                    {
                        const std::string pick = F.drow < (int)F.dPaths.size() ? F.dPaths[F.drow] : std::string("#close");
                        const std::string save = games[selected].folder + "/" + games[selected].saveName;
                        if (pick == "#close" || pick.empty()) closeDialog();
                        else if (F.dialog == 8 || F.dialog == 9 || F.dialog == 13 || F.dialog == 14)
                        {
                            // (Add To Tab, or a tab of your own's menu.)
                            const int r = Launcher::TabDialogPick(pick, tab, note);
                            if (r == 2) { kbLayer = 0; kbBuild(); }
                            if (r != 1) { scroll = 0; settle(); }
                        }
                        else if (F.dialog == 7)
                        {
                            // (Checked before it is kept: one that does not read is not chosen.)
                            std::vector<unsigned char> data;
                            std::vector<uint32_t> test;
                            closeDialog();
                            if (!Launcher::ReadFile(home + "/artwork/" + pick, data) || !Launcher::LoadSquareArt(data.data(), data.size(), test))
                                note = "THAT PICTURE COULD NOT BE READ: PNG OR JPG ONLY";
                            else
                            {
                                const bool ok = Launcher::SetPair(home + "/fulldex-art.txt", rom, pick);
                                refresh(selected);
                                note = ok ? "PICTURE CHANGED" : "COULD NOT SAVE THE PICTURE";
                            }
                        }
                        else if (F.dialog == 5)
                        {
                            F.dTitle = "Go Back To This Backup?";
                            F.dMsg = F.dItems[F.drow].substr(8) + ". Your save as it is now is kept beside it as a .before-restore copy.";
                            F.dItems = { "Restore Backup", "Cancel" };
                            F.dPaths = { pick, "#close" };
                            F.dialog = 6;
                            F.drow = 1;
                        }
                        else
                        {
                            std::error_code ec;
                            const auto now = std::filesystem::file_size(std::filesystem::u8path(save), ec);
                            const auto was = std::filesystem::file_size(std::filesystem::u8path(pick), ec);
                            std::vector<unsigned char> restored;
                            std::string error;
                            const bool ok = SaveBackups::Restore(save, pick, now > 0 ? (size_t)now : (size_t)was, restored, error);
                            closeDialog();
                            note = ok ? "SAVE RESTORED. THE ONE BEFORE IS KEPT AS A .BEFORE-RESTORE COPY" : error;
                        }
                    }
                }
            }
            else if (F.popup >= 0)
            {
                // The game's menu: up and down, A does it, B closes.
                if (dir == 3) F.popup = std::max(0, F.popup - 1);
                else if (dir == 4) F.popup = std::min(Launcher::PopupCount() - 1, F.popup + 1);
                if (bDown) F.popup = -1;
                else if (aDown && valid)
                {
                    const int item = Launcher::PopupItem(F.popup); // (the row shown -> what it does)
                    F.popup = -1;
                    const Launcher::Game& g = games[selected];
                    if (item == 0) play = true;
                    else if (item == 1)
                    {
                        const bool ok = library.Toggle(g.rom);
                        const bool now = library.Favourite(g.rom);
                        refresh(selected);
                        settle();
                        note = !ok ? "COULD NOT SAVE HOME" : now ? "ADDED TO HOME" : "REMOVED FROM HOME";
                    }
                    else if (item == 2) { F.kbFor = 0; F.text = Launcher::ShortName(g); F.dialog = 3; F.drow = 0; kbLayer = 0; kbBuild(); }
                    else if (item == 3) { F.dialog = 4; F.drow = 0; }
                    else if (item == 4 && g.platform == Launcher::N3DS) note = "3DS SAVES ARE KEPT IN THE GAME'S OWN FOLDER";
                    else if (item == 4) Launcher::OpenSaves(g, SavePath(g)); // (Manage Saves: a page of its own, saves_page.h)
                    else if (item == 5) Launcher::OpenAddToTab(g.rom);
                    else if (item == 6) { F.dialog = 1; F.drow = 0; }
                }
            }
            else
            {
                if (!onTabs) F.onSort = false;
                // A page of the sidebar open (About): its arrows, A and B.
                if (F.page >= 0)
                {
                    Launcher::PageMove(dir);
                    if (aDown)
                    {
                        if (F.side == Launcher::SIDE_SETTINGS) { guide = true; page.Open(cfgPath); held = 0; }
                        else if (F.side >= 0) { if (!Launcher::OpenSide(F.side)) note = std::string(Launcher::SideName(F.side)) + ": COMING SOON"; }
                        else if (F.page == Launcher::SIDE_ROMS)
                        {
                            // (The ROMs page: A on the list goes to the chosen ROM's actions; on those, does it.)
                            const std::vector<Launcher::Game>& roms = Launcher::RomList();
                            if (F.pact < 0) { if (!roms.empty()) F.pact = 0; }
                            else if (F.pact == 0 && F.prow >= 0 && F.prow < (int)roms.size())
                            {
                                const std::string rom = roms[F.prow].rom;
                                const bool was = roms[F.prow].hidden;
                                const bool ok = Launcher::SetPair(home + "/fulldex-hidden.txt", rom, was ? "" : "1");
                                refresh(selected);
                                if (selected < 0 && !games.empty()) selected = 0;
                                note = !ok ? "COULD NOT SAVE THE LIST" : was ? "BACK IN THE LIST" : "HIDDEN FROM THE LIST";
                            }
                        }
                        else if (F.page == Launcher::SIDE_LAUNCHER) { if (Launcher::LookDo(F.prow, 1) == 2) { kbLayer = 0; kbBuild(); } } // (2: My Themes' keyboard)
                        else if (F.page == Launcher::SIDE_HUB)
                        {
                            // (The Hub: A on the list goes to the chosen row's actions; on those, does it.)
                            if (F.pact < 0) { if (!Launcher::HubEnter(F.prow) && !Launcher::HubActions(F.prow).empty()) F.pact = 0; else if (Launcher::HubTop() && F.prow == 3) note = "SCAN THE CODE WITH YOUR PHONE"; }
                            else { const std::string said = Launcher::HubDo(F.prow, F.pact); if (!said.empty()) note = said; if (F.dialog == 3) { kbLayer = 0; kbBuild(); } }
                        }
                        else if (!Launcher::PageUrl(F.page, F.prow).empty()) note = "OPEN IT ON YOUR PHONE OR PC";
                    }
                    if (bDown) Launcher::PageBack();
                    if (down & HidNpadButton_Plus) quitList = true;
                }
                else
                {
                if (down & (tl | tr)) setTab((down & tr) ? 1 : -1);
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
                    if (aDown)
                    {
                        if (F.side == Launcher::SIDE_SETTINGS) { guide = true; page.Open(cfgPath); held = 0; }
                        else if (!Launcher::OpenSide(F.side)) note = std::string(Launcher::SideName(F.side)) + ": COMING SOON";
                    }
                }
                else if (onTabs)
                {
                    if (F.onSort)
                    {
                        // (Sort on the tab bar: left back to the tabs, down to the games, A the next order.)
                        if (panel && dir == 3) { F.onSort = false; onTabs = false; F.side = Launcher::SIDE_HOME; }
                    else if (dir == 1) { if (F.tool > 1) F.tool--; else F.onSort = false; }
                        else if (dir == 2) F.tool = std::min(3, F.tool + 1);
                        else if (dir == 4) { F.onSort = false; onTabs = false; if (banner && !alt) F.banner = 1; else intoGames(); }
                        else if (aDown && F.tool == 1 && !Launcher::TabHidden().empty()) Launcher::OpenPlusMenu(); // (+: new, or one hidden shown)
                        else if (aDown && F.tool != 3)
                        {
                            // (+ and Search: the keyboard, for a new tab or the search.)
                            F.kbFor = F.tool == 1 ? 1 : 2;
                            F.kbRom.clear();
                            F.text = F.tool == 2 ? Launcher::SearchText() : std::string();
                            F.dialog = 3; F.drow = 0; kbLayer = 0; kbBuild();
                        }
                        else if (aDown)
                        {
                            if (!Launcher::SetConfigLine(cfgPath, "list_sort", Launcher::NextSort(Launcher::ConfigValue(cfgPath, "list_sort")))) note = "COULD NOT SAVE THE SETTINGS";
                            refresh(selected);
                            settle();
                        }
                    }
                    else if (panel && dir == 3) { onTabs = false; F.side = Launcher::SIDE_HOME; }
                else if (dir == 1 && tab == Launcher::FirstTab()) { if (!alt) { onTabs = false; F.side = Launcher::SIDE_HOME; } }
                    else if (dir == 2 && tab == Launcher::LastTab()) { F.onSort = true; F.tool = 1; }
                    else if (dir == 1 || dir == 2) setTab(dir == 2 ? 1 : -1);
                    else if (aDown) Launcher::OpenTabMenu(tab); // (a tab: move, hide; one of your own also rename, delete)
                    else if (dir == 4 || aDown) { onTabs = false; if (banner && !alt) F.banner = 1; else intoGames(); }
                }
                else if (F.banner)
                {
                    if (dir == 1) { if (F.banner == 2) F.banner = 1; else { F.banner = 0; if (alt) intoGames(); else F.side = Launcher::SIDE_HOME; } }
                    else if (dir == 2) F.banner = 2;
                    else if (dir == 3) { F.banner = 0; if (tiles) intoGames(); else onTabs = true; }
                else if (tiles && dir == 4) { F.banner = 0; F.side = Launcher::SIDE_HOME; }
                    else if (dir == 4 && Launcher::FirstIn(view()) >= 0) { F.banner = 0; intoGames(); }
                    else if (aDown)
                    {
                        if (F.banner == 2) { const int cg = Launcher::ContinueGame(games, selected); if (cg >= 0) { selected = cg; F.banner = 0; F.popup = 0; } } // (Game Options: its menu)
                        else
                        {
                            const int cg = Launcher::ContinueGame(games, selected);
                            F.banner = 0;
                            if (cg >= 0) { selected = cg; play = true; }
                        }
                    }
                }
                else
                {
                    if (dir)
                    {
                        const Launcher::View v = view();
                        const int k = v.SpotOf(selected);
                        if (tiles)
                        {
                            if (dir == 3) onTabs = true;
                            else if (dir == 4) { if (banner) F.banner = 1; else F.side = Launcher::SIDE_HOME; }
                            else { const int to2 = v.spots.empty() ? -1 : Launcher::Step(v, selected, dir == 1 ? 3 : 4); if (to2 >= 0 && to2 != selected) { selected = to2; note.clear(); } }
                        }
                        else if (panel && dir == 1) {}
                        else if (panel && dir == 2 && banner) F.banner = 1;
                        else if (dir == 1 && (k < 0 || v.spots[k].x == v.g.gx)) F.side = Launcher::SIDE_HOME;
                        else
                        {
                            const int to = v.spots.empty() ? -1 : Launcher::Step(v, selected, dir);
                            if (to < 0) { if (dir == 3) { if (banner && !alt) F.banner = 1; else onTabs = true; } }
                            else if (to != selected) { selected = to; note.clear(); }
                        }
                    }
                    if (aDown && valid)
                    {
                        if (games[selected].known && !games[selected].comingSoon) { F.popup = 0; note.clear(); }
                        else note = games[selected].comingSoon ? games[selected].name + ": COMING SOON" : "THIS GAME IS NOT ONE THAT THIS PLAYS";
                    }
                }
                if (down & HidNpadButton_Plus) quitList = true;
                else if (bDown && F.side < 0 && !F.banner && !onTabs) Launcher::OpenQuitAsk(); // (B: asked first)
                if (bDown && (F.side >= 0 || F.banner || onTabs)) { F.side = -1; F.banner = 0; onTabs = false; intoGames(); }
                }
            }
            {
                // (What Manage Saves leaves for the platform: where the saves are, or the game started from a state.)
                std::string q;
                std::swap(q, Launcher::Saves().request);
                if (q.rfind("#folder:", 0) == 0) note = "SAVES ARE IN " + q.substr(8);
                else if (q.rfind("#play:", 0) == 0 || q == "#replay")
                {
                    for (size_t i = 0; i < games.size(); i++) if (games[i].rom == Launcher::Saves().rom) selected = (int)i;
                    if (q != "#replay") Launcher::PlayState() = q.substr(6);
                    Launcher::HubMode() = 0; Launcher::Focus().page = -1; Launcher::Focus().side = -1; Launcher::Focus().pact = -1;
                    play = true;
                }
            }
            if (play && selected >= 0 && selected < (int)games.size())
            {
                if (Playable(games[selected]))
                {
                    fprintf(stderr, "[launch] save check begin: %s\n", games[selected].code.c_str());
                    const bool blocked = Launcher::SavesPlayCheck(games[selected], SavePath(games[selected]));
                    fprintf(stderr, "[launch] save check: %s\n", blocked ? "confirmation needed" : "ready");
                    if (!blocked) { result = selected; break; }
                }
                else note = games[selected].known ? "THIS DOWNLOAD DOES NOT HAVE THIS GAME" : "THIS GAME IS NOT ONE THAT THIS PLAYS";
            }
            if (quitList) break;
        }
        else
        {
            // L and R (or the buttons CONTROLS gave them): the tabs, keeping the chosen game if
            // it is in the new one.
            const u64 tl = ctl.PadCustom() ? action(Controls::L) : (u64)HidNpadButton_L;
            const u64 tr = ctl.PadCustom() ? action(Controls::R) : (u64)HidNpadButton_R;
            auto setTab = [&](int by) {
                tab = Launcher::TabStep(tab, by); // (in the order shown)
                scroll = 0;
                const Launcher::View v = view();
                if (v.SpotOf(selected) < 0) selected = Launcher::FirstIn(v);
                if (Launcher::FirstIn(v) < 0) onTabs = true; // (no games: left and right go on)
                note.clear();
            };
            if (down & (tl | tr)) setTab((down & tr) ? 1 : -1);
            const bool step = held == 1 || (held > 20 && held % 6 == 0);
            const int dir = !step ? 0 : lf ? 1 : rt ? 2 : up ? 3 : dn ? 4 : 0;
            const bool aDown = (down & (ctl.PadCustom() ? action(Controls::A) : (u64)HidNpadButton_A)) != 0;
            const bool wasNav = onSettings || onTabs; // (A there is not a game's: it is not played as well)
            if (onSettings)
            {
                // (Down from SETTINGS: the tabs.)
                if (dir == 4)
                {
                    onSettings = false;
                    onTabs = true;
                }
            }
            else if (onTabs)
            {
                // Left and right: the tabs; down (or A): into the tab's games; up: SETTINGS.
                if (dir == 1 || dir == 2) setTab(dir == 2 ? 1 : -1);
                else if (dir == 3)
                {
                    onTabs = false;
                    onSettings = true;
                }
                else if ((dir == 4 || aDown) && Launcher::FirstIn(view()) >= 0)
                {
                    onTabs = false;
                    if (view().SpotOf(selected) < 0) selected = Launcher::FirstIn(view());
                }
            }
            else if (dir)
            {
                const int to = Launcher::Step(view(), selected, dir);
                if (to < 0 && dir == 3) onTabs = true; // (up from the first row)
                else if (to >= 0 && to != selected)
                {
                    selected = to;
                    note.clear();
                }
            }
            if (!wasNav && !onSettings && !onTabs && selected >= 0 && (down & (ctl.PadCustom() ? action(Controls::A) : (u64)HidNpadButton_A)))
            {
                if (Playable(games[selected]))
                {
                    result = selected;
                    break;
                }
                note = games[selected].comingSoon ? games[selected].name + ": COMING SOON"
                     : !games[selected].problem.empty() ? "PATCH: " + games[selected].problem
                     : games[selected].known ? "THIS DOWNLOAD DOES NOT HAVE THIS GAME" : "THIS GAME IS NOT ONE THAT THIS PLAYS";
            }
            if (down & (HidNpadButton_Plus | (ctl.PadCustom() ? action(Controls::B) : (u64)HidNpadButton_B))) break;
        }
        // (The game list only when the settings page is not open: drawn over it, the page was
        // hidden while it took the buttons, and the list seemed frozen.)
        // (The glass list is drawn again only when something on it changed: the Switch draws in
        // software about 20 times slower than a PC. Anything else drawn on the canvas forgets it.)
        static std::string glassKey;
        static std::string setKey; // (the glass settings page as last drawn: drawn again only when it changes)
        if (guide)
        {
            glassKey.clear();
            const char* hint = page.group < 0 ? "UP / DOWN: CHOOSE    A: OPEN    B: BACK" : "UP / DOWN: CHOOSE    LEFT / RIGHT / A: CHANGE    B: BACK";
            std::string setNow = std::to_string(page.group) + "/" + std::to_string(page.selected) + "/" + std::to_string(keysOpen) + "/";
            for (const std::string& l : page.lines) setNow += l + "\n";
            if (Launcher::Glassy(canvas) && !keysOpen && setNow == setKey) {}
            else if (Launcher::Glassy(canvas))
            {
                setKey = keysOpen ? std::string() : setNow;
                const u64 t0 = armGetSystemTick();
                // (The pause menu's glass settings pages, ui_settings.h, over the chosen game's picture blurred.)
                setMenu.touchRows = 1.5f; // (as on a phone: bigger rows, the page larger)
                setMenu.laneK = std::max(0.42f, Launcher::GU(canvas) * (480.0f / 720.0f) * 0.94f * 1.3f);
                Launcher::BuildSettingsMenu(setMenu, setMap, page, false);
                Launcher::DrawSettingsPage(canvas, setMenu, games, tab, scroll, page.group < 0 ? "Settings" : Launcher::SettingsGroups()[page.group].name,
                                           page.group < 0 ? "A: OPEN    B: BACK" : "A: CHANGE    B: BACK", theme);
                fprintf(stderr, "[launcher] glass settings drawn in %d ms\n", (int)(armTicksToNs(armGetSystemTick() - t0) / 1000000));
            }
            else page.Draw(canvas, hint, theme);
            if (keysOpen)
            {
                std::string kh = keysCapture >= 0 ? "OR WAIT 5 SECONDS: AS IT WAS" : "A: CHANGE   B: BACK";
                if (keysCapture < 0)
                    if (const char* h = Controls::ActionHint(keysMenu.selected)) kh = h;
                keysMenu.Draw(canvas, "KEYS AND BUTTONS", kh);
            }
        }
        else
        {
            {
                setKey.clear();
                FillUpdateStatus(pageUpdateWork);
                // (The blurred background follows the chosen game once it has stayed on it 0.4 s.)
                static int bgSel = -2;
                static u64 bgSince = 0;
                const u64 now = armGetSystemTick();
                if (selected != bgSel) { bgSel = selected; bgSince = now; }
                const std::string want = selected >= 0 && selected < (int)games.size() ? games[selected].rom : std::string();
                if (!want.empty() && want != Launcher::BackdropRom() && (Launcher::BackdropRom().empty() || armTicksToNs(now - bgSince) > 400000000ULL))
                    Launcher::BackdropRom() = want;
            }
            auto keyNow = [&]() { const Launcher::GlassFocus& F = Launcher::Focus();
                                  return std::to_string(F.side) + "," + std::to_string(F.banner) + "," + std::to_string(F.popup) + "," + std::to_string(F.dialog) + ","
                                         + std::to_string(F.drow) + "," + std::to_string(F.page) + "," + std::to_string(F.prow) + "," + std::to_string(F.upState) + F.upText + "," + std::to_string(F.nScroll) + "," + std::to_string(F.onSort) + std::to_string(F.tool) + Launcher::SortKey() + Launcher::SearchText() + Launcher::TabsSig() + "," + std::to_string(F.pact) + "," + Launcher::HubSig() + "," + std::to_string(Launcher::RomList().size()) + ","
                                         + std::to_string(std::count_if(Launcher::RomList().begin(), Launcher::RomList().end(), [](const Launcher::Game& g) { return g.hidden; })) + "," + F.text + "," + std::to_string(kb.selected) + std::to_string(kbLayer) + "," + std::to_string(games.size()) + "/"
                                         + Launcher::BackdropRom() + "/" + std::to_string(tab) + "/" + std::to_string(selected) + "/" + std::to_string(scroll) + "/" + note + "/"
                                         + std::to_string(onTabs) + std::to_string(onSettings); };
            bool overlay = false;
#ifdef SSR_UPDATER
            overlay = updateMenu.open;
#endif
            if (!Launcher::Glassy(canvas) || overlay || keyNow() != glassKey)
            {
                const u64 t0 = armGetSystemTick();
                if (Launcher::Focus().dialog == 3 && kb.open && Launcher::Glassy(canvas))
                {
                    // (Rename: the pause menu's keyboard page over the whole screen.)
                    Launcher::GlassBackdrop(canvas, selected >= 0 && selected < (int)games.size() ? &games[selected] : nullptr);
                    kb.Draw(canvas, Launcher::KbTitle(), "A: TYPE    X: DELETE    +: SAVE    B: CANCEL");
                }
                else Launcher::Draw(canvas, games, tab, !Launcher::TilesOn() && (onSettings || onTabs || Launcher::Focus().side >= 0 || Launcher::Focus().banner) ? -1 : selected,
                                    scroll, note, Launcher::Focus().popup >= 0 || Launcher::Focus().dialog || Launcher::Focus().page >= 0 ? "A: CHOOSE    B: BACK" : "A: PLAY    L / R: TABS    +: QUIT",
                                    theme, true, onTabs, onSettings);
                glassKey = overlay ? std::string() : keyNow();
                if (Launcher::Glassy(canvas)) fprintf(stderr, "[launcher] glass list drawn in %d ms\n", (int)(armTicksToNs(armGetSystemTick() - t0) / 1000000));
            }
        }
#ifdef SSR_UPDATER
        if (updateMenu.open)
            updateMenu.Draw(canvas, Updater::PopupTitle(), "A: CHOOSE    B: LATER",
                            Updater::PopupNote(Updater::Get().canInstall));
#endif
        {
            // The title band's buttons, as pc_launcher.cpp draws them (the chosen one framed).
            const int s = std::max(1, canvas.h / 240);
            const Launcher::Theme& th = theme;
            auto button = [&](int x, const char* text, bool on) {
                if (on) canvas.Fill(x - 3 * s, 4 * s, Canvas::TextWidth(text, s) + 5 * s, 13 * s, th.frame);
                canvas.Text(x, 7 * s, text, s, on ? th.title : th.hint);
            };
            if (guide && !keysOpen && !Launcher::Glassy(canvas)) button(8 * s, "< BACK", false); // (the glass page has its own)
            else if (!guide && !Launcher::Glassy(canvas)) button(canvas.w - 8 * s - Canvas::TextWidth("SETTINGS", s), "SETTINGS", onSettings);
        }
        u32 stride;
        u32* out = (u32*)framebufferBegin(&fb, &stride);
        for (int y = 0; y < 720; y++)
        {
            const uint32_t* src = &canvas.px[(size_t)y * 1280];
            u32* dst = out + (size_t)y * (stride / 4);
            // (ARGB to the framebuffer's RGBA bytes.)
            for (int x = 0; x < 1280; x++)
            {
                const uint32_t p = src[x];
                dst[x] = 0xFF000000u | (p & 0xFF) << 16 | (p & 0xFF00) | (p >> 16 & 0xFF);
            }
        }
        framebufferEnd(&fb);
#ifdef SSR_UPDATER
        // After the list is on the screen: one check at launch, on this thread (a second
        // thread here stops the program). A new version is only said, never installed unasked.
        if (!launchCheck && autoFrames == 0)
        {
            launchCheck = true;
            Updater::Check(true);
        }
#endif
    }
    {
        u32 stride;
        if (u32* out = (u32*)framebufferBegin(&fb, &stride))
        {
            for (int y = 0; y < 720; y++)
            {
                u32* dst = out + (size_t)y * (stride / 4);
                for (int x = 0; x < 1280; x++) dst[x] = 0xFF000000u;
            }
            framebufferEnd(&fb);
        }
    }
    ExitTrace("the list's screen closed");
    framebufferClose(&fb);
    // (The chosen game handed back as its place in gamesIn: the list shown may be in another order.)
    if (result >= 0 && result < (int)games.size())
    {
        const std::string rom = games[result].rom;
        result = -1;
        for (size_t i = 0; i < gamesIn.size(); i++)
            if (gamesIn[i].rom == rom) result = (int)i;
    }
    return result;
}

// The saves of the games in a folder go in its saves/ folder: those an older build left beside the
// ROMs (game.sav, emerald.sav, and their .sav1, .bak...) are moved there, none replaced.
// Whether a folder is Fulldex's own (its settings, or a Pokemon program in it). Another app's
// folder (melonDS keeps its saves beside its ROMs) is left exactly as it is.
static bool FulldexFolder(const std::string& folder)
{
    struct stat st;
    if (stat((folder + "/settings.cfg").c_str(), &st) == 0 || stat((folder + "/ssrecomp.cfg").c_str(), &st) == 0) return true;
    DIR* d = opendir(folder.c_str());
    if (!d) return false;
    bool own = false;
    while (dirent* e = readdir(d))
    {
        std::string n = e->d_name;
        for (char& c : n) c = (char)tolower((unsigned char)c);
        if (n.size() > 4 && n.compare(n.size() - 4, 4, ".nro") == 0 && n.find("pokemon") != std::string::npos) { own = true; break; }
    }
    closedir(d);
    return own;
}

static void MoveSavesIn(const std::string& folder)
{
    if (!FulldexFolder(folder)) return;
    DIR* d = opendir(folder.c_str());
    if (!d) return;
    std::vector<std::string> names;
    while (dirent* e = readdir(d))
    {
        struct stat st;
        const std::string n = e->d_name;
        if (n[0] != '.' && n.find(".sav") != std::string::npos && stat((folder + "/" + n).c_str(), &st) == 0 && S_ISREG(st.st_mode))
            names.push_back(n);
    }
    closedir(d);
    if (names.empty()) return;
    const std::string saves = folder + "/saves";
    mkdir(saves.c_str(), 0777);
    for (const std::string& n : names)
    {
        struct stat st;
        if (stat((saves + "/" + n).c_str(), &st) != 0 && rename((folder + "/" + n).c_str(), (saves + "/" + n).c_str()) == 0)
            fprintf(stderr, "[switch] save %s moved into %s\n", n.c_str(), saves.c_str());
    }
}

// Where a DS game's save is: the saves folder of the game's folder (MoveSavesIn), made when missing.
// (name: another save of the game's folder, e.g. a hack's game's: Launcher::AdoptHackSaves.)
static std::string SavePathFor(const Launcher::Game& g, const std::string& name)
{
    if (!FulldexFolder(g.folder)) return g.folder + "/" + name; // (another app's folder: beside the ROM, as it was)
    mkdir((g.folder + "/saves").c_str(), 0777);
    return g.folder + "/saves/" + name;
}
static std::string SavePath(const Launcher::Game& g) { return SavePathFor(g, g.saveName); }
// (Cloud saves and the Hub sync and move this one: the save the game really reads.)
static const bool g_hubSavePath = (Launcher::PlatformSavePath() = [](const Launcher::Game& g) { return SavePath(g); }, true);

// The games to choose from: rom.nds, or the ROMs in the program's folder (and its roms/ folder);
// in the all-games program also those in the other folders of /switch (the one-game programs'
// folders), each played with the save in its own folder's saves/ (SavePath), so that the two
// kinds of download share a game's save.
std::vector<Launcher::Game> FindGames(const std::string& dir, bool& ownFolder)
{
    std::vector<Launcher::Game> games;
    Launcher::Game g;
    ownFolder = true;
    // (ReadGame and ScanFolder look for a save beside the ROMs: the DS games' are in saves/.)
    auto savesIn = [](std::vector<Launcher::Game>& list) {
        struct stat st;
        for (Launcher::Game& x : list)
            if (x.platform == Launcher::DS) x.hasSave = stat(SavePath(x).c_str(), &st) == 0;
    };
    MoveSavesIn(dir);
    // (Each DS ROM checked against the clean releases, in the background the first time: a hack
    // gets a save of its own. romhacks.h)
    Launcher::BeginScan(dir + "/saves");
    fprintf(stderr, "[launch] library scan begin\n");
    struct EndScan
    {
        ~EndScan() { Launcher::EndScan(); fprintf(stderr, "[launch] library scan complete\n"); }
    } endScan;
    if (Launcher::ReadGame(dir + "/rom.nds", dir, g))
    {
        games = { g };
        savesIn(games);
        return games;
    }
#ifndef SSR_ALL_GAMES
    // (A one-game program: its own folder only, as always.)
    Launcher::ScanFolder(dir, games, false);
#else
    Launcher::ScanFolder(dir, games);
    {
        const size_t own = games.size();
        if (DIR* d = opendir("sdmc:/switch"))
        {
            while (dirent* e = readdir(d))
            {
                const std::string folder = std::string("sdmc:/switch/") + e->d_name;
                if (e->d_name[0] == '.' || folder == dir) continue;
                struct stat st;
                if (stat(folder.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
                const size_t had = games.size();
                Launcher::ScanFolder(folder, games);
                if (games.size() > had) MoveSavesIn(folder); // (only a folder with DS games of its own)
            }
            closedir(d);
        }
        ownFolder = own > 0;
    }
    // ROM hacks as patch files (.xdelta, .bps, .ups) in this folder or its roms: each made once
    // into hacks/ (waiting here for the ROMs it needs read: the list is looked at only once).
    Launcher::AddPatchedHacks({ dir, dir + "/roms" }, dir + "/hacks", dir, games, true);
#ifdef SSR_GBA
    // The GBA games: in this folder's roms (their saves in saves/ with the DS games') and a GBA folder in
    // it (with that folder's saves), and in Pokemon GBA Widescreen's own folder if it is on the
    // card too (with its saves, so they carry on).
    {
        std::map<std::string, std::vector<uint32_t>> art;
        const size_t before = games.size();
        GbaGames::AddWithGbaFolder(dir + "/roms", dir + "/saves", games, art);
        const std::string gbaApp = "sdmc:/switch/pokemon-gba-wide";
        struct stat st;
        if (dir != gbaApp && stat((gbaApp + "/roms").c_str(), &st) == 0)
        {
            const size_t mine = games.size();
            GbaGames::Add(gbaApp + "/roms", gbaApp + "/saves", games, art);
            for (size_t i = mine; i < games.size(); i++)
                for (size_t j = before; j < mine; j++)
                    if (games[j].code == games[i].code) games[i].where = "IN GBA-WIDE";
        }
        ownFolder |= games.size() > before;
    }
#endif
#endif
    savesIn(games);
    Launcher::AdoptHackSaves(games, SavePathFor);
    // Omega Ruby and Alpha Sapphire: the FullDex Builder's folders in /switch (n3ds.h).
    N3ds::AddSwitch(games);
    Launcher::AddComingSoon(games);
    N3ds::DropPlaceholders(games);
    Launcher::Sort(games);
    // The same game in two folders (two ROMs, two saves): each says where it is.
    for (size_t i = 0; i < games.size(); i++)
        for (size_t j = 0; j < games.size(); j++)
            if (i != j && games[i].platform == Launcher::DS && games[i].code == games[j].code && games[i].folder != dir)
            {
                // (pokemon-black2-wide: IN BLACK2)
                std::string name = games[i].folder.substr(games[i].folder.find_last_of('/') + 1);
                if (name.rfind("pokemon-", 0) == 0) name = name.substr(8);
                if (name.size() > 5 && name.compare(name.size() - 5, 5, "-wide") == 0) name.resize(name.size() - 5);
                games[i].where = "IN " + name.substr(0, 14);
            }
    return games;
}

// Set when the game list was shown: the pause menu can then go back to it.
bool g_canChange = false, g_changeGame = false;
}

// Set while a start with the JIT (and with its fast memory) has not yet run for a few
// seconds (see JitStarted).
static std::string g_jitMarker, g_fastMemMarker;

// (A trace of the closing down, in exit-trace.txt beside the program: each step written and
// closed at once, so the last line says where it stopped.)
static char g_exitTrace[512];
void ExitTrace(const char* step)
{
    if (!g_exitTrace[0]) return;
    if (FILE* f = fopen(g_exitTrace, "a")) { fprintf(f, "%llu %s\n", (unsigned long long)(armTicksToNs(armGetSystemTick()) / 1000000), step); fclose(f); }
}
static void ExitTraceAtExit() { ExitTrace("atexit: the last handler"); }
static void ExitTraceStart() { atexit(ExitTraceAtExit); }
// (Begun once the list knows its folder: exit-trace.txt beside the settings.)
void ExitTraceIn(const std::string& dir)
{
    snprintf(g_exitTrace, sizeof g_exitTrace, "%s/exit-trace.txt", dir.c_str());
    if (FILE* f = fopen(g_exitTrace, "w")) fclose(f);
    ExitTrace("started");
}
extern "C" void userAppExit(void)
{
    ExitTrace("libnx: final network cleanup");
    Achievements::ShutdownNetwork();
    ExitTrace("libnx: closing the services");
}

// Omega Ruby or Alpha Sapphire: its own program started as this one ends, told where this one is
// (FULLDEX_RETURN), so that quitting the game from its menu brings the list back.
static bool Start3ds(const Launcher::Game& g)
{
    if (!envHasNextLoad())
    {
        fprintf(stderr, "[3ds] cannot start another program from here (no homebrew loader)\n");
        return false;
    }
    const std::string args = "\"" + g.rom + "\"" + (g_self.empty() ? std::string() : " \"FULLDEX_RETURN=" + g_self + "\"");
    const Result rc = envSetNextLoad(g.rom.c_str(), args.c_str());
    fprintf(stderr, "[3ds] %s: %s (0x%x)\n", g.name.c_str(), args.c_str(), (unsigned)rc);
    ExitTrace("3ds handoff: closing network");
    Achievements::ShutdownNetwork();
    ExitTrace("3ds handoff: returning to loader");
    return false;
}

bool Start(int& argc, char**& argv)
{
    ExitTraceStart();
    // The folder the .nro was started from (the Homebrew Menu passes its path), or the usual one.
    std::string dir = "sdmc:/switch/ssrecomp";
    if (argc > 0 && argv[0] && strncmp(argv[0], "sdmc:/", 6) == 0)
    {
        std::string self = argv[0];
        size_t slash = self.find_last_of('/');
        if (slash != std::string::npos && slash > 5) dir = self.substr(0, slash);
    }
    mkdir(dir.c_str(), 0777);
    chdir(dir.c_str());
    if (argc > 0 && argv[0]) g_self = argv[0];
    // A test run (testrun.h) from test.txt, if there is one to do: its log goes to its folder.
    std::vector<std::string> testArgs;
    std::string testNotes;
    // (Not when started with arguments, which are then used as they are.)
    const bool testRun = argc < 2 && TestRun::Setup(dir, testArgs, g_test, testNotes);
    const std::string logPath = testRun ? g_test.results + "/game.log" : dir + "/game.log";

    // Messages go to game.log beside the game, as run.sh does on a PC, with the frame timing,
    // so that a log can be sent with a bug report. The last session's log is kept as
    // game-previous.log: QUIT to the game list starts the program again, which would otherwise
    // replace the log of the game just played with the list's few lines. (Only a log longer
    // than the list's is kept, so going to the list twice keeps the game's.)
    if (!testRun)
    {
        struct stat ls;
        if (stat(logPath.c_str(), &ls) == 0 && ls.st_size > 1024)
        {
            const std::string prev = dir + "/game-previous.log";
            remove(prev.c_str());
            rename(logPath.c_str(), prev.c_str());
        }
    }
    if (!freopen(logPath.c_str(), "w", stderr)) freopen("game.log", "w", stderr);
    setvbuf(stderr, nullptr, _IOLBF, 0);
    setenv("SSR_PERF", "1", 0);
    fprintf(stderr, "[switch] started from %s\n", argc > 0 && argv[0] ? argv[0] : "?");
    fputs(testNotes.c_str(), stderr);

    // Started from the album (applet mode) a homebrew program only gets a few hundred MB of
    // memory; the game's code alone is larger. Started in place of a game (holding R) it gets
    // nearly all of it.
    const AppletType type = appletGetAppletType();
    fprintf(stderr, "[switch] applet type %d\n", (int)type);
    if (type != AppletType_Application && type != AppletType_SystemApplication)
    {
        ShowMessage("  This needs the console's full memory.\n"
                    "  Start it from a game instead of the album:\n"
                    "  hold R while starting any game, then choose this game\n"
                    "  (or start it from its forwarder on the HOME Menu).");
        return false;
    }
    u64 total = 0, used = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    fprintf(stderr, "[switch] memory: %llu MB in use of %llu MB\n", (unsigned long long)(used >> 20),
        (unsigned long long)(total >> 20));

    // "--game IREO" (the HOME Menu icons: switch_forwarder.cpp): that game at once, without the
    // list. Other arguments (e.g. sent with nxlink while testing) are used as they are.
    std::string direct;
#ifdef SSR_ALL_GAMES
    if (argc == 3 && !strcmp(argv[1], "--game"))
    {
        direct = argv[2];
        for (char& c : direct) c = (char)toupper((unsigned char)c);
    }
    else
#endif
    if (argc >= 2) return true;

    // The game: the only one there is, or the one chosen from the list (launcher.h).
    bool ownFolder = true;
    std::vector<Launcher::Game> games = FindGames(dir, ownFolder);
#ifdef SSR_ALL_GAMES
    // The all-games download comes with an empty roms folder; made here too, in case it was lost
    // in copying, so that the message below points somewhere that is there.
    mkdir((dir + "/roms").c_str(), 0777);
    const std::string romsDir = dir + "/roms/";
#else
    const std::string romsDir = dir + "/";
#endif
    if (games.empty())
    {
        ShowMessage("  No game found. Copy your ROMs (Pokemon Black, White, Black 2 or White 2 in\n"
                    "  English; SoulSilver, HeartGold or Platinum US) into\n  " + romsDir);
        return false;
    }
    int chosen = 0;
#ifndef SSR_ALL_GAMES
    // The one-game programs have no game list (only the all-games download does).
    if (games.size() > 1)
    {
        ShowMessage("  More than one .nds file in\n  " + dir + "\n  Rename the one to play to rom.nds.");
        return false;
    }
#else
    int directGame = -1;
    // (The game itself, not a hack of it: a HOME Menu icon is for the game.)
    for (size_t i = 0; i < games.size() && !direct.empty(); i++)
        if (games[i].code == direct && Playable(games[i]) && (directGame < 0 || (games[directGame].hack && !games[i].hack)))
            directGame = (int)i;
    if (!direct.empty() && directGame < 0) fprintf(stderr, "[switch] --game %s: not in the list\n", direct.c_str());
    if (directGame >= 0)
    {
        // (Noted as the game played last, so that the list starts on it after QUIT.)
        chosen = directGame;
        if (FILE* f = fopen((dir + "/last-game.txt").c_str(), "w"))
        {
            fprintf(f, "%s\n", games[chosen].rom.c_str());
            fclose(f);
        }
        fprintf(stderr, "[switch] --game %s: %s\n", direct.c_str(), games[chosen].rom.c_str());
        g_canChange = true;
    }
    else if (games.size() > 1)
    {
        // The list starts on the game played last.
        const std::string lastPath = dir + "/last-game.txt";
        std::string last;
        if (FILE* f = fopen(lastPath.c_str(), "r"))
        {
            char line[768] = {};
            if (fgets(line, sizeof(line), f)) last = line;
            fclose(f);
            while (!last.empty() && (last.back() == '\n' || last.back() == '\r')) last.pop_back();
        }
        for (size_t i = 0; i < games.size(); i++)
            if (games[i].rom == last) chosen = (int)i;
        Launcher::ContinueRom() = last;
        const auto showList = std::find(testArgs.begin(), testArgs.end(), "--show-list");
        if (testRun && showList != testArgs.end())
        {
            // (Shown for 90 frames, then the game played last, as if chosen.)
            testArgs.erase(showList);
            for (Launcher::Game& g : games) Launcher::FindArt(g, dir);
            chosen = PickGame(games, chosen, SettingsPath(dir), 90);
            fprintf(stderr, "[switch] test run: %s, after the game list\n", games[chosen].rom.c_str());
        }
        else if (testRun)
        {
            // (A test run, test.txt, plays the game played last without the list: no one is there.)
            fprintf(stderr, "[switch] test run: %s, without the game list\n", games[chosen].rom.c_str());
        }
        else
        {
            // A game chosen from the list is played in a fresh start of the program: the list
            // leaves something behind in the graphics (not the window's settings: those put back
            // made no difference) that slowed the game drawn after it, putting each picture
            // together taking HeartGold 12 ms a frame on the Switch instead of 4, and a start
            // after it was as fast as ever (test runs, "arg --show-list"). So the choice is noted
            // in last-game.txt and start-chosen, the program starts again (as QUIT does to show
            // the list), and that start plays the game without showing the list.
            const std::string chosenMarker = dir + "/start-chosen";
            struct stat ms;
            const bool alreadyChosen = stat(chosenMarker.c_str(), &ms) == 0;
            if (alreadyChosen) remove(chosenMarker.c_str());
            else
            {
                // Each game's picture, if the player has one (Launcher::FindArt).
                for (Launcher::Game& g : games) Launcher::FindArt(g, dir);
                // A game with no picture of its own takes one another copy of it has (the icon of
                // its one-game program, say).
#ifdef SSR_PC_ART
                // Else the box art built into the program (before another copy's picture, which is
                // its one-game program's icon: that can be a placeholder).
                for (Launcher::Game& g : games)
                {
                    size_t size = 0;
                    if (g.art.empty())
                        if (const unsigned char* data = PcLauncher::BuiltInArt(g.code.c_str(), size)) Launcher::LoadArt(data, size, g.art);
                }
#endif
                for (Launcher::Game& g : games)
                    for (const Launcher::Game& other : games)
                        if (g.art.empty() && !other.art.empty() && other.code == g.code) g.art = other.art;
#ifdef SSR_UPDATER
                Updater::Init(dir, dir, g_self);
#endif
                chosen = PickGame(games, chosen, SettingsPath(dir));
#ifdef SSR_UPDATER
                // (Installed: the new .nro started as this one ends.)
                if (chosen < 0 && Updater::RestartPending()) Updater::Restart();
#endif
                if (chosen < 0) { ExitTrace("the list closed with no game"); return false; }
                if (FILE* f = fopen(lastPath.c_str(), "w"))
                {
                    fprintf(f, "%s\n", games[chosen].rom.c_str());
                    fclose(f);
                }
                if (games[chosen].platform == Launcher::N3DS) return Start3ds(games[chosen]);
                if (!g_self.empty() && envHasNextLoad())
                {
                    if (FILE* f = fopen(chosenMarker.c_str(), "wb")) fclose(f);
                    const std::string cmd = g_self.find(' ') == std::string::npos ? g_self : "\"" + g_self + "\"";
                    const Result rc = envSetNextLoad(g_self.c_str(), cmd.c_str());
                    fprintf(stderr, "[switch] %s chosen: the program starts again to play it (0x%x)\n",
                        games[chosen].rom.c_str(), (unsigned)rc);
                    if (R_SUCCEEDED(rc))
                    {
                        // The loader reuses this process. Close the updater's sockets
                        // now, while curl and all services are still alive, rather than
                        // relying on an atexit registration that may have been refused.
                        ExitTrace("handoff: closing network");
                        Achievements::ShutdownNetwork();
                        fprintf(stderr, "[launch] network closed; returning to loader\n");
                        ExitTrace("handoff: returning to loader");
                        return false;
                    }
                    remove(chosenMarker.c_str());
                }
            }
            g_canChange = true;
        }
    }
#endif
    if (games[chosen].platform == Launcher::N3DS) return Start3ds(games[chosen]);
#ifdef SSR_GBA
    // A GBA game (chosen in the list, which started the program again for it, or from its HOME
    // Menu icon): played here, Pokemon GBA Widescreen built in; then the program starts again for
    // the list, as QUIT does for a DS game.
    if (games[chosen].platform == Launcher::GBA)
    {
        const Launcher::Game& game = games[chosen];
        GbaWide::Options o;
        o.roms = game.rom.substr(0, game.rom.find_last_of('/'));
        o.saves = game.folder;
        o.data = dir;
        o.fullscreen = true;
        Launcher::SyncGbaSettings(SettingsPath(dir), o.data);
        fprintf(stderr, "[switch] GBA: %s, saves in %s\n", game.rom.c_str(), game.folder.c_str());
        // (Cloud saves: a newer save comes down first; the save goes up when the game closes.)
        const std::string cfgAt = SettingsPath(dir);
        CloudAuto::Start(cfgAt.substr(0, cfgAt.find_last_of('/')), Launcher::HubSavePath(game)); // (the save the game reads: its saves folder)
        const u64 gbaStart = armGetSystemTick();
        GbaRa::Play(game.rom, o, SettingsPath(dir));
        PlayTime::Add(cfgAt.substr(0, cfgAt.find_last_of('/')), (long long)(armTicksToNs(armGetSystemTick() - gbaStart) / 1000000000ULL));
        CloudAuto::Saved();
        CloudAuto::Finish();
        if (!g_self.empty() && envHasNextLoad())
        {
            const std::string cmd = g_self.find(' ') == std::string::npos ? g_self : "\"" + g_self + "\"";
            envSetNextLoad(g_self.c_str(), cmd.c_str());
        }
        return false;
    }
#endif
    // (A ROM not yet checked is, now: a hack plays with a save of its own.)
    const u64 checkStart = armGetSystemTick();
    fprintf(stderr, "[launch] ROM check begin: code %s, hack %d\n", games[chosen].code.c_str(), (int)games[chosen].hack);
    Launcher::ConfirmHack(games, (size_t)chosen, dir + "/saves", SavePathFor);
    fprintf(stderr, "[launch] ROM check complete: hack %d, %llu ms\n", (int)games[chosen].hack,
        (unsigned long long)(armTicksToNs(armGetSystemTick() - checkStart) / 1000000));
    const Launcher::Game& game = games[chosen];
    const std::string rom = game.rom, gameDir = game.folder;
    char code[5] = {};
    memcpy(code, game.code.data(), std::min<size_t>(4, game.code.size()));
    // SoulSilver and HeartGold (US): the same program, layouts from its own tables.
    const bool heartSilver = IsGame(code, "IPGE") || IsGame(code, "IPKE");
    // The save is named after the game (SoulSilver's name is the one earlier builds used), in
    // the saves folder of the game's own folder.
    fprintf(stderr, "[switch] %s (game code %s), save %s%s\n", rom.c_str(), code[0] ? code : "?",
        SavePath(game).c_str(), ownFolder ? "" : " (from another folder)");
    // The settings: settings.cfg beside the game (ssrecomp.cfg before; an old one is renamed).
    const std::string cfgPath = SettingsPath(dir);
    g_args = { argc > 0 && argv[0] ? argv[0] : "pokemon-ds-wide", rom, "--save", SavePath(game), "--fullscreen",
               "--config", cfgPath };
    if (game.hack) g_args.push_back("--rom-hack"); // (says once that widescreen is tried on it)
    // (Play From Slot, Manage Saves: the game starts from that state.)
    if (!Launcher::PlayState().empty() && game.platform == Launcher::DS && StateSlots::Exists(Launcher::PlayState())) { g_args.push_back("--state-load"); g_args.push_back(Launcher::PlayState()); }
    Launcher::PlayState().clear();
    // A game that is not recompiled (Black, White, Black 2, White 2) comes with its function table, made on the
    // PC by tools/bw2/funcmap.py or tools/bw1/funcmap.py; its layouts follow the functions that
    // run. SoulSilver and HeartGold need none (their layouts come from their own tables, built into the program)
    // and must not be given one: a function table switches to the Black games' layouts.
    // A funcs.txt beside the game is used first (made from the player's ROM by
    // tools/switch/build_game.sh); otherwise the shared build's own, one per version in its
    // romfs (tools/switch/build_game.sh bw), whose overlay bytes are read from the ROM.
    struct stat st;
    std::string funcsPath;
    if (!heartSilver)
    {
        const std::string own = gameDir + "/funcs.txt", builtIn = std::string("romfs:/funcs/") + code + ".txt";
        if (stat(own.c_str(), &st) == 0) funcsPath = own;
        else if (RomfsReady() && stat("romfs:/funcs", &st) == 0)
        {
            if (stat(builtIn.c_str(), &st) == 0) funcsPath = builtIn;
            else
            {
                ShowMessage(std::string("  ") + rom.substr(rom.find_last_of('/') + 1) + " is a game this program does not know\n"
                            "  (game code " + (code[0] ? code : "?") + ").\n\n"
                            "  It plays the English versions of:\n"
                            "    Pokemon Black 2 (IREO), Pokemon White 2 (IRDO), Pokemon Black (IRBO)\n"
                            "    and Pokemon White (IRAO), US and Europe;\n"
                            "    Pokemon SoulSilver (IPGE) and Pokemon HeartGold (IPKE), US, with the\n"
                            "    SoulSilver and HeartGold download;\n"
                            "    Pokemon Platinum (CPUE), with the Platinum download;\n"
                            "    Pokemon Diamond (ADAE) and Pearl (APAE);\n"
                            "    and in Spanish: Black (IRBS), White (IRAS), Black 2 (IRES), White 2 (IRDS),\n"
                            "    Platinum (CPUS), SoulSilver (IPGS), HeartGold (IPKS) and Diamond (ADAS);\n"
                            "    Diamond also in German and Italian, Pearl in French, Spanish, German and Korean.\n"
                            "  Other languages put the game's code in other places.");
                return false;
            }
        }
    }
    const bool funcs = !funcsPath.empty();
    if (funcs)
    {
        fprintf(stderr, "[switch] function table %s\n", funcsPath.c_str());
        g_args.push_back("--funcs");
        g_args.push_back(funcsPath);
    }
    // Black and Black 2 run on melonDS's JIT or its interpreter; so do SoulSilver and HeartGold in a build
    // with the JIT (tools/switch/build.sh with SSR_JIT=ON: about 1.7 times as fast as its
    // recompiled code; a build without the JIT runs the recompiled code, given neither option).
#ifdef JIT_ENABLED
    if (funcs || heartSilver)
    {
        // melonDS's JIT (code translated for the Switch's processor as it runs), unless the
        // L is held while the game starts, or the last start with the JIT never got going (a
        // file made before the start and removed a few seconds into the game says so).
        // Fast memory (the DS's memory mapped where the JIT's code reads it directly, see
        // melonDS/src/ARMJIT_Memory.cpp) likewise: if a start with it did not get going, the
        // next start goes without it, and the one after tries it again.
        bool jit = true, fastMem = true;
        if (FILE* f = fopen(cfgPath.c_str(), "r"))
        {
            char line[128];
            while (fgets(line, sizeof(line), f))
            {
                // (FAST JIT always: "jit=" and "fastmem=" from older versions' EMULATION
                // menu are not read any more, so no one is left on the slower choices.)
                // The widescreen drawing threads on core 0 too (melonDS/src/SsrThreads.h).
                if (!strncmp(line, "wide_core0=", 11) && line[11] == '0') setenv("SSR_WIDE_CORE0", "0", 1);
            }
            fclose(f);
        }
        PadState pad;
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&pad);
        padUpdate(&pad);
        if (jit && (padGetButtons(&pad) & HidNpadButton_L))
        {
            jit = false;
            fprintf(stderr, "[switch] L held: the interpreter this time\n");
        }
        const std::string marker = dir + "/jit-starting", fastMemMarker = dir + "/fastmem-starting";
        if (stat(fastMemMarker.c_str(), &st) == 0)
        {
            // (Fast memory is the likelier cause, so the JIT stays on this time.)
            remove(fastMemMarker.c_str());
            remove(marker.c_str());
            if (jit && fastMem)
            {
                fastMem = false;
                fprintf(stderr, "[switch] the last start with fast memory did not get going: "
                    "the JIT without it this time\n");
            }
        }
        if (jit && stat(marker.c_str(), &st) == 0)
        {
            jit = false;
            remove(marker.c_str());
            fprintf(stderr, "[switch] the last start with the JIT did not get going: the interpreter this time\n");
        }
        if (jit)
        {
            if (FILE* f = fopen(marker.c_str(), "wb")) fclose(f);
            g_jitMarker = marker;
            if (fastMem)
            {
                if (FILE* f = fopen(fastMemMarker.c_str(), "wb")) fclose(f);
                g_fastMemMarker = fastMemMarker;
            }
        }
        // (Read by the core, melonDS/src/ARMJIT_Memory.cpp, and by MakeConsole.)
        if (!fastMem) setenv("SSR_JIT_NOFASTMEM", "1", 1);
        g_args.push_back(jit ? "--jit" : "--interp");
    }
#else
    if (funcs) g_args.push_back("--interp");
#endif
    if (testRun)
    {
        g_args.insert(g_args.end(), testArgs.begin(), testArgs.end());
        // Each frame is shown alongside the next one's emulation, as in play (see
        // "pipelined" in main.cpp), pictures or not.
        setenv("SSR_PIPELINE", "1", 1);
    }
    for (auto& a : g_args) g_argv.push_back(a.data());
    g_argv.push_back(nullptr);
    argc = (int)g_args.size();
    argv = g_argv.data();
    fprintf(stderr, "[launch] emulator arguments ready\n");
    return true;
}

bool CanChangeGame() { return g_canChange && envHasNextLoad(); }
void ChangeGame() { g_changeGame = true; }

bool LocalTime(int& year, int& month, int& day, int& hour, int& minute, int& second)
{
    // newlib's localtime() knows no time zone here and gives UTC; the game lights the world
    // by the hour, so the console's own zone is asked for.
    u64 now = 0;
    if (R_FAILED(timeGetCurrentTime(TimeType_UserSystemClock, &now))) return false;
    TimeCalendarTime cal;
    TimeCalendarAdditionalInfo info;
    if (R_FAILED(timeToCalendarTimeWithMyRule(now, &cal, &info))) return false;
    year = cal.year;
    month = cal.month;
    day = cal.day;
    hour = cal.hour;
    minute = cal.minute;
    second = cal.second;
    return true;
}

static Thread g_emuThread;

bool StartThread(void (*entry)(void*), void* arg)
{
    if (R_FAILED(threadCreate(&g_emuThread, entry, arg, nullptr, 0x400000, 0x2C, 0))) return false;
    if (R_FAILED(threadStart(&g_emuThread)))
    {
        threadClose(&g_emuThread);
        return false;
    }
    return true;
}

void JoinThread()
{
    threadWaitForExit(&g_emuThread);
    threadClose(&g_emuThread);
}

void MoveMainThread()
{
    const Result rc = svcSetThreadCoreMask(CUR_THREAD_HANDLE, 2, 1 << 2);
    fprintf(stderr, "[switch] main thread to core 2: 0x%x\n", (unsigned)rc);
}

void JitStarted()
{
    if (!g_fastMemMarker.empty())
    {
        remove(g_fastMemMarker.c_str());
        g_fastMemMarker.clear();
    }
    if (g_jitMarker.empty()) return;
    remove(g_jitMarker.c_str());
    g_jitMarker.clear();
    fprintf(stderr, "[switch] the JIT is running\n");
}

std::string Status()
{
    // Test runs only, one session at a time, each closed straight after. The clocks come from
    // the system's pcv service and the temperatures from ptm, and pcv stops the whole console
    // when it has no room for another session (fatal 2011-0102, "out of session memory"; sys-clk
    // opens its own several times a second). That happened twice, both times a few seconds into
    // a game, at this line's first reading, when the three clock sessions were opened and kept
    // for the whole game. A player's log does without these numbers.
    if (!g_test.active) return {};
    char buf[128];
    int n = 0;
    buf[0] = 0;
    u32 mhz[3] = {};
    bool clocks = hosversionAtLeast(8, 0, 0) && R_SUCCEEDED(clkrstInitialize());
    if (clocks)
    {
        const PcvModuleId modules[3] = { PcvModuleId_CpuBus, PcvModuleId_GPU, PcvModuleId_EMC };
        for (int i = 0; i < 3 && clocks; i++)
        {
            ClkrstSession s;
            clocks = R_SUCCEEDED(clkrstOpenSession(&s, modules[i], 3));
            if (!clocks) break;
            clocks = R_SUCCEEDED(clkrstGetClockRate(&s, &mhz[i]));
            clkrstCloseSession(&s);
        }
        clkrstExit();
    }
    if (clocks)
        n += snprintf(buf + n, sizeof(buf) - n, "cpu %u gpu %u mem %u MHz", mhz[0] / 1000000, mhz[1] / 1000000, mhz[2] / 1000000);
    if (hosversionAtLeast(10, 0, 0) && R_SUCCEEDED(tsInitialize()))
    {
        const TsDeviceCode places[2] = { TsDeviceCode_LocationExternal, TsDeviceCode_LocationInternal };
        float temp[2] = {};
        bool ok = true;
        for (int i = 0; i < 2 && ok; i++)
        {
            TsSession s;
            ok = R_SUCCEEDED(tsOpenSession(&s, places[i]));
            if (!ok) break;
            ok = R_SUCCEEDED(tsSessionGetTemperature(&s, &temp[i]));
            tsSessionClose(&s);
        }
        if (ok) n += snprintf(buf + n, sizeof(buf) - n, "%schip %.0fC board %.0fC", n ? ", " : "", temp[0], temp[1]);
        tsExit();
    }
    return buf;
}

// The Switch's own keyboard (the system's software keyboard, full screen): the text typed, or false
// if it was closed without. (SDL's text input brought the console down on SEARCH.)
bool Keyboard(const char* guide, bool password, std::string& out)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return false;
    if (password) swkbdConfigMakePresetPassword(&kbd);
    else swkbdConfigMakePresetDefault(&kbd);
    if (guide && *guide) swkbdConfigSetGuideText(&kbd, guide);
    swkbdConfigSetStringLenMax(&kbd, 128);
    char text[512] = {};
    const Result rc = swkbdShow(&kbd, text, sizeof(text));
    swkbdClose(&kbd);
    fprintf(stderr, "[switch] keyboard: 0x%x\n", (unsigned)rc);
    if (R_FAILED(rc)) return false;
    out = text;
    return true;
}

void ForceExit()
{
    fflush(stderr);
    svcExitProcess();
    for (;;) {}
}

extern "C" void SsrJitRelease();

void Finish()
{
    JitStarted(); // (an ordinary exit: the JIT did start)
    SsrJitRelease(); // (the JIT's code memory given back: without that the Homebrew Menu cannot start the game list again)
    // (A game chosen from the list: its own closing down traced, in exit-trace-game.txt beside the program.)
    if (g_changeGame && !g_self.empty())
    {
        const size_t sl = g_self.find_last_of('/');
        if (sl != std::string::npos)
        {
            snprintf(g_exitTrace, sizeof g_exitTrace, "%s/exit-trace-game.txt", g_self.substr(0, sl).c_str());
            if (FILE* f = fopen(g_exitTrace, "w")) fclose(f);
            ExitTrace("game: finish");
        }
    }
    // QUIT in the pause menu, for a game chosen from the game list: the Homebrew Menu's loader
    // starts the program again, which shows the list.
    if (g_changeGame && !g_self.empty())
    {
        const std::string cmd = g_self.find(' ') == std::string::npos ? g_self : "\"" + g_self + "\"";
        const Result rc = envSetNextLoad(g_self.c_str(), cmd.c_str());
        fprintf(stderr, "[switch] back to the game list: 0x%x\n", (unsigned)rc);
    }
    fflush(stderr);
    if (TestRun::Finish(g_test))
    {
        // The next run: the Homebrew Menu's loader starts the .nro again once this one ends
        // (if it cannot, starting it again by hand does the same).
        if (!g_self.empty() && envHasNextLoad())
        {
            const std::string cmd = g_self.find(' ') == std::string::npos ? g_self : "\"" + g_self + "\"";
            const Result rc = envSetNextLoad(g_self.c_str(), cmd.c_str());
            fprintf(stderr, "[test] starting again for the next run: 0x%x\n", (unsigned)rc);
        }
        else fprintf(stderr, "[test] start the game again for the next run\n");
        fflush(stderr);
    }
}
}
#endif

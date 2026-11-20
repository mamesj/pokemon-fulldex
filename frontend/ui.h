// Software-drawn overlay: pause menu, touch cursor and status messages.
// Drawn into an ARGB buffer the size of the output, so the window and the
// headless screenshots show the same thing.
#pragma once
#include <array>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ui_text.h"
#include "ui_input.h"

// UI SIZE (settings: ui_size): how large the menus and overlays are, against what the screen alone
// makes them. Everything is drawn in proportion to the screen's lines, which on a handheld's small
// screen (640 x 480 at 3.5 inches) made the writing a millimetre high.
//
// For any piece of the interface, larger menus are:
//   UiScale()   the factor; multiply a size that follows the screen by it (1 unless UI SIZE says so).
//   UiLarge()   above 1: for a layout that has to change, not only grow (a panel filling the screen).
//   UiMinPx(px) a size of writing, never under 13 pixels with larger menus (the least that reads).
// At 1 (UiLarge() false) everything is drawn exactly as before.
#include "ssr_os.h"
inline float& UiScale() { static float s = 1.0f; return s; } // (set by UiScaleFor, from the settings)
inline bool UiLarge() { return UiScale() > 1.0f; }
inline int UiMinPx(int px) { return UiLarge() ? std::max(px, 13) : px; }
// ui_size's value: "auto" or a percentage from 50 to 300. Auto: 1.5 on a handheld's small screen,
// under 720 lines, else 1. Phones and the Switch: always 1, as they size their menus for their own
// screens already (PhoneUi, touchRows); a percentage is the player's own choice there too.
// screenLines: the display's own height, not the window's, so a small window on a large screen
// stays as it was.
inline float UiScaleFor(const std::string& v, int screenLines)
{
    if (v.empty() || v == "auto")
    {
#if defined(SSR_PHONE) || defined(__SWITCH__)
        (void)screenLines;
        return 1.0f;
#else
        return screenLines > 0 && screenLines < 720 ? 1.5f : 1.0f;
#endif
    }
    const int p = atoi(v.c_str());
    return p >= 50 && p <= 300 ? p / 100.0f : 1.0f;
}

struct Canvas
{
    int w = 0, h = 0;
    std::vector<uint32_t> px; // ARGB; alpha 0 means "show the game underneath"
    // The paused game blurred (glass.h, Glass::MakeBackdrop) while the menu is open; empty otherwise.
    std::vector<uint32_t> backdrop;
    // (Which backdrop this is, from a few of its pixels: a new one can be given the same memory as
    // the last, so its address does not tell them apart. For drawings kept while it stays.)
    uint64_t BackdropSig() const
    {
        uint64_t h = backdrop.size() * 1099511628211ull;
        if (!backdrop.empty())
            for (size_t i = 0; i < 97; i++) h = (h ^ backdrop[(i * 2654435761u) % backdrop.size()]) * 1099511628211ull;
        return h;
    }
    // The same moment unblurred (w x h), and the DS's two screens (256 x 192 each, top then bottom):
    // the pictures in a settings page's preview (ui_settings.h) are made from them. Empty when
    // there is nothing to read (the preview then draws plain shapes).
    std::vector<uint32_t> sharp, dsTop, dsBottom;
    // Where the top screen was shown in `sharp` (output pixels; w 0: not shown), so a part of it can
    // be set beside the same part of dsTop (3D RESOLUTION's preview: the DS's own against now).
    int baseX = 0, baseY = 0, baseW = 0, baseH = 0;

    void Resize(int nw, int nh)
    {
        if (nw != w || nh != h)
        {
            w = nw;
            h = nh;
            px.assign((size_t)w * h, 0);
            x0d = y0d = 0;
            x1d = w;
            y1d = h;
        }
    }
    // The rectangle that has been drawn into since the last Clear. Only that part is
    // cleared and sent to the screen: the canvas is as large as the window.
    int x0d = 0, y0d = 0, x1d = 0, y1d = 0;
    bool Dirty() const { return x1d > x0d && y1d > y0d; }
    void Clear()
    {
        for (int yy = y0d; yy < y1d; yy++)
            std::fill(px.begin() + (size_t)yy * w + x0d, px.begin() + (size_t)yy * w + x1d, 0);
        x0d = w;
        y0d = h;
        x1d = y1d = 0;
    }

    void Fill(int x, int y, int rw, int rh, uint32_t argb)
    {
        int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
        int x1 = x + rw > w ? w : x + rw, y1 = y + rh > h ? h : y + rh;
        if (x1 <= x0 || y1 <= y0) return;
        if (x0 < x0d) x0d = x0;
        if (y0 < y0d) y0d = y0;
        if (x1 > x1d) x1d = x1;
        if (y1 > y1d) y1d = y1;
        for (int yy = y0; yy < y1; yy++)
            for (int xx = x0; xx < x1; xx++)
                px[(size_t)yy * w + xx] = argb;
    }

    // 5x7 glyphs, one row per byte, bit 4 is the leftmost pixel.
    static const uint8_t* Glyph(char c)
    {
        static const uint8_t font[][7] = {
            { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, // A
            { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E }, // B
            { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E }, // C
            { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E }, // D
            { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F }, // E
            { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 }, // F
            { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F }, // G
            { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, // H
            { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E }, // I
            { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C }, // J
            { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 }, // K
            { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F }, // L
            { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 }, // M
            { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 }, // N
            { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, // O
            { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 }, // P
            { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D }, // Q
            { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 }, // R
            { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E }, // S
            { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, // T
            { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, // U
            { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 }, // V
            { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 }, // W
            { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 }, // X
            { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 }, // Y
            { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F }, // Z
            { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, // 0
            { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E }, // 1
            { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, // 2
            { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E }, // 3
            { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, // 4
            { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E }, // 5
            { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, // 6
            { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }, // 7
            { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, // 8
            { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C }, // 9
            { 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00 }, // :
            { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 }, // -
            { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 }, // +
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06 }, // .
            { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 }, // /
            { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 }, // <
            { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 }, // >
            { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 }, // !
            { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 }, // (
            { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 }, // )
            // PlayStation's button symbols (controls.h, PS_*): cross, circle, square, triangle. Smaller
            // than the letters, so the cross and the circle are not taken for an X and an O.
            { 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00 },
            { 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00 },
            { 0x00, 0x1F, 0x11, 0x11, 0x11, 0x1F, 0x00 },
            { 0x00, 0x04, 0x0A, 0x0A, 0x11, 0x1F, 0x00 },
            { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 }, // % (VOLUME)
        };
        if (c == '\'') { static const uint8_t apostrophe[] = { 4, 4, 8, 0, 0, 0, 0 }; return apostrophe; }
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= 'A' && c <= 'Z') return font[c - 'A'];
        if (c >= '0' && c <= '9') return font[26 + c - '0'];
        static const char punct[] = ":-+./<>!()\x01\x02\x03\x04%";
        const char* p = c ? strchr(punct, c) : nullptr;
        return p ? font[36 + (p - punct)] : nullptr;
    }

    // (In the interface's language, UiLang: its UTF-8 letters are one character each.)
    static int TextWidth(const std::string& s, int scale)
    {
        return UiLang::Units(UiLang::Tr(s)) * scale;
    }

    static int TextWidthRaw(const std::string& text,int scale) { return UiLang::Units(text)*scale; }
    void TextRaw(int x,int y,const std::string& text,int scale,uint32_t argb) { UiText::DrawRaw(*this,x,y,text,scale,argb); }
    void Text(int x,int y,const std::string& text,int scale,uint32_t argb)
    {
        UiText::Draw(*this,x,y,text,scale,argb);
    }

};

// The accent colours of COLOUR (the game list's settings, GAME LIST): the game list's lines and
// frame (launcher.h, ThemeNamed) and the pause menu's chosen row, its box and text shaded from it.
inline uint32_t AccentNamed(const std::string& name)
{
    static const struct { const char* name; uint32_t argb; } colours[] = {
        { "red", 0xFFE0242C }, { "blue", 0xFF2A7BE0 }, { "green", 0xFF2EB85C }, { "purple", 0xFF9B4DE0 },
        { "gold", 0xFFE0A82C }, { "pink", 0xFFE0479A }, { "teal", 0xFF1FB5A8 },
    };
    for (const auto& c : colours)
        if (name == c.name) return c.argb;
    return colours[0].argb;
}

#include "glass.h"
#include "art_smooth.h"
#include "ui_clean_font.h"

// Button presses the menu understands, edge-triggered.
// (Where a settings page's drawing time goes, for the log's [menu] line: the panel, the header,
// the rows, the preview, the choices, the rest.)
inline double* MenuProf() { static double p[8] = {}; return p; }
inline double MenuProfNow() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct MenuInput
{
    bool up = false, down = false, left = false, right = false, accept = false, back = false;
};

struct MenuItem
{
    std::string label;
    std::string value; // shown right-aligned; empty for plain actions
    bool info = false; // the value is only shown (no < > around it): nothing to change here
    int slider = -1;   // 0 to 10: also drawn as a bar of ten blocks, that many lit (VOLUME)
    int icon = -1;     // Glass::Icon, for a tile (Menu::columns)
    bool disabled = false; // shown greyed; choosing it does nothing (this game or engine has no such thing)
    // Settings pages (Menu::style 1, ui_settings.h): what the row is (Menu::K_*), its choices as the
    // preview names them and which is on now, the picture drawn for each (Menu::P_*, art: the
    // picture's variant for each choice), and the preview's description and tip.
    int kind = 0;
    std::string sub; // (Menu::tallRows: a short line under the row's name)
    std::vector<std::string> choices;
    std::vector<int> art;
    int choice = -1;
    int preview = -1;
    std::string desc, tip;
    int indent = 0; // (settings pages: a row inside an open folder, K_FOLDER, drawn further in)
    std::vector<uint32_t> thumb; int thumbW = 0, thumbH = 0; // (K_SLOT: the state's picture, ARGB)
    bool noArt = false; // (a row of a page about one Pokemon or item: its icon not drawn in the row again)
    int keyRow = -1, keyCol = 0, keySpan = 2; // (K_KEY: its row of the keyboard, where it starts and how wide, in half keys)
};

struct Menu
{
    bool open = false;
    int selected = 0;
    std::vector<MenuItem> items;
    // 0: a list (every page but one). More: the items are tiles in rows of that many (the pause
    // menu's first page: eight glass tiles, four to a row; two to a row on a portrait screen).
    int columns = 0;
    mutable int drawnColumns = 0; // (as last drawn: Update moves up and down by this many)
    // 1: a settings page (ui_settings.h): a header with a back button, the page's icon and title,
    // rows in sections on the left and a preview of the chosen row on the right.
    int style = 0;
    int pageIcon = -1;
    enum { K_PLAIN, K_TOGGLE, K_PICKER, K_LINK, K_SECTION, K_BACK, K_FOLDER, K_SLOT, K_KEY, K_ACTION };
    std::string headerNote; // (settings pages: a few words at the header's right, CHEATS' counts)
    // A preview's pictures (MenuItem::preview), one per choice (its art: which variant).
    enum { P_NONE, P_LAYOUT, P_LARGE, P_SECOND, P_ASPECT, P_FULLSCREEN, P_SMOOTH, P_BATTLE, P_FPS, P_COMPANION, P_PARTY, P_FILTER, P_MOTION, P_WIDE, P_RES, P_PAD, P_KEY, P_SHORTCUTS, P_TOUCH, P_CPARTY, P_CBAG, P_CDEX, P_CPLACE, P_CBATTLE, P_CMON, P_CITEM, P_CDEXMON, P_CGUIDE, P_CBFOE, P_CBMOVE, P_CBCOVER, P_CHEATNOTE };
    // P_PAD and P_KEY (CONTROLS): the controller drawn in its maker's layout (padStyle: Controls::Style;
    // a choice's art is the input lit, Controls::IN_*), the inputs held now lit too (padLive, a bit
    // per input), or a keyboard's key.
    int padStyle = 0;
    uint32_t padLive = 0;
    float padAxes[6] = {}; // (the sticks, -1 to 1: left x, y, right x, y; the triggers, 0 to 1: left, right)
    // (P_PAD: what each input does, Controls::IN_* order, drawn beside it with a line to it, as
    // KEYS AND BUTTONS; empty: no labels.) And how much of the panel the rows take (wide screens).
    std::vector<std::string> padLabels;
    std::string padNote; // (under the labelled controller: how else the menu opens, START + SELECT)
    float listShare = 0.605f;
    // (Settings pages inside the launcher's own page: the whole area, the list's width and where the
    // preview starts; no panel, header or hint of their own. laneX -1: the usual panel.)
    int laneX = -1, laneY = 0, laneW = 0, laneH = 0, laneListW = 0, lanePvX = 0, laneListH = 0, lanePvY = 0; // (laneListH > 0: a tall window, the preview under the rows)
    float touchRows = 1.0f; // (phones: the settings rows this much larger, for a finger)
    float laneK = 0; // (the launcher's settings page on a phone: its own size for the page, 0 for the usual)
    bool freeScroll = false; // (a finger has scrolled the list: the chosen row need not be in view)
    bool tallRows = false;
    float rowTall = 1.0f; // (a settings page's rows this many times as high, their text and icons a little larger: BAG)
    // COMPANION's previews (P_CPARTY, P_CBAG, P_CDEX, P_CPLACE): what the game holds now, read by
    // the companion (companion_data.h); icons and the map from the player's own ROM, ARGB.
    struct CompanionView
    {
        bool battleMapped = true; // (false: this game's battles are not read, Companion::BattleMapped)
        struct Mon
        {
            std::string name, item; int level = 0, hp = 0, maxHp = 0, status = 0; bool egg = false; std::vector<uint32_t> icon; int iconW = 0, iconH = 0;
            // (PARTY's summary, P_CMON: its types (CompanionTypes numbers, -1 none), nature, ability,
            // stats with IVs and EVs (HP, Attack, Defence, Speed, Sp. Atk, Sp. Def), moves.)
            int type1 = -1, type2 = -1;
            std::string nature, ability;
            int stats[6] = {}, iv[6] = {}, ev[6] = {};
            struct Move { std::string name; int type = -1, pp = 0, maxPp = 0; };
            std::vector<Move> moves;
            std::vector<std::string> upcoming; // (moves it learns next: "Lv 40 Flamethrower")
        };
        std::vector<Mon> party;
        struct Pocket { std::string name; int stacks = 0, total = 0; };
        std::vector<Pocket> pockets;
        // (BAG's items in the pocket shown, P_CITEM: what the game says of each, how many, and
        // which of the party hold one.)
        struct BagItem { std::string name, desc, pocket; int quantity = 0; std::vector<int> heldBy; std::vector<uint32_t> icon; }; // (icon: 32 x 32 from the ROM, or none)
        std::vector<BagItem> bag;
        // (POKEDEX's entries as listed, P_CDEXMON: number, types, seen or caught, the Pokedex's
        // words, base stats, abilities, how it evolves, its icon from the ROM.)
        struct DexEntry
        {
            int id = 0, type1 = -1, type2 = -1, base[6] = {}; bool seen = false, caught = false, hidden = false; // (hidden: not seen, not yet shown)
            bool stateKnown = true; // (false: whether seen or caught is not known (the GBA games'), so not said)
            std::string name, desc; std::vector<std::string> abilities, evolutions; std::vector<uint32_t> icon;
            struct Learn { int level = 0, type = -1; std::string move, label; };
            std::vector<Learn> learns; // (moves it learns by level, in order)
            // (Its own page, P_CDEXMON on PAGE_CDEXDETAIL: the battle picture from the ROM (ARGB,
            // normal and shiny), its details as label and value, what hits it hard or not, the
            // machines it can use (label "TM06") and its egg moves; companion_dex.h.)
            std::vector<uint32_t> pic, shinyPic; int picW = 0, picH = 0;
            std::vector<std::pair<std::string, std::string>> facts;
            struct Weakness { int pct = 100; std::vector<int> types; };
            std::vector<Weakness> weak; // (4x, 2x, half, quarter, none; 1x left out)
            std::vector<Learn> machines, eggs, tutors;
            std::vector<std::pair<std::string, std::string>> found; // (where to find it: place, how and levels)
        };
        std::vector<DexEntry> dex;
        // (GUIDE's answers, P_CGUIDE: each shown once chosen with Enter or a tap.)
        // (scroll: the first line shown in the preview; scrollMax: the last one it can start at, set
        // by the preview as it draws, so Left/Right in the Guide knows where the answer ends.)
        struct GuideAnswer { std::string text; bool shown = false; int scroll = 0; mutable int scrollMax = 0; };
        std::vector<GuideAnswer> guide;
        bool dexMoves = false;
        // (BATTLE INFO, in a battle: the foe (P_CBFOE: its types, ability, item, HP and what hits it
        // hard), your Pokemon's moves against it (P_CBMOVE) and how each of your party matches up
        // (P_CBCOVER: its best move against the foe, the foe's worst known move against it).
        // Percentages as CompanionTypes': 200 twice as strong; -1 not known.)
        Mon foe;
        std::string mine;
        struct BattleMove { std::string name, note; int type = -1, category = -1, power = 0, accuracy = 0, pp = 0, maxPp = 0, percent = -1; };
        std::vector<BattleMove> moves;
        struct Coverage { int party = -1, out = -1, in = -1; };
        std::vector<Coverage> coverage; // (P_CDEXMON shows the moves it learns, not its entry: Enter on a seen one)
        int seen = 0, caught = 0, dexTotal = 0, badges = -1;
        std::string place, area;
        std::vector<uint32_t> map; int mapW = 0, mapH = 0;
        float markerX = -1, markerY = -1; // (the player on the map, in parts of it; -1: not known)
        int balls = -1; // (Poke Balls carried, for the status bar; -1: not known)
        long long money = -1; // (the trainer's money, for the status bar and BAG; -1: not known)
        std::vector<Mon> battle; // (in a battle: the foe first, then yours)
        std::vector<std::string> battleLines; // (how the types match up, CompanionBattle::Lines)
    } companion;
    // (Touch: the preview's choice cards as drawn, each x, y, w, h and the choice it shows; one tapped
    // is pickWanted, which the caller reaches by pressing left or right on the row, a step a frame.)
    mutable std::vector<std::array<int, 5>> pickRects;
    int pickWanted = -1;
    int pickY = 0; // (the tap's y, for SettingsTap)  // (a page of a few links, as SETTINGS: rows sharing the list's height, a line under each name)
    // (Touch/mouse: the visible controls registered by the last draw — row labels (Focus), Off/On
    // pills and preview cards (Set), chevrons (Step), links and Back (Open/Back). The pad path
    // reaches the same changes from the focused row (Menu::UpdateSettings).)
    mutable std::vector<UiInput::Part> parts;
    // P_SHORTCUTS (CONTROLS): Fulldex's four shortcuts, FAST, SWAP, SAVE, LOAD, each with its key
    // and its controller button ("" where there is none). P_TOUCH: the on-screen controls, each
    // a circle (r) or a pill (w, h), in parts of the screen.
    std::vector<std::string> shortcutKeys, shortcutPads;
    struct TouchShape { float x, y, r, w, h; std::string label; };
    std::vector<TouchShape> touchShapes;

    // CLEAN text (on: Noto Sans, ui_clean_font.h, in title case) or the pixel font (the Retro look).
    // Japanese and Korean menus keep the pixel font, which has their letters.
    static inline bool Clean = true;
    static bool UseClean() { return Clean && (UiLang::lang < 5 || CleanFont::Cjk()); }
    static int Px(int scale, bool big = false) { return (big ? 14 : 12) * scale; }
    static int TW(const std::string& s, int scale, bool big = false)
    {
        return UseClean() ? CleanFont::Width(CleanFont::Title(UiLang::Tr(s)), Px(scale, big), big) : Canvas::TextWidth(s, scale);
    }
    // (y: the top of a pixel-font line; clean text sits on the same baseline as its capitals.)
    static void TX(Canvas& c, int x, int y, const std::string& s, int scale, uint32_t col, bool big = false)
    {
        if (UseClean()) CleanFont::Draw(c, x, y + 7 * scale, CleanFont::Title(UiLang::Tr(s)), Px(scale, big), col, big);
        else c.Text(x, y, s, scale, col);
    }
    static void TXRaw(Canvas& c, int x, int y, const std::string& s, int scale, uint32_t col, bool big = false)
    {
        if (UseClean()) CleanFont::Draw(c, x, y + 7 * scale, s, Px(scale, big), col, big);
        else c.TextRaw(x, y, s, scale, col);
    }
    // (Text as it is given: a song's name, not put in title case or translated.)
    static int TWRaw(const std::string& s, int scale, bool big = false)
    {
        return UseClean() ? CleanFont::Width(s, Px(scale, big), big) : Canvas::TextWidth(s, scale);
    }

    // Returns the index of an item that was activated this frame, or -1.
    // dir is -1/+1 when left/right was used on it, 0 for accept.
    int Update(const MenuInput& in, int& dir)
    {
        dir = 0;
        if (!open || items.empty()) return -1;
        int n = (int)items.size();
        if (style == 1) return UpdateSettings(in, dir);
        if (style == 2)
        {
            // A dialog has only its first and last actions; an inherited row must not hide focus.
            if (selected != 0 && selected != n - 1) selected = n - 1;
            // (Two buttons side by side: any direction goes to the other.)
            if (in.left || in.right || in.up || in.down) selected = selected == 0 ? n - 1 : 0;
            return in.accept ? selected : -1;
        }
        if (columns > 0)
        {
            const int columns = drawnColumns > 0 ? drawnColumns : this->columns;
            if (!nowPlaying.shown || nowPlaying.idle) npFocus = -1;
            if (npFocus >= 0)
            {
                // (On the NOW PLAYING bar: left and right between its buttons, up back to the tiles.)
                if (in.left) npFocus = std::max(0, npFocus - 1);
                if (in.right) npFocus = std::min(2, npFocus + 1);
                if (in.up) npFocus = -1;
                if (in.accept) npAction = npFocus;
                return -1;
            }
            if (in.down && nowPlaying.shown && !nowPlaying.idle && selected + columns >= n) { npFocus = 1; return -1; }
            if (in.left) selected = (selected + n - 1) % n;
            if (in.right) selected = (selected + 1) % n;
            if (in.up) selected = (selected + n - columns % n) % n;
            if (in.down) selected = (selected + columns) % n;
            return in.accept && !items[selected].disabled ? selected : -1;
        }
        if (in.up) selected = (selected + n - 1) % n;
        if (in.down) selected = (selected + 1) % n;
        if (in.accept) return items[selected].disabled ? -1 : selected;
        if (in.left && !items[selected].value.empty() && !items[selected].disabled)
        {
            dir = -1;
            return selected;
        }
        if (in.right && !items[selected].value.empty() && !items[selected].disabled)
        {
            dir = 1;
            return selected;
        }
        return -1;
    }

    // Where the box and its rows were last drawn (Draw), in the canvas's pixels, and the first
    // row shown (a list longer than the screen scrolls with the chosen row: CHEATS).
    mutable int drawnX = 0, drawnY = 0, drawnW = 0, drawnH = 0, drawnLineH = 0, drawnFirstY = 0;
    mutable int scroll = 0;
    // Where a slider's bar was drawn (the same on every row): its left edge and one block's step.
    mutable int barX = 0, barStep = 0;
    // Where each tile was drawn (columns > 0): x, y, size.
    mutable std::vector<int> tileX, tileY;
    mutable int tileSize = 0;

    // The value a tap at x on a slider's row stands for (0 to 10: the block tapped and those
    // before it; left of the bar, 0), or -1 for a tap beside the bar.
    int SliderAt(int x) const
    {
        if (barStep <= 0 || x >= barX + 10 * barStep + barStep / 2) return -1;
        if (x < barX) return x >= barX - 2 * barStep ? 0 : -1;
        return std::min(10, (x - barX) / barStep + 1);
    }

    // The row (or tile) at (x, y) of the canvas the menu was drawn on (touch screens): its index,
    // -1 for a point in the box but on no row, -2 for a point outside the box.
    int ItemAt(int x, int y) const
    {
        if (style == 1) return SettingsItemAt(x, y);
        if (drawnW <= 0 || x < drawnX || x >= drawnX + drawnW || y < drawnY || y >= drawnY + drawnH) return -2;
        if (columns > 0)
        {
            for (size_t i = 0; i < tileX.size() && i < items.size(); i++)
                if (x >= tileX[i] && x < tileX[i] + tileSize && y >= tileY[i] && y < tileY[i] + tileSize) return (int)i;
            return -1;
        }
        if (drawnLineH <= 0 || y < drawnFirstY) return -1;
        const int i = scroll + (y - drawnFirstY) / drawnLineH;
        return i < (int)items.size() ? i : -1;
    }

    // Informational values describe a row; they are not left/right settings. A tap
    // activates the row (party details, dated state slots, backup details) like Enter.
    void Tap(int x,int y,MenuInput& input,int& sliderValue)
    {
        if(style!=1&&columns>0&&!nowPlaying.idle){const int b=NowPlayingAt(x,y);if(b>=0){npFocus=-1;npAction=b;return;}}
        if(style==2)
        {
            // (On a button: it is chosen and pressed. Outside the card: CANCEL, as the hint says.)
            for(int b=0;b<2;b++)
                if(dialogW>0&&x>=dialogX[b]&&x<dialogX[b]+dialogW&&y>=dialogY&&y<dialogY+dialogH){selected=b==0?(int)items.size()-1:0;input.accept=true;return;}
            if (cardW > 0 && (x < cardX || x >= cardX + cardW || y < cardY || y >= cardY + cardH)) input.back = true;
            return;
        }
        const int item=ItemAt(x,y);
        if(style==1){pickY=y;SettingsTap(item,x,input);return;}
        if(item==-2)input.back=true;
        else if(item>=0)
        {
            selected=item;
            if(columns>0)input.accept=true;
            else if(items[item].slider>=0 && SliderAt(x)>=0){sliderValue=SliderAt(x);input.accept=true;}
            else if(items[item].value.empty()||items[item].info)input.accept=true;
            else if(x<drawnX+drawnW/3)input.left=true;
            else input.right=true;
        }
    }

    // note: a few lines of text in a strip under the box (CHEATS: what the database says about
    // the chosen cheat), wrapped to the box's width.
    // The accent (COLOUR, AccentNamed): kept for the game list; the glass menu is white on the game.
    static inline uint32_t Accent = 0xFFE0242C;

    // The glass colours (glass.h): panels a little lighter than the blurred game, the chosen row
    // or tile lighter still with a bright edge, text white, hints a soft grey.
    static constexpr uint32_t GlassPanel = 0x2BFFFFFF, GlassEdge = 0x3DFFFFFF;
    static constexpr uint32_t GlassChosen = 0x4AFFFFFF, GlassChosenEdge = 0xF2FFFFFF;
    static inline uint32_t GlassText = 0xFFF2F5F9, GlassHint = 0xFFB9C3CF, GlassOff = 0xFF7E8894; // (the launcher's Text Colour changes them: Look, launcher.h)
    // Lists: a little shade under the glass so white text reads over bright parts of the game,
    // and the chosen row an outline with a faint fill (a bright fill hid its text).
    static constexpr uint32_t GlassShade = 0x4D000000, GlassRow = 0x1FFFFFFF;
    // Frosted glass (Glass::Frost, the blurred game made lighter and more colourful) with a faint
    // white over it and a bright edge; the old grey shade where there is no blurred game to frost.
    static constexpr uint32_t FrostTint = 0x0FFFFFFF, FrostEdge = 0x5CFFFFFF, FrostChosen = 0x2EFFFFFF;
    // over: drawn again over itself as already drawn (Menu::DrawChosenTile): its shadow is there,
    // and its frost made from the game rather than from the frost.
    static void FrostBox(Canvas& c, bool solid, int x, int y, int w, int h, float r, bool chosen = false, float edge = 1.0f, bool over = false)
    {
        // (Raised glass, as the settings pages': a soft shadow, the frost with a deep tint, a lit rim.)
        if (!over) Glass::Shadow(c, x, y, w, h, r, std::max(6.0f, h * 0.06f), 0x70);
        if (solid) Glass::Frost(c, x, y, w, h, r, over ? &c.backdrop : nullptr);
        else Glass::Round(c, x, y, w, h, r, GlassShade);
        if (chosen) Glass::Round(c, x, y, w, h, r, FrostChosen);
        else Glass::Surface(c, 0, x, y, w, h, r, Glass::PanelTint()); // (the launcher's Panels: a colour or a picture)
        if (chosen) Glass::Round(c, x, y, w, h, r, 0, Glass::ChosenEdge(), edge); // (the accent: white unless chosen)
        else Glass::Rim(c, x, y, w, h, r, std::max(1.2f, edge * 1.4f));
    }

    // The first page's tiles (columns > 0): rows of glass squares, each an icon over its name,
    // centred on the blurred game with no panel round them; "FULLDEX" and "PAUSED" above.
    // NOW PLAYING: what the player's own music app is playing (frontend/now_playing.h), shown in
    // a bar under the tiles with its controls; nothing when no music app is playing.
    struct NowPlaying
    {
        bool shown = false, playing = false, canPrev = true, canNext = true;
        bool idle = false; // no music app playing: "No music playing", the buttons dimmed
        bool blind = false; // the buttons work but the song cannot be seen (Android without notification access)
        std::string title, artist, app;
        std::vector<uint32_t> art; int artW = 0, artH = 0; // the album art, ARGB, if the app gives it
    } nowPlaying;
    // The bar's buttons as drawn (previous, play / pause, next), for taps; -1 none.
    mutable int npX[3] = {}, npY = 0, npS = 0;
    int npFocus = -1;  // the bar's button chosen with the pad or keys (down from the tiles), -1: a tile
    int npAction = -1; // a button pressed this frame (0 previous, 1 play / pause, 2 next), for the caller
    int NowPlayingAt(int x, int y) const
    {
        if (!nowPlaying.shown || !npS) return -1;
        for (int i = 0; i < 3; i++)
            if (x >= npX[i] - npS && x < npX[i] + npS && y >= npY - npS && y < npY + npS) return i;
        return -1;
    }

    void DrawNowPlaying(Canvas& c, bool solid, int x, int y, int w, int h, int scale) const
    {
        const bool idle = nowPlaying.idle;
        const float r = h * 0.3f;
        FrostBox(c, solid, x, y, w, h, r);
        // (The album art, or a note on dark glass.)
        const int pad = h / 7, a = h - 2 * pad, ax = x + pad, ay = y + pad;
        Glass::Round(c, ax, ay, a, a, r * 0.6f, 0x400A111C, 0x2EFFFFFF, 1.0f);
        if (!idle && !nowPlaying.art.empty() && nowPlaying.artW > 0 && nowPlaying.artH > 0)
        {
            const float rr = r * 0.6f;
            for (int py = 0; py < a; py++)
                for (int px = 0; px < a; px++)
                {
                    const int X = ax + px, Y = ay + py;
                    if (X < 0 || Y < 0 || X >= c.w || Y >= c.h) continue;
                    // (Its rounded corners: outside them left as it is.)
                    const float qx = std::max(0.0f, std::fabs(px + 0.5f - a * 0.5f) - (a * 0.5f - rr)), qy = std::max(0.0f, std::fabs(py + 0.5f - a * 0.5f) - (a * 0.5f - rr));
                    const float cover = std::clamp(rr + 0.5f - Glass::Hyp(qx, qy), 0.0f, 1.0f);
                    if (cover <= 0) continue;
                    const uint32_t s = ArtSmooth::At(nowPlaying.art, nowPlaying.artW, nowPlaying.artH, px, py, a);
                    c.px[(size_t)Y * c.w + X] = Glass::Over(c.px[(size_t)Y * c.w + X], s | 0xFF000000u, (int)(cover * 255));
                }
            Glass::Mark(c, ax, ay, ax + a, ay + a);
        }
        else Glass::DrawIcon(c, Glass::I_NOTE, ax + a * 0.5f, ay + a * 0.5f, a * 0.3f, idle ? GlassHint : GlassText);
        // (Controls at the right: previous, play / pause, next.)
        const int bs = (int)(h * 0.17f), gap = (int)(h * 0.78f);
        const int cy = y + h / 2, cxN = x + w - pad - bs * 2, cxP = cxN - gap, cxB = cxP - gap;
        npX[0] = cxB; npX[1] = cxP; npX[2] = cxN; npY = cy; npS = (int)(gap * 0.5f);
        // (The button chosen with the pad or keys: a ring round it, as a chosen tile's glow.)
        if (npFocus >= 0)
        {
            const int fx = npX[npFocus], rr = (int)(bs * 2.5f);
            Glass::Glow(c, fx - rr, cy - rr, 2 * rr, 2 * rr, (float)rr, h / 90.0f);
            Glass::Round(c, fx - rr, cy - rr, 2 * rr, 2 * rr, (float)rr, npFocus == 1 ? 0 : 0x2EFFFFFF, 0xFFFFFFFF, std::max(2.0f, h / 34.0f));
        }
        Glass::Round(c, cxP - (int)(bs * 1.9f), cy - (int)(bs * 1.9f), (int)(bs * 3.8f), (int)(bs * 3.8f), bs * 1.9f, idle ? 0x1EFFFFFF : 0xF7FFFFFF, idle ? 0x33FFFFFF : 0, 1.0f);
        Glass::DrawIcon(c, nowPlaying.playing && !idle ? Glass::I_PAUSE : Glass::I_PLAY, cxP + (nowPlaying.playing && !idle ? 0 : bs * 0.12f), cy, bs * 0.9f, idle ? GlassOff : 0xFF0B1018);
        Glass::DrawIcon(c, Glass::I_PREV, cxB, cy, bs * 1.2f, nowPlaying.canPrev && !idle ? GlassText : GlassOff);
        Glass::DrawIcon(c, Glass::I_NEXT, cxN, cy, bs * 1.2f, nowPlaying.canNext && !idle ? GlassText : GlassOff);
        // (The song and who it is by, cut short to fit before the controls; the app beneath, dimmer.
        // Sized from the bar, as the text of a row.)
        const int tx = ax + a + pad, tw = cxB - (int)(bs * 2.4f) - tx;
        const int pxTitle = std::max(8, (int)(h * 0.4f)), pxSub = std::max(7, (int)(h * 0.32f));
        auto width = [&](const std::string& t, int px, bool bold) { return UseClean() ? CleanFont::Width(t, px, bold) : Canvas::TextWidth(t, scale); };
        auto fit = [&](std::string t, int px, bool bold) {
            if (width(t, px, bold) <= tw) return t;
            while (!t.empty() && width(t + "...", px, bold) > tw) { t.pop_back(); while (!t.empty() && (t.back() & 0xC0) == 0x80) t.pop_back(); }
            return t + "...";
        };
        const bool blind = nowPlaying.blind && !idle;
        const std::string line1 = idle ? UiLang::Tr("No music playing") : blind ? UiLang::Tr("Your music") : nowPlaying.title;
        const std::string line2 = idle ? UiLang::Tr("Play something in any music app to control it here")
            : blind ? UiLang::Tr("To see the song: Settings, Sound, Show Song Names")
            : nowPlaying.artist.empty() ? nowPlaying.app : nowPlaying.artist + "  \xC2\xB7  " + nowPlaying.app;
        if (UseClean())
        {
            CleanFont::Draw(c, tx, y + (int)(h * 0.45f), fit(line1, pxTitle, true), pxTitle, GlassText, true);
            CleanFont::Draw(c, tx, y + (int)(h * 0.79f), fit(line2, pxSub, false), pxSub, GlassHint);
        }
        else
        {
            c.TextRaw(tx, y + h / 2 - 9 * scale, fit(line1, 0, true), scale, GlassText);
            c.TextRaw(tx, y + h / 2 + 1 * scale, fit(line2, 0, false), scale, GlassHint);
        }
    }

    // (The page without its highlight, kept: moving between tiles puts it back and draws only the
    // chosen tile over it. Drawn whole each time, the Switch took about 200 ms a press.)
    mutable std::vector<uint32_t> tilesBase;
    mutable std::string tilesBaseKey;
    void DrawTiles(Canvas& c, const std::string& hint) const
    {
        std::string key = std::to_string(c.w) + "x" + std::to_string(c.h) + " " + std::to_string(c.BackdropSig()) + " " + std::to_string(columns) + " " + std::to_string(npFocus) + " " + hint + "|" +
            std::to_string(nowPlaying.shown) + std::to_string(nowPlaying.playing) + std::to_string(nowPlaying.idle) + std::to_string(nowPlaying.blind) +
            std::to_string(nowPlaying.canPrev) + std::to_string(nowPlaying.canNext) + nowPlaying.title + "|" + nowPlaying.artist + "|" + nowPlaying.app + "|" +
            std::to_string(nowPlaying.art.size()) + std::to_string(UiLang::lang);
        for (const auto& it : items) key += "|" + it.label + "|" + it.value + std::to_string(it.icon) + std::to_string(it.disabled);
        const bool reuse = key == tilesBaseKey && tilesBase.size() == c.px.size() && tileX.size() == items.size() && tileSize > 0;
        if (reuse)
        {
            std::copy(tilesBase.begin(), tilesBase.end(), c.px.begin());
            Glass::Mark(c, 0, 0, c.w, c.h);
            DrawChosenTile(c);
            return;
        }
        const bool solid = Glass::Backdrop(c);
        int scale = std::min(c.w, c.h) / 240;
        if (scale < 1) scale = 1;
        // (Two to a row on a portrait screen, so the tiles stay large enough to tap.)
        const int cols = c.h > c.w ? std::min(2, columns) : columns;
        drawnColumns = cols;
        const int n = (int)items.size(), rows = (n + cols - 1) / cols;
        const bool np = nowPlaying.shown;
        // (With the bar the tiles are a little smaller, to make room for it: 0.42 of a tile.)
        const float avW = c.w * 0.86f, avH = c.h * (c.h > c.w ? 0.7f : 0.6f);
        const float barRows = np ? 0.42f + 0.1f : 0.0f;
        const int s = (int)std::min(avW / (cols + 0.1f * (cols - 1)), avH / (rows + 0.1f * (rows - 1) + barRows));
        const int gap = s / 10;
        const int barH = np ? (int)(s * 0.42f) : 0;
        const int gridW = cols * s + (cols - 1) * gap, gridH = rows * s + (rows - 1) * gap;
        const int gx = (c.w - gridW) / 2, gy = (c.h - gridH - (np ? barH + gap : 0)) / 2;
        tileX.assign(n, 0); tileY.assign(n, 0); tileSize = s;
        drawnX = gx - gap; drawnY = gy - gap; drawnW = gridW + 2 * gap; drawnH = gridH + 2 * gap;
        drawnLineH = 0; drawnFirstY = 0; scroll = 0;
        const int headY = gy - 9 * scale - gap;
        TXRaw(c, gx, headY, "Fulldex", scale, GlassText);
        // (One size for every tile's name: the largest at which the longest still fits. Larger
        // menus: clean text a pixel at a time, as in whole steps of the pixel font's scale one long
        // name, as ACHIEVEMENTS, halved them all.)
        int ls = scale;
        for (int i = 0; i < n; i++)
            while (ls > 1 && TW(items[i].label, ls, true) > s * 0.88f) ls--;
        tilesLabelPx = 0;
        if (UseClean() && UiLarge())
        {
            int lp = Px(scale, true);
            for (int i = 0; i < n; i++)
                while (lp > Px(1, true) && CleanFont::Width(CleanFont::Title(UiLang::Tr(items[i].label)), lp, true) > s * 0.88f) lp--;
            tilesLabelPx = lp;
        }
        TX(c, gx + gridW - TW("PAUSED", scale), headY, "PAUSED", scale, GlassHint);
        for (int i = 0; i < n; i++)
        {
            // (A last row that isn't full is centred under the others.)
            const int inRow = i / cols == rows - 1 ? n - (rows - 1) * cols : cols;
            const int x = gx + (cols - inRow) * (s + gap) / 2 + (i % cols) * (s + gap), y = gy + (i / cols) * (s + gap);
            tileX[i] = x; tileY[i] = y;
            const bool sel = false, off = items[i].disabled; // (the highlight: DrawChosenTile, over this)
            if (sel) Glass::Glow(c, x, y, s, s, s * 0.09f, s / 200.0f);
            FrostBox(c, solid, x, y, s, s, s * 0.09f, sel, sel ? (float)std::max(2, scale) : 1.0f);
            const uint32_t ink = off ? GlassOff : GlassText;
            if (items[i].icon >= 0) Glass::DrawIcon(c, items[i].icon, x + s * 0.5f, y + s * 0.4f, s * 0.19f, ink);
            TileLabel(c, x, y, s, items[i].label, ls, ink);
            if (!items[i].value.empty())
            {
                const int vs = std::max(1, ls - 1);
                TX(c, x + s - TW(items[i].value, vs) - s / 14, y + s / 14, items[i].value, vs, off ? GlassOff : GlassHint);
            }
        }
        int below = gy + gridH + gap;
        if (np)
        {
            DrawNowPlaying(c, solid, gx, below, gridW, barH, scale);
            below += barH + gap;
        }
        else npS = 0;
        TX(c, (c.w - TW(hint, scale)) / 2, below + 2 * scale, hint, scale, GlassHint);
        if (solid) Glass::Opaque(c);
        tilesSolid = solid;
        tilesLabelScale = ls;
        tilesScale = scale;
        tilesBase = c.px;
        tilesBaseKey = key;
        DrawChosenTile(c);
    }
    mutable bool tilesSolid = false;
    mutable int tilesLabelScale = 1, tilesScale = 1, tilesLabelPx = 0;
    void TileLabel(Canvas& c, int x, int y, int s, const std::string& label, int ls, uint32_t ink) const
    {
        if (tilesLabelPx <= 0) { TX(c, x + (s - TW(label, ls, true)) / 2, y + (int)(s * 0.72f), label, ls, ink, true); return; }
        const std::string t = CleanFont::Title(UiLang::Tr(label));
        CleanFont::Draw(c, x + (s - CleanFont::Width(t, tilesLabelPx, true)) / 2, y + (int)(s * 0.72f) + tilesLabelPx / 2, t, tilesLabelPx, ink, true);
    }
    // The chosen tile, lit, over the page as DrawTiles left it.
    void DrawChosenTile(Canvas& c) const
    {
        const int i = selected;
        if (npFocus >= 0 || i < 0 || i >= (int)items.size() || i >= (int)tileX.size()) return;
        const int s = tileSize, x = tileX[i], y = tileY[i], ls = tilesLabelScale, scale = tilesScale;
        const bool off = items[i].disabled;
        Glass::Glow(c, x, y, s, s, s * 0.09f, s / 200.0f);
        FrostBox(c, tilesSolid, x, y, s, s, s * 0.09f, true, (float)std::max(2, scale), true);
        const uint32_t ink = off ? GlassOff : GlassText;
        if (items[i].icon >= 0) Glass::DrawIcon(c, items[i].icon, x + s * 0.5f, y + s * 0.4f, s * 0.19f, ink);
        TileLabel(c, x, y, s, items[i].label, ls, ink);
        if (!items[i].value.empty())
        {
            const int vs = std::max(1, ls - 1);
            TX(c, x + s - TW(items[i].value, vs) - s / 14, y + s / 14, items[i].value, vs, off ? GlassOff : GlassHint);
        }
        // (Made solid, as Glass::Opaque does the whole page: here only round the tile and its glow.)
        if (tilesSolid)
        {
            const int m = s / 4;
            for (int yy = std::max(0, y - m); yy < std::min(c.h, y + s + m); yy++)
                for (int xx = std::max(0, x - m); xx < std::min(c.w, x + s + m); xx++) c.px[(size_t)yy * c.w + xx] |= 0xFF000000u;
        }
    }

    // boxUnits: the box's width at scale 1 (0: 190, as the other pages). CHEATS passes 260 so a
    // code's name (32 letters) fits beside "< OFF >".
    void Draw(Canvas& c, const std::string& title, const std::string& hint, const std::string& note = "", int boxUnits = 0) const
    {
        if (style == 1) { DrawSettings(c, title, hint); return; }
        if (style == 2) { DrawDialog(c, title, note, hint); return; }
        if (columns > 0) { DrawTiles(c, hint); return; }
        int scale = std::min(c.w, c.h) / 240;
        if (scale < 1) scale = 1;
        if (touchRows > 1.0f) scale = std::max(scale, (int)std::lround(scale * touchRows * 0.9f)); // (phones: bigger for a finger)
        int lineH = (UiLang::Tall() ? 16 : 11) * scale;
        // (Rows closer together if they would not all fit on the screen, down to 9 * scale.)
        // With no title the box starts with the first item.
        const bool hasTitle = !title.empty();
        // (A value as drawn: translated before its arrows go round it, as the whole would not be
        // in the language's table.)
        auto shownValue = [](const MenuItem& item) { return item.info ? item.value : "< " + UiLang::Tr(item.value) + " >"; };
        int boxW = (boxUnits > 0 ? boxUnits : 190) * scale;
        if (UiLang::lang)
            for (const auto& item : items)
                boxW = std::max(boxW, TW(item.label, scale) + (item.value.empty() ? 0 : TW(shownValue(item), scale)) + 20 * scale);
        if (boxW > c.w - 8) boxW = c.w - 8;
        // The note's lines (up to 4, the last cut short), and the box moved up to make room.
        std::vector<std::string> noteLines;
        if (!note.empty())
        {
            const int maxW = boxW - 12 * scale;
            noteLines = UiLang::Wrap(UiLang::Tr(note), maxW / scale);
            if (noteLines.size() > 4) { noteLines.resize(4); noteLines[3] = UiLang::Elide(noteLines[3]+"...",maxW/scale); }
        }
        const int noteH = noteLines.empty() ? 0 : (int)noteLines.size() * UiLang::LineUnits() * scale + 6 * scale;
        // (As many rows as fit at the smallest spacing; the rest scroll into view.)
        const int fit = std::max(1, (c.h - 4 - 6 * scale - (noteH ? noteH + 3 * scale : 0)) / ((UiLang::Tall() ? 14 : 9) * scale) - (hasTitle ? 3 : 2));
        const int shown = std::min((int)items.size(), fit);
        // (A finger has scrolled the list, freeScroll: the chosen row need not be in view.)
        if (!freeScroll)
        {
            if (selected < scroll) scroll = selected;
            if (selected >= scroll + shown) scroll = selected - shown + 1;
        }
        scroll = std::max(0, std::min(scroll, (int)items.size() - shown));
        // (The note's strip sits under the box, so the box leaves room for it: at 800x480 it was cut off.)
        const int availH = c.h - 4 - (noteH ? noteH + 3 * scale : 0);
        const int rows = shown + (hasTitle ? 3 : 2);
        if (lineH * rows + 6 * scale > availH) lineH = std::max(UiLang::LineUnits() * scale, (availH - 6 * scale) / rows);
        int boxH = lineH * rows + 6 * scale;
        int bx = (c.w - boxW) / 2, by = std::max(2, (c.h - boxH - noteH) / 2);
        // (Kept for ItemAt: where the rows were drawn.)
        drawnX = bx;
        drawnY = by;
        drawnW = boxW;
        drawnH = boxH;
        drawnLineH = lineH;
        drawnFirstY = by + (hasTitle ? lineH : 0) + 5 * scale - 2 * scale;

        // (The glass boxes over the game, the same while the page is up: drawn once and copied after,
        // the rows drawn over them each press.)
        const float radius = 5.0f * scale;
        const int noteY = by + boxH + 3 * scale;
        const std::string listKey = std::to_string(c.w) + "x" + std::to_string(c.h) + " " + std::to_string(c.BackdropSig()) + " " + std::to_string(bx) + "," +
            std::to_string(by) + "," + std::to_string(boxW) + "," + std::to_string(boxH) + "," + std::to_string(noteLines.empty() ? 0 : noteH);
        bool solid;
        if (listKey == listBaseKey && listBase.size() == c.px.size())
        {
            std::copy(listBase.begin(), listBase.end(), c.px.begin());
            Glass::Mark(c, 0, 0, c.w, c.h);
            solid = listBaseSolid;
        }
        else
        {
            solid = Glass::Backdrop(c);
            FrostBox(c, solid, bx, by, boxW, boxH, radius);
            if (!noteLines.empty()) FrostBox(c, solid, bx, noteY, boxW, noteH, radius);
            listBase = c.px; listBaseKey = listKey; listBaseSolid = solid;
        }
        if (hasTitle) TX(c, bx + (boxW - TW(title, scale, true)) / 2, by + 3 * scale, title, scale, GlassText, true);

        int y = by + (hasTitle ? lineH : 0) + 5 * scale;
        // (Marks at the box's right edge where rows are scrolled out of view, above or below.)
        if (scroll > 0) TX(c, bx + boxW - 8 * scale, y - lineH + 2 * scale, "+", scale, GlassHint);
        if (scroll + shown < (int)items.size()) TX(c, bx + boxW - 8 * scale, y + shown * lineH - 2 * scale, "+", scale, GlassHint);
        for (int i = scroll; i < scroll + shown; i++)
        {
            bool sel = i == selected;
            if (sel) Glass::Round(c, bx + 2 * scale, y - 2 * scale, boxW - 4 * scale, lineH, 3.0f * scale, GlassRow, GlassChosenEdge, (float)std::max(1, scale / 2));
            uint32_t col = items[i].disabled ? GlassOff : sel ? 0xFFFFFFFF : GlassText;
            TX(c, bx + 6 * scale, y, items[i].label, scale, col);
            if (!items[i].value.empty())
            {
                const std::string v = shownValue(items[i]);
                // (A slider's value right-aligned in room for its widest, "< 100% >", the bar left
                // of it, so the bar stays put as the number changes.)
                const int vx = bx + boxW - 6 * scale - TW(items[i].slider >= 0 ? "< 100% >" : v, scale);
                TX(c, items[i].slider >= 0 ? bx + boxW - 6 * scale - TW(v, scale) : vx, y, v, scale, col);
                if (items[i].slider >= 0)
                {
                    barStep = 5 * scale;
                    barX = vx - 4 * scale - 10 * barStep;
                    for (int k = 0; k < 10; k++)
                        c.Fill(barX + k * barStep, y, 4 * scale, 7 * scale,
                               k < items[i].slider ? 0xFFFFFFFF : (sel ? 0xFF9AA5B1 : 0xFF5C6672));
                }
            }
            y += lineH;
        }
        TX(c, bx + (boxW - TW(hint, scale)) / 2, y + 4 * scale, hint, scale, GlassHint);
        if (!noteLines.empty())
        {
            const int ny = noteY;
            for (size_t i = 0; i < noteLines.size(); i++)
                TXRaw(c, bx + 6 * scale, ny + 4 * scale + (int)i * UiLang::LineUnits() * scale, noteLines[i], scale, GlassText);
        }
        if (solid) Glass::Opaque(c);
    }

    mutable std::vector<uint32_t> listBase; // (a list page's boxes over the game, as drawn: Draw)
    mutable std::string listBaseKey;
    mutable bool listBaseSolid = false;
    // Settings pages (style 1): ui_settings.h.
    int UpdateSettings(const MenuInput& in, int& dir);
    int SettingsItemAt(int x, int y) const;
    void SettingsTap(int item, int x, MenuInput& input);
    void DrawSettings(Canvas& c, const std::string& title, const std::string& hint) const;
    // style 2: a question in a card over the paused game ("Quit Game?"): items[0] the action, the
    // last item CANCEL, drawn side by side as pills (CANCEL first); dialogIcon above the title.
    int dialogIcon = -1;
    mutable int dialogX[2] = {}, dialogY = 0, dialogW = 0, dialogH = 0;
    // (The card's whole rect, for a tap outside it: that cancels, as the page's hint says.)
    mutable int cardX = 0, cardY = 0, cardW = 0, cardH = 0;
    void DrawDialog(Canvas& c, const std::string& title, const std::string& text, const std::string& hint) const;
    void DrawDialogButtons(Canvas& c, int py, int ph, float k, bool solid) const;
    // (A settings page of state slots, SAVE and LOAD: its other rows as pills across the top, the
    // K_SLOT rows as cards in a grid, each with its picture and date. Where each was drawn:
    // slotRects {x, y, w, h, item}; the grid's columns: slotCols.)
    bool slotGrid = false;
    mutable std::vector<std::array<int, 5>> slotRects;
    mutable int slotCols = 5;
    void DrawSlotGrid(Canvas& c, int x0, int w0, int top, int bottom, float k) const;
    // (A settings page that is a keyboard, SEARCH and RetroAchievements' log in: what is typed in a
    // box across the top (keyText; dots when keySecret; keyPrompt while empty), then the K_KEY rows as
    // keys, chosen with the stick or a tap. Where each was drawn: keyRects {x, y, w, h, item}.)
    bool keyGrid = false, keySecret = false;
    std::string keyText, keyPrompt;
    mutable std::vector<std::array<int, 5>> keyRects;
    void DrawKeyGrid(Canvas& c, int x0, int w0, int top, int bottom, float k) const;
    int FirstRow() const; // the first row that can be chosen (not a section's heading or the back button)
    // Where the settings page was last drawn: each item's row (y, -1 when not shown), the rows' x
    // and size, the controls' box on the right of them, and the back button.
    mutable std::vector<int> rowY;
    mutable int rowX = 0, rowW = 0, rowH = 0, ctlX = 0, ctlW = 0, backX = 0, backY = 0, backS = 0, scrollPx = 0;
    // (The preview's box and a Guide answer's line height: a finger dragged on the preview scrolls
    // that answer rather than the list.)
    mutable int previewX = 0, previewY = 0, previewW = 0, previewH = 0, previewLineH = 1;
};

#include "ui_settings.h"


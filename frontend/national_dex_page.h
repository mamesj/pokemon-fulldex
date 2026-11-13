#pragma once
// Included inside namespace Launcher, after the shared Hub navigation helpers.
struct DexPageState
{
    NationalDex::Selection selection;
    NationalDex::Result result;
    std::vector<NationalDex::Source> sources;
    std::vector<int> entries;
    std::array<std::string, 722> names;
    std::string config, message;
    std::map<int, NationalDexArt::Image> art;
    NationalDexArt::Bytes gen6Art;
    bool triedGen6Art = false;
    int detailKey = -1;
    int mode = 0; // 0 entries, 1 regions, 2 saves
    NationalDex::Filter filter = NationalDex::Filter::All;
#ifdef SSR_POKEMON_HUB
    NationalDex::Scanner scanner{[](const NationalDex::Source& source, PokemonHub::DexSnapshot& out, std::string& error) {
        return PokemonHub::ReadDex(source.path, out, error, source.hack);
    }};
#endif
};
inline DexPageState& DexPage() { static DexPageState state; return state; }
// Support runtimes with an extracted save archive under the title's directory.
// Locate its "main" file, never a backup or a save from another title. Do not guess
// which file is current when a runtime provides multiple candidates.
inline NationalDex::Source Dex3dsSource(const Game& g)
{
    namespace fs = std::filesystem;
    NationalDex::Source source{g.folder + "/main", ShortName(g), false, "No 3DS main save found"};
    std::vector<std::string> found;
    std::error_code ec;
    fs::recursive_directory_iterator it(fs::u8path(g.folder), fs::directory_options::skip_permission_denied, ec), end;
    int visited = 0;
    for (; !ec && it != end && ++visited <= 4096; it.increment(ec))
    {
        const auto name = it->path().filename().u8string();
        if (it->is_directory(ec))
        {
            if (it.depth() >= 8 || name == "backups" || name == "backup" || name == "import" || name == "romfs") it.disable_recursion_pending();
        }
        else if (name == "main" && it->is_regular_file(ec)) found.push_back(it->path().u8string());
    }
    if (ec || visited > 4096) source.unavailable = "Could not finish locating the 3DS save";
    else if (found.size() == 1) { source.path = found.front(); source.unavailable.clear(); }
    else if (found.size() > 1) source.unavailable = "Multiple 3DS main saves; current save is ambiguous";
    return source;
}
inline void DexDiscover()
{
    auto& d = DexPage(); d.sources.clear();
    std::set<std::string> paths;
    for (const Game& g : RomList())
    {
        if (!g.known || g.comingSoon) continue;
        NationalDex::Source s;
        if (g.platform == N3DS) s = Dex3dsSource(g);
        else
        {
            if (g.saveName.empty()) continue;
            s = {HubSavePath(g), ShortName(g), IsHack(g), ""};
            if (!g.where.empty()) s.name += " · " + WhereName(g.where);
            std::error_code ec;
            if (!std::filesystem::is_regular_file(std::filesystem::u8path(s.path), ec)) s.unavailable = "No in-game save yet";
            if (s.hack) s.unavailable = "ROM hack Pokédex layout is unsupported";
        }
        s.path = NationalDex::Identity(s.path);
        if (paths.insert(s.path).second) d.sources.push_back(std::move(s));
    }
    for (const auto& selected : d.selection.saves)
        if (paths.insert(selected.first).second) d.sources.push_back({selected.first, selected.second, false, "Save is no longer in the library"});
}
inline void DexRefresh()
{
    auto& d = DexPage(); DexDiscover();
    d.art.clear(); d.gen6Art.clear(); d.triedGen6Art = false;
#ifdef SSR_POKEMON_HUB
    MonIcons() = {};
#endif
    d.result = {}; // Never display progress from deselected saves while the new scan runs.
    d.entries = d.result.Entries(d.selection, d.filter);
#ifdef SSR_POKEMON_HUB
    d.scanner.Request(d.sources, d.selection);
#else
    d.message = "National Dex requires a build with Pokémon Hub enabled";
#endif
    Hub().version++;
}
inline void DexPoll()
{
#ifdef SSR_POKEMON_HUB
    auto& d = DexPage();
    if (d.scanner.Poll(d.result)) { d.entries = d.result.Entries(d.selection, d.filter); Hub().version++; }
#endif
}
inline void DexOpen()
{
    auto& d = DexPage();
    const auto config = HubDir() + "/national-dex.cfg";
    if (config != d.config)
    {
        d.config = config;
        NationalDex::LoadSelection(config, d.selection);
#ifdef SSR_POKEMON_HUB
        for (int id = 1; id <= 721; ++id) d.names[id] = MonName(PokemonHub::DexSpeciesName(id));
#endif
    }
    d.mode = d.selection.saves.empty() ? 2 : 0; d.message.clear();
    HubMode() = 4; Mover().open = false;
    Focus().prow = 0; Focus().pact = -1; Focus().rScroll = 0; Focus().freeScroll = false;
    DexRefresh();
}
inline int DexRows()
{
    const auto& d = DexPage();
    return d.mode == 1 ? 7 : d.mode == 2 ? 1 + (int)d.sources.size() : 4 + (int)d.entries.size();
}
inline void DexKeep()
{
    auto& d = DexPage();
    d.message = NationalDex::SaveSelection(d.config, d.selection) ? "" : "Could not save your selections";
    DexRefresh();
}
inline bool DexEnter(int row)
{
    auto& d = DexPage();
    if (row < 0 || row >= DexRows()) return true;
    const int previousMode = d.mode;
    if (d.mode == 1)
    {
        if (!row) d.mode = 0;
        else { d.selection.regions ^= 1u << (row - 1); DexKeep(); }
    }
    else if (d.mode == 2)
    {
        if (!row) d.mode = 0;
        else
        {
            const auto source = d.sources[row - 1];
            const auto selected = d.selection.saves.find(source.path);
            if (selected != d.selection.saves.end()) d.selection.saves.erase(selected);
            else if (source.unavailable.empty()) d.selection.saves[source.path] = source.name;
            else { d.message = source.unavailable; Hub().version++; return true; }
            DexKeep();
        }
    }
    else if (row == 0) d.mode = 1;
    else if (row == 1) d.mode = 2;
    else if (row == 2)
    {
        d.filter = static_cast<NationalDex::Filter>((static_cast<int>(d.filter) + 1) % 4);
        d.entries = d.result.Entries(d.selection, d.filter);
    }
    else if (row == 3) { d.message.clear(); DexRefresh(); }
    if (d.mode != previousMode) { Focus().prow = 0; Focus().rScroll = 0; }
    Hub().version++;
    return true;
}
inline bool DexBack()
{
    auto& d = DexPage();
    if (Focus().pact >= 0) { Focus().pact = -1; return true; }
    if (d.mode) { d.mode = 0; Focus().prow = 0; }
    else { HubMode() = 0; Focus().prow = 4; }
    Focus().rScroll = 0; Focus().pact = -1; Hub().version++;
    return true;
}
inline bool DexMove(int dir)
{
    auto& f = Focus();
    if (f.pact >= 0)
    {
        if (dir == 1) f.pact = -1;
        else if (dir == 3 || dir == 4) f.dScroll = std::clamp(f.dScroll + (dir == 3 ? -1 : 1) * 32, 0, f.dMax);
        return true;
    }
    if (dir == 2 && f.dMax > 0) { f.pact = 0; return true; }
    return false;
}
inline std::string DexNumber(int id) { char s[8]; snprintf(s, sizeof s, "#%03d", id); return s; }
inline std::string DexTotals(const NationalDex::Totals& t)
{ return std::to_string(t.seen) + " seen · " + std::to_string(t.caught) + "/" + std::to_string(t.total) + " caught · " + std::to_string(t.Percent()) + "%"; }
#ifdef SSR_POKEMON_HUB
inline const NationalDexArt::Image& DexSprite(int id)
{
    auto& d = DexPage();
    auto cached = d.art.find(id); if (cached != d.art.end()) return cached->second;
    NationalDexArt::Image image;
    if (!d.triedGen6Art)
    {
        d.triedGen6Art = true;
        for (const auto& g : RomList())
        {
            if (!g.known || g.comingSoon || g.platform != N3DS) continue;
            NationalDexArt::File file(g.rom);
            if (NationalDexArt::RomFsFile(file, "a/0/9/1", d.gen6Art)) break; // ORAS
        }
    }
    NationalDexArt::Bytes member;
    if (!d.gen6Art.empty() && NationalDexArt::GarcMember(d.gen6Art, id, member)) NationalDexArt::Bclim(std::move(member), image);
    if (image.pixels.empty() && MonIcon(id, false, image.pixels)) image.width = image.height = 32;
    if (image.pixels.empty() && id <= 386)
        for (const auto& g : RomList())
        {
            if (!g.known || g.comingSoon || g.platform != GBA || IsHack(g)) continue;
            NationalDexArt::File file(g.rom);
            if (NationalDexArt::GbaIcon(file, PokemonHub::DexGbaSpeciesIndex(id), image)) break;
        }
    return d.art.emplace(id, std::move(image)).first->second;
}
inline void DrawDexSprite(Canvas& c, const NationalDexArt::Image& image, NationalDex::Status status, int x, int y, int size)
{
    if (status == NationalDex::Status::Unseen || image.pixels.empty() || size <= 0) return;
    // Integer scaling preserves the source game's pixel edges and aspect ratio.
    const int scale = std::max(1, size / std::max(image.width, image.height));
    for (int iy = 0; iy < image.height * scale; ++iy)
        for (int ix = 0; ix < image.width * scale; ++ix)
        {
            if (x + ix < 0 || x + ix >= c.w || y + iy < 0 || y + iy >= c.h) continue;
            uint32_t pixel = image.pixels[(iy / scale) * image.width + ix / scale];
            if (status == NationalDex::Status::Seen) pixel &= 0xff000000u;
            if (pixel >> 24) c.px[(y + iy) * c.w + x + ix] = pixel;
        }
}
#endif
inline void DrawDexPage(Canvas& c, bool solid, float u, int L, int R, int top, int bottom)
{
    DexPoll();
    auto& d = DexPage(); auto& f = Focus();
    const auto text = Menu::GlassText, hint = Menu::GlassHint;
    const int gap = std::max(8, (int)(14 * u)), pad = gap;
    const bool stacked = c.h > c.w;
    const int width = R - L, height = bottom - top;
    const int lw = stacked ? width : (width - gap) / 2;
    const int lh = stacked ? height * 55 / 100 : height;
    const int dx = stacked ? L : L + lw + gap, dy = stacked ? top + lh + gap : top;
    const int dw = stacked ? width : width - lw - gap;
    const int rowH = std::max(28, (int)(44 * u));
    const int header = std::max(rowH, (int)(52 * u));
    const int n = DexRows();
    f.prow = std::clamp(f.prow, 0, std::max(0, n - 1)); f.aW = 0;
    f.pX = L + pad; f.pY = top + header; f.pW = lw - 2 * pad; f.pRow = rowH;
    f.rVis = std::max(1, (lh - header - pad) / rowH);
    if (!f.freeScroll) f.rScroll = std::clamp(f.rScroll, std::max(0, f.prow - f.rVis + 1), f.prow);
    f.rScroll = std::clamp(f.rScroll, 0, std::max(0, n - f.rVis));
    Menu::FrostBox(c, solid, L, top, lw, lh, 16 * u);
    const char* filters[] = {"All", "Unseen", "Seen but Uncaught", "Caught"};
    std::string heading = d.mode == 1 ? "Regions" : d.mode == 2 ? "Select Saves" : DexTotals(d.result.Count(d.selection));
#ifdef SSR_POKEMON_HUB
    if (!d.mode && d.scanner.Busy()) heading = "Reading selected saves…";
#endif
    if (d.result.partial) heading = "Partial · " + heading;
    GTFit(c, L + pad, top + header / 2, heading, 15 * u, lw - 2 * pad, text, true);
    for (int k = 0; k < f.rVis && f.rScroll + k < n; ++k)
    {
        const int row = f.rScroll + k, y = f.pY + k * rowH;
        std::string label;
        if (d.mode == 1) label = row ? std::string(d.selection.regions & (1u << (row - 1)) ? "[x] " : "[ ] ") + NationalDex::Regions[row - 1].name : "Done";
        else if (d.mode == 2)
        {
            if (!row) label = "Done";
            else { const auto& s = d.sources[row - 1]; label = std::string(d.selection.saves.count(s.path) ? "[x] " : "[ ] ") + s.name + (s.unavailable.empty() ? "" : " · Unavailable"); }
        }
        else if (row == 0) label = "Regions";
        else if (row == 1) label = "Select Saves · " + std::to_string(d.selection.saves.size());
        else if (row == 2) label = std::string("Filter · ") + filters[(int)d.filter];
        else if (row == 3) label = "Refresh";
        else
        {
            const int id = d.entries[row - 4];
            label = DexNumber(id) + "  " + NationalDex::EntryName(d.result.status[id], d.names[id]) + " · " + NationalDex::StatusName(d.result.status[id]);
        }
        if (row == f.prow) Glass::Round(c, f.pX, y, f.pW, rowH - 3, 9 * u, Menu::FrostChosen, Glass::ChosenEdge(), u);
        GTFit(c, f.pX + pad / 2, y + rowH / 2, label, 16 * u, f.pW - pad, text, row == f.prow);
    }
    if (dy >= bottom) return;
    Menu::FrostBox(c, solid, dx, dy, dw, bottom - dy, 16 * u);
    const int key = d.mode * 1000 + f.prow;
    if (d.detailKey != key) { d.detailKey = key; f.dScroll = 0; f.dMax = 0; }
    f.pdY = stacked ? dy : 0;
    f.dScroll = std::clamp(f.dScroll, 0, f.dMax);
    int y = dy + pad - f.dScroll;
    auto line = [&](const std::string& s, bool bold = false) {
        const int h = std::max(20, (int)(26 * u));
        for (const auto& part : WrapLines(s, (bold ? 17 : 13) * u, dw - 2 * pad, bold))
        {
            if (y >= dy + pad && y + h <= bottom - pad) GTFit(c, dx + pad, y + h / 2, part, (bold ? 17 : 13) * u, dw - 2 * pad, bold ? text : hint, bold);
            y += h;
        }
    };
    if (!d.message.empty()) line(d.message);
    if (d.result.partial) line("Some selected saves could not be read.", true);
    if (d.mode == 2 && f.prow > 0)
    {
        const auto& source = d.sources[f.prow - 1];
        line(source.name, true); line(source.path);
        if (!source.unavailable.empty()) line(source.unavailable);
        for (const auto& s : d.result.sources) if (s.source.path == source.path)
        { if (!s.error.empty()) line(s.error); else { line(s.dex.game); line("Trainer: " + s.dex.trainer); } }
        line("Choose this row to include or exclude it.");
    }
    else if (d.mode == 0 && f.prow >= 4)
    {
        const int id = d.entries[f.prow - 4]; const auto status = d.result.status[id];
        line(DexNumber(id) + "  " + NationalDex::EntryName(status, d.names[id]), true);
        line(NationalDex::StatusName(status));
#ifdef SSR_POKEMON_HUB
        if (status != NationalDex::Status::Unseen)
        {
            const auto& image = DexSprite(id);
            if (!image.pixels.empty())
            {
                const int size = std::min((int)(112 * u), dw - 2 * pad);
                if (size >= std::max(image.width, image.height))
                {
                    if (y >= dy + pad && y + size <= bottom - pad) DrawDexSprite(c, image, status, dx + pad, y, size);
                    y += size;
                }
            }
            else line("Artwork unavailable in your game files");
        }
#endif
        for (const auto& s : d.result.sources)
            if (s.error.empty() && (s.dex.seen[id] || s.dex.caught[id])) line(s.source.name + " · " + (s.dex.caught[id] ? "Caught" : "Seen"));
    }
    else
    {
        line(d.mode == 1 ? "Choose debut generations" : "Combined National Dex", true);
        if (d.selection.saves.empty()) line("Choose the saves that count toward progress.");
        if (!d.selection.regions) line("Select at least one region to display entries.");
        for (int r = 0; r < 6; ++r) if (d.selection.regions & (1u << r)) line(std::string(NationalDex::Regions[r].name) + " · " + DexTotals(d.result.Count(d.selection, r)));
        for (const auto& s : d.result.sources) if (!s.error.empty()) line(s.source.name + ": " + s.error);
    }
    f.dMax = std::max(0, y + f.dScroll + pad - bottom);
}

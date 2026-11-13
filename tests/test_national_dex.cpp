#include "national_dex.h"
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
using namespace NationalDex;
int main()
{
    Selection s;
    Result empty;
    assert(empty.Count(s).total == 721);
    for (int r = 0; r < 6; ++r)
    {
        assert(RegionOf(Regions[r].first) == r && RegionOf(Regions[r].last) == r);
        assert(empty.Count(s,r).total == Regions[r].last - Regions[r].first + 1);
    }
    assert(RegionOf(0) == -1 && RegionOf(722) == -1);
    SourceResult a{{Identity("a.sav"), "A"}, {}, ""}, b{{Identity("b.sav"), "B"}, {}, ""};
    a.dex.seen[1] = true; b.dex.caught[1] = true; b.dex.caught[721] = true;
    auto merged = Merge({a,b,a});
    assert(merged.sources.size() == 2 && merged.Count(s).caught == 2 && merged.Count(s).seen == 2);
    assert(Merge({a}).status[1] == Status::Seen); // Removal actually removes progress.
    assert(merged.Entries(s, Filter::Caught) == std::vector<int>({1,721}));
    assert(merged.Entries(s, Filter::Seen).empty());
    s.regions = 1; assert(merged.Count(s).caught == 1 && merged.Count(s).total == 151);
    s.regions = 0; assert(merged.Count(s).Percent() == 0 && merged.Entries(s,Filter::All).empty());
    SourceResult bad{{Identity("missing.sav"), "Missing"}, {}, "Missing"}; bad.dex.caught[2] = true;
    assert(Merge({a,bad}).partial && Merge({a,bad}).status[2] == Status::Unseen);
    assert(EntryName(Status::Unseen, "Bulbasaur") == "???");
    const std::vector<uint32_t> pixels{0,0xff123456,0x80556677};
    assert(VisibleArt(Status::Unseen,pixels).empty());
    assert(VisibleArt(Status::Seen,pixels) == std::vector<uint32_t>({0,0xff000000,0x80000000}));
    assert(VisibleArt(Status::Caught,pixels) == pixels);
    const auto root = std::filesystem::temp_directory_path() / ("fulldex-dex-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto config = (root / "national-dex.cfg").string();
    s.regions = 31; s.saves[a.source.path] = "Save with spaces and \"quotes\" 日本語";
    assert(SaveSelection(config,s)); Selection loaded;
    assert(LoadSelection(config,loaded) && loaded.saves == s.saves && loaded.regions == 31);
    s.regions = 63; assert(SaveSelection(config,s));
    assert(LoadSelection(config,loaded) && loaded.regions == 63);
    { std::ofstream f(config); f << "FULLDEX_NATIONAL_DEX 99 63 0\n"; }
    assert(!LoadSelection(config,loaded) && loaded.saves.empty() && loaded.regions == 63);
    auto read = [&](const Source&, PokemonHub::DexSnapshot& out, std::string&) { out = a.dex; return true; };
    assert(Scan({a.source},s,read).status[1] == Status::Seen);
    assert(Scan({},s,read).partial);
    // Block an older request, submit a newer one, and prove stale work cannot publish.
    std::promise<void> started, release;
    auto released = release.get_future().share();
    Scanner scanner([&](const Source&, PokemonHub::DexSnapshot& out, std::string&) {
        started.set_value(); released.wait(); out = a.dex; return true;
    });
    scanner.Request({a.source},s); started.get_future().wait();
    scanner.Request({},Selection{}); release.set_value();
    Result latest; bool ready = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!(ready = scanner.Poll(latest)) && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    assert(ready && latest.sources.empty() && latest.status[1] == Status::Unseen && !scanner.Busy());
    std::filesystem::remove_all(root);
    std::cout << "National Dex model checks passed\n";
}

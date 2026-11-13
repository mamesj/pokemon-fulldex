#pragma once
// National Dex model: no renderer, emulator, networking or save writes.
#include "pokemon_hub.h"
#include <algorithm>
#include <array>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

namespace NationalDex
{
constexpr int LastSpecies = 721;
struct Region { const char* name; int first, last; };
inline constexpr Region Regions[] = {
    {"Kanto", 1, 151}, {"Johto", 152, 251}, {"Hoenn", 252, 386},
    {"Sinnoh", 387, 493}, {"Unova", 494, 649}, {"Kalos", 650, 721}
};
enum class Status { Unseen, Seen, Caught };
enum class Filter { All, Unseen, Seen, Caught };
inline const char* StatusName(Status s)
{ return s == Status::Caught ? "Caught" : s == Status::Seen ? "Seen" : "Unseen"; }
inline int RegionOf(int id)
{
    for (int i = 0; i < 6; ++i) if (id >= Regions[i].first && id <= Regions[i].last) return i;
    return -1;
}
inline std::string Identity(const std::string& path)
{
    std::error_code ec;
    auto p = std::filesystem::weakly_canonical(std::filesystem::u8path(path), ec);
    if (ec) p = std::filesystem::u8path(path).lexically_normal();
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}
struct Source
{
    std::string path, name;
    bool hack = false;
    std::string unavailable;
};
struct Selection
{
    unsigned regions = 63;
    // Canonical path -> retained display name. Missing saves remain selected.
    std::map<std::string, std::string> saves;
    bool Includes(int id) const
    { const int r = RegionOf(id); return r >= 0 && (regions & (1u << r)); }
};
inline bool LoadSelection(const std::string& path, Selection& out)
{
    out = {};
    std::ifstream in(std::filesystem::u8path(path));
    std::string magic; unsigned version, regions; size_t count;
    if (!(in >> magic >> version >> regions >> count) || magic != "FULLDEX_NATIONAL_DEX" || version != 1 || regions > 63 || count > 4096) return false;
    Selection read; read.regions = regions;
    for (size_t i = 0; i < count; ++i)
    {
        std::string key, name;
        if (!(in >> std::quoted(key) >> std::quoted(name)) || key.empty()) return false;
        read.saves[Identity(key)] = name;
    }
    out = std::move(read);
    return true;
}
inline bool SaveSelection(const std::string& path, const Selection& s)
{
    const auto target = std::filesystem::u8path(path), tmp = std::filesystem::u8path(path + ".tmp");
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) return false;
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << "FULLDEX_NATIONAL_DEX 1 " << (s.regions & 63) << ' ' << s.saves.size() << '\n';
        for (const auto& kv : s.saves) out << std::quoted(kv.first) << ' ' << std::quoted(kv.second) << '\n';
        out.close();
        if (!out) { std::filesystem::remove(tmp, ec); return false; }
    }
    std::filesystem::rename(tmp, target, ec);
    if (ec) { std::filesystem::remove(tmp, ec); return false; }
    return true;
}
struct SourceResult
{
    Source source;
    PokemonHub::DexSnapshot dex;
    std::string error;
};
struct Totals
{
    int seen = 0, caught = 0, total = 0;
    int Percent() const { return total ? caught * 100 / total : 0; }
};
struct Result
{
    std::vector<SourceResult> sources;
    std::array<Status, LastSpecies + 1> status{};
    bool partial = false;
    Totals Count(const Selection& selected, int region = -1) const
    {
        Totals t;
        for (int id = 1; id <= LastSpecies; ++id)
            if (selected.Includes(id) && (region < 0 || RegionOf(id) == region))
            { ++t.total; t.seen += status[id] != Status::Unseen; t.caught += status[id] == Status::Caught; }
        return t;
    }
    std::vector<int> Entries(const Selection& s, Filter f) const
    {
        std::vector<int> out;
        for (int id = 1; id <= LastSpecies; ++id)
            if (s.Includes(id) && (f == Filter::All || (f == Filter::Unseen && status[id] == Status::Unseen) ||
                (f == Filter::Seen && status[id] == Status::Seen) || (f == Filter::Caught && status[id] == Status::Caught))) out.push_back(id);
        return out;
    }
};
inline Result Merge(std::vector<SourceResult> sources)
{
    Result out;
    std::set<std::string> seenPaths;
    for (auto& source : sources)
    {
        if (!seenPaths.insert(Identity(source.source.path)).second) continue;
        if (!source.error.empty()) out.partial = true;
        else for (int id = 1; id <= LastSpecies; ++id)
        {
            const auto status = source.dex.caught[id] ? Status::Caught : source.dex.seen[id] ? Status::Seen : Status::Unseen;
            out.status[id] = std::max(out.status[id], status);
        }
        out.sources.push_back(std::move(source));
    }
    return out;
}
using Reader = std::function<bool(const Source&, PokemonHub::DexSnapshot&, std::string&)>;
inline Result Scan(const std::vector<Source>& available, const Selection& s, const Reader& reader)
{
    std::map<std::string, Source> byPath;
    for (const auto& source : available) byPath.emplace(Identity(source.path), source);
    std::vector<SourceResult> results;
    for (const auto& kv : s.saves)
    {
        SourceResult r;
        const auto found = byPath.find(kv.first);
        r.source = found == byPath.end() ? Source{kv.first, kv.second, false, "Save is no longer in the library"} : found->second;
        if (!r.source.unavailable.empty()) r.error = r.source.unavailable;
        else if (!reader(r.source, r.dex, r.error) && r.error.empty()) r.error = "Could not read this save";
        results.push_back(std::move(r));
    }
    return Merge(std::move(results));
}
// A single worker coalesces requests. Only the most recent request can be published.
class Scanner
{
public:
    explicit Scanner(Reader read) : reader(std::move(read)), worker([this] { Run(); }) {}
    ~Scanner()
    { { std::lock_guard<std::mutex> lock(mutex); stopping = true; } wake.notify_one(); worker.join(); }
    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;
    void Request(std::vector<Source> sources, Selection selection)
    {
        { std::lock_guard<std::mutex> lock(mutex); available = std::move(sources); selected = std::move(selection); ++version; pending = true; ready = false; busy = true; }
        wake.notify_one();
    }
    bool Poll(Result& out)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!ready) return false;
        out = std::move(result); ready = false; return true;
    }
    bool Busy() const { std::lock_guard<std::mutex> lock(mutex); return busy; }
private:
    Reader reader;
    mutable std::mutex mutex;
    std::condition_variable wake;
    bool stopping = false, pending = false, ready = false, busy = false;
    unsigned long long version = 0;
    std::vector<Source> available;
    Selection selected;
    Result result;
    std::thread worker;
    void Run()
    {
        for (;;)
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this] { return stopping || pending; });
            if (stopping) return;
            const auto request = version; auto sources = available; auto selection = selected; pending = false;
            lock.unlock();
            auto next = Scan(sources, selection, reader);
            lock.lock();
            if (version == request) { result = std::move(next); ready = true; busy = false; }
        }
    }
};
inline std::string EntryName(Status status, const std::string& name)
{ return status == Status::Unseen ? "???" : name; }
inline std::vector<uint32_t> VisibleArt(Status status, std::vector<uint32_t> pixels)
{
    if (status == Status::Unseen) return {};
    if (status == Status::Seen) for (auto& pixel : pixels) pixel &= 0xFF000000u;
    return pixels;
}
}

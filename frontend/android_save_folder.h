#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace AndroidSaveFolder
{
// A fresh launcher creates caches in its fallback saves directory before the
// player chooses any ROMs. Those caches must not pin that fallback location.
// Keep existing progress, including saves from direct ROM launches and backups.
inline bool Established(const std::string& roms, const std::string& saves)
{
    if (!roms.empty()) return true;
    if (saves.empty()) return false;
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = fs::u8path(saves);
    if (!fs::exists(root, ec)) return bool(ec);
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec)) continue;
        std::string name = it->path().filename().u8string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
        const std::string ext = fs::path(name).extension().string();
        if (ext == ".sav" || ext == ".srm" || ext == ".dsv" || ext == ".state" ||
            ext == ".st" || ext == ".sbin" || name.find(".sav.") != std::string::npos) return true;
    }
    return bool(ec); // Do not abandon a location whose contents cannot be checked.
}
}

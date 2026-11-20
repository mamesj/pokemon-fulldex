// The text a bug report needs, gathered on the device: what the program is, the platform, the
// game being played and the end of game.log. REPORT A BUG copies it to the clipboard and writes
// diagnostics.txt beside the log, so a reporter can paste it into Discord or attach the file.
// Nothing is sent anywhere from here, and the player's home folder is shortened to "~".
#pragma once
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace Diagnostics
{
inline std::string& RecordedLog()
{
    static std::string path;
    return path;
}

inline const char* PlatformName()
{
#if defined(__SWITCH__)
    return "Nintendo Switch";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(SSR_IOS) || defined(TARGET_OS_IPHONE)
    return "iOS";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(_WIN32)
    return "Windows";
#else
    return "Linux";
#endif
}

// Said by the platform code that opened the log, so the report reads the same file.
inline void SetLogPath(const std::string& path) { RecordedLog() = path; }

// The log this run writes: set by the platform's own freopen, or found beside the program.
inline std::string FindLog(const std::string& baseDir)
{
    namespace fs = std::filesystem;
    if (!RecordedLog().empty()) return RecordedLog();
    std::error_code ec;
    for (const char* name : { "game.log", "log.txt" })
    {
        const fs::path p = fs::u8path(baseDir) / name;
        if (fs::is_regular_file(p, ec)) return p.u8string();
    }
    return (fs::u8path(baseDir) / "game.log").u8string();
}

// The last `lines` lines of a file, reading at most the end of it (a log can be very long).
inline std::string Tail(const std::string& path, int lines, std::size_t maxBytes = 96 * 1024)
{
    if (lines <= 0) return {};
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size <= 0) return {};
    std::streamoff start = size > (std::streamoff)maxBytes ? size - (std::streamoff)maxBytes : 0;
    if (start > 0)
    {
        in.seekg(start);
        std::string cut;
        std::getline(in, cut); // (the first line was cut in half: drop it)
        start = in.tellg();
        if (start < 0) start = 0;
    }
    in.seekg(start);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::string> all;
    std::size_t pos = 0;
    while (pos < text.size())
    {
        std::size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        all.push_back(std::move(line));
        pos = nl + 1;
    }
    const std::size_t first = all.size() > (std::size_t)lines ? all.size() - (std::size_t)lines : 0;
    std::string out;
    for (std::size_t i = first; i < all.size(); i++)
    {
        out += all[i];
        if (i + 1 < all.size()) out += '\n';
    }
    return out;
}

// A path with the player's own folder taken out, so a public issue does not carry their name.
inline void Redact(std::string& text)
{
    std::vector<std::string> homes;
    for (const char* name : { "HOME", "USERPROFILE" })
        if (const char* h = std::getenv(name); h && *h) homes.push_back(h);
    for (const std::string& home : homes)
        for (std::size_t at = text.find(home); at != std::string::npos; at = text.find(home, at + 1))
            text.replace(at, home.size(), "~");
#ifdef _WIN32
    // (C:\Users\name, when USERPROFILE is not set.)
    if (homes.empty())
        if (const char* drive = std::getenv("HOMEDRIVE"))
            if (const char* path = std::getenv("HOMEPATH"))
            {
                const std::string home = std::string(drive) + path;
                for (std::size_t at = text.find(home); at != std::string::npos; at = text.find(home, at + 1))
                    text.replace(at, home.size(), "~");
            }
#endif
}

inline std::string Now()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char text[32] = {};
    std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &tm);
    return text;
}

// The whole report. `fields` are the caller's lines (version, platform, game, settings...).
inline std::string Build(const std::string& logPath, const std::vector<std::pair<std::string, std::string>>& fields,
                         int lines = 200)
{
    std::string out = "Pokemon Fulldex diagnostics\n";
    out += "generated: " + Now() + "\n";
    for (const auto& field : fields)
        if (!field.first.empty() && !field.second.empty()) out += field.first + ": " + field.second + "\n";
    out += "log: " + logPath + "\n";
    out += "--- last " + std::to_string(lines) + " log lines ---\n";
    const std::string tail = Tail(logPath, lines);
    out += tail.empty() ? "(the log is empty or not there yet)\n" : tail;
    if (out.empty() || out.back() != '\n') out += '\n';
    Redact(out);
    return out;
}

inline std::string Save(const std::string& logPath, const std::string& text)
{
    namespace fs = std::filesystem;
    const fs::path path = fs::u8path(logPath).parent_path() / "diagnostics.txt";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return {};
    out.write(text.data(), (std::streamsize)text.size());
    if (!out) return {};
    out.close();
    return path.u8string();
}

// Build, save and copy. Returns the file written (empty if it could not be), and sets `copied`.
// `copy` may be null where the platform has no clipboard: the file is still written.
using CopyFn = bool (*)(const std::string&);
inline std::string Report(const std::string& logPath, const std::vector<std::pair<std::string, std::string>>& fields,
                          CopyFn copy, bool& copied, int lines = 200)
{
    const std::string text = Build(logPath, fields, lines);
    const std::string saved = Save(logPath, text);
    copied = copy && copy(text);
    return saved;
}

} // namespace Diagnostics

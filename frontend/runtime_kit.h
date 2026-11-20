#pragma once
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

namespace RuntimeKit
{
inline std::filesystem::path NativePath(const std::filesystem::path& input, std::error_code& ec)
{
#ifdef _WIN32
    auto path = std::filesystem::absolute(input, ec).lexically_normal();
    if (ec) return {};
    const std::wstring wide = path.make_preferred().wstring();
    if (wide.rfind(L"\\\\?\\", 0) == 0) return path;
    // (\\?\ is only needed for long paths. A mapped or virtual drive rejects \\?\D:\... with
    // ERROR_INVALID_PARAMETER, and every 3DS kit path is short.)
    if (wide.size() < 248) return path;
    if (wide.rfind(L"\\\\", 0) == 0) return std::filesystem::path(L"\\\\?\\UNC\\" + wide.substr(2));
    return std::filesystem::path(L"\\\\?\\" + wide);
#else
    (void)ec;
    return input;
#endif
}

// Retry an interrupted kit installation even if its executable was copied first.
// Existing files can contain a prepared game or player settings: never replace them.
inline void Fail(const char* stage, const std::filesystem::path& path, const std::error_code& ec)
{
    std::fprintf(stderr, "[3ds] kit copy: %s [%s]: %d %s\n", stage, path.u8string().c_str(), ec.value(), ec.message().c_str());
}

inline bool CopyMissing(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& ec)
{
    namespace fs = std::filesystem;
    ec.clear();
    bool reported = false; // (a failure already logged here: do not log it twice at the end)
    const auto nativeFrom = NativePath(from, ec);
    if (ec) { Fail("source path", from, ec); return false; }
    const auto nativeTo = NativePath(to, ec);
    if (ec) { Fail("destination path", to, ec); return false; }
    fs::create_directories(nativeTo, ec);
    if (ec) { Fail("destination folder", nativeTo, ec); return false; }
    auto prefix = nativeFrom.native();
    if (prefix.empty()) { ec = std::make_error_code(std::errc::invalid_argument); Fail("empty source path", nativeFrom, ec); return false; }
    if (prefix.back() != fs::path::preferred_separator) prefix += fs::path::preferred_separator;
    fs::path last;
    for (fs::recursive_directory_iterator it(nativeFrom, ec), end; !ec && it != end; it.increment(ec))
    {
        last = it->path();
        // Validate the iterator's source prefix before stripping it. Both relative path
        // operations misparse extended Windows paths in some MinGW libraries.
        const auto entry = it->path().native();
        if (entry.size() <= prefix.size() || entry.compare(0, prefix.size(), prefix) != 0)
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            std::fprintf(stderr, "[3ds] kit copy: entry outside the source folder: entry [%s], expected prefix [%s], source [%s]\n",
                         fs::path(entry).u8string().c_str(), fs::path(prefix).u8string().c_str(), nativeFrom.u8string().c_str());
            reported = true;
            break;
        }
        const fs::path relative(entry.substr(prefix.size()));
        const fs::path dest = nativeTo / relative;
        const bool directory = it->is_directory(ec);
        if (ec) { Fail("inspect source entry", it->path(), ec); reported = true; break; }
        if (directory)
        {
            fs::create_directories(dest, ec);
            if (ec) { Fail("make destination folder", dest, ec); reported = true; break; }
        }
        else
        {
            const bool exists = fs::exists(dest, ec);
            if (ec) { Fail("check destination", dest, ec); reported = true; break; }
            if (!exists)
            {
                fs::copy_file(it->path(), dest, ec);
                if (ec)
                {
                    std::fprintf(stderr, "[3ds] kit copy: copy [%s] -> [%s]: %d %s\n",
                                 it->path().u8string().c_str(), dest.u8string().c_str(), ec.value(), ec.message().c_str());
                    reported = true;
                    break;
                }
#ifndef _WIN32
                const auto permissions = fs::status(it->path(), ec).permissions();
                if (!ec) fs::permissions(dest, permissions, ec);
                if (ec) { Fail("copy permissions", dest, ec); reported = true; break; }
#endif
            }
        }
        // iterator.increment(ec) clears ec on success; do not let it hide a failed copy.
        if (ec) break;
    }
    // (An error from the iterator itself, not from a file: name the last entry reached.)
    if (ec && !reported)
        std::fprintf(stderr, "[3ds] kit copy: walking [%s] stopped at [%s]: %d %s\n",
                     nativeFrom.u8string().c_str(), last.u8string().c_str(), ec.value(), ec.message().c_str());
    return !ec;
}
}

#include "../frontend/diagnostics.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string>

static std::string gCopied;
static bool Copy(const std::string& text) { gCopied = text; return true; }

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string dir = argv[1];
    const std::string log = dir + "/game.log";
    {
        std::ofstream out(log, std::ios::binary);
        for (int i = 1; i <= 300; i++) out << "line " << i << (i == 150 ? "\r" : "") << "\n";
    }

    // The platform's own log path wins over anything beside the program.
    Diagnostics::SetLogPath(log);
    assert(Diagnostics::FindLog(dir + "/elsewhere") == log);
    const std::string tail = Diagnostics::Tail(log, 5);
    assert(tail.find("line 296") != std::string::npos);
    assert(tail.find("line 295") == std::string::npos);
    assert(tail.find("line 300") != std::string::npos);

    // A longer file is read from its end, on a line boundary.
    {
        std::ofstream out(dir + "/big.log", std::ios::binary);
        for (int i = 0; i < 20000; i++) out << "filler " << i << " ........................................\n";
        out << "the last line\n";
    }
    const std::string big = Diagnostics::Tail(dir + "/big.log", 2);
    assert(big.find("the last line") != std::string::npos);
    assert(big.size() < 4096);

    // Fields and the tail, with the player's home folder taken out.
    const char* home = std::getenv("HOME");
    bool copied = false;
    const std::string saved = Diagnostics::Report(log,
        { { "version", "1.6" }, { "game", "Platinum" }, { "folder", home ? std::string(home) + "/games" : std::string("/games") } },
        Copy, copied);
    assert(copied);
    assert(!saved.empty());
    assert(gCopied.find("version: 1.6") != std::string::npos);
    assert(gCopied.find("game: Platinum") != std::string::npos);
    assert(gCopied.find("last 200 log lines") != std::string::npos);
    assert(gCopied.find("line 300") != std::string::npos);
    assert(gCopied.find('\r') == std::string::npos);
    if (home && *home) assert(gCopied.find(home) == std::string::npos);

    std::ifstream in(saved, std::ios::binary);
    const std::string written((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert(written == gCopied);

    // No log yet: the report still builds, and says so.
    const std::string missing = Diagnostics::Build(dir + "/not-there.log", {});
    assert(missing.find("the log is empty") != std::string::npos);
    return 0;
}

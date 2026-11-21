// Ruby and Sapphire are games, not ROM hacks (#238): their codes are the cartridge's own, so
// Launcher::IsHack() (which reads AXV and AXP) keeps them out of the hacks tab.
#include <cstdint>
#define SSR_GBA 1
#include "../frontend/gba_games.h"
#include <cassert>
#include <string>

namespace
{
Launcher::Game GbaGame(const std::string& key)
{
    Launcher::Game g;
    g.platform = Launcher::GBA;
    g.known = true;
    g.code = GbaGames::CodeOf(key);
    return g;
}
}

int main()
{
    assert(GbaGames::CodeOf("ruby") == "AXVE");
    assert(GbaGames::CodeOf("sapphire") == "AXPE");

    for (const char* key : { "ruby", "rubyde", "rubyfr", "rubyes", "rubyit", "rubyjp",
                             "sapphire", "sapphirede", "sapphirefr", "sapphirees", "sapphireit", "sapphirejp" })
        assert(!Launcher::IsHack(GbaGame(key)));

    // (A hack still is one: its own code, and a hack's save key.)
    assert(Launcher::IsHack(GbaGame("radicalred")));
    assert(Launcher::IsHack(GbaGame("hack-ruby")));
    return 0;
}

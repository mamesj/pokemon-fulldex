#pragma once
// Included in Launcher. Read entry text from the player's DS files using the
// existing bounded NARC/text decoders; no game text or artwork is bundled.
struct DexEntryInfo
{
    PokemonHub::DexSpeciesData species;
    std::string description, source;
};
struct DexInfoCache
{
    bool loaded = false;
    std::array<DexEntryInfo, 722> entries;
    void Load()
    {
        if (loaded) return;
        loaded = true;
#ifdef SSR_POKEMON_HUB
        for (int id = 1; id <= 721; ++id) entries[id].species = PokemonHub::DexSpeciesDetails(id);
        // Prefer Gen 5's larger catalogue, then fill gaps from Gen 4.
        for (int generation : {5, 4}) for (const auto& game : RomList())
        {
            if (!game.known || game.platform != DS || IsHack(game) || game.code.size() < 4) continue;
            const auto family = game.code.substr(0, 3);
            const bool hg = family == "IPG" || family == "IPK", pt = family == "CPU";
            const bool bw = family == "IRB" || family == "IRA", b2 = family == "IRE" || family == "IRD";
            if ((generation == 5 && !bw && !b2) || (generation == 4 && !hg && !pt)) continue;
            FILE* file = fopen(game.rom.c_str(), "rb");
            if (!file) continue;
            const auto messages = Companion::Narc(RomFileFrom(file, generation == 5 ? "a/0/0/2" : hg ? "a/0/2/7" : "msgdata/pl_msg.narc"));
            const auto personal = Companion::Narc(RomFileFrom(file, generation == 5 ? "a/0/1/6" : hg ? "a/0/0/2" : "poketool/personal/pl_personal.narc"));
            fclose(file);
            const int bank = b2 ? 442 : bw ? 235 : hg ? (family == "IPG" ? 804 : 803) : 706;
            if (bank >= (int)messages.size()) continue;
            const auto text = Companion::Text(messages[bank], generation == 5);
            const int last = generation == 5 ? 649 : 493;
            static const char* types[] = {"Normal", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug", "Ghost", "Steel", "Unknown", "Fire", "Water", "Grass", "Electric", "Psychic", "Ice", "Dragon", "Dark"};
            for (int id = 1; id <= last && id < (int)text.size(); ++id)
            {
                if (!entries[id].description.empty() || text[id].empty()) continue;
                auto& entry = entries[id];
                // Collapse cartridge line/page breaks for a resizable paragraph.
                std::istringstream words(text[id]); std::string word;
                while (words >> word) { if (!entry.description.empty()) entry.description += ' '; entry.description += word; }
                entry.source = ShortName(game);
                if (id < (int)personal.size() && personal[id].size() >= 8)
                {
                    const auto& p = personal[id];
                    auto type = [&](int value) { if (generation == 5 && value >= 9) ++value; return value >= 0 && value < 18 ? std::string(types[value]) : std::string(); };
                    entry.species.type1 = type(p[6]); entry.species.type2 = p[7] == p[6] ? "" : type(p[7]);
                    entry.species.stats = {p[0], p[1], p[2], p[4], p[5], p[3]};
                }
            }
        }
#endif
    }
};

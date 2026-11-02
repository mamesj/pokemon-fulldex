#pragma once
// Tells the official Pokemon cartridges' ROMs from ROM hacks: a ROM is official when the CRC32 of
// the whole file is one of the No-Intro dumps below (DS and GBA, every region), or one the player
// lists in originals.txt (one hex CRC32 per line) beside the roms folder. A hack changes the file,
// so its CRC is in neither. Hashing a 512 MB DS ROM takes seconds, so each result is kept in
// rom-verify.cache (size, modified time, CRC) and a file is hashed again only when it changes.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <sys/stat.h>

namespace Official
{

inline bool KnownCrc(uint32_t crc)
{
    static const uint32_t table[] = {
    0x0041AE66u, // Pokemon - Versione Argento SoulSilver (Italy)
    0x018996B2u, // Pokemon Toroje (Korea)
    0x01BD60E3u, // Pocket Monsters - Sapphire (Japan) (Rev 1)
    0x0203083Eu, // Pokemon Bulgasaui Dungeon - Eodumui Tamheomdae (Korea)
    0x025DDADCu, // Pokemon Ranger (Japan) (Rev 1)
    0x06CE7DD3u, // Pokemon - Edicion Negra 2 (Spain) (NDSi Enhanced)
    0x0989A8A1u, // Pokemon Conquest (USA, Australia) (NDSi Enhanced)
    0x0A48556Bu, // Pocket Monsters - LeafGreen (Japan)
    0x0A994C0Cu, // Pokemon - Goldene Edition HeartGold (Germany)
    0x0ABA4256u, // Pokemon Conquest (Europe) (NDSi Enhanced)
    0x0C7992A9u, // Pokemon - Versione Zaffiro (Italy)
    0x0D8B2B06u, // Pokemon Dash (USA)
    0x0DDBE664u, // Pokemon - Versione Platino (Italy)
    0x0DFE5C99u, // Pokemon Ranger - Shadows of Almia (Europe) (En,Fr,De,Es,It) (Wii U Virtual Console)
    0x0E3670CDu, // Pokemon - Edicion Oro HeartGold (Spain)
    0x100F8DA2u, // Pokemon Ranger (Japan) (Rev 1) (Wii U Virtual Console)
    0x10DF0E03u, // Pokemon Ranger - Guardian Signs (Europe) (En,Fr,De,Es,It)
    0x11870BD9u, // Pokemon - Leaf Green (USA) (Pirate)
    0x15E1E280u, // Pokemon - Rubin-Edition (Germany)
    0x1658A03Au, // Pokemon Ranger (Europe) (En,Fr,De,Es,It)
    0x16974506u, // Pokemon - Versione Verde Foglia (Italy)
    0x1A1BAA50u, // Pokemon Fushigi no Dungeon - Aka no Kyuujotai (Japan)
    0x1A81EEDFu, // Pokemon - Feuerrote Edition (Germany)
    0x1ED511ABu, // Pokemon - Versione Diamante (Italy) (Rev 5)
    0x1F1C08FBu, // Pokemon - Emerald Version (USA, Europe)
    0x20A378A5u, // Pokemon Bulgasaui Dungeon - Parang Gujodae (Korea)
    0x22DDE080u, // Pokemon Mystery Dungeon - Explorers of Sky (USA)
    0x23850525u, // Pocket Monsters - HeartGold (Korea)
    0x24C80FA6u, // Pokemon - Aurora Ticket Distribution (USA) (Kiosk)
    0x260190E1u, // Pokemon Ranger (USA) (Wii U Virtual Console)
    0x2643663Du, // Pokemon Ranger - Shadows of Almia (USA) (Wii U Virtual Console)
    0x2646AE79u, // Pokemon - Versione Oro HeartGold (Italy)
    0x27A56E8Eu, // Pokemon Ranger - Hikari no Kiseki (Japan) (Wii U Virtual Console)
    0x27F31411u, // Pokemon - Version Blanche 2 (France) (NDSi Enhanced)
    0x2912E76Eu, // Pokemon - Aura Mew Distribution (Europe) (Kiosk)
    0x29C68B2Bu, // Pocket Monsters - Black 2 (Japan) (Rev 1) (NDSi Enhanced)
    0x2AD292A4u, // Pokemon - Aura Mew Distribution (France) (Kiosk)
    0x2AE49146u, // Pokemon - Version Saphir (France) (Rev 1)
    0x2CA11D59u, // Pokemon - Edicion Verde Hoja (Spain)
    0x2CA88FAAu, // Pokemon - Edicion Negra (Spain) (NDSi Enhanced)
    0x2E6901A8u, // Pokemon - Version Diamant (France) (Rev 5)
    0x2F025937u, // Pokemon - Versione Bianca (Italy) (NDSi Enhanced)
    0x2FE1A094u, // Pocket Monsters - Pearl (Japan) (Rev 5)
    0x308F9D56u, // Pokemon - Version Noire (France) (NDSi Enhanced)
    0x31554C22u, // Pokemon - FireRed Version (USA, Europe) (Rev 10) (Switch)
    0x32657121u, // Pokemon - Edicion Blanca (Spain) (NDSi Enhanced)
    0x32F2186Du, // Pokemon Mystery Dungeon - Blue Rescue Team (USA) (Wii U Virtual Console)
    0x33AF46D5u, // Pocket Monsters - Black (Japan) (NDSi Enhanced)
    0x34C9DF89u, // Pokemon - Smaragd-Edition (Germany)
    0x3581A05Fu, // Pokemon - Version Saphir (France)
    0x36D0A539u, // Pokemon - Version Argent SoulSilver (France)
    0x36E1D4E0u, // Pokemon Fushigi no Dungeon - Ao no Kyuujotai (Japan) (Rev 1) (Wii U Virtual Console)
    0x37E9D9E5u, // Pokemon - Platin-Edition (Germany)
    0x3B0F21C6u, // Pokemon - Version Perle (France) (Rev 5)
    0x3B2056E9u, // Pocket Monsters - FireRed (Japan)
    0x3EFB7C88u, // Pokemon Plus Nobunaga no Yabou (Japan) (NDSi Enhanced)
    0x40686C47u, // Pokemon - Rubin-Edition (Germany) (Debug Version)
    0x486D837Bu, // Pokemon Ranger (Japan)
    0x4881F3F8u, // Pocket Monsters - Emerald (Japan)
    0x49DDF652u, // Pokemon - LeafGreen Version (USA, Europe) (Rev 10) (Switch)
    0x4B3E6450u, // Pokemon - Edicion Perla (Spain) (Rev 5)
    0x4D4433D9u, // Pokemon - Versione Bianca 2 (Italy) (NDSi Enhanced)
    0x4F6E5580u, // Pokemon - Black Version (USA, Europe) (NDSi Enhanced)
    0x50238ECBu, // Pokemon - Edicion Plata SoulSilver (Spain)
    0x50DD7518u, // Pocket Monsters - Pearl (Japan)
    0x523E8A13u, // Pocket Monsters - Black (Korea) (NDSi Enhanced)
    0x533A6E55u, // Pokemon - Version Or HeartGold (France)
    0x53A6E192u, // Pokemon Mystery Dungeon - Explorers of Sky (Europe) (En,Fr,De,Es,It) (Wii U Virtual Console)
    0x54EFE367u, // Pokemon Ranger - Guardian Signs (Europe) (En,Fr,De,Es,It) (Wii U Virtual Console)
    0x554DEDC4u, // Pokemon - Sapphire Version (USA, Europe)
    0x59641A93u, // Pokemon Dash (Europe) (En,Fr,De,Es,It)
    0x5A7A126Au, // Strongest Pokemon Distribution (Japan) (Wi-Fi Kiosk) (Save Data)
    0x5A8AAEDBu, // Pokemon Mystery Dungeon - Explorers of Time (Europe) (En,Fr,De,Es,It)
    0x5D2537E9u, // Pokemon Mystery Dungeon - Explorers of Sky (Europe) (En,Fr,De,Es,It)
    0x5D8D853Au, // Pokemon Mystery Dungeon - Explorers of Darkness (Europe) (En,Fr,De,Es,It)
    0x5DC668F6u, // Pokemon - Version Rouge Feu (France)
    0x5EFE4493u, // Pokemon - Versione Zaffiro (Italy) (Rev 1)
    0x5F13D9F7u, // Pokemon Dash (Korea)
    0x5F520677u, // Pokemon Ranger - Shadows of Almia (USA)
    0x606C9ADBu, // Pocket Monsters - Ruby (Japan) (Pokemon Box)
    0x61641576u, // Pokemon - Ruby Version (USA, Europe) (Rev 1)
    0x6346FD59u, // Pokemon - Doel Deoxys Distribution (Netherlands) (Ja) (Kiosk)
    0x66B25E29u, // Pocket Monsters - Diamond (Japan) (Rev 5)
    0x672539F1u, // Pocket Monsters - White (Korea) (NDSi Enhanced)
    0x690FD310u, // Pokemon - Version Rubis (France)
    0x691A264Du, // Pocket Monsters - SoulSilver (Korea)
    0x69D628E8u, // Pokemon - Platinum Version (USA) (Rev 1)
    0x69F9CE34u, // Pokemon Fushigi no Dungeon - Sora no Tankentai (Japan) (Rev 2)
    0x6AA7AA37u, // Pokemon - 10th Anniversary Distribution (France) (Kiosk)
    0x6C632E0Bu, // Pokemon Ranger (USA)
    0x6D276C92u, // Pokemon Card Game - Asobikata DS (Japan)
    0x6FCD7A98u, // Pokemon - Saphir-Edition (Germany)
    0x70AE5C10u, // Pocket Monsters Pt - Giratina (Korea)
    0x71ACA63Au, // Pokemon - Platinum Version (Europe)
    0x732BC6DCu, // Pokemon - Versione Nera (Italy) (NDSi Enhanced)
    0x73A72167u, // Pokemon - Versione Rosso Fuoco (Italy)
    0x73D4AD7Au, // Learn with Pokemon - Typing Adventure (Europe)
    0x7641BF1Bu, // Lernen mit Pokemon - Tasten-Abenteuer (Germany)
    0x777EB04Fu, // Pokemon - White Version 2 (USA, Europe) (NDSi Enhanced)
    0x7BB14936u, // Pokemon - Weisse Edition 2 (Germany) (NDSi Enhanced)
    0x7E3BA6A2u, // Pokemon - 10th Anniversary Distribution (Europe) (Kiosk)
    0x82658753u, // Pokemon - 10th Anniversary Distribution (Italy) (Kiosk)
    0x84427823u, // Pokemon - Diamond Version (USA) (Rev 5)
    0x84EE4776u, // Pokemon - FireRed Version (USA, Europe) (Rev 1)
    0x86A602ABu, // Pokemon - Sapphire Version - Gotta Catch 'em All! (USA) (Pirate)
    0x87CF7B1Fu, // Pokemon - Diamant-Edition (Germany) (Rev 5)
    0x8ADF152Bu, // Pokemon Mystery Dungeon - Explorers of Sky (USA) (Wii U Virtual Console)
    0x8AF1EC73u, // Pokemon Pinball - Ruby & Sapphire (Japan)
    0x8B4CBA44u, // Pokemon - Version Blanche (France) (NDSi Enhanced)
    0x8C4D3108u, // Pokemon - Edicion Esmeralda (Spain)
    0x8DA4601Au, // Pocket Monsters - LeafGreen (Japan) (Rev 1)
    0x8E0044A4u, // Impara con Pokemon - Avventura Tra i Tasti (Italy)
    0x8F9ED0D9u, // Pokemon Fushigi no Dungeon - Sora no Tankentai (Japan)
    0x8FE6A84Cu, // Pocket Monsters - Pearl (Japan) (Rev 6)
    0x9050E5DAu, // Pokemon Fushigi no Dungeon - Yami no Tankentai (Japan)
    0x91E863E7u, // Pokemon - Versione Nera 2 (Italy) (NDSi Enhanced)
    0x9253921Du, // Pokemon - Platinum Version (USA)
    0x925A5684u, // Pokemon Pinball - Ruby & Sapphire (Japan) (Rev 1)
    0x9305E793u, // Pokemon - Versione Rubino (Italy) (Rev 1)
    0x94E4A5B5u, // Pokemon - Silberne Edition SoulSilver (Germany)
    0x95A4FCABu, // Pokemon - Perl-Edition (Germany) (Rev 5)
    0x95CF004Du, // Pokemon - Weisse Edition (Germany) (NDSi Enhanced)
    0x968CDE08u, // Pokemon - SoulSilver Version (Europe)
    0x9928FD8Du, // Pocket Monsters - Black 2 (Korea) (NDSi Enhanced)
    0x99AFB52Fu, // Pokemon - Version Noire 2 (France) (NDSi Enhanced)
    0x9A704AF8u, // Pokemon - Aura Mew Distribution (Germany) (Kiosk)
    0x9B4C38AEu, // Pokemon - Edicion Platino (Spain)
    0x9CC4410Eu, // Pokemon - Sapphire Version (USA, Europe) (Rev 2)
    0x9E5F97B1u, // Pokemon Mystery Dungeon - Blue Rescue Team (Europe) (En,Fr,De,Es,It) (Wii U Virtual Console)
    0x9EAEA813u, // Pocket Monsters - Sapphire (Japan) (Pokemon Box)
    0x9F08064Eu, // Pokemon - Edicion Rojo Fuego (Spain)
    0x9F981F72u, // Pokemon - Version Rubis (France) (Rev 1)
    0x9F9C293Du, // Pocket Monsters - Pearl (Japan) (Rev 1)
    0xA0166347u, // Pokemon Conquest (USA, Australia) (Rev 1) (NDSi Enhanced)
    0xA04F5F0Bu, // Pokemon - Edicion Zafiro (Spain)
    0xA0AEC80Au, // Pokemon - Versione Smeraldo (Italy)
    0xA28B5A7Au, // Pokemon Mystery Dungeon - Blue Rescue Team (Europe) (En,Fr,De,Es,It)
    0xA3FDCCB1u, // Pokemon - Version Emeraude (France)
    0xA53AC14Bu, // Pokemon Pinball - Ruby & Sapphire (Europe) (En,Fr,De,Es,It)
    0xA54A9984u, // Pokemon - Version Platine (France)
    0xA5CA6241u, // Pokemon Ranger - Guardian Signs (USA, Australia)
    0xA6374EA6u, // Pokemon Trose (Japan)
    0xA9833314u, // Pocket Monsters - White 2 (Japan) (Rev 1) (NDSi Enhanced)
    0xAA3E6B39u, // Pokemon - Edicion Diamante (Spain) (Rev 5)
    0xAA6224A2u, // Pocket Monsters - Diamond (Japan) (Rev 6)
    0xAB691637u, // Pokemon Ranger (Europe) (En,Fr,De,Es,It) (Wii U Virtual Console)
    0xAE2DB9A9u, // Pocket Monsters - Diamond (Japan)
    0xAE57B39Eu, // Pokemon - Aura Mew Distribution (Spain) (Kiosk)
    0xAEAC73E6u, // Pokemon - Ruby Version (USA, Europe) (Rev 2)
    0xB03837F1u, // Pokemon Bulgasaui Dungeon - Siganui Tamheomdae (Korea)
    0xB0C40C7Cu, // Pokemon - Saphir-Edition (Germany) (Rev 1)
    0xB1458DA6u, // Pokemon Link! (Europe) (En,Fr,De,Es,It)
    0xB185A382u, // Pocket Monsters DP - Dialga (Korea)
    0xB448A337u, // Pocket Monsters - Ruby (Japan) (Rev 1)
    0xB552501Cu, // Pokemon - White Version (USA, Europe) (NDSi Enhanced)
    0xB5D625D9u, // Battle & Get! - Pokemon Typing DS (Japan)
    0xB64A5EFBu, // Pokemon - HeartGold Version (Europe)
    0xB6C4143Eu, // Pokemon Mystery Dungeon - Blue Rescue Team (USA, Australia)
    0xB798013Eu, // Pokemon Fushigi no Dungeon - Sora no Tankentai (Japan) (Rev 2) (Wii U Virtual Console)
    0xB980FFF5u, // Pokemon - Edicion Rubi (Spain) (Rev 1)
    0xB992A3C0u, // Pokemon Pinball - Ruby & Sapphire (USA)
    0xBA3285E3u, // Pokemon - Version Vert Feuille (France)
    0xBA6D0397u, // Pokemon - Pearl Version (Europe) (Rev 5)
    0xBAFEDAE5u, // Pokemon - Sapphire Version (USA, Europe) (Rev 1)
    0xBB640DF7u, // Pocket Monsters - FireRed (Japan) (Rev 1)
    0xBC378DE8u, // Pokemon Fushigi no Dungeon - Toki no Tankentai (Japan)
    0xBD728697u, // Apprends avec Pokemon A la Conquete du Clavier (France)
    0xBE130BC9u, // Pocket Monsters - Platinum (Japan)
    0xBF479590u, // Pokemon Fushigi no Dungeon - Ao no Kyuujotai (Japan)
    0xC180A0E9u, // Pokemon - HeartGold Version (USA)
    0xC18231A9u, // Pokemon - Versione Rubino (Italy)
    0xC1D18FE4u, // Pokemon Mystery Dungeon - Red Rescue Team (Europe) (En,Fr,De,Es,It)
    0xC547D653u, // Pokemon Ranger - Batonnage (Japan)
    0xC6EACC7Du, // Pokemon - Versione Perla (Italy) (Rev 5)
    0xC91DE98Eu, // Aprende Con Pokemon - Aventura Entre Las Teclas (Spain)
    0xCAA820CBu, // Pocket Monsters - White (Japan) (NDSi Enhanced)
    0xCAE89464u, // Pokemon - Rubin-Edition (Germany) (Rev 1)
    0xCC4C2BEFu, // Pocket Monsters - SoulSilver (Japan)
    0xCD08F2B2u, // Pokemon - 10th Anniversary Distribution (Spain) (Kiosk)
    0xCE81E95Au, // Pokemon Ranger - Batonnage (Japan) (Wii U Virtual Console)
    0xCEE9471Au, // Pocket Monsters - Ruby (Japan)
    0xD04A0344u, // Pocket Monsters DP - Palkia (Korea)
    0xD111CC19u, // Pokemon - Aura Mew Distribution (Italy) (Kiosk)
    0xD12F1FDDu, // Pokemon - Blattgruene Edition (Germany)
    0xD3E316EFu, // Pocket Monsters - White 2 (Korea) (NDSi Enhanced)
    0xD4427FD1u, // Pokemon - Black Version 2 (USA, Europe) (NDSi Enhanced)
    0xD4F9C8E7u, // Pokemon Mystery Dungeon - Explorers of Time (USA)
    0xD69C96CCu, // Pokemon - LeafGreen Version (USA, Europe)
    0xD78B5A1Fu, // Pokemon - Liechi Berry Glitch Fix & Shiny Zigzagoon Distribution (Europe) (Kiosk)
    0xD8EA6090u, // Pokemon - SoulSilver Version (USA, Australia)
    0xDAFFECECu, // Pokemon - LeafGreen Version (USA, Europe) (Rev 1)
    0xDBF39320u, // Pokemon - 10th Anniversary Distribution (Germany) (Kiosk)
    0xDCF7050Bu, // Pokemon - Diamond Version (Europe) (Rev 5)
    0xDD0AC86Cu, // Pokemon Mystery Dungeon - Red Rescue Team (USA, Australia)
    0xDD88761Cu, // Pokemon - FireRed Version (USA, Europe)
    0xDF2DD2B0u, // Pokemon Ranger - Guardian Signs (USA) (Wii U Virtual Console)
    0xE2A6A6CFu, // Pocket Monsters - Diamond (Japan) (Rev 1)
    0xE2D87EBFu, // Pokemon - Pearl Version (USA) (Rev 5)
    0xE31566CFu, // Pocket Monsters Diamond - Pocket Monsters Pearl - Manaphy Present Campaign Senyou Cartridge (Japan) (En)
    0xE4D657EBu, // Pokemon - Slot 2 Distribution (Japan) (Proto)
    0xE752BBA0u, // Pokemon Ranger - Shadows of Almia (Europe) (En,Fr,De,Es,It)
    0xEA70C572u, // Pokemon - Edicion Blanca 2 (Spain) (NDSi Enhanced)
    0xEB0729CFu, // Pokemon - Edicion Rubi (Spain)
    0xED8751A6u, // Pokemon Trozei! (USA)
    0xEE469543u, // Pokemon - Schwarze Edition (Germany) (NDSi Enhanced)
    0xF0815EE7u, // Pokemon - Ruby Version (USA, Europe)
    0xF2C88931u, // Pokemon - Edicion Zafiro (Spain) (Rev 1)
    0xF2EE539Eu, // Pokemon Mystery Dungeon - Explorers of Darkness (USA)
    0xF4DCA07Cu, // Pokemon Ranger - Hikari no Kiseki (Japan)
    0xFA496FD0u, // Pokemon - Schwarze Edition 2 (Germany) (NDSi Enhanced)
    0xFB65485Au, // Pokemon Fushigi no Dungeon - Ao no Kyuujotai (Japan) (Rev 1)
    0xFC7D8F28u, // Pocket Monsters - HeartGold (Japan)
    0xFCF86579u, // Pokemon Dash (Japan)
    0xFD1EEB78u, // Pocket Monsters - Sapphire (Japan)
    0x25BA3AA6u, // Black 2 (USA) [b] dump
    0xE2BEE619u, // Black (USA) [b] dump
    0xB979777Bu, // Edicion Blanca (Spain) [b] dump
    0x84E882FFu, // Edicion Oro HeartGold (Spain) alt dump
    };
    for (uint32_t c : table)
        if (c == crc) return true;
    return false;
}

inline uint32_t Crc32(FILE* f)
{
    static uint32_t t[256];
    if (!t[1])
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ c >> 1 : c >> 1;
            t[i] = c;
        }
    static unsigned char buf[1 << 16];
    uint32_t crc = ~0u;
    for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;)
        for (size_t i = 0; i < n; i++) crc = t[(crc ^ buf[i]) & 0xFF] ^ crc >> 8;
    return ~crc;
}

// Where the results are kept, and the player's own list of good dumps (set by the launcher).
inline std::string& Folder() { static std::string f; return f; }

struct Entry { long long size, mtime; uint32_t crc; };
inline std::map<std::string, Entry>& Cache()
{
    static std::map<std::string, Entry> m;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        if (FILE* f = fopen((Folder() + "/rom-verify.cache").c_str(), "r"))
        {
            char path[2048];
            long long size, mtime;
            unsigned crc;
            while (fscanf(f, "%lld %lld %x %2047[^\n]", &size, &mtime, &crc, path) == 4) m[path] = { size, mtime, crc };
            fclose(f);
        }
    }
    return m;
}

inline bool Listed(uint32_t crc)
{
    if (FILE* f = fopen((Folder() + "/originals.txt").c_str(), "r"))
    {
        char line[128];
        bool hit = false;
        while (fgets(line, sizeof line, f))
            if ((uint32_t)strtoul(line, nullptr, 16) == crc && line[0] != '#') hit = true;
        fclose(f);
        return hit;
    }
    return false;
}

// The CRC32 of a file (0 and false if unreadable), from the cache when it is unchanged.
inline bool FileCrc(const std::string& path, uint32_t& crc)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    auto& cache = Cache();
    auto it = cache.find(path);
    if (it != cache.end() && it->second.size == (long long)st.st_size && it->second.mtime == (long long)st.st_mtime)
    {
        crc = it->second.crc;
        return true;
    }
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    crc = Crc32(f);
    fclose(f);
    cache[path] = { (long long)st.st_size, (long long)st.st_mtime, crc };
    if (Folder().empty()) return true;
    if (FILE* out = fopen((Folder() + "/rom-verify.cache").c_str(), "a"))
    {
        fprintf(out, "%lld %lld %08x %s\n", (long long)st.st_size, (long long)st.st_mtime, crc, path.c_str());
        fclose(out);
    }
    return true;
}

// Whether the ROM at `path` is an official dump (a hack, a patched or an unreadable file is not).
inline bool Verified(const std::string& path)
{
    uint32_t crc;
    return FileCrc(path, crc) && (KnownCrc(crc) || Listed(crc));
}

}

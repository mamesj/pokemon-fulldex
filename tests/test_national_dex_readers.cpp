// Synthetic game-format saves only. No ROMs or player saves are required.
#include "pokemon_hub.h"
#include "sav/SavRS.hpp"
#include "sav/SavE.hpp"
#include "sav/SavFRLG.hpp"
#include "sav/SavDP.hpp"
#include "sav/SavPT.hpp"
#include "sav/SavHGSS.hpp"
#include "sav/SavBW.hpp"
#include "sav/SavB2W2.hpp"
#include "sav/SavXY.hpp"
#include "sav/SavORAS.hpp"
#include "utils/endian.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace pksm;
static void Write(const std::filesystem::path& p, const std::shared_ptr<u8[]>& bytes, size_t size)
{ std::ofstream f(p,std::ios::binary); f.write((const char*)bytes.get(),size); }
static std::vector<char> Read(const std::filesystem::path& p)
{ std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
template<class T> void Check(const std::filesystem::path& root, int generation, size_t size, int dex, int marker = 0, bool second = false)
{
    auto bytes = std::shared_ptr<u8[]>(new u8[size]{});
    if (generation == 3)
    {
        for (int slot=0;slot<2;++slot) for (int block=0;block<14;++block)
        {
            const int offset=slot*0xe000+block*0x1000;
            LittleEndian::convertFrom<u16>(&bytes[offset+0xff4],block);
            LittleEndian::convertFrom<u32>(&bytes[offset+0xffc],slot ? 0 : 1);
        }
        if constexpr(std::is_same_v<T,SavFRLG>) bytes[0xac]=1;
        if constexpr(std::is_same_v<T,SavE>) { bytes[0xac]=2; bytes[0xee0]=1; }
    }
    if (generation == 4)
    {
        const int shift=second ? 0x40000 : 0;
        LittleEndian::convertFrom<u32>(&bytes[shift+marker-12],marker & 0xffff);
        LittleEndian::convertFrom<u32>(&bytes[shift+marker-8],0x20060623);
        // General block counter; select the second slot explicitly when requested.
        if (second) LittleEndian::convertFrom<u32>(&bytes[marker-20+shift],10);
    }
    T save(bytes);
    save.language(Language::ENG);
    const int last = generation==3 ? 386 : generation==4 ? 493 : generation==5 ? 649 : 721;
    const int caught = generation==3 ? 0x10 : generation==4 ? 4 : 8;
    const int seen = generation<=4 ? 0x44 : generation==5 ? 0x5c : 0x68;
    const int stride = generation==5 ? 0x54 : generation==6 ? 0x60 : 0;
    const int banks = generation>=5 ? 4 : 1;
    for(int bank=0;bank<banks;++bank) bytes[dex+seen+bank*stride] |= 1u << bank;
    bytes[dex+caught+(last-1)/8] |= 1u << ((last-1)%8);
    bool s=false,c=false;
    for(int bank=0;bank<banks;++bank) { assert(save.speciesDex(Species{u16(bank+1)},s,c) && s && !c); }
    assert(save.speciesDex(Species{u16(last)},s,c) && c && !s);
    assert(!save.speciesDex(Species{0},s,c) && !s && !c);
    assert(!save.speciesDex(Species{u16(last+1)},s,c));
    save.finishEditing(); assert(save.checksumsValid());
    const auto path=root/"synthetic.sav"; Write(path,bytes,size); const auto before=Read(path);
    PokemonHub::DexSnapshot result; std::string error;
    if(!PokemonHub::ReadDex(path.string(),result,error)) { std::cerr << generation << ": " << error << '\n'; std::abort(); }
    assert(result.generation == generation && result.caught[last] && result.seen[last]);
    for(int bank=0;bank<banks;++bank) assert(result.seen[bank+1]);
    assert(Read(path)==before);
    assert(!PokemonHub::ReadDex(path.string(),result,error,true));
    bytes[dex+seen] ^= 0x80; Write(path,bytes,size);
    assert(!PokemonHub::ReadDex(path.string(),result,error));
}
int main()
{
    const auto root=std::filesystem::temp_directory_path()/("fulldex-dex-readers-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    Check<SavRS>(root,3,0x20000,0x18);
    Check<SavE>(root,3,0x20000,0x18);
    Check<SavFRLG>(root,3,0x20000,0x18);
    Check<SavDP>(root,4,0x80000,0x12dc,0xc100);
    Check<SavPT>(root,4,0x80000,0x1328,0xcf2c);
    Check<SavHGSS>(root,4,0x80000,0x12b8,0xf628);
    Check<SavDP>(root,4,0x80000,0x412dc,0xc100,true);
    Check<SavBW>(root,5,0x80000,0x21600);
    Check<SavB2W2>(root,5,0x80000,0x21400);
    Check<SavXY>(root,6,0x65600,0x15000);
    Check<SavORAS>(root,6,0x76000,0x15000);
    PokemonHub::DexSnapshot dex; std::string error;
    assert(!PokemonHub::ReadDex((root/"missing").string(),dex,error));
    {std::ofstream f(root/"short"); f<<"invalid";}
    assert(!PokemonHub::ReadDex((root/"short").string(),dex,error));
    auto blank=std::shared_ptr<u8[]>(new u8[0x80000]{});Write(root/"blank",blank,0x80000);
    assert(!PokemonHub::ReadDex((root/"blank").string(),dex,error));
    assert(PokemonHub::DexSpeciesName(1)=="BULBASAUR" && !PokemonHub::DexSpeciesName(721).empty());
    auto pika=PokemonHub::DexSpeciesDetails(25);
    assert(pika.type1=="Electric" && pika.type2.empty() && pika.stats[0]==35 && pika.stats[5]==90);
    auto bulba=PokemonHub::DexSpeciesDetails(1);
    assert(bulba.type1=="Grass" && bulba.type2=="Poison");
    assert(PokemonHub::DexSpeciesDetails(0).type1.empty() && PokemonHub::DexSpeciesDetails(722).type1.empty());
    std::filesystem::remove_all(root); std::cout<<"National Dex save reader checks passed\n";
}

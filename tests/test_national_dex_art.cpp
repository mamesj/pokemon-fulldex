#include "national_dex_art.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace NationalDexArt;
static void W16(Bytes& b,size_t at,unsigned value) {b[at]=value;b[at+1]=value>>8;}
static void W32(Bytes& b,size_t at,unsigned value) {W16(b,at,value);W16(b,at+2,value>>16);}
static void Magic(Bytes& b,size_t at,const char* value) {std::memcpy(b.data()+at,value,4);}
static void Write(const std::filesystem::path& path,const Bytes& b)
{std::ofstream f(path,std::ios::binary);f.write((const char*)b.data(),b.size());}
int main()
{
    const auto root=std::filesystem::temp_directory_path()/("fulldex-art-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    Bytes b(64*2+40);const size_t footer=b.size()-40;
    Magic(b,footer,"CLIM");W16(b,footer+4,0xfeff);Magic(b,footer+20,"imag");W16(b,footer+28,8);W16(b,footer+30,8);b[footer+32]=7;
    W16(b,0,0xf801); // Opaque red at (0,0).
    W16(b,2,0x07c1); // Opaque green at (1,0).
    W16(b,4,0x003f); // Opaque blue at (0,1), Morton order.
    Image image;assert(Bclim(b,image));assert(image.width==8 && image.height==8);
    assert(image.pixels[0]==0xffff0000 && image.pixels[1]==0xff00ff00 && image.pixels[8]==0xff0000ff && image.pixels[2]==0);
    auto invalid=b;W16(invalid,footer+28,0xffff);assert(!Bclim(invalid,image));
    invalid=b;invalid.resize(40);assert(!Bclim(invalid,image));
    Bytes compressed{0x10,4,0,0,0x40,0x42,0,0};assert(Uncompress(compressed));assert(compressed==Bytes(4,0x42));
    compressed={0x11,4,0,0,0x40,0x42,0x20,0};assert(Uncompress(compressed));assert(compressed==Bytes(4,0x42));
    compressed={0x10,4,0,0,0x80,0,0};assert(!Uncompress(compressed));
    // Tiny synthetic GARC with two entries and a BCLIM in entry one.
    Bytes garc(28+20+44+12+b.size()); Magic(garc,0,"CRAG");W32(garc,4,28);W16(garc,8,0xfeff);W16(garc,10,0x400);W32(garc,16,104);
    Magic(garc,28,"OTAF");W32(garc,32,20);W16(garc,36,2);W32(garc,40,0);W32(garc,44,16);
    Magic(garc,48,"BTAF");W32(garc,52,44);W32(garc,56,2);W32(garc,60,1);W32(garc,76,1);W32(garc,84,b.size());W32(garc,88,b.size());
    Magic(garc,92,"BMIF");std::copy(b.begin(),b.end(),garc.begin()+104);
    Bytes member;assert(GarcMember(garc,1,member) && member==b);assert(!GarcMember(garc,2,member));
    auto corrupt=garc;W32(corrupt,84,0xffffffff);W32(corrupt,88,0xffffffff);assert(!GarcMember(corrupt,1,member));
    // GBA header points to synthetic tile and palette tables.
    Bytes gba(8192);std::memcpy(gba.data()+0x108,"pokemon emerald version",23);
    W32(gba,0x138,0x08001000);W32(gba,0x13c,0x08001100);W32(gba,0x140,0x08001200);
    W32(gba,0x1004,0x08001400);W32(gba,0x1200,0x08001300);W16(gba,0x1302,31);gba[0x1400]=1;
    Write(root/"gba",gba);File gbaFile((root/"gba").string());assert(GbaIcon(gbaFile,1,image));assert(image.pixels[0]==0xffff0000 && image.pixels[1]==0);
    assert(!GbaIcon(gbaFile,0,image));
    // Synthetic decrypted NCCH -> IVFC -> RomFS, with a single root file.
    Bytes rom(0x6000);Magic(rom,0x100,"NCCH");rom[0x18f]=4;W32(rom,0x1b0,1);W32(rom,0x1b4,0x2f);
    Magic(rom,0x200,"IVFC");W32(rom,0x208,0x20);W32(rom,0x244,0x3000);W32(rom,0x24c,12);
    const size_t lv=0x1200;W32(rom,lv,0x28);W32(rom,lv+12,0x28);W32(rom,lv+16,24);W32(rom,lv+28,0x40);W32(rom,lv+32,40);W32(rom,lv+36,0x70);
    W32(rom,lv+0x28+8,0xffffffff);W32(rom,lv+0x28+12,0);
    W32(rom,lv+0x40+4,0xffffffff);W32(rom,lv+0x40+16,garc.size());W32(rom,lv+0x40+28,8);
    for(int i=0;i<4;++i)W16(rom,lv+0x60+i*2,"test"[i]);std::copy(garc.begin(),garc.end(),rom.begin()+lv+0x70);
    Write(root/"rom",rom);File romFile((root/"rom").string());Bytes archive;assert(RomFsFile(romFile,"test",archive) && archive==garc);assert(!RomFsFile(romFile,"missing",archive));
    rom[0x18f]=0;Write(root/"encrypted",rom);File encrypted((root/"encrypted").string());assert(!RomFsFile(encrypted,"test",archive));
    std::filesystem::remove_all(root);std::cout<<"National Dex artwork checks passed\n";
}

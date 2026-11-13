#pragma once
// Read-only menu sprite decoders. No artwork is shipped or fetched from the network.
// Format references: pret/pokeemerald src/rom_header_gf.c; Project Pokémon's Gen 6
// filesystem research; kwsch/pk3DS CTR/Images; d0k3/GodMode9 game/romfs.h.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace NationalDexArt
{
using Bytes = std::vector<uint8_t>;
struct Image { int width = 0, height = 0; std::vector<uint32_t> pixels; };
inline bool Range(uint64_t size, uint64_t at, uint64_t count) { return at <= size && count <= size - at; }
inline uint16_t U16(const Bytes& b, size_t at) { return uint16_t(b[at]) | uint16_t(b[at+1]) << 8; }
inline uint32_t U32(const Bytes& b, size_t at) { return U16(b,at) | uint32_t(U16(b,at+2)) << 16; }
inline uint64_t U64(const Bytes& b, size_t at) { return U32(b,at) | uint64_t(U32(b,at+4)) << 32; }
class File
{
    std::ifstream input;
public:
    uint64_t size = 0;
    explicit File(const std::string& path) : input(std::filesystem::u8path(path),std::ios::binary)
    { input.seekg(0,std::ios::end); const auto n=input.tellg(); if(n>=0) size=uint64_t(n); }
    bool Read(uint64_t at, size_t count, Bytes& out)
    {
        out.clear();
        if(count > 32*1024*1024 || !Range(size,at,count)) return false;
        input.clear(); input.seekg(at); out.resize(count);
        if(!input.read(reinterpret_cast<char*>(out.data()),count)) {out.clear();return false;}
        return true;
    }
};
inline bool GbaIcon(File& f, int internalSpecies, Image& image)
{
    image={}; if(internalSpecies<1 || internalSpecies>411) return false;
    Bytes head;
    if(!f.Read(0,size_t(std::min<uint64_t>(4096,f.size)),head)) return false;
    // GFRomHeader is an intentional public graphics interface, shared by FRLG/E.
    // Locate its fixed game-name field rather than hardcoding regional ROM addresses.
    for(size_t name=8;name+60<=head.size();name+=4)
    {
        if(std::memcmp(head.data()+name,"pokemon ",8)) continue;
        auto ptr=[&](const Bytes& b,size_t at)->uint64_t {
            const uint32_t p=U32(b,at); return p>=0x08000000 && p<0x0a000000 ? p-0x08000000 : f.size;
        };
        const uint64_t icons=ptr(head,name+48), indices=ptr(head,name+52), palettes=ptr(head,name+56);
        Bytes entry,index,paletteEntry,gfx,palette;
        if(!f.Read(icons+internalSpecies*4,4,entry) || !f.Read(indices+internalSpecies,1,index) || index[0]>=6 ||
           !f.Read(palettes+index[0]*8,8,paletteEntry) || !f.Read(ptr(entry,0),512,gfx) || !f.Read(ptr(paletteEntry,0),32,palette)) continue;
        image.width=image.height=32; image.pixels.assign(1024,0);
        for(int i=0;i<1024;++i)
        {
            const unsigned c=(gfx[i/2] >> ((i%2)*4)) & 15;
            if(!c) continue;
            const unsigned rgb=U16(palette,c*2), tile=i/64, local=i%64;
            image.pixels[((tile/4)*8+local/8)*32+(tile%4)*8+local%8] = 0xff000000u |
                ((rgb&31)*255/31)<<16 | (((rgb>>5)&31)*255/31)<<8 | ((rgb>>10)&31)*255/31;
        }
        return true;
    }
    return false;
}
// Read one named RomFS file from an unencrypted NCSD/NCCH game dump. All offsets
// are bounded against both the container and the individual metadata tables.
inline bool RomFsFile(File& f, const std::string& path, Bytes& out)
{
    Bytes h; out.clear(); if(!f.Read(0,0x200,h)) return false;
    uint64_t ncch=0;
    if(!std::memcmp(h.data()+0x100,"NCSD",4)) ncch=uint64_t(U32(h,0x120))*512;
    if(!f.Read(ncch,0x200,h) || std::memcmp(h.data()+0x100,"NCCH",4) || !(h[0x18f]&4)) return false;
    const uint64_t rom=ncch+uint64_t(U32(h,0x1b0))*512, romSize=uint64_t(U32(h,0x1b4))*512;
    if(!Range(f.size,rom,romSize) || !f.Read(rom,0x60,h) || std::memcmp(h.data(),"IVFC",4)) return false;
    const unsigned shift=U32(h,0x4c); if(shift<9 || shift>20) return false;
    const uint64_t block=uint64_t(1)<<shift, level=((uint64_t(0x60)+U32(h,8)+block-1)/block)*block;
    const uint64_t levelSize=U64(h,0x44);
    if(!Range(romSize,level,levelSize) || !f.Read(rom+level,0x28,h) || U32(h,0)!=0x28) return false;
    const uint64_t base=rom+level, dirsAt=U32(h,0x0c), dirsSize=U32(h,0x10), filesAt=U32(h,0x1c), filesSize=U32(h,0x20), dataAt=U32(h,0x24);
    Bytes dirs,files;
    if(dirsSize>4*1024*1024 || filesSize>8*1024*1024 || !Range(levelSize,dirsAt,dirsSize) || !Range(levelSize,filesAt,filesSize) || dataAt>levelSize ||
       !f.Read(base+dirsAt,dirsSize,dirs) || !f.Read(base+filesAt,filesSize,files)) return false;
    auto nameIs=[](const Bytes& table,size_t at,size_t bytes,const std::string& name) {
        if(bytes!=name.size()*2 || !Range(table.size(),at,bytes)) return false;
        for(size_t i=0;i<name.size();++i) if(U16(table,at+i*2)!=(unsigned char)name[i]) return false;
        return true;
    };
    size_t current=0, pos=0;
    for(int depth=0;depth<16;++depth)
    {
        if(!Range(dirs.size(),current,24)) return false;
        const size_t slash=path.find('/',pos); const auto part=path.substr(pos,slash==std::string::npos?slash:slash-pos);
        uint32_t entry=U32(dirs,current+(slash==std::string::npos?12:8)); bool found=false;
        for(int limit=0;entry!=0xffffffffu && limit<8192;++limit)
        {
            if(slash==std::string::npos)
            {
                if(!Range(files.size(),entry,32)) return false;
                if(nameIs(files,entry+32,U32(files,entry+28),part))
                {
                    const uint64_t offset=U64(files,entry+8), size=U64(files,entry+16);
                    if(size>32*1024*1024 || !Range(levelSize-dataAt,offset,size)) return false;
                    return f.Read(base+dataAt+offset,size_t(size),out);
                }
                entry=U32(files,entry+4);
            }
            else
            {
                if(!Range(dirs.size(),entry,24)) return false;
                if(nameIs(dirs,entry+24,U32(dirs,entry+20),part)) {current=entry;pos=slash+1;found=true;break;}
                entry=U32(dirs,entry+4);
            }
        }
        if(!found) return false;
    }
    return false;
}
inline bool GarcMember(const Bytes& b, unsigned member, Bytes& out)
{
    out.clear(); if(b.size()<28 || std::memcmp(b.data(),"CRAG",4) || U16(b,8)!=0xfeff) return false;
    const size_t fato=U32(b,4), data=U32(b,16);
    if(!Range(b.size(),fato,12) || std::memcmp(b.data()+fato,"OTAF",4) || member>=U16(b,fato+8)) return false;
    const size_t fatoSize=U32(b,fato+4);
    if(!Range(fatoSize,12+uint64_t(member)*4,4) || !Range(b.size(),fato,fatoSize)) return false;
    const size_t fatb=fato+fatoSize;
    if(!Range(b.size(),fatb,12) || std::memcmp(b.data()+fatb,"BTAF",4)) return false;
    const size_t fatbSize=U32(b,fatb+4), rel=U32(b,fato+12+member*4);
    if(!Range(b.size(),fatb,fatbSize) || !Range(fatbSize,12+uint64_t(rel),16)) return false;
    const size_t at=fatb+12+rel;
    if(!(U32(b,at)&1)) return false; // base form, subfile zero
    const uint64_t start=U32(b,at+4), end=U32(b,at+8), length=U32(b,at+12);
    if(end<start || length>end-start || length>256*1024 || !Range(b.size(),uint64_t(data)+start,length)) return false;
    out.assign(b.begin()+data+start,b.begin()+data+start+length); return true;
}
inline bool Uncompress(Bytes& b)
{
    if(b.empty() || (b[0]!=0x10 && b[0]!=0x11)) return true;
    if(b.size()<4) return false;
    const bool eleven=b[0]==0x11; const size_t size=U32(b,0)>>8;
    if(size==0 || size>256*1024) return false;
    Bytes out; out.reserve(size); size_t pos=4;
    while(out.size()<size)
    {
        if(pos>=b.size()) return false;
        const unsigned flags=b[pos++];
        for(int bit=7;bit>=0 && out.size()<size;--bit)
        {
            if(pos>=b.size()) return false;
            if(!(flags&(1u<<bit))) {out.push_back(b[pos++]);continue;}
            unsigned first=b[pos++], length=first>>4, distance;
            if(eleven && length<=1)
            {
                const bool longRun=length==1;
                if(!Range(b.size(),pos,longRun?3:2)) return false;
                unsigned second=b[pos++];
                if(longRun) {length=((first&15)<<12 | second<<4 | b[pos]>>4)+0x111;first=b[pos++];}
                else {length=((first&15)<<4 | second>>4)+0x11; first=second;}
                distance=((first&15)<<8 | b[pos++])+1;
            }
            else
            {
                if(pos>=b.size()) return false;
                length+=eleven?1:3; distance=((first&15)<<8 | b[pos++])+1;
            }
            if(distance>out.size() || length>size-out.size()) return false;
            while(length--) out.push_back(out[out.size()-distance]);
        }
    }
    b=std::move(out); return true;
}
inline bool Bclim(Bytes b, Image& image)
{
    image={}; if(!Uncompress(b) || b.size()<40) return false;
    const size_t footer=b.size()-40;
    if(std::memcmp(b.data()+footer,"CLIM",4) || U16(b,footer+4)!=0xfeff || std::memcmp(b.data()+footer+20,"imag",4)) return false;
    const unsigned w=U16(b,footer+28), h=U16(b,footer+30), format=b[footer+32];
    if(!w || !h || w>128 || h>128) return false;
    unsigned stride=8, rows=8; while(stride<w) stride*=2; while(rows<h) rows*=2;
    const size_t count=size_t(stride)*rows;
    size_t start=0; unsigned paletteCount=0; std::vector<uint32_t> palette;
    auto rgba=[](unsigned v,unsigned fmt)->uint32_t {
        if(fmt==9) return (v&255)<<24 | v>>8;
        if(fmt==8) return (v&15)*17u<<24 | ((v>>12)&15)*17u<<16 | ((v>>8)&15)*17u<<8 | ((v>>4)&15)*17u;
        if(fmt==7) return ((v&1)?0xff000000u:0) | ((v>>11)&31)*255u/31<<16 | ((v>>6)&31)*255u/31<<8 | ((v>>1)&31)*255u/31;
        return 0xff000000u | ((v>>11)&31)*255u/31<<16 | ((v>>5)&63)*255u/63<<8 | (v&31)*255u/31;
    };
    if(format==7 && footer>=4 && U16(b,0)==2)
    {
        paletteCount=U16(b,2);
        if(!paletteCount || paletteCount>256 || !Range(footer,4,paletteCount*2)) return false;
        for(unsigned i=0;i<paletteCount;++i) palette.push_back(i?rgba(U16(b,4+i*2),5):0);
        start=4+paletteCount*2;
    }
    const unsigned bytes=format==9?4:2;
    if(!paletteCount && format!=5 && format!=7 && format!=8 && format!=9) return false;
    const size_t encoded=paletteCount ? (paletteCount<16?(count+1)/2:count) : count*bytes;
    if(!Range(footer,start,encoded)) return false;
    image.width=w;image.height=h;image.pixels.assign(w*h,0);
    for(size_t i=0;i<count;++i)
    {
        const unsigned m=i&63, tile=i>>6;
        const unsigned x=(tile%(stride/8))*8+(m&1)+((m>>1)&2)+((m>>2)&4);
        const unsigned y=(tile/(stride/8))*8+((m>>1)&1)+((m>>2)&2)+((m>>3)&4);
        uint32_t color;
        if(paletteCount)
        {
            const unsigned index=paletteCount<16 ? (b[start+i/2]>>((i%2)*4))&15 : b[start+i];
            if(index>=paletteCount) {image={};return false;} color=palette[index];
        }
        else color=rgba(bytes==4?U32(b,start+i*4):U16(b,start+i*2),format);
        if(x<w && y<h) image.pixels[y*w+x]=color;
    }
    return true;
}
}

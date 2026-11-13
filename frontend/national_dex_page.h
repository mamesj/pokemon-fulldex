#pragma once
// Included inside Launcher after shared Hub and ROM helpers.
#include "national_dex_info.h"
struct DexRect
{
    int x=0, y=0, w=0, h=0, row=-1;
    bool Contains(int px, int py) const { return px >= x && px < x+w && py >= y && py < y+h; }
};
struct DexPageState
{
    NationalDex::Selection selection;
    NationalDex::Result result;
    std::vector<NationalDex::Source> sources;
    std::vector<int> entries;
    std::array<std::string, 722> names;
    std::string config, message;
    std::map<int, NationalDexArt::Image> art;
    NationalDexArt::Bytes gen6Art;
    DexInfoCache info;
    bool triedGen6Art=false, grid=true;
    int detailKey=-1, selectedId=1, cols=1, mode=0; // entries, regions, saves
    DexRect detail;
    std::vector<DexRect> hits;
    NationalDex::Filter filter=NationalDex::Filter::All;
#ifdef SSR_POKEMON_HUB
    NationalDex::Scanner scanner{[](const NationalDex::Source& source, PokemonHub::DexSnapshot& out, std::string& error) {
        return PokemonHub::ReadDex(source.path, out, error, source.hack);
    }};
#endif
};
inline constexpr int DexFirst=6; // List, Grid, Regions, Saves, Filter, Refresh.
inline DexPageState& DexPage() { static DexPageState state; return state; }
// Support runtimes with an extracted save archive under the title's directory.
// Locate its "main" file, never a backup or a save from another title. Do not guess
// which file is current when a runtime provides multiple candidates.
inline NationalDex::Source Dex3dsSource(const Game& g)
{
    namespace fs = std::filesystem;
    NationalDex::Source source{g.folder + "/main", ShortName(g), false, "No 3DS main save found"};
    std::vector<std::string> found;
    std::error_code ec;
    fs::recursive_directory_iterator it(fs::u8path(g.folder), fs::directory_options::skip_permission_denied, ec), end;
    int visited = 0;
    for (; !ec && it != end && ++visited <= 4096; it.increment(ec))
    {
        const auto name = it->path().filename().u8string();
        if (it->is_directory(ec))
        {
            if (it.depth() >= 8 || name == "backups" || name == "backup" || name == "import" || name == "romfs") it.disable_recursion_pending();
        }
        else if (name == "main" && it->is_regular_file(ec)) found.push_back(it->path().u8string());
    }
    if (ec || visited > 4096) source.unavailable = "Could not finish locating the 3DS save";
    else if (found.size() == 1) { source.path = found.front(); source.unavailable.clear(); }
    else if (found.size() > 1) source.unavailable = "Multiple 3DS main saves; current save is ambiguous";
    return source;
}
inline void DexDiscover()
{
    auto& d = DexPage(); d.sources.clear();
    std::set<std::string> paths;
    for (const Game& g : RomList())
    {
        if (!g.known || g.comingSoon) continue;
        NationalDex::Source s;
        if (g.platform == N3DS) s = Dex3dsSource(g);
        else
        {
            if (g.saveName.empty()) continue;
            s = {HubSavePath(g), ShortName(g), IsHack(g), ""};
            if (!g.where.empty()) s.name += " · " + WhereName(g.where);
            std::error_code ec;
            if (!std::filesystem::is_regular_file(std::filesystem::u8path(s.path), ec)) s.unavailable = "No in-game save yet";
            if (s.hack) s.unavailable = "ROM hack Pokédex layout is unsupported";
        }
        s.path = NationalDex::Identity(s.path);
        if (paths.insert(s.path).second) d.sources.push_back(std::move(s));
    }
    for (const auto& selected : d.selection.saves)
        if (paths.insert(selected.first).second) d.sources.push_back({selected.first, selected.second, false, "Save is no longer in the library"});
}
inline int DexRows()
{
    const auto& d=DexPage();
    return d.mode==1 ? 7 : d.mode==2 ? 1+(int)d.sources.size() : DexFirst+(int)d.entries.size();
}
inline void DexReindex()
{
    auto& d=DexPage(); auto& f=Focus();
    d.entries=d.result.Entries(d.selection,d.filter);
    auto at=std::find(d.entries.begin(),d.entries.end(),d.selectedId);
    if (at==d.entries.end()) { d.selectedId=d.entries.empty()?0:d.entries.front(); at=d.entries.begin(); }
    if (!d.mode && f.prow>=DexFirst) f.prow=d.entries.empty()?0:DexFirst+(int)(at-d.entries.begin());
}
inline void DexRefresh()
{
    auto& d=DexPage(); DexDiscover();
    d.art.clear(); d.gen6Art.clear(); d.triedGen6Art=false; d.info={};
#ifdef SSR_POKEMON_HUB
    MonIcons()={};
#endif
    d.result={}; DexReindex();
#ifdef SSR_POKEMON_HUB
    d.scanner.Request(d.sources,d.selection);
#else
    d.message="National Dex requires a build with Pokémon Hub enabled";
#endif
    Hub().version++;
}
inline void DexPoll()
{
#ifdef SSR_POKEMON_HUB
    auto& d=DexPage();
    if (d.scanner.Poll(d.result)) { DexReindex(); Hub().version++; }
#endif
}
inline void DexOpen()
{
    auto& d=DexPage();
    const auto config=HubDir()+"/national-dex.cfg";
    if (config!=d.config)
    {
        d.config=config; NationalDex::LoadSelection(config,d.selection);
        int view=1; std::ifstream(config+".view") >> view; d.grid=view!=0;
#ifdef SSR_POKEMON_HUB
        for(int id=1;id<=721;++id)d.names[id]=MonName(PokemonHub::DexSpeciesName(id));
#endif
    }
    d.mode=d.selection.saves.empty()?2:0; d.message.clear(); d.detailKey=-1;
    HubMode()=4; Mover().open=false;
    auto& f=Focus(); f.prow=d.mode?0:DexFirst; f.pact=-1; f.rScroll=0; f.freeScroll=false;
    DexRefresh();
}
inline void DexKeep()
{
    auto& d=DexPage();
    d.message=NationalDex::SaveSelection(d.config,d.selection)?"":"Could not save your selections";
    DexRefresh();
}
inline bool DexEnter(int row)
{
    auto& d=DexPage(); auto& f=Focus();
    if(row<0 || row>=DexRows())return true;
    const int previousMode=d.mode;
    if(d.mode==1)
    {
        if(!row)d.mode=0;
        else { d.selection.regions^=1u<<(row-1); DexKeep(); }
    }
    else if(d.mode==2)
    {
        if(!row)d.mode=0;
        else
        {
            const auto s=d.sources[row-1]; auto found=d.selection.saves.find(s.path);
            if(found!=d.selection.saves.end())d.selection.saves.erase(found);
            else if(s.unavailable.empty())d.selection.saves[s.path]=s.name;
            else { d.message=s.unavailable; Hub().version++; return true; }
            DexKeep();
        }
    }
    else if(row<2)
    {
        d.grid=row==1; f.rScroll=0; f.freeScroll=false;
        std::ofstream(d.config+".view") << (d.grid?1:0) << '\n';
    }
    else if(row==2)d.mode=1;
    else if(row==3)d.mode=2;
    else if(row==4)
    {
        d.filter=static_cast<NationalDex::Filter>((static_cast<int>(d.filter)+1)%4);
        DexReindex(); f.rScroll=0;
    }
    else if(row==5) { d.message.clear(); DexRefresh(); }
    else { d.selectedId=d.entries[row-DexFirst]; if(f.dMax>0)f.pact=0; }
    if(d.mode!=previousMode) { f.prow=d.mode?0:(d.entries.empty()?0:DexFirst); f.rScroll=0; f.pact=-1; }
    Hub().version++; return true;
}
inline bool DexBack()
{
    auto& d=DexPage(); auto& f=Focus();
    if(f.pact>=0) { f.pact=-1; return true; }
    if(d.mode) { d.mode=0; f.prow=d.entries.empty()?0:DexFirst; }
    else { HubMode()=0; f.prow=4; }
    f.rScroll=0; f.pact=-1; Hub().version++; return true;
}
inline bool DexMove(int dir)
{
    auto& d=DexPage(); auto& f=Focus();
    if(f.pact>=0)
    {
        if(dir==1)f.pact=-1;
        else if(dir==3 || dir==4)f.dScroll=std::clamp(f.dScroll+(dir==3?-1:1)*40,0,f.dMax);
        return true;
    }
    if(d.mode)
    {
        if(dir==2 && f.dMax>0) { f.pact=0; return true; }
        return false;
    }
    if(f.prow<DexFirst)
    {
        if(dir==1) { if(f.prow) --f.prow; else f.side=f.page; }
        else if(dir==2)f.prow=std::min(DexFirst-1,f.prow+1);
        else if(dir==4 && !d.entries.empty())
        {
            auto at=std::find(d.entries.begin(),d.entries.end(),d.selectedId);
            f.prow=DexFirst+(at==d.entries.end()?0:(int)(at-d.entries.begin()));
        }
        return true;
    }
    int index=f.prow-DexFirst, cols=d.grid?std::max(1,d.cols):1;
    if(dir==3) { if(index<cols)f.prow=d.grid?1:0; else f.prow-=cols; }
    else if(dir==4)f.prow=DexFirst+std::min((int)d.entries.size()-1,index+cols);
    else if(dir==1) { if(d.grid && index%cols) --f.prow; else f.side=f.page; }
    else if(dir==2)
    {
        if(d.grid && index%cols<cols-1 && index+1<(int)d.entries.size())++f.prow;
        else f.pact=0;
    }
    if(f.prow>=DexFirst && f.prow<DexRows())d.selectedId=d.entries[f.prow-DexFirst];
    return true;
}
inline int DexRowAt(int x,int y)
{
    for(const auto& r:DexPage().hits)if(r.Contains(x,y))return r.row;
    return -1;
}
inline bool DexDetailAt(int x,int y) { return DexPage().detail.Contains(x,y); }
inline bool DexInstant(int row) { return DexPage().mode!=0 || row<DexFirst; }
inline std::string DexNumber(int id) { char s[8]; snprintf(s,sizeof s,"#%03d",id); return s; }
inline std::string DexTotals(const NationalDex::Totals& t)
{ return std::to_string(t.caught)+" / "+std::to_string(t.total)+" caught"; }
#ifdef SSR_POKEMON_HUB
inline const NationalDexArt::Image& DexSprite(int id)
{
    auto& d = DexPage();
    auto cached = d.art.find(id); if (cached != d.art.end()) return cached->second;
    NationalDexArt::Image image;
    if (!d.triedGen6Art)
    {
        d.triedGen6Art = true;
        for (const auto& g : RomList())
        {
            if (!g.known || g.comingSoon || g.platform != N3DS) continue;
            NationalDexArt::File file(g.rom);
            if (NationalDexArt::RomFsFile(file, "a/0/9/1", d.gen6Art)) break; // ORAS
        }
    }
    NationalDexArt::Bytes member;
    if (!d.gen6Art.empty() && NationalDexArt::GarcMember(d.gen6Art, id, member)) NationalDexArt::Bclim(std::move(member), image);
    if (image.pixels.empty() && MonIcon(id, false, image.pixels)) image.width = image.height = 32;
    if (image.pixels.empty() && id <= 386)
        for (const auto& g : RomList())
        {
            if (!g.known || g.comingSoon || g.platform != GBA || IsHack(g)) continue;
            NationalDexArt::File file(g.rom);
            if (NationalDexArt::GbaIcon(file, PokemonHub::DexGbaSpeciesIndex(id), image)) break;
        }
    return d.art.emplace(id, std::move(image)).first->second;
}
#endif
// Center the visible pixels, preserving whole-number scaling. Transparent padding
// in a menu icon no longer makes its large detail image appear tiny/off-center.
inline void DrawDexSprite(Canvas& c,const NationalDexArt::Image& image,NationalDex::Status status,int x,int y,int size)
{
    if(status==NationalDex::Status::Unseen || image.pixels.empty() || size<=0)return;
    int x0=image.width,y0=image.height,x1=-1,y1=-1;
    for(int py=0;py<image.height;++py)for(int px=0;px<image.width;++px)
        if(image.pixels[py*image.width+px]>>24) { x0=std::min(x0,px); y0=std::min(y0,py); x1=std::max(x1,px); y1=std::max(y1,py); }
    if(x1<x0 || y1<y0)return;
    const int w=x1-x0+1,h=y1-y0+1,longest=std::max(w,h);
    const int scale=std::max(1,size/longest);
    const int dw=longest>size?std::max(1,w*size/longest):w*scale;
    const int dh=longest>size?std::max(1,h*size/longest):h*scale;
    x+=(size-dw)/2; y+=(size-dh)/2;
    for(int py=0;py<dh;++py)for(int px=0;px<dw;++px)
    {
        uint32_t pixel=image.pixels[(y0+py*h/dh)*image.width+x0+px*w/dw];
        if(!(pixel>>24))continue;
        if(status==NationalDex::Status::Seen)pixel=0xff101917;
        c.Fill(x+px,y+py,1,1,pixel);
    }
}
namespace DexStyle
{
constexpr uint32_t Shell=0xffbf424d, Rim=0xffec7d7f, Dark=0xff122b32, Panel=0xff1b343b,
    Muted=0xffa9c6c6, White=0xfff3f6ed, Mint=0xff8ce1c1, Ink=0xff16332f,
    Paper=0xffeef1e5, Screen=0xffc9ddbd, Edge=0xff3b5960;
inline float Font(float size) { return size*1.25f; }
inline void Text(Canvas& c,int x,int y,const std::string& text,float size,int width,uint32_t color,bool bold=false)
{ GTFit(c,x,y,text,Font(size),std::max(1,width),color,bold); }
inline void Center(Canvas& c,int x,int y,int w,const std::string& text,float size,uint32_t color,bool bold=false)
{ Text(c,x+std::max(0,(w-GW(text,Font(size),bold))/2),y,text,size,w,color,bold); }
inline void Box(Canvas& c,int x,int y,int w,int h,uint32_t fill,uint32_t edge=0,int radius=10)
{ if(w>0 && h>0)Glass::Round(c,x,y,w,h,radius,fill,edge,edge?1.0f:0.0f); }
inline void Question(Canvas& c,int x,int y,int size,uint32_t color)
{ Center(c,x,y+size/2,size,"?",size*.65f,color,true); }
}
inline void DrawDexPage(Canvas& c,bool solid,float u,int L,int R,int top,int bottom)
{
    (void)solid;
    using namespace DexStyle;
    DexPoll(); auto& d=DexPage(); auto& f=Focus(); d.hits.clear();
    const bool stacked=c.h>c.w;
    u=std::max(.5f,std::min(u,(R-L)/(stacked?500.f:950.f)));
    auto S=[&](float n){return std::max(1,(int)(n*u));};
    const int pad=S(14),gap=S(12),width=R-L;
    const auto totals=d.result.Count(d.selection);
    const int regions=(int)std::count_if(std::begin(NationalDex::Regions),std::end(NationalDex::Regions),[&](const NationalDex::Region& r){return d.selection.Includes(r.first);});
    f.prow=std::clamp(f.prow,0,std::max(0,DexRows()-1)); f.aW=0;
    if(!d.mode && f.prow>=DexFirst)d.selectedId=d.entries[f.prow-DexFirst];
    Box(c,L,top,width,bottom-top,Shell,Rim,S(22));
    // Physical instrument header: lens, status lamps, a large completion readout.
    Box(c,L+pad,top+S(15),S(38),S(38),0xff4ccdea,White,S(19));
    Box(c,L+pad+S(8),top+S(20),S(11),S(8),0xffd8fbff,0,S(5));
    for(int i=0;i<3;++i)Box(c,L+pad+S(46+i*11),top+S(15),S(6),S(6),i==0?0xfff9cb72:i==1?Mint:0xff6f2434,0,S(3));
    Text(c,L+pad+S(48),top+S(34),"NATIONAL",S(10),S(140),0xffffd8d5,true);
    Text(c,L+pad+S(48),top+S(54),"POKÉDEX",S(24),S(190),White,true);
    int counterX=L+width*49/100;
    Text(c,counterX,top+S(19),"CAUGHT / SELECTED REGIONS",S(10),R-counterX-pad,0xffffd8d5,true);
    Text(c,counterX,top+S(46),std::to_string(totals.caught)+" / "+std::to_string(totals.total),S(32),R-counterX-pad,White,true);
    std::string caption=std::to_string(totals.Percent())+"% complete  ·  "+std::to_string(totals.seen)+" seen";
#ifdef SSR_POKEMON_HUB
    if(d.scanner.Busy())caption="Reading selected saves…";
#endif
    if(d.result.partial)caption="Partial progress · check selected saves";
    Text(c,counterX,top+S(69),caption,S(11),R-counterX-pad,White);
    const int toolsY=top+S(85),toolsH=S(32),bodyY=toolsY+toolsH+gap;
    auto button=[&](int x,int y,int w,int h,int row,const std::string& text,bool active=false)
    {
        const bool focus=f.side<0 && f.pact<0 && f.prow==row;
        Box(c,x,y,w,h,active?Mint:Dark,focus?White:(active?Mint:0xff93414b),S(7));
        Center(c,x+S(3),y+h/2,w-S(6),text,S(12),active?Ink:White,true);
        d.hits.push_back({x,y,w,h,row});
    };
    if(!d.mode)
    {
        const int bw=(width-2*pad-3*gap)/4;
        const char* filters[]={"All entries","Unseen","Seen only","Caught"};
        const std::string labels[]={"Regions · "+std::to_string(regions),"Saves · "+std::to_string(d.selection.saves.size()),filters[(int)d.filter],"Refresh"};
        for(int i=0;i<4;++i)button(L+pad+i*(bw+gap),toolsY,bw,toolsH,2+i,labels[i]);
    }
    else Text(c,L+pad,toolsY+toolsH/2,d.mode==1?"REGIONS   /   Choose which species count toward completion":"SAVE SOURCES   /   Choose which progress counts",S(13),width-2*pad,White,true);
    const int innerW=width-2*pad,availableH=std::max(S(100),bottom-bodyY-pad);
    const int lw=stacked?innerW:(innerW-gap)*55/100;
    const int lh=stacked?(availableH-gap)*43/100:availableH;
    const int dx=stacked?L+pad:L+pad+lw+gap,dy=stacked?bodyY+lh+gap:bodyY;
    const int dw=stacked?innerW:innerW-lw-gap,dh=std::max(1,bottom-pad-dy);
    Box(c,L+pad,bodyY,lw,lh,Dark,0xfff18c8b,S(12));
    const int lx=L+pad+S(12),right=L+pad+lw-S(12),head=S(48),foot=S(24);
    const int listY=bodyY+head,listH=std::max(1,lh-head-foot-S(8));
    if(!d.mode)
    {
        button(lx,bodyY+S(10),S(78),S(28),0,"LIST",!d.grid);
        button(lx+S(84),bodyY+S(10),S(78),S(28),1,"GRID",d.grid);
        Text(c,lx+S(175),bodyY+S(24),std::to_string(d.entries.size())+" entries",S(11),right-lx-S(175),Muted);
    }
    else Text(c,lx,bodyY+S(25),d.mode==1?"REGION SELECTION":"YOUR LIBRARY SAVES",S(13),right-lx,White,true);
    f.pX=lx; f.pY=listY; f.pW=right-lx; f.pdY=stacked?dy:0;
    const bool grid=d.grid && !d.mode;
    d.cols=grid?std::clamp((f.pW+S(8))/S(102),2,6):1;
    const int rowH=grid?S(82):S(46),cellGap=S(7),cellW=(f.pW-(d.cols-1)*cellGap)/d.cols;
    f.pRow=rowH; f.rVis=std::max(1,listH/rowH);
    const int count=d.mode?DexRows():(int)d.entries.size(),visualRows=(count+d.cols-1)/d.cols;
    const auto selected=std::find(d.entries.begin(),d.entries.end(),d.selectedId);
    const int selectedRow=d.mode?f.prow:(selected==d.entries.end()?-1:(int)(selected-d.entries.begin())/d.cols);
    if(!f.freeScroll && selectedRow>=0)f.rScroll=std::clamp(f.rScroll,std::max(0,selectedRow-f.rVis+1),selectedRow);
    f.rScroll=std::clamp(f.rScroll,0,std::max(0,visualRows-f.rVis));
    auto icon=[&](Canvas& target,int id,NationalDex::Status status,int x,int y,int size,uint32_t question)
    {
        if(status==NationalDex::Status::Unseen) { Question(target,x,y,size,question); return; }
#ifdef SSR_POKEMON_HUB
        const auto& art=DexSprite(id);
        if(!art.pixels.empty()) { DrawDexSprite(target,art,status,x,y,size); return; }
#endif
        Center(target,x,y+size/2,size,"—",size/2,question,true);
    };
    for(int r=0;r<f.rVis;++r)for(int col=0;col<d.cols;++col)
    {
        const int index=(r+f.rScroll)*d.cols+col;
        if(index>=count)continue;
        const int row=d.mode?index:DexFirst+index,x=lx+col*(cellW+cellGap),y=listY+r*rowH;
        const bool focus=f.prow==row && f.side<0 && f.pact<0;
        d.hits.push_back({x,y,cellW,rowH-cellGap,row});
        if(d.mode)
        {
            bool checked=false;std::string label,sub;
            if(!index)label="Done";
            else if(d.mode==1)
            {
                checked=d.selection.regions&(1u<<(index-1));
                const auto& reg=NationalDex::Regions[index-1];label=reg.name;
                sub="#"+std::to_string(reg.first)+"–"+std::to_string(reg.last);
            }
            else { const auto& source=d.sources[index-1];checked=d.selection.saves.count(source.path);label=source.name;sub=source.unavailable.empty()?(checked?"Included":"Excluded"):"Unavailable"; }
            Box(c,x,y,cellW,rowH-cellGap,focus?0xff335359:Panel,focus?Mint:Edge,S(7));
            if(index) { Box(c,x+S(10),y+S(12),S(17),S(17),checked?Mint:Dark,checked?Mint:Muted,S(4));if(checked) { c.Fill(x+S(14),y+S(19),S(9),S(3),Ink);c.Fill(x+S(17),y+S(16),S(3),S(9),Ink); } }
            Text(c,x+S(index?38:12),y+S(20),label,S(15),cellW-S(140),White,true);
            Text(c,x+cellW-S(95),y+S(20),sub,S(10),S(86),Muted);
            continue;
        }
        int id=d.entries[index];auto status=d.result.status[id];
        const bool selected=id==d.selectedId;
        if(grid)
        {
            const uint32_t fill=status==NationalDex::Status::Unseen?Panel:status==NationalDex::Status::Seen?0xffb5c6b0:0xffdae8d0;
            Box(c,x,y,cellW,rowH-cellGap,fill,selected?0xffff8f91:status==NationalDex::Status::Unseen?Edge:0xffaec8b3,S(7));
            if(selected) { c.Fill(x+S(8),y,S(22),S(3),0xffff8f91);c.Fill(x,y+S(8),S(3),S(15),0xffff8f91); }
            const uint32_t fg=status==NationalDex::Status::Unseen?Muted:Ink;
            Text(c,x+S(8),y+S(11),DexNumber(id),S(10),cellW-S(12),fg,true);
            const int sz=S(43);icon(c,id,status,x+(cellW-sz)/2,y+S(21),sz,Muted);
            Center(c,x+S(3),y+rowH-S(15),cellW-S(6),NationalDex::EntryName(status,d.names[id]),S(10),fg,true);
            if(status==NationalDex::Status::Caught)Box(c,x+cellW-S(14),y+S(8),S(6),S(6),0xff31866b,0,S(3));
        }
        else
        {
            Box(c,x,y,cellW,rowH-cellGap,selected?Mint:Panel,selected?White:Edge,S(6));
            const auto fg=selected?Ink:White;const int sz=S(28);
            Text(c,x+S(10),y+S(19),DexNumber(id),S(11),S(44),selected?Ink:Muted,true);
            icon(c,id,status,x+S(56),y+S(5),sz,selected?Ink:Muted);
            Text(c,x+S(96),y+S(19),NationalDex::EntryName(status,d.names[id]),S(15),cellW-S(175),fg,true);
            Text(c,x+cellW-S(66),y+S(19),NationalDex::StatusName(status),S(11),S(60),selected?Ink:Muted);
        }
    }
    if(visualRows>f.rVis)
    {
        const int trackH=f.rVis*rowH-cellGap,thumb=std::max(S(16),trackH*f.rVis/visualRows);
        Box(c,right+S(5),listY,S(3),trackH,Panel,0,S(1));
        Box(c,right+S(5),listY+(trackH-thumb)*f.rScroll/(visualRows-f.rVis),S(3),thumb,Muted,0,S(1));
    }
    if(!count)Text(c,lx,listY+S(38),!d.selection.regions?"Choose a region to begin.":"No entries match this filter.",S(15),f.pW,Muted);
    Text(c,lx,bodyY+lh-S(12),grid?"? Unseen   ·   Silhouette: seen   ·   Color: caught":"Select an entry to inspect its record",S(10),f.pW,Muted);
    // The detail viewport is composited separately so long text, many sources and
    // portrait scrolling cannot escape the device screen.
    d.detail={dx,dy,dw,dh,-1};
    Box(c,dx,dy,dw,dh,Dark,f.pact>=0?Mint:0xfff18c8b,S(12));
    const int inset=S(8),vw=std::max(1,dw-2*inset),vh=std::max(1,dh-2*inset);
    Canvas screen;screen.Resize(vw,vh);screen.Fill(0,0,vw,vh,Paper);
    const int key=d.mode?10000+d.mode*1000+f.prow:d.selectedId;
    if(d.detailKey!=key) { d.detailKey=key; f.dScroll=0;f.dMax=0; }
    f.dScroll=std::clamp(f.dScroll,0,f.dMax);
    int y=S(12)-f.dScroll;const int tx=S(12),tw=vw-2*tx;
    auto paragraph=[&](const std::string& text,int px,uint32_t color,bool bold=false)
    {
        for(const auto& part:WrapLines(text,Font(px),tw,bold))
        {
            const int lineH=(int)Font(px)+S(4);
            if(y+lineH>0 && y<vh)Text(screen,tx,y+lineH/2,part,(float)px,tw,color,bold);
            y+=lineH;
        }
    };
    auto label=[&](const std::string& text) { y+=S(8);paragraph(text,S(10),0xff53736a,true); };
    if(!d.message.empty()) { paragraph(d.message,S(12),0xff9b3347,true);y+=S(6); }
    if(d.result.partial) { paragraph("PARTIAL · Some selected saves could not be read",S(11),0xff9b3347,true);y+=S(6); }
    if(d.mode)
    {
        paragraph(d.mode==1?"Build your collection":"Progress sources",S(23),Ink,true);
        paragraph(d.mode==1?"Only the regions you include count toward the total.":"New saves stay excluded until you choose them.",S(13),0xff53736a);
        y+=S(8);
        paragraph(DexTotals(totals),S(27),Ink,true);
        label("REGIONAL COMPLETION");
        for(int r=0;r<6;++r)if(d.selection.regions&(1u<<r))
        {
            auto t=d.result.Count(d.selection,r);
            Text(screen,tx,y+S(13),NationalDex::Regions[r].name,S(14),tw/2,Ink,true);
            Text(screen,tx+tw/2,y+S(13),std::to_string(t.caught)+" / "+std::to_string(t.total),S(14),tw/2,Ink,true);
            Box(screen,tx,y+S(28),tw,S(5),0xffd1dece,0,S(2));
            if(t.caught)Box(screen,tx,y+S(28),std::max(S(4),tw*t.caught/std::max(1,t.total)),S(5),0xff42947a,0,S(2));
            y+=S(43);
        }
        if(d.mode==2 && f.prow>0)
        {
            const auto& source=d.sources[f.prow-1];label("SELECTED SAVE");paragraph(source.name,S(16),Ink,true);
            if(!source.unavailable.empty())paragraph(source.unavailable,S(12),0xff9b3347);
            for(const auto& s:d.result.sources)if(s.source.path==source.path)
                paragraph(s.error.empty()?"Trainer: "+s.dex.trainer:s.error,S(12),Ink);
            paragraph(source.path,S(11),0xff53736a);
        }
        for(const auto& s:d.result.sources)if(!s.error.empty()) { label(s.source.name);paragraph(s.error,S(12),0xff9b3347); }
    }
    else if(!d.selectedId || d.entries.empty())
    {
        paragraph("Your field guide",S(25),Ink,true);
        paragraph(!d.selection.regions?"Select at least one region to browse its Pokémon.":"Change the filter to explore more entries.",S(15),0xff53736a);
    }
    else
    {
        const int id=d.selectedId;const auto status=d.result.status[id];const bool known=status!=NationalDex::Status::Unseen;
        paragraph(DexNumber(id)+"  /  "+NationalDex::Regions[NationalDex::RegionOf(id)].name,S(11),0xff53736a,true);
        paragraph(NationalDex::EntryName(status,d.names[id]),S(25),Ink,true);
        const int heroH=S(136),heroY=y+S(6);
        Box(screen,tx,heroY,tw,heroH,Screen,0xff99b398,S(7));
        for(int gx=tx+S(10);gx<tx+tw-S(4);gx+=S(12))screen.Fill(gx,heroY+S(6),1,heroH-S(12),0xffbfd3b4);
        for(int gy=heroY+S(10);gy<heroY+heroH-S(4);gy+=S(12))screen.Fill(tx+S(6),gy,tw-S(12),1,0xffbfd3b4);
        const int sz=heroH-S(18);icon(screen,id,status,tx+(tw-sz)/2,heroY+S(8),sz,0xff6c866f);
        Text(screen,tx+S(8),heroY+S(12),"RECORD "+DexNumber(id),S(8),tw/2,0xff658064,true);
        Text(screen,tx+tw-S(63),heroY+heroH-S(12),NationalDex::StatusName(status),S(10),S(59),Ink,true);
        y=heroY+heroH+S(8);
        if(known)
        {
            d.info.Load();const auto& info=d.info.entries[id];
            int typeX=tx;
            for(const auto& type:{info.species.type1,info.species.type2})if(!type.empty())
            {
                const int typeW=std::min(tw,GW(type,Font(S(11)),true)+S(22));
                Box(screen,typeX,y,typeW,S(22),type=="Electric"?0xffedd36b:type=="Fire"?0xffedaa80:type=="Water"?0xff99cbd7:0xffc6ddc8,0,S(5));
                Center(screen,typeX,y+S(11),typeW,type,S(11),Ink,true);typeX+=typeW+S(6);
            }
            y+=S(24);
            bool anyCaught=false,anySeen=false;
            for(const auto& s:d.result.sources)if(s.error.empty()) { anyCaught|=s.dex.caught[id];anySeen|=s.dex.seen[id]&&!s.dex.caught[id]; }
            label(anyCaught?"CAUGHT IN":"SEEN IN");
            for(const auto& s:d.result.sources)if(s.error.empty() && (anyCaught?s.dex.caught[id]:s.dex.seen[id]))
            {
                Box(screen,tx,y,tw,S(27),0xffdce6d7,0,S(5));
                Text(screen,tx+S(9),y+S(14),s.source.name,S(13),tw-S(18),Ink,true);y+=S(31);
            }
            if(!anyCaught && !anySeen)paragraph("No contributing save available.",S(12),0xff53736a);
            label("POKÉDEX ENTRY");
            paragraph(info.description.empty()?"Entry text is unavailable in your current game files.":info.description,S(13),Ink);
            if(!info.source.empty()) { y+=S(3);paragraph("From "+info.source,S(10),0xff53736a); }
            label("BASE STATS");
            const char* statNames[]={"HP","ATK","DEF","SP.A","SP.D","SPD"};
            const int sw=tw/6;
            for(int i=0;i<6;++i)
            {
                Center(screen,tx+i*sw,y+S(10),sw,statNames[i],S(9),0xff53736a,true);
                Center(screen,tx+i*sw,y+S(29),sw,std::to_string(info.species.stats[i]),S(16),Ink,true);
            }
            y+=S(44);
            if(anyCaught && anySeen)
            {
                label("ALSO SEEN IN");
                for(const auto& s:d.result.sources)if(s.error.empty() && s.dex.seen[id]&&!s.dex.caught[id])paragraph(s.source.name,S(13),Ink);
            }
        }
        else
        {
            label("NO OBSERVATIONS YET");
            paragraph("Encounter this Pokémon in a selected save to reveal its name and silhouette. Catch it to complete the record.",S(14),Ink);
        }
    }
    f.dMax=std::max(0,y+f.dScroll+S(12)-vh);
    for(int row=0;row<vh;++row)
    {
        const int targetY=dy+inset+row;
        if(targetY<0 || targetY>=c.h)continue;
        for(int col=0;col<vw;++col)if(dx+inset+col>=0 && dx+inset+col<c.w)c.px[targetY*c.w+dx+inset+col]=screen.px[row*vw+col];
    }
    if(f.dMax>0)
    {
        const int trackH=vh-S(12),thumb=std::max(S(20),trackH*vh/(vh+f.dMax));
        Box(c,dx+dw-S(7),dy+inset+S(6),S(3),trackH,Edge,0,S(1));
        Box(c,dx+dw-S(7),dy+inset+S(6)+(trackH-thumb)*f.dScroll/std::max(1,f.dMax),S(3),thumb,Mint,0,S(1));
    }
}

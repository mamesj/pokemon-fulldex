#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "launcher.h"
#include <cassert>
#include <chrono>
#include <iostream>
const LauncherFontInfo kLauncherFonts[] = {{"noto", "Noto", "Sans", nullptr, nullptr, 1}};
const int kLauncherFontCount = 1;
namespace CloudSaves { Settings LoadSettings(const std::string&) { return {}; } }
namespace HubLink { Status Snapshot() { return {}; } void Leave() {} int Version() { return 0; } }
namespace ThemePack {
bool Jpeg(const uint32_t*, int, int, int, std::vector<unsigned char>&) { return false; }
bool ZipRead(const std::string&, std::map<std::string, std::vector<unsigned char>>&, size_t, const std::string&) { return false; }
}
namespace Achievements { bool HttpRequest(const HttpCall&, int&, std::string&, std::string&) { return false; } }
int main()
{
    using namespace Launcher;
    const auto root=std::filesystem::temp_directory_path()/("fulldex-dex-ui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);HubDir()=root.string();
    auto& d=DexPage();auto& f=Focus();
    f.page=SIDE_HUB;f.side=-1;DexOpen();assert(HubMode()==4 && d.mode==2 && d.selection.saves.empty());
#ifdef SSR_POKEMON_HUB
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(d.scanner.Busy() && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
    assert(!d.scanner.Busy());DexPoll();
#endif
    DexEnter(0);assert(d.mode==0 && DexRows()==725);
    DexEnter(0);assert(d.mode==1);f.prow=1;DexEnter(1);assert(f.prow==1 && d.selection.regions==62);
    DexEnter(1);assert(d.selection.regions==63);DexEnter(0);assert(d.mode==0);
#ifdef SSR_POKEMON_HUB
    while(d.scanner.Busy() && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();DexPoll();
#endif
    d.result.status[1]=NationalDex::Status::Seen;d.result.status[2]=NationalDex::Status::Caught;
    d.entries=d.result.Entries(d.selection,d.filter);
    d.names[1]="Bulbasaur";d.names[2]="Ivysaur";d.names[3]="Hidden name";
    d.art[1]={8,8,std::vector<uint32_t>(64,0xff11aa22)};d.art[2]=d.art[1];
    for(auto size:{std::pair<int,int>{1280,720},{720,1280},{640,480},{540,720}})
    {
        Canvas c;c.Resize(size.first,size.second);PhoneUi()=true;Orient(c);
        for(int mode=0;mode<3;++mode)
        {
            d.mode=mode;f.prow=0;f.rScroll=0;
            DrawDexPage(c,true,GU(c),20,c.w-20,30,c.h-30);
            assert(f.pW>0 && f.rVis>0 && f.pY+f.rVis*f.pRow<=c.h-30);
            if(mode==0)
            {
                f.prow=724;DrawDexPage(c,true,GU(c),20,c.w-20,30,c.h-30);
                assert(f.rScroll>0 && RomRowAt(f.pX+5,f.pY+(724-f.rScroll)*f.pRow+1)==724);
                f.freeScroll=true;f.rScroll=10;DrawDexPage(c,true,GU(c),20,c.w-20,30,c.h-30);assert(f.rScroll==10);
                f.prow=724;assert(PageMove(3));assert(f.prow==723 && !f.freeScroll);
                assert(RomRowAt(f.pX-1,f.pY)==-1);
                f.prow=4;DrawDexPage(c,true,GU(c),20,c.w-20,30,c.h-30);
                f.prow=5;DrawDexPage(c,true,GU(c),20,c.w-20,30,c.h-30);
                if (size.first==1280 && std::getenv("SSR_DEX_CAPTURE"))
                {
                    std::fill(c.px.begin(),c.px.end(),0xff182330);
                    DrawDexPage(c,true,GU(c),20,c.w-20,30,c.h-30);
                    std::ofstream image(std::getenv("SSR_DEX_CAPTURE"),std::ios::binary);
                    image<<"P6\n"<<c.w<<" "<<c.h<<"\n255\n";
                    for(auto pixel:c.px) {const char rgb[]={(char)(pixel>>16),(char)(pixel>>8),(char)pixel};image.write(rgb,3);}
                }
                f.prow=6;DrawDexPage(c,true,GU(c),20,c.w-20,30,c.h-30);
            }
        }
    }
    // Long contributor lists remain reachable without moving the selected species.
    d.mode=0;f.prow=5;f.pact=-1;
    for(int i=0;i<40;++i)
    {
        NationalDex::SourceResult source;
        source.source.name="Contributing save "+std::to_string(i);source.dex.caught[2]=true;
        d.result.sources.push_back(std::move(source));
    }
    Canvas detail;detail.Resize(640,480);Orient(detail);
    DrawDexPage(detail,true,GU(detail),20,620,30,450);
    assert(f.dMax>0 && PageMove(2) && f.pact==0);
    for(int i=0;i<100;++i) assert(PageMove(4));
    assert(f.dScroll==f.dMax && f.prow==5);
    assert(DexBack() && f.pact==-1 && HubMode()==4);
    d.mode=2;assert(DexBack() && d.mode==0);assert(DexBack() && HubMode()==0 && f.prow==4 && HubRows()==5);
    // A 3DS title's extracted main is discovered; backups and ambiguous sources are excluded.
    Game g;g.folder=(root/"3ds").string();g.name="ORAS";g.platform=N3DS;
    std::filesystem::create_directories(root/"3ds"/"save");std::filesystem::create_directories(root/"3ds"/"backups");
    std::ofstream(root/"3ds"/"save"/"main")<<"synthetic";std::ofstream(root/"3ds"/"backups"/"main")<<"backup";
    assert(Dex3dsSource(g).unavailable.empty());
    std::ofstream(root/"3ds"/"main")<<"ambiguous";assert(!Dex3dsSource(g).unavailable.empty());
    std::filesystem::remove_all(root);std::cout<<"National Dex launcher checks passed\n";
}

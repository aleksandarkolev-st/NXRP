#pragma once
#include "Maze.Story.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Adventure
{
constexpr int itemCount=6;
inline constexpr const char* itemNames[]={"Verdant draught","Veil potion","Fleetfoot tonic","Wayfinder sigil","Recall rune","Echo charm"};
inline constexpr const char* descriptions[]={"Restore breath and steady your nerves.","12 seconds hidden from sight. Noise still carries.","15 seconds of faster, tireless running.","A trail of lights to your next objective for 20s.","Return to your last refuge. One use.","Throw a phantom sound ahead to distract the warden."};
inline constexpr const char* materials[]={"Dewleaf","Moon salt","Old brass"};
inline constexpr unsigned prices[]={6,10,8,12,18,7};
inline constexpr const char* shopNames[]={"THE MOSS & MORTAR","THE COPPER FINCH","THE WAYFARER'S REST","THE VELVET FOX"};
inline constexpr const char* shopSpecialties[]={"Apothecary / discounted draughts and veils","Tinkerer / discounted tonics and echoes","Cartographer / discounted wayfinders and recall","Gaming house / cards, reels and rare supplies"};
inline unsigned price(int item,int house=-1)
{
    const bool discount=(house==0 && item<2) || (house==1 && (item==2 || item==5)) || (house==2 && (item==3 || item==4));
    return prices[item]-(discount?2u:0u);
}
inline constexpr unsigned recipes[6][3]={{2,0,0},{1,2,0},{1,0,1},{0,2,1},{2,2,2},{0,1,2}};
struct Pack
{
    unsigned coins=12;
    std::array<unsigned,3> materials{{2,1,1}};
    std::array<unsigned,6> items{{1,0,0,1,0,0}};
    unsigned districts=0,recalls=0,restores=0,echoes=0;
    std::array<float,3> effects{}; // veil, fleetfoot, wayfinder: remaining seconds
    float echoTime=0.f;
    float useCooldown=0.f;
    MazePosition echo{-1,-1};
};
struct Cache {MazePosition cell;int kind=0;unsigned coins=0;};
inline std::vector<Cache> caches(const MazeGrid& maze,std::uint32_t seed,const std::array<MazePosition,3>& stones)
{
    std::vector<std::pair<unsigned,Cache>> candidates;
    const auto houses=generateSafehouses(maze.size(),seed);
    for(int r=0;r<maze.size().rows;++r) for(int c=0;c<maze.size().cols;++c)
    {
        const MazePosition p{r,c};
        if(maze.at(r,c)!=MazeCell::Passage || r+c<5 || safehouseAt(p,houses)>=0) continue;
        bool reserved=false;
        for(auto stone:stones) reserved=reserved || std::abs(stone.row-r)+std::abs(stone.col-c)<2;
        if(reserved) continue;
        const auto hash=storyHash(seed^static_cast<unsigned>(r*1009+c*9176));
        const int district=storyDistrict(p,maze.size());
        candidates.push_back({hash,{p,district==3?static_cast<int>(hash%3):district,3u+hash%5u}});
    }
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    std::vector<Cache> result;
    const auto limit=static_cast<std::size_t>(std::clamp(maze.size().rows*maze.size().cols/22,8,48));
    for(const auto& candidate:candidates)
    {
        bool spaced=true;
        for(const auto& old:result) if(std::abs(old.cell.row-candidate.second.cell.row)+std::abs(old.cell.col-candidate.second.cell.col)<3) {spaced=false;break;}
        if(spaced) result.push_back(candidate.second);
        if(result.size()==limit) break;
    }
    return result;
}
inline bool buy(Pack& pack,int item,int house=-1)
{
    if(item<0 || item>=itemCount || pack.coins<price(item,house) || pack.items[item]>=99) return false;
    pack.coins-=price(item,house);++pack.items[item];return true;
}
inline bool craft(Pack& pack,int item)
{
    if(item<0 || item>=itemCount || pack.items[item]>=99) return false;
    for(int i=0;i<3;++i) if(pack.materials[i]<recipes[item][i]) return false;
    for(int i=0;i<3;++i) pack.materials[i]-=recipes[item][i];
    ++pack.items[item];return true;
}
inline bool sell(Pack& pack)
{
    for(auto amount:pack.materials) if(!amount) return false;
    for(auto& amount:pack.materials)--amount;
    pack.coins=std::min(99999u,pack.coins+9u);return true;
}
inline void tick(Pack& pack,float seconds)
{
    for(auto& effect:pack.effects) effect=std::max(0.f,effect-seconds);
    pack.echoTime=std::max(0.f,pack.echoTime-seconds);
    pack.useCooldown=std::max(0.f,pack.useCooldown-seconds);
}
inline bool valid(const Pack& p)
{
    if(p.coins>99999 || p.districts>15) return false;
    for(auto amount:p.materials) if(amount>999) return false;
    for(auto amount:p.items) if(amount>99) return false;
    for(int i=0;i<3;++i) if(!std::isfinite(p.effects[i]) || p.effects[i]<0 || p.effects[i]>(i==0?12.f:i==1?15.f:20.f)) return false;
    return std::isfinite(p.echoTime) && p.echoTime>=0 && p.echoTime<=8.f && std::isfinite(p.useCooldown) && p.useCooldown>=0.f && p.useCooldown<=1.2f;
}
}

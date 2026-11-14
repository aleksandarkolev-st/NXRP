#pragma once
#include "Maze.Safehouse.h"
#include "MazeGrid.h"
#include <array>
#include <optional>
#include <limits>
#include <queue>

inline unsigned storyHash(unsigned value)noexcept
{value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;return value^(value>>16);}
inline int storyDistrict(MazePosition p,MazeSize size)noexcept
{return (p.row>=size.rows/2?2:0)+(p.col>=size.cols/2?1:0);}
inline const char* storyDistrictName(int district)noexcept
{
    constexpr const char* names[]={"ARRIVALS","WORKERS' WALK","RELAY GARDEN","THE RETURN"};
    return names[std::clamp(district,0,3)];
}
struct StoryRuin{MazePosition cell;bool alongRows=true;};
inline bool storyRuinReserved(MazePosition p,const std::optional<StoryRuin>& ruin)noexcept
{return ruin && std::abs(p.row-ruin->cell.row)<=(ruin->alongRows?3:1) && std::abs(p.col-ruin->cell.col)<=(ruin->alongRows?1:3);}
inline bool storyRuinBlocked(MazePosition p,const std::optional<StoryRuin>& ruin)noexcept
{return ruin && std::abs(p.row-ruin->cell.row)<=(ruin->alongRows?1:0) && std::abs(p.col-ruin->cell.col)<=(ruin->alongRows?0:1);}
inline std::optional<StoryRuin> generateStoryRuin(MazeSize size,std::uint32_t seed)
{
    const auto houses=generateSafehouses(size,seed);
    std::optional<StoryRuin> best;unsigned bestScore=0;
    for(int orientation=0;orientation<2;++orientation)
    {
        const int rr=orientation==0?3:1,cc=orientation==0?1:3;
        for(int row=rr;row<size.rows-rr;++row)for(int col=cc;col<size.cols-cc;++col)
        {
            bool allowed=true;
            for(int r=row-rr;r<=row+rr && allowed;++r)for(int c=col-cc;c<=col+cc;++c)
                if(safehouseAt({r,c},houses)>=0 || (r==0 && c==0) || (r==size.rows-1 && c==size.cols-1))
                {allowed=false;break;}
            if(!allowed)continue;
            const unsigned score=(storyHash(seed^static_cast<unsigned>(row*997+col*71+orientation*197))&0x3fffffffu)
                +(storyDistrict({row,col},size)==3?0x40000000u:0u);
            if(!best || score>bestScore){best=StoryRuin{{row,col},orientation==0};bestScore=score;}
        }
    }
    return best;
}
struct StorySite{MazePosition cell;float x=.5f,z=.5f;float angle=0.f;};
inline std::array<StorySite,4> generateStorySites(const MazeGrid& maze,std::uint32_t seed,
    const std::array<MazePosition,3>& stones)
{
    std::array<StorySite,4> sites{};
    const auto size=maze.size();const auto houses=generateSafehouses(size,seed);const auto ruin=generateStoryRuin(size,seed);
    for(int kind=0;kind<4;++kind)
    {
        unsigned bestScore=0;bool found=false;
        for(int row=0;row<size.rows;++row)for(int col=0;col<size.cols;++col)
        {
            const MazePosition p{row,col};
            if(maze.at(row,col)!=MazeCell::Passage || safehouseAt(p,houses)>=0 || row+col<3)continue;
            bool reserved=false;
            for(const auto stone:stones)reserved=reserved || (std::abs(stone.row-row)+std::abs(stone.col-col)<2);
            for(int i=0;i<kind;++i)reserved=reserved || (std::abs(sites[i].cell.row-row)+std::abs(sites[i].cell.col-col)<2);
            if(reserved || (kind!=3 && storyRuinReserved(p,ruin)))continue;
            unsigned score=storyHash(seed^static_cast<unsigned>(row*1013+col*277+kind*193))&0xffffu;
            if(storyDistrict(p,size)==kind)score+=0x10000u;
            if(kind==3 && storyRuinReserved(p,ruin))score+=0x40000u;
            int exits=0;
            constexpr MazePosition ds[]={{-1,0},{1,0},{0,-1},{0,1}};
            for(auto d:ds){const MazePosition n{row+d.row,col+d.col};exits+=maze.contains(n)&&maze.at(n.row,n.col)!=MazeCell::Wall;}
            if(exits==1)score+=0x1000u;
            if(!found || score>bestScore){sites[kind]={p,.5f,.5f,static_cast<float>(kind)*.7f};bestScore=score;found=true;}
        }
        if(!found)
        {
            // Tiny maps may not have four widely separated stone-free sites.
            // A refuge corner is reachable and never occupies its central exits.
            const auto h=houses.empty()?MazePosition{1,1}:houses.front();
            sites[kind]={{h.row-1,h.col-1},.25f+kind*.14f,.25f,0.f};
        }
        else
        {
            auto& site=sites[kind];constexpr MazePosition sides[]={{-1,0},{1,0},{0,-1},{0,1}};
            const float angles[]={0.f,3.14159265f,1.57079633f,-1.57079633f};
            int wall=2;
            for(int side=0;side<4;++side)
            {const auto d=sides[side];const MazePosition n{site.cell.row+d.row,site.cell.col+d.col};
             if(!maze.contains(n) || maze.at(n.row,n.col)==MazeCell::Wall){wall=side;break;}}
            site.x=.5f+sides[wall].col*.36f;site.z=.5f+sides[wall].row*.36f;site.angle=angles[wall];
        }
    }
    return sites;
}
inline const char* storyNote(int note)noexcept
{
    constexpr const char* notes[]={
        "ARRIVALS / Mara's luggage tag\nWe came to repair the storm relay. The suitcases came back\nwithout us. I left Ivo's tools where the hedge smells of iron.\nIf he writes again, ask why every ticket has tomorrow's date.",
        "WORKERS' WALK / Ivo's repair sheet\nMara's tag was in my locked toolbox. The shelter roof folded\ninward before the storm. Nobody stood on it. I took the recorder\nto the relay garden. Keep the doors shut while you listen.",
        "RELAY GARDEN / Recorder transcript\nIvo: Those are our boots in the mud. Mara: We haven't walked\nthere yet. [Three knocks.] The recording continues for thirteen\nminutes after the batteries are removed. Find the collapsed cabin.",
        "THE RETURN / Note under the collapsed frame\nThe prints lead here from every direction. None lead out.\nThe old sign said SAFE ROUTE; someone scratched out SAFE.\nWe lit the houses bright, and left two exits ready. Keep one quiet.",
        "KEEPER / First refuge\nMara's recorder is still playing somewhere in the hedge.\nWe built the bar and games so people could remember being alive.\nEvery laugh carries. Rest here, but choose your next exit first.",
        "KEEPER / Second refuge\nIvo kept using the same escape. By the third night it waited\nat that corner. Now we change routes and stop talking at doors.\nIt searches where it last heard us; silence is still a way out.",
        "KEEPER / Third refuge\nThe lamps need no oil, but the shelter is no promise.\nThe first strike shakes dust loose. The second is your warning.\nLeave before the third. Someone waited at the gate for you.",
        "KEEPER / Fourth refuge\nThe tickets, the tools, the recording: all dated tomorrow.\nI wrote these notes before I found them. If you reach the gate,\nremember Mara and Ivo. Let the next arrivals know the houses burn bright."
    };
    return notes[std::clamp(note,0,7)];
}

struct StoryFootstep{float x,z,cosine,sine;};
inline std::vector<StoryFootstep> generateStoryFootsteps(const MazeGrid& maze,
    const std::array<StorySite,4>& sites,const std::vector<MazePosition>& houses)
{
    std::vector<StoryFootstep> prints;
    const auto size=maze.size();
    for(const auto& site:sites)
    {
        std::vector<int> parent(maze.cellCount(),-1);std::queue<MazePosition> pending;
        parent[0]=0;pending.push({0,0});const int goal=site.cell.row*size.cols+site.cell.col;
        constexpr MazePosition ds[]={{-1,0},{1,0},{0,-1},{0,1}};
        while(!pending.empty() && parent[goal]<0)
        {
            const auto p=pending.front();pending.pop();
            for(auto d:ds)
            {
                const MazePosition n{p.row+d.row,p.col+d.col};
                if(!maze.contains(n) || maze.at(n.row,n.col)==MazeCell::Wall || !refugeEdgeAllowed(p,n,houses))continue;
                const int index=n.row*size.cols+n.col;
                if(parent[index]>=0)continue;
                parent[index]=p.row*size.cols+p.col;pending.push(n);
            }
        }
        if(parent[goal]<0)continue;
        std::vector<int> route;int at=goal;
        while(at!=0 && route.size()<maze.cellCount()){route.push_back(at);at=parent[at];}
        const std::size_t count=std::min<std::size_t>(3,route.size());
        for(std::size_t r=count;r>0;--r)
        {
            const int cell=route[r-1],previous=parent[cell];
            const MazePosition p{cell/size.cols,cell%size.cols};
            if(safehouseAt(p,houses)>=0)continue;
            const float sine=static_cast<float>(p.col-previous%size.cols),cosine=static_cast<float>(p.row-previous/size.cols);
            for(int foot=0;foot<2;++foot)
            {
                const float along=foot==0?-.20f:.20f,side=foot==0?-.07f:.07f;
                prints.push_back({p.col+.5f+sine*along+cosine*side,p.row+.5f+cosine*along-sine*side,cosine,sine});
            }
        }
    }
    return prints;
}

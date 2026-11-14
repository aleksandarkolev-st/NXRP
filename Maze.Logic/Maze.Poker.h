#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <random>

// Free-play five-card draw. Each survivor owns a separate hand and score.
namespace RefugePoker
{
struct Hand
{
    std::array<int,5> cards{};
    unsigned rounds=0, held=0, score=0;
    int phase=0; // 0 unplayed, 1 choosing holds, 2 revealed
    float readyAt=0.f;
};
inline std::array<int,52> deck(std::uint32_t seed,unsigned round,int player)
{
    std::array<int,52> result{};
    for(int i=0;i<52;++i) result[i]=i;
    std::mt19937 random(seed ^ (round*2654435761u) ^ (static_cast<unsigned>(player)*0x85ebca6bu));
    // Explicit shuffle keeps seeded deals stable across library implementations.
    for(int i=51;i>0;--i) std::swap(result[i],result[random()%static_cast<unsigned>(i+1)]);
    return result;
}
inline int category(const std::array<int,5>& cards)
{
    std::array<int,13> counts{};
    bool flush=true;
    for(int c:cards) { ++counts[c%13]; flush=flush && c/13==cards[0]/13; }
    int pairs=0,three=0,four=0,unique=0,low=13,high=0;
    for(int i=0;i<13;++i) if(counts[i])
    {
        ++unique; low=std::min(low,i); high=i;
        pairs+=counts[i]==2; three+=counts[i]==3; four+=counts[i]==4;
    }
    // Ranks are 2..A. Ace may also complete A,2,3,4,5.
    const bool straight=unique==5 && (high-low==4 || (counts[12] && counts[0] && counts[1] && counts[2] && counts[3]));
    if(flush && straight) return low==8?9:8;
    if(four) return 7;
    if(three && pairs) return 6;
    if(flush) return 5;
    if(straight) return 4;
    if(three) return 3;
    if(pairs==2) return 2;
    if(pairs) return 1;
    return 0;
}
inline const char* name(int rank)
{
    static constexpr const char* names[]={"HIGH CARD","ONE PAIR","TWO PAIR","THREE OF A KIND","STRAIGHT","FLUSH","FULL HOUSE","FOUR OF A KIND","STRAIGHT FLUSH","ROYAL FLUSH"};
    return names[rank];
}
inline unsigned payout(int rank)
{
    static constexpr unsigned values[]={0,1,2,3,4,6,9,25,50,100};
    return values[rank];
}
inline bool play(Hand& hand,std::uint32_t seed,int player,float now)
{
    if(now<hand.readyAt) return false;
    if(hand.phase!=1)
    {
        ++hand.rounds; hand.held=0; hand.phase=1;
        const auto shuffled=deck(seed,hand.rounds,player);
        std::copy_n(shuffled.begin(),5,hand.cards.begin());
    }
    else
    {
        const auto shuffled=deck(seed,hand.rounds,player); int next=5;
        for(int i=0;i<5;++i) if(!(hand.held&(1u<<i))) hand.cards[i]=shuffled[next++];
        hand.phase=2; hand.score+=payout(category(hand.cards));
    }
    hand.readyAt=now+1.2f;
    return true;
}
inline bool valid(const Hand& hand)
{
    if(hand.phase<0 || hand.phase>2 || hand.held>31 || (hand.phase==0)!=(hand.rounds==0)) return false;
    if(!hand.phase) return hand.held==0 && hand.score==0 && std::all_of(hand.cards.begin(),hand.cards.end(),[](int c){return c==0;});
    std::uint64_t seen=0;
    for(int c:hand.cards)
    {
        if(c<0 || c>=52 || (seen&(std::uint64_t{1}<<c))) return false;
        seen|=std::uint64_t{1}<<c;
    }
    return true;
}
}

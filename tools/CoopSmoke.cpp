#include <SFML/Graphics.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cassert>
#include <cmath>
#include <iostream>
#define private public
#include "MazeApplication.h"
#undef private
#include "Maze.Game.h"
#include "VoicePacket.h"
#include "GameView.cpp"
#include "MazeApplication.cpp"

#ifdef NDEBUG
#error The co-op smoke test requires assertions to be enabled.
#endif

int main(int argc,char** argv)
{
    if(argc==2 && std::string(argv[1])=="--adventure")
    {
        sf::Font font;assert(font.openFromFile("assets/fonts/SourceSans3-Semibold.ttf"));
        GameState game;startGame(game,generateMaze({30,30},43),43);
        GameView view(font);view.initialize(game);view.resize({900,650});
        sf::RenderTexture target({900,650},sf::ContextSettings{24});
        const auto shot=[&](const std::string& name){view.draw(target,game);target.display();assert(target.getTexture().copyToImage().saveToFile("build/"+name+".png"));};
        assert(view.safehouses_.size()==4);
        for(std::size_t i=0;i<view.safehouses_.size();++i)
        {
            const auto house=view.safehouses_[i];view.x_=house.col+.5f;view.z_=house.row+.5f;game.player=house;
            view.angle_=3.8f;view.pitch_=0;view.messageClock_=0;view.updateAdventure(game,.01f);
            shot("wayward-refuge-"+std::to_string(i));
        }
        const auto cache=view.caches_.front();
        view.x_=cache.cell.col+.5f;view.z_=cache.cell.row+.95f;game.player=cache.cell;view.angle_=3.14159265f;view.pitch_=-.65f;
        shot("wayward-cache-closed");
        view.openedCaches_=1;view.cacheOpenClocks_[0]=.65f;shot("wayward-cache-opening");
        view.cacheOpenClocks_[0]=0;shot("wayward-cache-open");
        view.sitAtBar(0);game.player=view.safehouses_[0];view.seatedHouse_=-1;
        view.updateAdventure(game,.01f);
        for(int mode=1;mode<=4;++mode){view.adventurePanels_[0]=mode;shot("wayward-panel-"+std::to_string(mode));}
        view.adventurePanels_[0]=0;view.adventureMotionItem_=0;view.adventureMotionClock_=.65f;
        shot("wayward-potion-use");
        view.adventureMotionClock_=0;view.packs_[0].effects[2]=20;view.angle_=0;shot("wayward-wayfinder");
        view.resize({640,560});sf::RenderTexture small({640,560},sf::ContextSettings{24});
        for(int mode=1;mode<=4;++mode)
        {view.adventurePanels_[0]=mode;view.draw(small,game);small.display();assert(small.getTexture().copyToImage().saveToFile("build/wayward-small-panel-"+std::to_string(mode)+".png"));}
        for(auto mesh:view.adventureMeshes_)assert(mesh!=0);
        std::cout<<"Four refuge identities, sourced models, chest animation, magic and adventure panels rendered.\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--guide")
    {
        sf::Font font;assert(font.openFromFile("assets/fonts/SourceSans3-Semibold.ttf"));
        sf::RenderWindow window(sf::VideoMode({640,560}),"Field guide layout check");window.setVisible(false);
        sf::RenderTexture target({640,560});Menu guide(font);guide.resize({640,560});guide.showHowToPlay();
        for(int page=0;page<5;++page)
        {
            assert(guide.guidePage_==page && guide.rules_.getCharacterSize()>=18);
            const auto bounds=guide.rules_.getGlobalBounds();
            std::cout<<"Guide page "<<page+1<<": font "<<guide.rules_.getCharacterSize()<<", bottom "<<bounds.position.y+bounds.size.y<<std::endl;
            assert(bounds.position.x>=24 && bounds.position.x+bounds.size.x<=616);
            assert(bounds.position.y+bounds.size.y<guide.backButton_.shape.getPosition().y-12.f);
            assert(guide.title_.getGlobalBounds().position.x>=0.f);
            guide.update({-1,-1});target.clear({13,19,17});guide.draw(target);target.display();
            assert(target.getTexture().copyToImage().saveToFile("build/field-guide-"+std::to_string(page)+".png"));
            sf::Event::KeyPressed next;next.code=sf::Keyboard::Key::Right;guide.handleEvent(sf::Event{next},window);
        }
        assert(!guide.guideNext_.enabled && guide.guidePrevious_.enabled && guide.guidePage_==4);
        sf::Event::MouseButtonPressed previous;previous.button=sf::Mouse::Button::Left;
        const auto at=guide.guidePrevious_.shape.getPosition();previous.position={static_cast<int>(at.x+10),static_cast<int>(at.y+10)};
        assert(guide.handleEvent(sf::Event{previous},window)==MenuAction::None && guide.guidePage_==3);
        sf::Event::KeyPressed back;back.code=sf::Keyboard::Key::Escape;
        assert(guide.handleEvent(sf::Event{back},window)==MenuAction::Back);
        guide.showHowToPlay();assert(guide.guidePage_==0 && !guide.guidePrevious_.enabled);
        std::cout<<"Five field-guide pages fit at 640x560; keyboard, pointer, bounds and Back passed.\n";
        return 0;
    }
    SoundscapeFrame cardsFrame;cardsFrame.active=true;cardsFrame.pokerEvent=1;cardsFrame.pokerGain=1.f;cardsFrame.pokerPan=-1.f;
    SoundscapeFrame quietCards=cardsFrame;quietCards.pokerGain=0.f;
    SoundscapeSynth cardsSynth,quietSynth;
    std::array<std::int16_t,8192> cardsAudio{},quietAudio{};
    cardsSynth.render(cardsAudio.data(),4096,cardsFrame);quietSynth.render(quietAudio.data(),4096,quietCards);
    unsigned changedLeft=0;
    for(int i=0;i<4096;++i)
    {changedLeft+=cardsAudio[i*2]!=quietAudio[i*2];assert(cardsAudio[i*2+1]==quietAudio[i*2+1]);}
    assert(changedLeft>1000);
    // Exhaust all 2,598,960 hands: catches ace-low and royal-flush edge cases.
    std::array<unsigned,10> pokerCounts{};
    for(int a=0;a<48;++a) for(int b=a+1;b<49;++b) for(int c=b+1;c<50;++c)
    for(int d=c+1;d<51;++d) for(int e=d+1;e<52;++e)
        ++pokerCounts[RefugePoker::category({a,b,c,d,e})];
    assert((pokerCounts==std::array<unsigned,10>{1302540,1098240,123552,54912,10200,5108,3744,624,36,4}));
    for(unsigned seed=0;seed<100;++seed) for(unsigned held=0;held<32;++held)
    {
        RefugePoker::Hand hand;
        assert(RefugePoker::play(hand,seed,1,0.f)); const auto original=hand.cards;
        hand.held=held;
        assert(!RefugePoker::play(hand,seed,1,.5f));
        assert(RefugePoker::play(hand,seed,1,2.f));assert(RefugePoker::valid(hand));
        for(int i=0;i<5;++i) if(held&(1u<<i)) assert(hand.cards[i]==original[i]);
        assert(hand.score==RefugePoker::payout(RefugePoker::category(hand.cards)));
    }
    std::istringstream truncatedRig("NXR1");assert(!HandMotion::readRig(truncatedRig));
    std::istringstream oversizedRig(std::string("NXR1")+std::string(12,static_cast<char>(255)));
    assert(!HandMotion::readRig(oversizedRig));
    for(const HandMotion::V target: {HandMotion::V{0,0,0},HandMotion::V{0,0,-10},HandMotion::V{0,.2f,0}})
    {
        const auto solved=HandMotion::solveArm({},target,{0,-1,0},.25f,.22f);
        assert(std::isfinite(solved.elbow.x) && std::isfinite(solved.elbow.y) && std::isfinite(solved.elbow.z));
        assert(std::abs(HandMotion::length(solved.elbow)-.25f)<.0001f);
        assert(std::abs(HandMotion::length(solved.wrist-solved.elbow)-.22f)<.0001f);
    }
    // The hand must grip before the lever moves, follow its complete return,
    // then open before withdrawing. Check motion across all phase boundaries.
    assert(slotLeverDegrees(.29f,2.f)==0.f);
    assert(slotHandGrip(.30f,2.f)>.999f);
    assert(slotHandReach(1.64f,2.f)>.999f);
    assert(slotHandGrip(1.80f,2.f)==0.f);
    assert(slotHandReach(1.95f,2.f)==0.f);
    for(float boundary: {.30f,.85f,.98f,1.65f})
    {
        constexpr float h=.0005f;
        const float before=(slotLeverDegrees(boundary,2.f)-slotLeverDegrees(boundary-h,2.f))/h;
        const float after=(slotLeverDegrees(boundary+h,2.f)-slotLeverDegrees(boundary,2.f))/h;
        assert(std::abs(before-after)<2.f);
    }
    for(int reel=0;reel<3;++reel)for(int result=0;result<4;++result)
    {
        assert(slotReelTravel(.74f,2.f,reel,result)==0.f);
        float previous=0.f;
        for(int frame=0;frame<=480;++frame)
        {
            const float travel=slotReelTravel(frame/240.f,2.f,reel,result);
            assert(travel>=previous-.00001f && travel<=12.f+result+.00001f);
            previous=travel;
        }
        assert(std::abs(previous-(12.f+result))<.0001f);
    }
    // Stair rise and gallery transition must agree with collision/camera height.
    const std::vector<MazePosition> refugeProbe{ { 4, 4 } };
    float stairHeight = 0.f;
    for (int step = 0; step <= 100; ++step)
    {
        const float z = 4.5f + refugeScale * (.30f + .85f * step / 100.f);
        const float next = refugeFloorHeight(4.5f + .98f * refugeScale, z, stairHeight, refugeProbe);
        assert(next >= stairHeight - .0001f && next - stairHeight < .03f);
        stairHeight = next;
    }
    assert(std::abs(stairHeight - refugeUpperFloor) < .001f);
    assert(refugeFloorHeight(4.5f, 4.5f + 1.15f * refugeScale, stairHeight, refugeProbe) == refugeUpperFloor);
    for (int step = 100; step >= 0; --step)
        stairHeight = refugeFloorHeight(4.5f + .98f * refugeScale, 4.5f + refugeScale * (.30f + .85f * step / 100.f), stairHeight, refugeProbe);
    assert(stairHeight < .001f);
    // Voice framing is bounded, independently decodable and rejects malformed input.
    VoicePacket voice;voice.generation=3;voice.actor=2;voice.sequence=7;
    for(std::size_t sample=0;sample<voiceSamples;++sample)
    {
        const auto pcm=static_cast<std::int16_t>(std::sin(sample*.12)*12000.);
        voice.samples[sample]=voiceEncodeSample(pcm);
        assert(std::abs(static_cast<int>(voiceDecodeSample(voice.samples[sample]))-pcm)<1024);
    }
    VoicePacket decoded;assert(parseVoice(serializeVoice(voice),decoded));
    assert(decoded.samples==voice.samples && decoded.actor==2 && decoded.sequence==7 && voiceLevel(decoded)>.1f);
    const auto unchanged=decoded.samples;
    assert(!parseVoice("V 3 9 7 00",decoded));
    assert(!parseVoice("V -1 2 7 "+std::string(640,'0'),decoded));
    assert(!parseVoice("V 3 2 7 "+std::string(640,'z'),decoded));
    assert(!parseVoice(serializeVoice(voice)+" extra",decoded));
    assert(decoded.samples==unchanged);
    sf::Font font; assert(font.openFromFile("assets/fonts/SourceSans3-Semibold.ttf"));
    sf::RenderWindow window(sf::VideoMode({640, 560}), "Co-op smoke test"); window.setVisible(false);
    Menu menu(font); menu.resize({640,560});
    sf::Event::MouseButtonPressed click{}; click.button = sf::Mouse::Button::Left; click.position = {320,405};
    assert(menu.handleEvent(sf::Event{click}, window) == MenuAction::Multiplayer);
    menu.showMultiplayer(); click.position = {200,344};
    assert(menu.handleEvent(sf::Event{click}, window) == MenuAction::HostGame);
    click.position = {430,344}; assert(menu.handleEvent(sf::Event{click}, window) == MenuAction::JoinGame);
    sf::Event::KeyPressed clear{}; clear.code = sf::Keyboard::Key::Delete;
    menu.handleEvent(sf::Event{clear}, window);
    for (char character : std::string("tcp://example.com:1234")) {
        sf::Event::TextEntered text{}; text.unicode = static_cast<char32_t>(character);
        menu.handleEvent(sf::Event{text}, window);
    }
    assert(menu.multiplayerAddress() == "tcp://example.com:1234");

    constexpr std::uint16_t testPort = 24817; // Do not interrupt a game hosted on 24816.
    MazeApplication host(font, ""), first(font, ""), second(font, ""), third(font, ""), extra(font, "");
    std::array<MazeApplication*, 4> guests{ &first, &second, &third, &extra };
    host.voice_.setDevicesEnabled(false);
    for(auto* guest:guests)guest->voice_.setDevicesEnabled(false);
    host.game_.selectedSeed = 42; host.beginNewGame();
    host.networkRole_ = MazeApplication::NetworkRole::Host; host.networkGeneration_ = 1;
    host.gameView_.update(host.game_, .01f, GameInput{}); host.network_.host(testPort);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    auto pump = [&](int steps) {
        for (int i = 0; i < steps; ++i) {
            host.updateNetwork(.02f);
            for (auto* guest : guests) guest->updateNetwork(.02f);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };
    auto join = [&](MazeApplication& guest) {
        guest.networkRole_ = MazeApplication::NetworkRole::Guest;
        guest.network_.join("127.0.0.1", testPort); pump(100);
    };
    join(first); join(second); join(third); pump(100);
    assert(host.network_.status() == Multiplayer::Status::Connected);
    assert(first.networkSlot_ == 1 && second.networkSlot_ == 2 && third.networkSlot_ == 3);
    assert(host.network_.connectedMask() == 14u);
    for (int i = 0; i < 3; ++i) {
        auto& guest = *guests[i];
        assert(guest.joinedGeneration_ == 1 && guest.game_.phase == GamePhase::Playing);
        assert(guest.game_.mazeSeed == host.game_.mazeSeed);
        assert(guest.gameView_.playerCount() == 4);
        for (int slot = 0; slot < 4; ++slot)
            assert(guest.gameView_.remote_[slot].present == (slot != guest.networkSlot_));
    }
    assert(host.gameView_.playerCount() == 4);
    // Synthetic speech traverses actual host relay; device I/O remains disabled.
    VoicePacket speech=voice;speech.generation=1;speech.actor=3;speech.sequence=10;
    assert(first.network_.send(serializeVoice(speech)));pump(30);
    assert(host.voiceReceived_[1]==1 && second.voiceReceived_[1]==1 && third.voiceReceived_[1]==1);
    assert(host.voiceReceived_[3]==0); // Claimed actor 3 was replaced by peer 1.
    assert(first.voiceReceived_[1]==0); // No self echo.
    assert(host.gameView_.voiceNoiseTime_[1]>0.f);
    assert(first.network_.send(serializeVoice(speech)));pump(20);
    assert(host.voiceReceived_[1]==1); // Replay rejected.
    ++speech.sequence;speech.generation=99;
    assert(first.network_.send(serializeVoice(speech)));pump(20);assert(host.voiceReceived_[1]==1);
    speech.generation=1;++speech.sequence;
    second.mutedVoices_|=1u<<1;
    assert(first.network_.send(serializeVoice(speech)));pump(20);
    assert(host.voiceReceived_[1]==2 && second.voiceReceived_[1]==1 && third.voiceReceived_[1]==2);
    second.mutedVoices_=0;
    const float nearX=third.gameView_.x_,nearZ=third.gameView_.z_;
    third.gameView_.x_=host.game_.maze.size().cols-.5f;third.gameView_.z_=host.game_.maze.size().rows-.5f;third.sendPlayer();pump(20);
    ++speech.sequence;assert(first.network_.send(serializeVoice(speech)));pump(20);
    assert(third.voiceReceived_[1]==2); // Host culls out-of-range recipients.
    third.gameView_.x_=nearX;third.gameView_.z_=nearZ;third.sendPlayer();pump(20);
    const auto beforeFlood=host.voiceReceived_[1];host.voiceTokens_[1]=2.f;
    for(unsigned frame=100;frame<130;++frame)
    {speech.sequence=frame;host.receiveVoice(serializeVoice(speech),1);}
    assert(host.voiceReceived_[1]==beforeFlood+2 && host.voiceTokens_[1]==0.f);
    pump(20); // Rate-excess speech cannot consume unbounded relay capacity.
    join(extra); pump(100);
    assert(extra.networkRole_ == MazeApplication::NetworkRole::Solo);
    assert(extra.menu_.notice_.find("four players") != std::string::npos);

    auto& hv = host.gameView_; auto& gv = first.gameView_;
    const auto beforeMalformed = gv.worldSnapshot();
    assert(!gv.applyWorldSnapshot(first.game_, "not a valid snapshot", first.networkSlot_));
    assert(gv.worldSnapshot() == beforeMalformed);
    const float before = gv.angle_; gv.look(40.f, 0.f); assert(gv.angle_ < before);
    GameInput turn; turn.turn = 1.f; const float beforeKey = gv.angle_;
    gv.update(first.game_, .05f, turn); assert(gv.angle_ < beforeKey);
    gv.angle_ = 0.f; gv.x_ = gv.z_ = 4.5f;
    for (int r=3;r<=5;++r) for(int c=3;c<=5;++c) first.game_.maze.at(r,c)=MazeCell::Passage;
    first.game_.player = {4,4}; GameInput strafe; strafe.strafe = 1.f;
    gv.update(first.game_, .05f, strafe); assert(gv.x_ < 4.5f);
    host.game_.phase = GamePhase::Paused; pump(30);
    for (int i = 0; i < 3; ++i) assert(guests[i]->networkPaused_);
    host.game_.phase = GamePhase::Playing; pump(30);
    for (int i = 0; i < 3; ++i) assert(!guests[i]->networkPaused_);
    host.restartCurrentGame(); hv.update(host.game_, .01f, GameInput{}); pump(60);
    for (int i = 0; i < 3; ++i) {
        assert(guests[i]->joinedGeneration_ == 2);
        assert(guests[i]->gameView_.x_ == .5f && guests[i]->gameView_.z_ == .5f);
    }
    auto sendKey = [&](MazeApplication& guest, char key) {
        guest.sendPlayer(); assert(guest.network_.send(std::string("K ")+key)); pump(40);
    };
    sendKey(first, 'E');
    assert(hv.inscriptionRead_ && first.gameView_.inscriptionRead_ && second.gameView_.inscriptionRead_ && third.gameView_.inscriptionRead_);
    for (int step=0; step<3; ++step) {
        auto& guest = *guests[step]; const int stone = hv.order_[step];
        guest.gameView_.x_ = hv.shrines_[stone].col+.5f; guest.gameView_.z_ = hv.shrines_[stone].row+.5f;
        guest.game_.player = hv.shrines_[stone]; sendKey(guest, 'E');
        assert(hv.solved_ == step+1);
        for (int i=0; i<3; ++i) assert(guests[i]->gameView_.solved_ == step+1 && guests[i]->gameView_.discoveredClues_[stone]);
    }
    const auto size = host.game_.maze.size();
    MazePosition neighbor{size.rows-1,size.cols-2};
    if (host.game_.maze.at(neighbor.row,neighbor.col)==MazeCell::Wall) neighbor={size.rows-2,size.cols-1};
    third.gameView_.x_=neighbor.col+.5f; third.gameView_.z_=neighbor.row+.5f; third.game_.player=neighbor;
    for (int digit : hv.gateCode_) sendKey(third, static_cast<char>('0'+digit));
    sendKey(third, 'N');
    assert(hv.gateOpen_);
    for (int i=0; i<3; ++i) assert(guests[i]->gameView_.gateOpen_);

    // A wall cannot close on any guest, including the third guest.
    hv.updateShift(host.game_,hv.shiftDelay_ + .01f); hv.updateShift(host.game_,2.f);
    assert(hv.shiftPhase_ == GameView::ShiftPhase::Open);
    third.gameView_.x_=hv.shift_.col+.5f; third.gameView_.z_=hv.shift_.row+.5f; third.game_.player=hv.shift_;
    third.sendPlayer(); pump(40); hv.updateShift(host.game_,hv.shiftDelay_ + .01f);
    assert(hv.shiftPhase_ == GameView::ShiftPhase::Open);

    // Visiting a refuge records a personal checkpoint and protects its occupant.
    assert(!hv.safehouses_.empty());
    const auto refuge = hv.safehouses_[0];
    const float houseX = refuge.col+.5f, houseZ = refuge.row+.5f;
    // Safehouse doors start closed, block the threshold and can be opened.
    assert(!hv.walkable(host.game_, houseX, houseZ - 1.45f * refugeScale));
    hv.toggleDoor(0, 2);
    hv.updateDoors(1.f);
    assert(hv.doors_[0][2].open);
    assert(hv.walkable(host.game_, houseX, houseZ - 1.45f * refugeScale));
    assert(!hv.walkable(host.game_, houseX + 1.02f * refugeScale, houseZ - 1.45f * refugeScale));
    second.gameView_.x_=refuge.col+.5f; second.gameView_.z_=refuge.row+.5f; second.game_.player=refuge;
    second.sendPlayer(); pump(40);
    assert(hv.checkpoints_[second.networkSlot_]==0 && second.gameView_.checkpoints_[second.networkSlot_]==0);
    hv.elapsed_=13.f; hv.monster_=hv.monsterFrom_=refuge; hv.monsterClock_=0.f;
    hv.updateMonster(host.game_,.001f,false,false); pump(20);
    assert(second.gameView_.seenRemoteCaptures_>0);
    hv.remoteCaptureCooldown_[second.networkSlot_]=0.f;

    // Capture guest two outside the refuge without teleporting either other guest.
    const auto victim = hv.shrines_[hv.order_[1]];
    second.gameView_.x_=victim.col+.5f; second.gameView_.z_=victim.row+.5f; second.game_.player=victim;
    second.sendPlayer(); pump(40);
    hv.elapsed_=13.f; hv.monster_=hv.monsterFrom_=victim; hv.monsterClock_=0.f;
    const auto firstCaptures=first.gameView_.seenRemoteCaptures_, thirdCaptures=third.gameView_.seenRemoteCaptures_;
    hv.updateMonster(host.game_,.001f,false,false); pump(40);
    assert(second.gameView_.x_==refuge.col+.5f && second.gameView_.z_==refuge.row+.5f && second.gameView_.seenRemoteCaptures_>0);
    assert(first.gameView_.seenRemoteCaptures_==firstCaptures && third.gameView_.seenRemoteCaptures_==thirdCaptures);

    const int recycledSlot = second.networkSlot_;
    second.endNetwork(); pump(70);
    assert(host.networkRole_==MazeApplication::NetworkRole::Host && host.network_.connectedMask()==10u);
    assert(first.gameView_.playerCount()==3 && third.gameView_.playerCount()==3);
    join(second); pump(80);
    assert(second.networkSlot_==recycledSlot && second.gameView_.seenRemoteCaptures_==0);
    assert(first.gameView_.playerCount()==4 && third.gameView_.playerCount()==4);
    third.gameView_.x_=size.cols-.5f; third.gameView_.z_=size.rows-.5f;
    third.game_.player={size.rows-1,size.cols-1}; third.sendPlayer(); pump(70);
    assert(host.game_.phase==GamePhase::Won);
    for (int i=0; i<3; ++i) assert(guests[i]->game_.phase==GamePhase::Won);

    // A different seed selects the balance variant and shares lit stones.
    host.game_.selectedSeed = 43; host.beginNewGame(); ++host.networkGeneration_;
    hv.update(host.game_,.01f,GameInput{}); pump(70);
    assert(hv.puzzle_.stoneKind == StonePuzzleKind::Balance);
    for (int i=0; i<3; ++i) assert(guests[i]->joinedGeneration_==3);
    sendKey(first, 'E');
    unsigned answer = 0;
    for (unsigned mask=1; mask<8; ++mask)
        if (stoneBalance(hv.puzzle_,mask)==hv.puzzle_.balanceTarget) answer=mask;
    assert(answer != 0);
    int touched = 0;
    for (int stone=0; stone<3; ++stone) if (answer & (1u << stone))
    {
        auto& guest = *guests[touched++];
        guest.gameView_.x_=hv.shrines_[stone].col+.5f; guest.gameView_.z_=hv.shrines_[stone].row+.5f;
        guest.game_.player=hv.shrines_[stone]; sendKey(guest,'E');
        for (int i=0; i<3; ++i) assert(guests[i]->gameView_.activeStones_==hv.activeStones_);
    }
    assert(hv.solved_==3);
    for (int i=0; i<3; ++i) assert(guests[i]->gameView_.solved_==3);
    // Physical games are host-authoritative and shared between actual TCP peers.
    const auto gamesHouse = hv.safehouses_[0];
    for (auto* guest : { &first, &second })
    {
        guest->gameView_.x_ = gamesHouse.col + .5f - .7f;
        guest->gameView_.z_ = gamesHouse.row + .5f + .5f;
        guest->gameView_.elevation_ = refugeUpperFloor;
        guest->gameView_.angle_ = 0.f; guest->game_.player = gamesHouse;
        guest->sendPlayer();
    }
    pump(30); sendKey(first, 'G');
    assert(hv.refugeBalls_[0][1].carrier == first.networkSlot_);
    sendKey(second, 'G');
    assert(hv.refugeBalls_[0][1].carrier == first.networkSlot_);
    for (int i=0; i<3; ++i) assert(guests[i]->gameView_.refugeBalls_[0][1].carrier == first.networkSlot_);
    sendKey(first, 'G');
    assert(hv.refugeBalls_[0][1].carrier==first.networkSlot_ && hv.refugeBalls_[0][1].throwWindup>.27f);
    for(int i=0;i<3;++i)assert(guests[i]->gameView_.refugeBalls_[0][1].throwWindup>.27f);
    hv.updateRefugeGames(.14f);
    assert(hv.refugeBalls_[0][1].carrier==first.networkSlot_);
    hv.updateRefugeGames(.14f);
    assert(hv.refugeBalls_[0][1].carrier == -1 && hv.refugeBalls_[0][1].vy > 0.f);
    auto& basket = hv.refugeBalls_[0][1];
    basket.x = -.55f; basket.z = 1.53f; basket.y = 1.13f;
    basket.vx = basket.vz = 0.f; basket.vy = -1.f;
    hv.updateRefugeGames(.03f); assert(basket.scoreA == 1);
    auto& football = hv.refugeBalls_[0][0];
    football.x = -.55f; football.z = 1.91f; football.vz = 2.f;
    hv.updateRefugeGames(.03f); assert(football.scoreA == 1 && football.scoreB == 0);
    football.x = -.55f; football.z = -1.91f; football.vz = -2.f;
    hv.updateRefugeGames(.03f); assert(football.scoreA == 1 && football.scoreB == 1);
    pump(30);
    for (int i=0;i<3;++i)
    { assert(guests[i]->gameView_.refugeBalls_[0][1].scoreA == 1); assert(guests[i]->gameView_.refugeBalls_[0][0].scoreB == 1); }
    // Ground-floor players cannot take upstairs balls.
    first.gameView_.elevation_ = 0.f; first.sendPlayer(); pump(20); sendKey(first, 'G');
    assert(basket.carrier == -1);
    // Guests pull the physical cabinet handle; cooldown is shared per cabinet.
    first.gameView_.x_ = gamesHouse.col + .5f - .56f * refugeScale;
    first.gameView_.z_ = gamesHouse.row + .5f - .85f * refugeScale + .50f;
    first.gameView_.angle_ = 3.14159265f; first.gameView_.elevation_ = 0.f;
    first.sendPlayer(); pump(20); sendKey(first, 'E');
    assert(hv.refugeSlots_[0].spins == 1);
    for (int i=0;i<3;++i) assert(guests[i]->gameView_.refugeSlots_[0].symbols == hv.refugeSlots_[0].symbols);
    sendKey(first, 'E'); assert(hv.refugeSlots_[0].spins == 1);
    if (hv.refugeSlots_.size() > 1) assert(hv.refugeSlots_[1].spins == 0);
    first.gameView_.elevation_ = refugeUpperFloor;
    hv.elapsed_ += 3.f; first.sendPlayer(); pump(20); sendKey(first, 'E');
    assert(hv.refugeSlots_[0].spins == 1);
    // Bar drinks restore the guest who drank, never the host or another guest.
    first.gameView_.x_=gamesHouse.col+.5f-.85f*refugeScale;
    first.gameView_.z_=gamesHouse.row+.5f+.85f*refugeScale;
    first.gameView_.elevation_=0.f; first.gameView_.stamina_=.2f;
    hv.stamina_=.3f; first.sendPlayer(); pump(20); sendKey(first,'E');
    assert(hv.drinks_[first.networkSlot_]==1 && first.gameView_.stamina_==1.f);
    assert(first.gameView_.seatedHouse_==0);
    first.sendPlayer();pump(20);
    assert(hv.remote_[first.networkSlot_].seated && second.gameView_.remote_[first.networkSlot_].seated);
    const float seatX=first.gameView_.x_,seatZ=first.gameView_.z_;
    GameInput seatedWalk; seatedWalk.forward=1.f;
    first.gameView_.update(first.game_,.05f,seatedWalk);
    assert(first.gameView_.x_==seatX && first.gameView_.z_==seatZ);
    sf::Event::KeyPressed stand; stand.code=sf::Keyboard::Key::Q;
    first.gameView_.handleEvent(sf::Event{stand},first.game_);
    assert(first.gameView_.seatedHouse_==-1);
    assert(hv.stamina_==.3f && hv.refugeNoise_[0]>0.f);
    sendKey(first,'E'); assert(hv.drinks_[first.networkSlot_]==1);
    second.gameView_.x_=first.gameView_.x_; second.gameView_.z_=first.gameView_.z_;
    second.gameView_.elevation_=0.f; second.sendPlayer(); pump(20); sendKey(second,'E');
    assert(hv.drinks_[second.networkSlot_]==1 && second.gameView_.stamina_==1.f);
    // Guest poker actions use host authority; other survivors retain their hands.
    sf::Event::KeyPressed pokerKey; pokerKey.code=sf::Keyboard::Key::P;
    first.handleEvent(sf::Event{pokerKey},window);pump(40);
    assert(first.gameView_.pokerVisible_);
    const int pokerSlot=first.networkSlot_;
    assert(hv.poker_[pokerSlot].phase==1 && first.gameView_.poker_[pokerSlot].phase==1);
    assert(hv.poker_[0].phase==0 && hv.poker_[second.networkSlot_].phase==0);
    sendKey(first,'1');sendKey(first,'3');
    assert(hv.poker_[pokerSlot].held==5 && second.gameView_.poker_[pokerSlot].held==5);
    const auto beforeDraw=hv.poker_[pokerSlot].cards;
    hv.elapsed_+=2.f; pump(20);sendKey(first,'P');
    assert(hv.poker_[pokerSlot].phase==2 && first.gameView_.poker_[pokerSlot].phase==2);
    assert(hv.poker_[pokerSlot].cards[0]==beforeDraw[0] && hv.poker_[pokerSlot].cards[2]==beforeDraw[2]);
    assert(first.gameView_.poker_[pokerSlot].cards==hv.poker_[pokerSlot].cards);
    assert(hv.refugeNoise_[0]>0.f);
    auto malformedPoker=hv.worldSnapshot();
    malformedPoker.replace(malformedPoker.find_last_of(' ')+1,std::string::npos,"52");
    assert(!first.gameView_.applyWorldSnapshot(first.game_,malformedPoker,pokerSlot));
    first.handleEvent(sf::Event{stand},window);
    assert(!first.gameView_.pokerVisible_);
    // Explore -> loot -> buy -> craft -> use: all owned by the host in co-op.
    sendKey(first,'Q');
    assert(!hv.caches_.empty());const auto cache=hv.caches_[0];
    first.gameView_.x_=cache.cell.col+.5f;first.gameView_.z_=cache.cell.row+.5f;first.gameView_.elevation_=0;
    first.sendPlayer();pump(20);
    const auto beforeLoot=hv.packs_[pokerSlot];sendKey(first,'E');
    assert((hv.openedCaches_&1) && (second.gameView_.openedCaches_&1));
    assert(hv.packs_[pokerSlot].coins==beforeLoot.coins+cache.coins);
    assert(hv.packs_[pokerSlot].materials[cache.kind]==beforeLoot.materials[cache.kind]+2);
    const auto lootedCoins=hv.packs_[pokerSlot].coins;sendKey(first,'E');assert(hv.packs_[pokerSlot].coins==lootedCoins);
    first.gameView_.x_=gamesHouse.col+.5f-.85f*refugeScale;first.gameView_.z_=gamesHouse.row+.5f+.85f*refugeScale;
    first.sendPlayer();pump(20);sendKey(first,'O');assert(first.gameView_.adventurePanels_[pokerSlot]==2);
    const auto purchasedBefore=hv.packs_[pokerSlot].items[0];sendKey(first,'1');
    assert(hv.packs_[pokerSlot].items[0]==purchasedBefore+1 && hv.packs_[pokerSlot].coins==lootedCoins-Adventure::price(0,0));
    sendKey(first,'C');assert(hv.adventurePanels_[pokerSlot]==3);sendKey(first,'1');
    assert(hv.packs_[pokerSlot].items[0]==purchasedBefore+2);
    sendKey(first,'I');first.gameView_.stamina_=.1f;sendKey(first,'1');
    assert(first.gameView_.stamina_==1.f && hv.packs_[pokerSlot].items[0]==purchasedBefore+1);
    sendKey(first,'4');assert(hv.packs_[pokerSlot].effects[2]==0.f); // Animation cooldown prevents double-use.
    hv.updateAdventure(host.game_,1.3f);sendKey(first,'4');assert(hv.packs_[pokerSlot].effects[2]==20.f && second.gameView_.packs_[pokerSlot].effects[2]==20.f);
    hv.packs_[pokerSlot].items[4]=1;
    first.gameView_.x_=cache.cell.col+.5f;first.gameView_.z_=cache.cell.row+.5f;first.sendPlayer();pump(20);hv.updateAdventure(host.game_,1.3f);sendKey(first,'5');
    assert(first.gameView_.x_==gamesHouse.col+.5f && first.gameView_.z_==gamesHouse.row+.5f);
    assert(hv.packs_[pokerSlot].items[4]==0 && first.gameView_.packs_[pokerSlot].recalls==1);
    hv.packs_[pokerSlot].items[1]=1;hv.updateAdventure(host.game_,1.3f);sendKey(first,'2');assert(hv.packs_[pokerSlot].effects[0]==12.f);
    hv.packs_[pokerSlot].items[5]=1;hv.updateAdventure(host.game_,1.3f);sendKey(first,'6');assert(hv.packs_[pokerSlot].echoTime==8.f);
    sendKey(first,'Q');assert(hv.adventurePanels_[pokerSlot]==0);
    hv.packs_[pokerSlot].effects={};hv.packs_[pokerSlot].echoTime=0;
    // Walk up the stair, onto the gallery, back to the stair and down again.
    hv.x_ = gamesHouse.col + .5f + .98f * refugeScale;
    hv.z_ = gamesHouse.row + .5f + .20f * refugeScale;
    hv.elevation_ = 0.f; hv.angle_ = 0.f; hv.elapsed_ = 0.f;
    host.game_.player = gamesHouse;
    GameInput stairWalk; stairWalk.forward = 1.f;
    for (int i=0;i<30;++i) hv.update(host.game_,.05f,stairWalk);
    assert(hv.elevation_ > refugeUpperFloor - .01f);
    stairWalk.forward = 0.f; stairWalk.strafe = 1.f;
    for (int i=0;i<16;++i) hv.update(host.game_,.05f,stairWalk);
    assert(hv.elevation_ == refugeUpperFloor && hv.x_ < gamesHouse.col + .5f + .65f * refugeScale);
    stairWalk.strafe = -1.f;
    for (int i=0;i<16;++i) hv.update(host.game_,.05f,stairWalk);
    stairWalk.strafe = 0.f; stairWalk.forward = -1.f;
    for (int i=0;i<30;++i) hv.update(host.game_,.05f,stairWalk);
    assert(hv.elevation_ < .01f);
    // A complete pickup/throw arc can score; scoring isn't just a HUD command.
    hv.x_ = gamesHouse.col + .5f - .55f; hv.z_ = gamesHouse.row + .5f - .993f;
    hv.elevation_ = refugeUpperFloor; hv.angle_ = 0.f;
    basket.x = -.55f; basket.z = -.993f; basket.y = .13f; basket.carrier = -1;
    basket.vx = basket.vy = basket.vz = 0.f;
    hv.playRefugeBall(true, 0); assert(basket.carrier == 0);
    hv.updateRefugeGames(.01f); hv.playRefugeBall(true, 0);
    const unsigned beforeArc = basket.scoreA;
    hv.updateRefugeGames(1.5f); assert(basket.scoreA == beforeArc + 1);
    basket.carrier = first.networkSlot_; hv.remote_[first.networkSlot_].present = false;
    hv.updateRefugeGames(.01f); assert(basket.carrier == -1);
    // Jump rises and lands; holding crouch prevents jumping and slows movement.
    hv.jumpHeight_ = hv.jumpVelocity_ = 0.f; hv.jumpHeld_ = false;
    GameInput leap; leap.jump = true;
    hv.update(host.game_, .05f, leap); assert(hv.jumpHeight_ > .05f);
    for (int i=0;i<25;++i) hv.update(host.game_, .05f, GameInput{});
    assert(hv.jumpHeight_ == 0.f && hv.jumpVelocity_ == 0.f);
    GameInput sneak; sneak.crouch = true; sneak.jump = true;
    hv.update(host.game_, .05f, sneak);
    assert(hv.crouched_ && hv.jumpHeight_ == 0.f && hv.crouchBlend_ > 0.f);
    assert(hv.shiftDelay_ >= 8.f && hv.shiftDelay_ <= 35.f);
    // Co-op stance is carried through actual TCP player and world messages.
    first.gameView_.jumpHeight_=.25f; first.gameView_.crouched_=false;
    first.sendPlayer(); pump(30);
    assert(std::abs(hv.remote_[first.networkSlot_].jumpHeight-.25f)<.001f);
    assert(std::abs(second.gameView_.remote_[first.networkSlot_].jumpHeight-.25f)<.001f);
    first.gameView_.jumpHeight_=0.f; first.gameView_.crouched_=true;
    first.sendPlayer(); pump(30);
    assert(hv.remote_[first.networkSlot_].crouched && second.gameView_.remote_[first.networkSlot_].crouched);
    first.gameView_.crouched_=false;
    // The hunter follows the physical staircase up, then descends when abandoned.
    for(auto& player:hv.remote_)player.present=false;
    hv.interiorHunter_=false; hv.hunterY_=0.f;
    hv.monster_=hv.monsterFrom_=gamesHouse; hv.hunter_.cell=gamesHouse;
    hv.hunter_.mode=HunterMode::Patrol;hv.hunter_.memory=0;
    hv.x_=gamesHouse.col+.5f-.55f; hv.z_=gamesHouse.row+.5f-.5f;
    hv.elevation_=refugeUpperFloor; hv.jumpHeight_=0.f;
    hv.updateInteriorHunter(host.game_,.05f);
    assert(!hv.interiorHunter_); // Silent players cannot be seen through the floor.
    const HunterTarget upstairsNoise{{static_cast<int>(hv.z_),static_cast<int>(hv.x_)},true,true,12,false,false,refugeUpperFloor};
    advanceHunterGroup(host.game_.maze,hv.hunter_,&upstairsNoise,1,hv.safehouses_,~hv.doorMask());
    assert(hv.hunter_.mode==HunterMode::Search && hv.hunter_.lastKnownElevation==refugeUpperFloor);
    for(int i=0;i<400 && hv.interiorPhase_!=4;++i) hv.updateInteriorHunter(host.game_,.05f);
    assert(hv.interiorHunter_ && hv.interiorPhase_==4 && hv.hunterY_==refugeUpperFloor);
    for(int i=0;i<100;++i) hv.updateInteriorHunter(host.game_,.05f);
    assert(std::abs(hv.hunterX_-hv.x_)<.01f && std::abs(hv.hunterZ_-hv.z_)<.01f);
    hv.elevation_=0.f;
    for(auto& player:hv.remote_) player.present=false;
    hv.initialized_=false; // No fresh sightings or sounds while the trail expires.
    for(int i=0;i<400 && hv.interiorHunter_;++i) hv.updateInteriorHunter(host.game_,.05f);
    assert(!hv.interiorHunter_ && hv.hunterY_==0.f && hv.hunter_.mode==HunterMode::Patrol);
    hv.updateInteriorHunter(host.game_,.05f);assert(!hv.interiorHunter_);
    hv.initialized_=true;
    // Closed doors delay entry, then the hunter forces the actual doorway.
    hv.x_=gamesHouse.col+.5f; hv.z_=gamesHouse.row+.5f; hv.elevation_=0.f;
    host.game_.player=gamesHouse; hv.elapsed_=13.f;
    hv.monster_=hv.monsterFrom_={gamesHouse.row,gamesHouse.col-3};
    hv.hunter_.cell=hv.monster_; hv.hunter_.previous={gamesHouse.row,gamesHouse.col-4};
    hv.hunter_.mode=HunterMode::Chase; hv.monsterClock_=0.f;hv.refugeNoise_[0]=5.f;
    hv.doors_[0][0].open=false; hv.doors_[0][0].swing=0.f; hv.doorStrikes_.fill(0);
    bool enteredHouse=false;
    unsigned warnedStrikes=0;
    pump(20); // Deliver earlier capture acknowledgements before placing listeners.
    for(int i=0;i<8 && !enteredHouse;++i)
    {
        for(auto& player:hv.remote_)player.present=false; // Isolate the doorway pursuit target.
        const auto previousDoorEvent=hv.doorEvent_;
        hv.updateMonster(host.game_,1.2f,false,false);
        if(hv.doorEvent_!=previousDoorEvent)
        {
            pump(20);
            assert(first.gameView_.doorEvent_==hv.doorEvent_ && first.gameView_.doorSource_==0);
            assert(first.gameView_.doors_[0][0].rattle==hv.doors_[0][0].rattle);
            assert(first.gameView_.doorStrikes_[0]==hv.doorStrikes_[0]);
            assert(first.gameView_.soundFrame().doorSlam);
            // Audio direction belongs to the listener, including after respawn.
            first.gameView_.x_=gamesHouse.col+1.5f;first.gameView_.z_=gamesHouse.row+.5f;first.gameView_.angle_=0.f;first.gameView_.elevation_=0.f;
            second.gameView_.x_=gamesHouse.col+.5f-3.2f;second.gameView_.z_=gamesHouse.row+.5f;second.gameView_.angle_=0.f;second.gameView_.elevation_=0.f;
            assert(first.gameView_.soundFrame().doorPan>.9f && second.gameView_.soundFrame().doorPan<-.9f);
            if(hv.doorStrikes_[0]) ++warnedStrikes;
        }
        hv.updateDoors(1.f);
        enteredHouse=insideSafehouse(hv.monster_,gamesHouse);
    }
    assert(hv.doors_[0][0].open && enteredHouse && warnedStrikes==2);
    // Upper-floor capture compares physical height, not footprint immunity.
    hv.interiorHunter_=true; hv.interiorHouse_=0; hv.interiorPhase_=4;
    hv.hunterX_=hv.x_; hv.hunterZ_=hv.z_; hv.hunterY_=refugeUpperFloor;
    hv.elevation_=refugeUpperFloor; hv.elapsed_=13.f;
    hv.hunter_.routes[0]={{4,4},{4,5},3};
    const auto beforeUpperCapture=hv.captureCount_;
    hv.refugeSlots_[0].stopTime=14.4f;hv.poker_[0].readyAt=13.7f;
    const auto preservedHand=hv.poker_[first.networkSlot_].cards;
    hv.updateMonster(host.game_,.01f,false,false);
    assert(hv.captureCount_==beforeUpperCapture+1);
    assert(hv.elapsed_==0.f && std::abs(hv.refugeSlots_[0].stopTime-1.4f)<.001f);
    assert(std::abs(hv.poker_[0].readyAt-.7f)<.001f);
    assert(hv.poker_[first.networkSlot_].cards==preservedHand);
    assert(hv.hunter_.routes[0].confidence==3 && hv.hunter_.routes[0].exit.col==5);
    Multiplayer unavailable; unavailable.join("127.0.0.1", 1);
    for (int i=0; i<400 && unavailable.status()==Multiplayer::Status::Connecting; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(unavailable.status()==Multiplayer::Status::Error);
    // Render current cabin geometry on both floors for visual review.
    host.game_.phase = GamePhase::Playing;
    const auto renderHouse = hv.safehouses_.front();
    hv.x_ = renderHouse.col + .5f; hv.z_ = renderHouse.row + .5f;
    host.game_.player = renderHouse; hv.angle_ = 3.8f; hv.pitch_ = 0.f;
    hv.elevation_ = 0.f; hv.resize({640,560});
    sf::RenderTexture refugeTarget({640,560}, sf::ContextSettings{24});
    hv.draw(refugeTarget, host.game_); refugeTarget.display();
    for (const auto mesh : hv.refugePropMeshes_) assert(mesh != 0);
    assert(hv.slotsTexture_ != 0 && hv.barTexture_ != 0);
    for (const auto& reel : hv.reelMeshes_) for (const auto mesh : reel) assert(mesh != 0);
    assert(hv.handMeshes_[0] && hv.handMeshes_[1] && hv.handTexture_);
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/refuge-ground.png"));
    hv.doors_[0][0].open=false;hv.doors_[0][0].rattle=1.f;hv.doorStrikes_[0]=2;
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/door-warning.png"));
    hv.doors_[0][0].open=true;hv.doors_[0][0].rattle=0.f;hv.doorStrikes_[0]=0;
    for(auto mesh:hv.storyMeshes_) assert(mesh!=0);
    for(int i=0;i<4;++i) assert(hv.storyTextures_[i]!=0 && hv.storySigns_[i]);
    assert(hv.footprintsReady_ && !hv.storyFootsteps_.empty());
    hv.elevation_ = refugeUpperFloor; hv.angle_ = 0.f; hv.pitch_ = -.12f;
    hv.draw(refugeTarget, host.game_); refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/refuge-upper.png"));
    hv.elevation_=0.f; hv.angle_=3.8f; hv.pitch_=0.f;
    hv.refugeSlots_[0].stopTime=hv.elapsed_+1.6f;
    hv.draw(refugeTarget,host.game_); refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/refuge-spinning.png"));
    // Settled reel results must visibly change the downloaded model itself.
    hv.refugeSlots_[0].stopTime=0.f; hv.refugeSlots_[0].symbols={0,0,0};
    hv.draw(refugeTarget,host.game_); refugeTarget.display();
    const auto reelA=refugeTarget.getTexture().copyToImage();
    hv.refugeSlots_[0].symbols={3,3,3};
    hv.draw(refugeTarget,host.game_); refugeTarget.display();
    const auto reelB=refugeTarget.getTexture().copyToImage();
    std::size_t changedReelPixels=0;
    for(unsigned y=165;y<340;++y) for(unsigned x=170;x<420;++x)
        if(reelA.getPixel({x,y})!=reelB.getPixel({x,y})) ++changedReelPixels;
    assert(changedReelPixels>20);
    assert(hv.barStoolMesh_ && hv.drinkMesh_ && hv.drinkTexture_);
    const auto knobRest=slotKnobPoint(renderHouse,0.f,2.f);
    const auto knobPulled=slotKnobPoint(renderHouse,.65f,2.f);
    assert(knobPulled.z<knobRest.z-.08f && knobPulled.y<knobRest.y);
    hv.sitAtBar(0); hv.seatedBlend_=1.f; hv.drinkClock_=2.f;
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/bar-drinking.png"));
    RefugePoker::play(hv.poker_[0],42,0,hv.elapsed_+2.f);hv.pokerVisible_=true;
    hv.poker_[0].readyAt=hv.elapsed_+.9f;
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/poker-dealing.png"));
    hv.poker_[0].readyAt=hv.elapsed_;
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/refuge-poker.png"));
    hv.pokerVisible_=false;
    hv.seatedHouse_=-1;hv.seatedBlend_=0.f;hv.drinkClock_=0.f;
    hv.crouched_=false;hv.jumpHeight_=0.f;hv.pitch_=0.f;hv.bobBlend_=0.f;hv.ballHoldBlend_=1.f;hv.crouchBlend_=0.f;hv.elevation_=refugeUpperFloor;hv.x_=renderHouse.col+.5f-.55f;hv.z_=renderHouse.row+.5f-.5f;hv.angle_=0.f;
    hv.refugeBalls_[0][1].carrier=0;hv.updateRefugeGames(.01f);
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/basketball-hands.png"));
    hv.heldBallLast_=true;hv.playRefugeBall(true,0);
    hv.update(host.game_,.05f,GameInput{});
    assert(hv.refugeBalls_[0][1].carrier==0 && hv.refugeBalls_[0][1].throwWindup>0.f);
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/basketball-windup.png"));
    for(int frame=0;frame<6;++frame)hv.update(host.game_,.05f,GameInput{});
    assert(hv.throwClock_>0.f && hv.refugeBalls_[0][1].carrier==-1);
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/basketball-release.png"));
    for(int side=0;side<2;++side)
    {
        assert(hv.handRigs_[side] && hv.handSkinTexture_);
        const auto& rig=*hv.handRigs_[side];
        assert(rig.vertices.size()>10000 && rig.bones.size()>=19);
        HandMotion::Pose openRig,closedRig;
        const HandMotion::V shoulder{side==0?-.22f:.22f,-.27f,.015f},palm{side==0?-.15f:.15f,-.12f,-.31f};
        HandMotion::skin(rig,openRig,shoulder,palm,{0,.25f,-1},{side==0?1.f:-1.f,0,0},0.f);
        HandMotion::skin(rig,closedRig,shoulder,palm,{0,.25f,-1},{side==0?1.f:-1.f,0,0},1.f);
        const auto& joints=closedRig.joints;
        assert(std::abs(HandMotion::length(joints[rig.lower]-shoulder)-HandMotion::length(rig.bones[rig.lower].at-rig.bones[rig.upper].at))<.00001f);
        assert(std::abs(HandMotion::length(joints[rig.hand]-joints[rig.lower])-HandMotion::length(rig.bones[rig.hand].at-rig.bones[rig.lower].at))<.00001f);
        assert(HandMotion::length(joints[rig.hand]-(palm-HandMotion::unit({0,.25f,-1})*rig.palmLength))<.00001f);
        std::size_t deformed=0;
        for(std::size_t v=0;v<rig.vertices.size();++v)
        {
            const auto& a=openRig.vertices[v];const auto& b=closedRig.vertices[v];
            for(float n:b.position)assert(std::isfinite(n));
            deformed+=std::abs(a.position[0]-b.position[0])+std::abs(a.position[1]-b.position[1])+std::abs(a.position[2]-b.position[2])>.0001f;
        }
        assert(deformed>500);
        assert(hv.handOpen_[side] && hv.handGrip_[side]);
        const auto& open=*hv.handOpen_[side];const auto& grip=*hv.handGrip_[side];
        std::size_t changed=0;
        for(std::size_t vertex=0;vertex<open.vertices.size();++vertex)
            if(std::abs(open.vertices[vertex].position[0]-grip.vertices[vertex].position[0])+
               std::abs(open.vertices[vertex].position[1]-grip.vertices[vertex].position[1])+
               std::abs(open.vertices[vertex].position[2]-grip.vertices[vertex].position[2])>.0001f) ++changed;
        assert(changed>100);
    }
    hv.refugeBalls_[0][1].carrier=-1;hv.ballHoldBlend_=0.f;hv.elevation_=0.f;
    const auto restKnob=slotKnobPoint(renderHouse,0.f,2.f);
    hv.alignSlotView(0);
    hv.refugeSlots_[0].actor=0;hv.refugeSlots_[0].stopTime=hv.elapsed_+1.35f;
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(refugeTarget.getTexture().copyToImage().saveToFile("build/slot-hand-contact.png"));
    hv.x_=.5f;hv.z_=.5f;hv.elevation_=0.f;hv.pitch_=-.20f;hv.angle_=0.f;
    hv.drinkClock_=0.f;hv.seatedHouse_=-1;hv.seatedBlend_=0.f;hv.throwClock_=0.f;
    host.game_.player={0,0};
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    assert(hv.worldShader_ && hv.postShader_ && hv.groundMapsReady_ && hv.woodMapsReady_);
    const auto mapped=refugeTarget.getTexture().copyToImage();
    assert(mapped.saveToFile("build/exterior-materials.png"));
    hv.groundMapsReady_=false;
    hv.draw(refugeTarget,host.game_);refugeTarget.display();
    const auto unmapped=refugeTarget.getTexture().copyToImage();
    std::size_t materialPixels=0;
    for(unsigned y=350;y<500;++y)for(unsigned x=40;x<600;++x)
        materialPixels+=mapped.getPixel({x,y})!=unmapped.getPixel({x,y});
    assert(materialPixels>500); // Normal/roughness maps affect the actual GPU render.
    hv.groundMapsReady_=true;
    int longest=0;MazePosition sightStart{},sightEnd{};float sightAngle=0.f;
    const auto mazeSize=host.game_.maze.size();
    for(int row=0;row<mazeSize.rows;++row)for(int col=0;col<mazeSize.cols;++col)
    for(int direction=0;direction<2;++direction)
    {
        int run=0;
        while(run<8)
        {
            const MazePosition at{row+(direction==0?run:0),col+(direction==1?run:0)};
            if(!host.game_.maze.contains(at) || host.game_.maze.at(at.row,at.col)==MazeCell::Wall || safehouseAt(at,hv.safehouses_)>=0)break;
            ++run;
        }
        if(run>longest)
        {longest=run;sightStart={row,col};sightEnd={row+(direction==0?run-1:0),col+(direction==1?run-1:0)};sightAngle=direction==0?0.f:1.57079633f;}
    }
    if(longest>=4)
    {
        hv.x_=sightStart.col+.5f;hv.z_=sightStart.row+.5f;hv.angle_=sightAngle;hv.pitch_=0.f;
        host.game_.player=sightStart;hv.elapsed_=13.f;hv.interiorHunter_=false;
        hv.monster_=hv.monsterFrom_=hv.hunter_.cell=sightEnd;hv.hunter_.mode=HunterMode::Search;
        hv.draw(refugeTarget,host.game_);refugeTarget.display();
        assert(refugeTarget.getTexture().copyToImage().saveToFile("build/exterior-hunter-sightline.png"));
    }
    // Inspect the actual downloaded story assets in their seeded locations.
    hv.elevation_=0.f;hv.elapsed_=2.f;hv.interiorHunter_=false;hv.messageClock_=0.f;
    for(int i=0;i<4;++i)
    {
        const auto site=hv.storySites_[i];
        hv.x_=site.cell.col+.5f;hv.z_=site.cell.row+.5f;host.game_.player=site.cell;
        hv.angle_=std::atan2(site.x-.5f,site.z-.5f);hv.pitch_=-.85f;
        assert(hv.nearbyStorySite()==i);
        sf::Event::KeyPressed readNote;readNote.code=sf::Keyboard::Key::F;
        hv.handleEvent(sf::Event{readNote},host.game_);assert(hv.fieldNotes_&(1u<<i));
        hv.draw(refugeTarget,host.game_);refugeTarget.display();
        assert(refugeTarget.getTexture().copyToImage().saveToFile("build/story-site-"+std::to_string(i)+".png"));
    }
    if(hv.storyRuin_)
    {
        const auto ruin=*hv.storyRuin_;
        hv.x_=ruin.cell.col+.5f+(ruin.alongRows?0.f:3.f);
        hv.z_=ruin.cell.row+.5f+(ruin.alongRows?3.f:0.f);
        host.game_.player={static_cast<int>(hv.z_),static_cast<int>(hv.x_)};
        hv.angle_=ruin.alongRows?3.14159265f:-1.57079633f;hv.pitch_=0.f;hv.messageClock_=0.f;
        assert(host.game_.maze.at(host.game_.player.row,host.game_.player.col)==MazeCell::Passage);
        hv.draw(refugeTarget,host.game_);refugeTarget.display();
        assert(refugeTarget.getTexture().copyToImage().saveToFile("build/story-ruin.png"));
    }
    std::cout << "All poker rankings, hold masks, directional card audio, guest poker and story assets passed.\n";
    std::cout << "Screen-space ray tracing active: " << hv.rayTracingActive() << "\n";
    std::cout << "Four-player joins, capacity, both puzzles, refuges, captures, shifting, reconnect, and victory passed.\n";
}

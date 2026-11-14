#include "MazeApplication.h"
#include "Maze.Game.h"
#include "Maze.Acoustics.h"
#include "Maze.Generation.h"
#include "Maze.Settings.h"
#include "Maze.Validation.h"
#include "SettingsPath.h"

#include <algorithm>
#include <exception>
#include <charconv>
#include <cmath>
#include <iostream>
#include <sstream>
#include <utility>

namespace
{
    bool parseAddress(std::string text, std::string& host, std::uint16_t& port)
    {
        if (text.rfind("tcp://", 0) == 0) text.erase(0, 6);
        const auto colon = text.rfind(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 == text.size()) return false;
        host = text.substr(0, colon);
        if (host.size() > 253 || host.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") != std::string::npos) return false;
        unsigned value = 0;
        const auto* first = text.data() + colon + 1; const auto* last = text.data() + text.size();
        const auto parsed = std::from_chars(first, last, value);
        if (parsed.ec != std::errc{} || parsed.ptr != last || value == 0 || value > 65535) return false;
        port = static_cast<std::uint16_t>(value); return true;
    }

    char networkKey(sf::Keyboard::Key key)
    {
        if(key==sf::Keyboard::Key::I)return 'I';
        if(key==sf::Keyboard::Key::O)return 'O';
        if(key==sf::Keyboard::Key::N)return 'C';
        if(key==sf::Keyboard::Key::Tab)return 'A';
        if(key==sf::Keyboard::Key::Q)return 'Q';
        if (key == sf::Keyboard::Key::G) return 'G';
        if (key == sf::Keyboard::Key::P) return 'P';
        if (key == sf::Keyboard::Key::K) return 'K';
        if (key == sf::Keyboard::Key::E) return 'E';
        if (key == sf::Keyboard::Key::Backspace) return 'B';
        if (key == sf::Keyboard::Key::Enter) return 'N';
        const sf::Keyboard::Key top[] = { sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2, sf::Keyboard::Key::Num3,
            sf::Keyboard::Key::Num4, sf::Keyboard::Key::Num5, sf::Keyboard::Key::Num6,
            sf::Keyboard::Key::Num7, sf::Keyboard::Key::Num8, sf::Keyboard::Key::Num9 };
        const sf::Keyboard::Key pad[] = { sf::Keyboard::Key::Numpad1, sf::Keyboard::Key::Numpad2, sf::Keyboard::Key::Numpad3,
            sf::Keyboard::Key::Numpad4, sf::Keyboard::Key::Numpad5, sf::Keyboard::Key::Numpad6,
            sf::Keyboard::Key::Numpad7, sf::Keyboard::Key::Numpad8, sf::Keyboard::Key::Numpad9 };
        for (int i = 0; i < 9; ++i) if (key == top[i] || key == pad[i]) return static_cast<char>('1' + i);
        return 0;
    }

    sf::Keyboard::Key decodeNetworkKey(char key)
    {
        if(key=='I')return sf::Keyboard::Key::I;
        if(key=='O')return sf::Keyboard::Key::O;
        if(key=='C')return sf::Keyboard::Key::N;
        if(key=='A')return sf::Keyboard::Key::Tab;
        if(key=='Q')return sf::Keyboard::Key::Q;
        if (key == 'G') return sf::Keyboard::Key::G;
        if (key == 'P') return sf::Keyboard::Key::P;
        if (key == 'K') return sf::Keyboard::Key::K;
        if (key == 'E') return sf::Keyboard::Key::E;
        if (key == 'B') return sf::Keyboard::Key::Backspace;
        if (key == 'N') return sf::Keyboard::Key::Enter;
        const sf::Keyboard::Key keys[] = { sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2, sf::Keyboard::Key::Num3,
            sf::Keyboard::Key::Num4, sf::Keyboard::Key::Num5, sf::Keyboard::Key::Num6,
            sf::Keyboard::Key::Num7, sf::Keyboard::Key::Num8, sf::Keyboard::Key::Num9 };
        return key >= '1' && key <= '9' ? keys[key - '1'] : sf::Keyboard::Key::Unknown;
    }
}

MazeApplication::MazeApplication(const sf::Font& font)
    : MazeApplication(font, settingsFilePath())
{
}

MazeApplication::MazeApplication(const sf::Font& font, std::filesystem::path settingsPath,
    Clock::time_point now)
    : settingsPath_(std::move(settingsPath)), previousTick_(now), previousFrame_(now), menu_(font), gameView_(font)
{
    game_.selectedSize = loadMazeSizePreferences(settingsPath_);
    showMainMenu();
}

void MazeApplication::resize(sf::Vector2u size)
{
    menu_.resize(size);
    gameView_.resize(size);
}

const GameState& MazeApplication::gameState() const noexcept
{
    return game_;
}

bool MazeApplication::canContinue() const noexcept
{
    return game_.hasActiveGame()
        && (game_.phase == GamePhase::Playing || game_.phase == GamePhase::Paused);
}

bool MazeApplication::canRestart() const noexcept
{
    if (networkRole_ == NetworkRole::Guest) return false;
    return canContinue() || (game_.hasActiveGame() && game_.phase == GamePhase::Won);
}

void MazeApplication::showMainMenu()
{
    voice_.reset();voiceSpeaking_.fill(0.f);
    if (game_.phase == GamePhase::Playing) game_.phase = GamePhase::Paused;
    menu_.setHasActiveGame(canContinue());
    menu_.setCanRestart(canRestart());
    menu_.showMain();
}

void MazeApplication::beginNewGame()
{
    try
    {
        const auto seed = game_.selectedSeed ? *game_.selectedSeed : generateMazeSeed();
        startGame(game_, generateMaze(game_.selectedSize, seed), seed);
        gameView_.reset();
    }
    catch (const std::exception& error)
    {
        menu_.showMessage("Could not start a new maze. Please try again.");
        std::cerr << "Starting a new maze failed: " << error.what() << '\n';
    }
}

void MazeApplication::restartCurrentGame()
{
    if (networkRole_ == NetworkRole::Guest) return;
    gameView_.reset(&game_);
    restartGame(game_);
    if (networkRole_ == NetworkRole::Host) ++networkGeneration_;
}

void MazeApplication::endNetwork()
{
    voice_.reset();voiceSequenceSeen_.fill(false);voiceSpeaking_.fill(0.f);voiceGeneration_=0;
    network_.stop(); networkRole_ = NetworkRole::Solo; networkStatus_ = Multiplayer::Status::Idle;
    networkPaused_ = false; joinedGeneration_ = 0; networkSlot_ = 0; networkSendClock_ = networkWaitClock_ = 0.f;
    gameView_.setNetworkGuest(false); gameView_.remoteDisconnected();
    gameView_.setNetworkStatus("");
}

void MazeApplication::sendPlayer()
{
    auto player = gameView_.localPlayer();
    if (!player.present) return;
    if (game_.phase != GamePhase::Playing || networkPaused_) player.moving = player.running = false;
    std::ostringstream out; out.precision(8);
    out << "P " << player.x << ' ' << player.z << ' ' << player.angle << ' ' << player.moving << ' ' << player.running << ' ' << player.elevation << ' ' << player.jumpHeight << ' ' << player.crouched << ' ' << player.seated;
    network_.send(out.str());
}

void MazeApplication::playVoice(const VoicePacket& packet)
{
    if(!voiceEnabled_ || (mutedVoices_&(1u<<packet.actor)))return;
    const auto source=gameView_.playerAt(packet.actor),listener=gameView_.localPlayer();
    if(!source.present || !listener.present)return;
    const float dx=source.x-listener.x,dz=source.z-listener.z;
    const float distance=std::sqrt(dx*dx+dz*dz);
    if(distance>=12.f)return;
    const auto houses=generateSafehouses(game_.maze.size(),game_.mazeSeed.value_or(0));
    const auto transmission=acousticTransmission(game_.maze,
        {static_cast<int>(source.z),static_cast<int>(source.x)},game_.player,houses,~gameView_.openDoorMask());
    const bool otherFloor=std::abs(source.elevation-listener.elevation)>1.f;
    const float gain=transmission.gain*(1.f-distance/12.f)*(otherFloor?.55f:1.f);
    const float pan=distance>.001f?std::clamp((-dx*std::cos(listener.angle)+dz*std::sin(listener.angle))/distance,-1.f,1.f):0.f;
    voice_.play(packet,gain,pan,transmission.clarity*(otherFloor?.5f:1.f));
    ++voiceReceived_[packet.actor];voiceSpeaking_[packet.actor]=.25f;
}

void MazeApplication::transmitVoice(VoicePacket packet)
{
    if(networkRole_==NetworkRole::Solo || game_.phase!=GamePhase::Playing || networkPaused_ || !gameView_.initialized())return;
    packet.generation=networkRole_==NetworkRole::Host?networkGeneration_:joinedGeneration_;
    if(!packet.generation)return;
    packet.actor=networkRole_==NetworkRole::Host?0:networkSlot_;
    if(networkRole_==NetworkRole::Guest){network_.send(serializeVoice(packet));return;}
    gameView_.emitVoiceNoise(0,voiceLevel(packet));
    const auto source=gameView_.localPlayer();
    for(int peer=1;peer<maxCoopPlayers;++peer)
    {
        const auto listener=gameView_.playerAt(peer);
        if(!listener.present || (source.x-listener.x)*(source.x-listener.x)+(source.z-listener.z)*(source.z-listener.z)>=144.f)continue;
        network_.send(serializeVoice(packet),peer);
    }
}

void MazeApplication::receiveVoice(const std::string& message,int peer)
{
    if(game_.phase!=GamePhase::Playing || networkPaused_ || !gameView_.initialized())return;
    VoicePacket packet;if(!parseVoice(message,packet))return;
    const unsigned generation=networkRole_==NetworkRole::Host?networkGeneration_:joinedGeneration_;
    if(!generation || packet.generation!=generation)return;
    if(voiceGeneration_!=generation)
    {voiceGeneration_=generation;voiceSequenceSeen_.fill(false);voiceTokens_.fill(6.f);voice_.reset();}
    if(networkRole_==NetworkRole::Host)
    {
        if(peer<1 || peer>=maxCoopPlayers || !(network_.connectedMask()&(1u<<peer)))return;
        packet.actor=peer; // Never trust a guest's claimed speaker identity.
    }
    else if(networkRole_!=NetworkRole::Guest || peer!=0 || packet.actor==networkSlot_)return;
    const int actor=packet.actor;
    if(voiceTokens_[actor]<1.f || (voiceSequenceSeen_[actor] &&
        static_cast<std::int32_t>(packet.sequence-voiceSequence_[actor])<=0))return;
    voiceTokens_[actor]-=1.f;voiceSequenceSeen_[actor]=true;voiceSequence_[actor]=packet.sequence;
    if(networkRole_==NetworkRole::Host)
    {
        const auto source=gameView_.playerAt(actor);if(!source.present)return;
        gameView_.emitVoiceNoise(actor,voiceLevel(packet));
        for(int destination=1;destination<maxCoopPlayers;++destination)
        {
            if(destination==actor)continue;
            const auto listener=gameView_.playerAt(destination);
            if(!listener.present || (source.x-listener.x)*(source.x-listener.x)+(source.z-listener.z)*(source.z-listener.z)>=144.f)continue;
            network_.send(serializeVoice(packet),destination);
        }
    }
    playVoice(packet);
}

void MazeApplication::updateVoice(sf::RenderWindow& window)
{
    const unsigned generation=networkRole_==NetworkRole::Host?networkGeneration_:joinedGeneration_;
    if(voiceGeneration_!=generation)
    {voice_.reset();voiceGeneration_=generation;voiceSequenceSeen_.fill(false);voiceTokens_.fill(6.f);}
    const bool eligible=voiceEnabled_ && window.hasFocus() && game_.phase==GamePhase::Playing &&
        !networkPaused_ && networkRole_!=NetworkRole::Solo && network_.connectedMask()!=0 &&
        (networkRole_==NetworkRole::Host || joinedGeneration_>0) && gameView_.initialized();
    const bool talking=eligible && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V);
    voice_.setTalking(talking);
    for(auto packet:voice_.captured())if(talking)transmitVoice(packet);
    std::string status=!voiceEnabled_?"VOICE OFF / F8":"VOICE / HOLD V";
    if(talking)status=voice_.status()==-1?"VOICE / NO MICROPHONE":"VOICE / TRANSMITTING";
    else if(voice_.status()==-2)status="VOICE / NO OUTPUT DEVICE";
    for(int peer=0;peer<maxCoopPlayers;++peer)if(voiceSpeaking_[peer]>0.f)status+=" "+std::to_string(peer+1);
    gameView_.setVoiceStatus(status);
}

void MazeApplication::updateNetwork(float seconds)
{
    for(int actor=0;actor<maxCoopPlayers;++actor)
    {voiceTokens_[actor]=std::min(6.f,voiceTokens_[actor]+std::clamp(seconds,0.f,.2f)*60.f);voiceSpeaking_[actor]=std::max(0.f,voiceSpeaking_[actor]-seconds);}
    if (networkRole_ == NetworkRole::Solo) return;
    const auto status = network_.status();
    if (status != networkStatus_)
    {
        networkStatus_ = status;
        if (status == Multiplayer::Status::Connected)
        {
            if (networkRole_ == NetworkRole::Guest) menu_.showMessage("Connected. Waiting for the host's maze...");
            networkSendClock_ = .1f;
        }
        else if (status == Multiplayer::Status::Listening && networkRole_ == NetworkRole::Host)
            gameView_.setNetworkStatus("CO-OP 1/4 / WAITING");
    }
    for (const auto& message : network_.receive())
    {
        if (networkRole_ == NetworkRole::Host && message.kind == MultiplayerMessage::Kind::Joined)
        {
            voiceSequenceSeen_[message.peer]=false;voiceTokens_[message.peer]=6.f;voiceSpeaking_[message.peer]=0.f;
            gameView_.remoteDisconnected(message.peer);
            gameView_.showNetworkNotice("A companion joined the maze.");
            networkSendClock_ = .1f;
            continue;
        }
        if (networkRole_ == NetworkRole::Host && message.kind == MultiplayerMessage::Kind::Left)
        {
            voiceSequenceSeen_[message.peer]=false;voiceTokens_[message.peer]=6.f;voiceSpeaking_[message.peer]=0.f;
            gameView_.remoteDisconnected(message.peer);
            gameView_.showNetworkNotice("A companion left the maze.");
            continue;
        }
        if (message.kind != MultiplayerMessage::Kind::Data) continue;
        std::istringstream in(message.text); char type = 0; in >> type;
        if(type=='V'){receiveVoice(message.text,message.peer);continue;}
        if (networkRole_ == NetworkRole::Guest && type == 'F')
        {
            endNetwork(); menu_.showMultiplayer(); menu_.showMessage("That game already has four players.");
            return;
        }
        if (networkRole_ == NetworkRole::Host && type == 'P' && gameView_.initialized())
        {
            RemotePlayer player; int moving, running, crouching, seating; std::string extra;
            if (!(in >> player.x >> player.z >> player.angle >> moving >> running >> player.elevation >> player.jumpHeight >> crouching >> seating) || (in >> extra) ||
                !std::isfinite(player.jumpHeight) || player.jumpHeight<0.f || player.jumpHeight>.6f || (crouching!=0 && crouching!=1) || (seating!=0 && seating!=1) || !std::isfinite(player.elevation) || player.elevation < 0.f || player.elevation > refugeUpperFloor || !std::isfinite(player.x) || !std::isfinite(player.z) || !std::isfinite(player.angle) ||
                player.x < 0 || player.z < 0 || player.x >= game_.maze.size().cols || player.z >= game_.maze.size().rows ||
                (moving != 0 && moving != 1) || (running != 0 && running != 1)) continue;
            const MazePosition cell{ static_cast<int>(player.z), static_cast<int>(player.x) };
            if (game_.maze.at(cell.row, cell.col) == MazeCell::Wall) continue;
            player.moving = moving != 0; player.running = running != 0; player.present = true; player.crouched=crouching!=0; player.seated=seating!=0;
            gameView_.setRemotePlayer(message.peer, player);
            if (game_.maze.at(cell.row, cell.col) == MazeCell::Exit && gameView_.gateOpen() && game_.phase == GamePhase::Playing)
            {
                // A guest can only enter the exit after receiving the host's open-gate state.
                game_.phase = GamePhase::Won;
                menu_.setHasActiveGame(false); menu_.setCanRestart(canRestart());
                menu_.showVictory(game_.maze.size(), game_.statistics, game_.mazeSeed);
            }
        }
        else if (networkRole_ == NetworkRole::Host && type == 'K' && game_.phase == GamePhase::Playing)
        {
            char key = 0; std::string extra;
            if (!(in >> key) || (in >> extra)) continue;
            const auto code = decodeNetworkKey(key);
            if (code == sf::Keyboard::Key::Unknown) continue;
            sf::Event::KeyPressed event{}; event.code = code;
            gameView_.handleRemoteEvent(message.peer, sf::Event{event}, game_);
        }
        else if (networkRole_ == NetworkRole::Guest && type == 'W')
        {
            unsigned version, generation, seed, phase; int rows, cols, selfSlot;
            if (!(in >> version)) continue;
            if (version != 16)
            {
                endNetwork(); menu_.showMultiplayer(); menu_.showMessage("Different game version. Everyone needs the same build.");
                return;
            }
            if (!(in >> generation >> rows >> cols >> seed >> phase >> selfSlot) || generation == 0 ||
                phase > 2 || selfSlot < 1 || selfSlot >= maxCoopPlayers || !isValidMazeSize({ rows, cols })) continue;
            std::string world; std::getline(in, world);
            const bool newGeneration = joinedGeneration_ != generation;
            if (!newGeneration && networkSlot_ != selfSlot) continue;
            if (newGeneration)
            {
                try
                {
                    gameView_.reset(&game_); startGame(game_, generateMaze({ rows, cols }, seed), seed);
                    gameView_.setNetworkGuest(true); gameView_.update(game_, 0.f, GameInput{});
                }
                catch (const std::exception&) { continue; }
            }
            if (!game_.mazeSeed || *game_.mazeSeed != seed || game_.maze.size().rows != rows || game_.maze.size().cols != cols ||
                !gameView_.applyWorldSnapshot(game_, world, selfSlot))
            {
                if (newGeneration) { gameView_.reset(&game_); endGame(game_); }
                continue;
            }
            if (newGeneration) { joinedGeneration_ = generation; networkSlot_ = selfSlot; }
            gameView_.setNetworkStatus("CO-OP " + std::to_string(gameView_.playerCount()) + "/4");
            const bool wasPaused = networkPaused_;
            networkPaused_ = phase == 1;
            if (wasPaused != networkPaused_)
                gameView_.showNetworkNotice(networkPaused_ ? "The host paused the game." : "The host resumed the game.");
            if (phase == 2 && game_.phase != GamePhase::Won)
            {
                game_.phase = GamePhase::Won;
                menu_.setHasActiveGame(false); menu_.setCanRestart(false);
                menu_.showVictory(game_.maze.size(), game_.statistics, game_.mazeSeed);
            }
        }
    }
    if (status == Multiplayer::Status::Error || status == Multiplayer::Status::Disconnected)
    {
        const bool guest = networkRole_ == NetworkRole::Guest;
        endNetwork();
        if (guest) { endGame(game_); showMainMenu(); menu_.showMessage("Connection lost. Return to Multiplayer to try again."); }
        else gameView_.showNetworkNotice("Connection lost. Continuing alone.");
        return;
    }
    if (status == Multiplayer::Status::Connected && networkRole_ == NetworkRole::Guest && joinedGeneration_ == 0)
    {
        networkWaitClock_ += std::clamp(seconds, 0.f, .2f);
        if (networkWaitClock_ > 10.f)
        {
            endNetwork(); endGame(game_); menu_.showMultiplayer();
            menu_.showMessage("No compatible game received. Check the address and game version.");
            return;
        }
    }
    if (status != Multiplayer::Status::Connected) return;
    if (networkRole_ == NetworkRole::Host)
    {
        int count = 1;
        for (int slot = 1; slot < maxCoopPlayers; ++slot) if (network_.connectedMask() & (1u << slot)) ++count;
        gameView_.setNetworkStatus("CO-OP " + std::to_string(count) + "/4");
    }
    networkSendClock_ += std::clamp(seconds, 0.f, .2f);
    if (networkSendClock_ < .1f) return;
    networkSendClock_ = 0.f;
    if (networkRole_ == NetworkRole::Guest) sendPlayer();
    else if (gameView_.initialized() && game_.mazeSeed)
    {
        const unsigned phase = game_.phase == GamePhase::Won ? 2u : game_.phase == GamePhase::Playing ? 0u : 1u;
        const auto world = gameView_.worldSnapshot();
        for (int slot = 1; slot < maxCoopPlayers; ++slot)
        {
            if (!(network_.connectedMask() & (1u << slot))) continue;
            std::ostringstream out;
            out << "W 16 " << networkGeneration_ << ' ' << game_.maze.size().rows << ' ' << game_.maze.size().cols
                << ' ' << *game_.mazeSeed << ' ' << phase << ' ' << slot << ' ' << world;
            network_.send(out.str(), slot);
        }
    }
}

void MazeApplication::updateTime(Clock::time_point now)
{
    if (now <= previousTick_) return;
    advanceGameTime(game_, now - previousTick_);
    previousTick_ = now;
}

void MazeApplication::handleEvent(const sf::Event& event, sf::RenderWindow& window,
    Clock::time_point now)
{
    // Account for time in the old phase before an event pauses, resumes or wins.
    updateTime(now);
    if(const auto* key=event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code==sf::Keyboard::Key::F8)
        {voiceEnabled_=!voiceEnabled_;voice_.setMuted(!voiceEnabled_);return;}
        const sf::Keyboard::Key muteKeys[]={sf::Keyboard::Key::F1,sf::Keyboard::Key::F2,sf::Keyboard::Key::F3,sf::Keyboard::Key::F4};
        for(int peer=0;peer<maxCoopPlayers;++peer)if(key->code==muteKeys[peer])
        {mutedVoices_^=1u<<peer;voice_.reset();voiceSpeaking_[peer]=0.f;gameView_.showNetworkNotice("Voice "+std::to_string(peer+1)+((mutedVoices_&(1u<<peer))?" muted":" unmuted"));return;}
    }
    if (event.is<sf::Event::Closed>())
    {
        endNetwork();
        endGame(game_);
        window.close();
        return;
    }
    if (const auto* resized = event.getIf<sf::Event::Resized>())
    {
        if (resized->size.x != 0 && resized->size.y != 0)
        {
            window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, {
                static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)
            })));
            resize(resized->size);
        }
        return;
    }

    if (game_.phase == GamePhase::Playing && event.is<sf::Event::FocusLost>())
    {
        showMainMenu();
        return;
    }
    if (game_.phase == GamePhase::Playing)
    {
        const auto* key = event.getIf<sf::Event::KeyPressed>();
        if (key && key->code == sf::Keyboard::Key::Escape) showMainMenu();
        else if (key && key->code == sf::Keyboard::Key::R) restartCurrentGame();
        else if (key && key->code == sf::Keyboard::Key::B) gameView_.toggleHeadMotion();
        else if (key && key->code == sf::Keyboard::Key::T) gameView_.toggleRayTracing();
        else if (key && key->code == sf::Keyboard::Key::M)
        {
            soundscape_.setMuted(!soundscape_.muted());
            gameView_.setSoundMuted(soundscape_.muted());
        }
        else
        {
            const char remoteKey = key && networkRole_ == NetworkRole::Guest && !networkPaused_ ? networkKey(key->code) : 0;
            if (remoteKey)
            {
                sendPlayer(); network_.send(std::string("K ") + remoteKey);
                if(key->code==sf::Keyboard::Key::E)gameView_.previewGather();
                if(key->code==sf::Keyboard::Key::P || key->code==sf::Keyboard::Key::I || key->code==sf::Keyboard::Key::O || key->code==sf::Keyboard::Key::N || key->code==sf::Keyboard::Key::Tab || key->code==sf::Keyboard::Key::Q) gameView_.handleEvent(event,game_);
            }
            else if (networkRole_ != NetworkRole::Guest || (key && !networkPaused_ &&
                (key->code == sf::Keyboard::Key::Q || key->code == sf::Keyboard::Key::F || key->code == sf::Keyboard::Key::J ||
                 key->code == sf::Keyboard::Key::Space))) gameView_.handleEvent(event, game_);
            if (game_.phase == GamePhase::Won)
            {
                menu_.setHasActiveGame(false);
                menu_.setCanRestart(canRestart());
                menu_.showVictory(game_.maze.size(), game_.statistics, game_.mazeSeed);
            }
        }
        return;
    }

    switch (menu_.handleEvent(event, window))
    {
    case MenuAction::HowToPlay:
        menu_.showHowToPlay();
        break;
    case MenuAction::Settings:
        menu_.showSettings(game_.selectedSize, game_.selectedSeed);
        break;
    case MenuAction::Multiplayer:
        menu_.showMultiplayer();
        break;
    case MenuAction::HostGame:
        endNetwork(); beginNewGame();
        if (game_.phase == GamePhase::Playing)
        {
            gameView_.update(game_, 0.f, GameInput{});
            networkRole_ = NetworkRole::Host; networkGeneration_ = 1;
            network_.host(); networkStatus_ = Multiplayer::Status::Idle;
            gameView_.setNetworkStatus("CO-OP 1/4 / WAITING");
            gameView_.showNetworkNotice("Hosting on port 24816. Start ngrok and share its TCP address.");
        }
        break;
    case MenuAction::JoinGame:
    {
        std::string host; std::uint16_t port;
        if (!parseAddress(menu_.multiplayerAddress(), host, port))
        { menu_.showMessage("Enter a TCP address like 1.tcp.ngrok.io:12345."); break; }
        endNetwork(); networkRole_ = NetworkRole::Guest;
        network_.join(host, port); networkStatus_ = Multiplayer::Status::Idle;
        menu_.showMessage("Connecting...");
        break;
    }
    case MenuAction::Back:
        if (networkRole_ == NetworkRole::Guest && joinedGeneration_ == 0) endNetwork();
        showMainMenu();
        break;
    case MenuAction::ApplySettings:
    {
        std::optional<std::uint32_t> seed;
        if (isValidMazeSize(menu_.draftSize()) && menu_.draftSeed(seed))
        {
            game_.selectedSize = menu_.draftSize();
            game_.selectedSeed = seed;
            const bool saved = saveMazeSizePreferences(settingsPath_, game_.selectedSize);
            showMainMenu();
            if (saved)
            {
                menu_.showMessage("Saved size: " + std::to_string(game_.selectedSize.rows)
                    + " x " + std::to_string(game_.selectedSize.cols) + ". Next seed: "
                    + (seed ? std::to_string(*seed) : "random") + ".");
            }
            else
            {
                menu_.showMessage("Size set for this launch only. Could not save preferences.");
                std::cerr << "Could not save maze size preferences to: " << settingsPath_ << '\n';
            }
        }
        break;
    }
    case MenuAction::NewGame:
        endNetwork();
        beginNewGame();
        break;
    case MenuAction::RestartMaze:
        restartCurrentGame();
        break;
    case MenuAction::Continue:
        if (canContinue())
        {
            game_.phase = GamePhase::Playing;
            gameView_.resetFeedback();
        }
        break;
    case MenuAction::Exit:
        endNetwork();
        endGame(game_);
        window.close();
        break;
    case MenuAction::None:
        break;
    }
}

void MazeApplication::update(sf::RenderWindow& window, Clock::time_point now)
{
    updateTime(now);
    const float seconds = std::chrono::duration<float>(now - previousFrame_).count();
    previousFrame_ = now;
    updateNetwork(seconds);
    updateVoice(window);
    if (game_.phase == GamePhase::Playing && window.hasFocus())
    {
        const auto size = window.getSize();
        const sf::Vector2i center{ static_cast<int>(size.x / 2), static_cast<int>(size.y / 2) };
        if (!mouseCaptured_)
        {
            window.setMouseCursorGrabbed(true);
            window.setMouseCursorVisible(false);
            sf::Mouse::setPosition(center, window);
            mouseCaptured_ = true;
        }
        else
        {
            const auto cursor = sf::Mouse::getPosition(window);
            gameView_.look(static_cast<float>(cursor.x - center.x), static_cast<float>(cursor.y - center.y));
            sf::Mouse::setPosition(center, window);
        }
        if (!networkPaused_) gameView_.update(game_, seconds);
        if (networkRole_ == NetworkRole::Guest && game_.phase == GamePhase::Won)
        {
            // The host confirms the shared victory after receiving our position.
            game_.phase = GamePhase::Playing; sendPlayer();
        }
        if (game_.phase == GamePhase::Won)
        {
            menu_.setHasActiveGame(false);
            menu_.setCanRestart(canRestart());
            menu_.showVictory(game_.maze.size(), game_.statistics, game_.mazeSeed);
        }
    }
    gameView_.setSoundAvailability(soundscape_.status());
    if (game_.phase == GamePhase::Playing && window.hasFocus())
        soundscape_.setFrame(gameView_.soundFrame());
    else
        soundscape_.setFrame({});
    if (game_.phase != GamePhase::Playing || !window.hasFocus())
    {
        if (mouseCaptured_)
        {
            window.setMouseCursorGrabbed(false);
            window.setMouseCursorVisible(true);
            mouseCaptured_ = false;
        }
        menu_.update(window.mapPixelToCoords(sf::Mouse::getPosition(window)));
    }
}

void MazeApplication::draw(sf::RenderTarget& target)
{
    if (game_.phase == GamePhase::Playing) gameView_.draw(target, game_);
    else menu_.draw(target);
}

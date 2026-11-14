#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <string>
#include <memory>
#include <utility>
#include <vector>
#include "GameState.h"
#include "Maze.Hunter.h"
#include "Maze.Soundscape.h"
#include "Maze.Puzzle.h"
#include "Maze.Safehouse.h"
#include "Maze.CreatureMesh.h"
#include "HandRig.h"
#include "Maze.Story.h"
#include "Maze.Poker.h"
#include "Maze.Adventure.h"

struct GameInput
{
    float forward = 0.f;
    float strafe = 0.f;
    float turn = 0.f;
    bool run = false;
    bool crouch = false, jump = false;
};

struct RemotePlayer
{
    float x = .5f, z = .5f, angle = 0.f;
    bool moving = false, running = false, present = false;
    float elevation = 0.f;
    float jumpHeight = 0.f;
    bool crouched = false, seated = false;
};

struct SafehouseDoor
{
    bool open = false;
    float swing = 0.f;
    float rattle = 0.f;
    float travel = 0.f;
};

struct RefugeSlots
{
    unsigned spins = 0;
    int actor = 0;
    float stopTime = 0.f;
    std::array<int, 3> symbols{};
};

struct RefugeBall
{
    float x = 0.f, y = .13f, z = 0.f;
    float vx = 0.f, vy = 0.f, vz = 0.f;
    float throwWindup = 0.f;
    int carrier = -1;
    unsigned scoreA = 0, scoreB = 0;
};

constexpr int maxCoopPlayers = 4;

class GameView
{
public:
    explicit GameView(const sf::Font& font);
    void resize(sf::Vector2u size);
    void reset(GameState* game = nullptr);
    void resetFeedback();
    void handleEvent(const sf::Event& event, GameState& game);
    void update(GameState& game, float seconds);
    void update(GameState& game, float seconds, const GameInput& input);
    void look(float horizontalPixels, float verticalPixels);
    void draw(sf::RenderTarget& target, const GameState& game);
    SoundscapeFrame soundFrame() const;
    void setSoundMuted(bool muted);
    void setSoundAvailability(int status) noexcept;
    void toggleHeadMotion();
    void toggleRayTracing();
    bool rayTracingEnabled() const noexcept { return rayTraceEnabled_; }
    bool rayTracingActive() const noexcept { return postReady_ && rayTraceEnabled_; }
    void setNetworkGuest(bool guest) noexcept { networkGuest_ = guest; if (!guest) networkSelfSlot_ = 0; }
    RemotePlayer localPlayer() const noexcept;
    unsigned openDoorMask() const noexcept { return doorMask(); }
    RemotePlayer playerAt(int slot) const noexcept { return slot==networkSelfSlot_?localPlayer():slot>=0 && slot<maxCoopPlayers?remote_[slot]:RemotePlayer{}; }
    void emitVoiceNoise(int slot,float level);
    void setVoiceStatus(std::string text) { voiceStatus_=std::move(text); }
    void setRemotePlayer(int slot, const RemotePlayer& player) noexcept;
    std::string worldSnapshot() const;
    bool applyWorldSnapshot(GameState& game, const std::string& data, int selfSlot);
    void handleRemoteEvent(int slot, const sf::Event& event, GameState& game);
    void remoteDisconnected(int slot = -1) noexcept;
    bool initialized() const noexcept { return initialized_; }
    bool gateOpen() const noexcept { return gateOpen_; }
    int playerCount() const noexcept
    {
        int count = 1;
        for (const auto& player : remote_) if (player.present) ++count;
        return count;
    }
    void showNetworkNotice(const std::string& text) { notice_ = text; messageClock_ = 5.f; }
    void setNetworkStatus(std::string text) { networkStatusText_ = std::move(text); }
    void previewGather();

private:
    void initialize(GameState& game);
    bool walkable(const GameState& game, float x, float z) const;
    void interact(GameState& game);
    int nearbyShrine() const;
    int nearbyDoor() const;
    int nearbySlotHandle() const;
    int nearbyBar() const;
    bool pokerEvent(sf::Keyboard::Key key, const GameState& game);
    void drawPoker(sf::RenderTarget& target);
    void drawDoorWarning(sf::RenderTarget& target);
    bool adventureEvent(sf::Keyboard::Key key,GameState& game);
    int nearbyCache() const;
    int adventureHouse() const;
    void useAdventureItem(int actor,int item,GameState& game);
    void recallAdventurer(int actor,GameState& game);
    void updateAdventure(const GameState& game,float seconds);
    void drawAdventure(const GameState& game);
    void drawAdventureHud(sf::RenderTarget& target,const GameState& game);
    std::array<Adventure::Pack,maxCoopPlayers> packs_{};
    std::array<int,maxCoopPlayers> adventurePanels_{};
    std::vector<Adventure::Cache> caches_;
    std::uint64_t openedCaches_=0;
    unsigned seenRecall_=0,seenRestore_=0;
    std::vector<bool> surveyed_;
    std::array<unsigned int,5> adventureMeshes_{},adventureTextures_{};
    std::array<float,48> cacheOpenClocks_{};
    float adventureMotionClock_=0.f;
    int adventureMotionItem_=0;
    int lootingCache_=-1;
    bool adventureAssetsAttempted_=false;
    std::array<std::unique_ptr<sf::RenderTexture>,4> adventureSigns_{};
    void sitAtBar(int house);
    void pullSlotHandle(const GameState& game);
    void alignSlotView(int house);
    bool nearGate(const GameState& game) const;
    bool nearInscription() const;
    void resetHunter(const GameState& game);
    void updateMonster(GameState& game, float seconds, bool playerMoving, bool playerRunning);
    void updateInteriorHunter(const GameState& game,float seconds);
    void updateShift(GameState& game, float seconds);
    void updateDoors(float seconds);
    void updateFear(GameState& game, float seconds);
    void updateScares(GameState& game, float seconds);
    void updateRefugeGames(float seconds);
    void playRefugeBall(bool basketball, int slot);
    void toggleDoor(int house, int side, bool slam = false);
    bool doorBlocks(float x, float z) const;
    unsigned int doorMask() const noexcept;
    void drawDoors(const GameState& game);
    bool preparePostProcessing();
    void releasePostTargets();
    void drawPostProcess(const GameState& game, float flash);
    void invalidateChunksAround(MazePosition cell);
    void prepareChunks(const GameState& game);
    void updateChunkVisibility(const GameState& game);
    void drawGroundChunks(const GameState& game, int r0, int r1, int c0, int c1);
    void drawHedgeChunks(const GameState& game, int r0, int r1, int c0, int c1);
    void drawStory(const GameState& game);
    void prepareStoryAssets(sf::RenderTarget& target);
    int nearbyStorySite() const noexcept;
    void drawFoliageChunks(const GameState& game, int r0, int r1, int c0, int c1);
    sf::Vector2u viewport_{ 900u, 600u };
    sf::Text voiceLabel_;
    std::string voiceStatus_="VOICE / HOLD V";
    std::array<float,maxCoopPlayers> voiceNoiseTime_{},voiceNoiseLevel_{};
    sf::Text heading_, objective_, journal_, hint_, seedText_, feedback_, staminaLabel_, soundLabel_;
    bool initialized_ = false;
    std::unique_ptr<sf::Shader> worldShader_, creatureShader_, foliageShader_, postShader_, reelShader_;
    bool shaderAttempted_ = false;
    bool postReady_ = false;
    sf::Texture sceneColor_;
    unsigned int sceneDepth_ = 0, sceneFbo_ = 0;
    sf::Vector2u postSize_{};
    unsigned int hedgeTexture_ = 0, groundTexture_ = 0, monsterTexture_ = 0, foliageTexture_ = 0;
    unsigned int shelterTexture_ = 0;
    std::optional<StoryRuin> storyRuin_;
    std::array<StorySite,4> storySites_{};
    std::array<unsigned int,5> storyMeshes_{},storyTextures_{};
    std::array<std::unique_ptr<sf::RenderTexture>,4> storySigns_{};
    sf::Texture footprintLeft_,footprintRight_;
    bool footprintsReady_=false;
    std::vector<StoryFootstep> storyFootsteps_;
    bool storyAssetsAttempted_=false;
    sf::Texture groundAlbedo_,groundNormal_,groundMaterial_,woodAlbedo_,woodNormal_,woodMaterial_;
    bool groundMapsReady_=false,woodMapsReady_=false;
    std::array<unsigned int, 4> refugePropMeshes_{};
    std::array<std::array<unsigned int,4>,3> reelMeshes_{};
    unsigned int slotsTexture_ = 0, barTexture_ = 0;
    unsigned int barStoolMesh_ = 0, drinkMesh_ = 0, drinkTexture_ = 0;
    bool refugePropsAttempted_ = false;
    std::array<unsigned int,2> handMeshes_{};
    std::array<std::optional<CreatureMesh>,2> handOpen_{}, handGrip_{};
    std::array<std::optional<HandMotion::Rig>,2> handRigs_{};
    std::array<HandMotion::Pose,2> handPoses_{};
    unsigned int handSkinTexture_=0;
    unsigned int handTexture_ = 0;
    bool handsAttempted_ = false;
    unsigned int creatureMesh_ = 0, creatureTexture_ = 0;
    unsigned int survivorMesh_ = 0, survivorTexture_ = 0, survivorFrames_ = 0;
    float survivorDuration_ = 0.f;
    bool survivorMeshAttempted_ = false;
    bool creatureMeshAttempted_ = false;
    bool groundTextureAttempted_ = false, monsterTextureAttempted_ = false, foliageTextureAttempted_ = false;
    bool shelterTextureAttempted_ = false;
    std::vector<unsigned int> hedgeChunks_, groundChunks_, foliageChunks_;
    std::vector<bool> dirtyChunks_, visibleChunks_;
    int chunkRows_ = 0, chunkCols_ = 0;
    bool clearChunks_ = true;
    float x_ = .5f, z_ = .5f, angle_ = 0.f, pitch_ = 0.f, elapsed_ = 0.f;
    float elevation_ = 0.f;
    float jumpHeight_ = 0.f, jumpVelocity_ = 0.f, crouchBlend_ = 0.f;
    bool crouched_ = false, jumpHeld_ = false;
    float inscriptionX_ = .10f, inscriptionZ_ = .75f;
    float ambientTime_ = 0.f, darkness_ = 0.f;
    float bobPhase_ = 0.f, bobBlend_ = 0.f, sprintBlend_ = 0.f;
    float fear_ = 0.f, scareClock_ = 0.f, rattleClock_ = 0.f;
    std::uint32_t scareCount_ = 0, whisperEvent_ = 0, doorEvent_ = 0;
    int doorSource_ = -1;
    float whisperPan_ = 0.f, doorPan_ = 0.f;
    bool doorSlam_ = false, rayTraceEnabled_ = true;
    bool headMotionEnabled_ = true;
    std::uint32_t thunderEvent_ = 0;
    bool soundMoving_ = false, soundRunning_ = false, soundMuted_ = false;
    bool networkGuest_ = false;
    int networkSelfSlot_ = 0;
    std::array<RemotePlayer, maxCoopPlayers> remote_{}, remoteVisual_{};
    std::vector<MazePosition> safehouses_;
    std::vector<std::array<RefugeBall, 2>> refugeBalls_;
    unsigned fieldNotes_ = 0;
    int journalPage_ = -1;
    std::vector<RefugeSlots> refugeSlots_;
    std::array<RefugePoker::Hand, maxCoopPlayers> poker_{};
    bool pokerVisible_ = false;
    std::vector<std::array<SafehouseDoor, 4>> doors_;
    std::array<int, maxCoopPlayers> checkpoints_{ { -1, -1, -1, -1 } };
    std::array<std::uint32_t, maxCoopPlayers> remoteCaptures_{};
    std::uint32_t seenRemoteCaptures_ = 0;
    std::array<float, maxCoopPlayers> remoteCaptureCooldown_{};
    int soundStatus_ = 0;
    float hunterSoundGain_ = 1.f, hunterSoundClarity_ = 1.f;
    float monsterClock_ = 0.f, monsterStepDuration_ = 1.15f, shiftClock_ = 0.f, messageClock_ = 0.f;
    float directorClock_ = 0.f;
    float shiftDelay_ = 14.f;
    std::vector<float> refugeNoise_;
    float drinkClock_ = 0.f;
    int seatedHouse_ = -1;
    float seatedBlend_ = 0.f, ballHoldBlend_ = 0.f, throwClock_ = 0.f;
    bool heldBallLast_ = false;
    HandMotion::V lastHeldBall_{},handInertia_{};
    std::array<float, maxCoopPlayers> drinkCooldowns_{};
    std::array<std::uint32_t, maxCoopPlayers> drinks_{};
    std::uint32_t seenDrink_ = 0;
    int interactionSlot_ = 0;
    std::array<unsigned, 16> doorStrikes_{};
    std::uint32_t directorEvent_ = 0;
    bool interiorHunter_ = false;
    int interiorHouse_ = -1, interiorPhase_ = 0;
    float hunterX_ = 0.f, hunterZ_ = 0.f, hunterY_ = 0.f;
    float interiorGoalX_=0.f,interiorGoalZ_=0.f,interiorGoalY_=0.f,interiorMemory_=0.f;
    float monsterHeading_ = 0.f, monsterTargetHeading_ = 0.f, creatureGait_ = 0.f;
    enum class ShiftPhase { Idle, Opening, Open, Closing };
    ShiftPhase shiftPhase_ = ShiftPhase::Idle;
    float shiftProgress_ = 0.f;
    std::uint32_t shiftCount_ = 0, captureCount_ = 0;
    float stamina_ = 1.f;
    bool exhausted_ = false;
    bool movementHeard_ = false, runningHeard_ = false;
    HunterState hunter_{};
    MazePosition monster_{}, monsterFrom_{}, shift_{ -1, -1 };
    MazePuzzle puzzle_{};
    std::array<MazePosition, 3> shrines_{};
    std::array<int, 3> order_{};
    std::array<int, 3> clues_{}, gateCode_{};
    std::array<bool, 3> discoveredClues_{};
    GateRule gateRule_ = GateRule::Forward;
    std::string gateInput_;
    bool gateOpen_ = false, inscriptionRead_ = false;
    int solved_ = 0;
    unsigned activeStones_ = 0;
    std::string notice_;
    std::string networkStatusText_;
};

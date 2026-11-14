#pragma once

#include <cstddef>
#include <cstdint>

struct SoundscapeFrame
{
    std::uint32_t slotEvent = 0, drinkEvent = 0;
    std::uint32_t pokerEvent = 0;
    bool slotSpinning = false, slotJackpot = false;
    float slotPan = 0.f;
    float pokerPan = 0.f, pokerGain = 0.f;
    bool active = false;
    bool moving = false;
    bool running = false;
    float threat = 0.f;
    float hunterGain=1.f,hunterClarity=1.f,ambienceHush=0.f;
    float shifting = 0.f;
    float hunterPan = 0.f;
    float shiftingPan = 0.f;
    bool hunterMoving = false;
    bool hunterChasing = false;
    float hunterStepSeconds = 1.f;
    std::uint32_t thunderEvent = 0;
    // The hedge reacts to the player's fear, the safehouse doors and the
    // occasional whisper from the dark. Events are counters so the audio
    // thread can notice a single trigger without dropping rapid repeats.
    float fear = 0.f;
    std::uint32_t doorEvent = 0;
    float doorPan = 0.f;
    float doorGain = 1.f;
    bool doorSlam = false;
    std::uint32_t whisperEvent = 0;
    float whisperPan = 0.f;
};

// Stateful, allocation-free interleaved stereo PCM synthesis. count is the number
// of frames; output must have room for 2 * count samples. The caller may update parameters
// between blocks; audio playback and device management live in the graphics layer.
class SoundscapeSynth
{
public:
    explicit SoundscapeSynth(unsigned int sampleRate = 22050);
    void render(std::int16_t* output, std::size_t count, const SoundscapeFrame& frame);
private:
    unsigned int sampleRate_;
    std::uint32_t random_ = 0x9e3779b9u, thunderEvent_ = 0, doorEvent_ = 0, whisperEvent_ = 0;
    float time_ = 0.f, footPhase_ = 0.f, heartPhase_ = 0.f, hunterPhase_ = 0.f;
    float wind_ = 0.f, rumble_ = 0.f, thunder_ = 0.f, thunderDelay_ = 0.f, gain_ = 0.f;
    float hunterPan_ = 0.f, shiftingPan_ = 0.f;
    float breathNoise_ = 0.f, vocalPhase_ = 0.f, breathPhase_ = 0.f, roar_ = 0.f;
    float fear_ = 0.f, doorPhase_ = 0.f, doorAmp_ = 0.f, doorPan_ = 0.f;
    float whisperPhase_ = 0.f, whisperAmp_ = 0.f, whisperPan_ = 0.f, whisperNoise_ = 0.f;
    bool doorSlam_ = false, wasChasing_ = false;
    std::uint32_t slotEvent_ = 0, drinkEvent_ = 0;
    bool wasSpinning_ = false, slotWin_ = false;
    float slotPhase_ = 0.f, slotPull_ = 0.f, slotResult_ = 0.f, slotResultTime_ = 0.f;
    float glass_ = 0.f, glassPhase_ = 0.f;
    float occludedHunter_ = 0.f;
    std::uint32_t pokerEvent_ = 0;
    float cardTime_ = 1.f;
};

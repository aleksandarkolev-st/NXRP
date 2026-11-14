#include "Maze.Soundscape.h"
#include <algorithm>
#include <cmath>

namespace { constexpr float pi = 3.14159265358979323846f; }

SoundscapeSynth::SoundscapeSynth(unsigned int sampleRate)
    : sampleRate_(sampleRate ? sampleRate : 22050)
{
}

void SoundscapeSynth::render(std::int16_t* output, std::size_t count, const SoundscapeFrame& frame)
{
    if (!output) return;
    if (frame.thunderEvent != thunderEvent_)
    {
        if (frame.thunderEvent != 0 && frame.active) thunderDelay_ = 1.2f;
        thunderEvent_ = frame.thunderEvent;
    }
    if (frame.doorEvent != doorEvent_)
    {
        if (frame.doorEvent != 0 && frame.active)
        {
            doorPhase_ = 0.f; doorAmp_ = (frame.doorSlam ? 1.f : .62f)*std::clamp(frame.doorGain,0.f,1.f); doorSlam_ = frame.doorSlam;
        }
        doorEvent_ = frame.doorEvent;
    }
    if (frame.whisperEvent != whisperEvent_)
    {
        if (frame.whisperEvent != 0 && frame.active) { whisperPhase_ = 0.f; whisperAmp_ = 1.f; }
        whisperEvent_ = frame.whisperEvent;
    }
    if (frame.active && frame.hunterChasing && !wasChasing_ && frame.threat > .05f) roar_ = 1.f;
    wasChasing_ = frame.active && frame.hunterChasing;
    if(frame.pokerEvent!=pokerEvent_)
    { if(frame.active && frame.pokerEvent) cardTime_=0.f; pokerEvent_=frame.pokerEvent; }
    if (frame.slotEvent != slotEvent_)
    { if (frame.active && frame.slotEvent) slotPull_=1.f; slotEvent_=frame.slotEvent; }
    if (frame.drinkEvent != drinkEvent_)
    { if (frame.active && frame.drinkEvent) { glass_=1.f; glassPhase_=0.f; } drinkEvent_=frame.drinkEvent; }
    if (frame.active && wasSpinning_ && !frame.slotSpinning)
    { slotResult_=1.f; slotResultTime_=0.f; slotWin_=frame.slotJackpot; }
    wasSpinning_=frame.active && frame.slotSpinning;
    const float dt = 1.f / static_cast<float>(sampleRate_);
    const float threat = std::clamp(frame.threat, 0.f, 1.f);
    const float fear = std::clamp(frame.fear, 0.f, 1.f);
    const float shifting = std::clamp(frame.shifting, 0.f, 1.f);
    const float hunterPan = std::clamp(frame.hunterPan, -1.f, 1.f);
    const float shiftingPan = std::clamp(frame.shiftingPan, -1.f, 1.f);
    const float doorPan = std::clamp(frame.doorPan, -1.f, 1.f);
    const float whisperPan = std::clamp(frame.whisperPan, -1.f, 1.f);
    const float hunterPeriod = std::clamp(frame.hunterStepSeconds, .3f, 2.f);
    for (std::size_t i = 0; i < count; ++i)
    {
        random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
        const float noise = static_cast<float>(random_ & 0xffffu) / 32767.5f - 1.f;
        wind_ += .0015f * (noise - wind_);
        rumble_ += .010f * (noise - rumble_);
        breathNoise_ += .09f * (noise - breathNoise_);
        const float gust = .65f + .35f * std::sin(time_ * .19f);
        const float drone = std::sin(time_ * 2.f * pi * 46.f) * .012f;
        float sample = (wind_ * 1.1f * gust + drone)*(1.f-.9f*std::clamp(frame.ambienceHush,0.f,1.f));
        if (frame.moving)
        {
            const float period = frame.running ? .34f : .53f;
            footPhase_ += dt;
            if (footPhase_ >= period) footPhase_ -= period;
            const float envelope = std::exp(-footPhase_ * 32.f);
            sample += envelope * (.10f * std::sin(footPhase_ * 2.f * pi * 62.f) + .03f * noise);
        }
        else footPhase_ = 0.f;
        fear_ += .0015f * (fear - fear_);
        const float pulse = std::max(threat, fear_ * .62f);
        heartPhase_ += dt;
        const float heartPeriod = 1.25f - .35f * pulse;
        if (heartPhase_ >= heartPeriod) heartPhase_ -= heartPeriod;
        const float first = std::exp(-heartPhase_ * 24.f);
        const float second = heartPhase_ >= .20f ? std::exp(-(heartPhase_ - .20f) * 30.f) * .7f : 0.f;
        sample += pulse * .11f * (first + second) * std::sin(heartPhase_ * 2.f * pi * 47.f);
        // A sub-bass dread bed rises when the dark feels close.
        sample += fear_ * (.020f * std::sin(time_ * 2.f * pi * 31.f)
            + .010f * std::sin(time_ * 2.f * pi * 47.f + std::sin(time_ * .7f)));

        doorPhase_ += dt;
        doorAmp_ *= std::exp(-dt / (doorSlam_ ? .45f : 1.15f));
        const float doorEnvelope = std::exp(-doorPhase_ * (doorSlam_ ? 10.f : 2.4f));
        const float doorTone = doorSlam_
            ? std::sin(doorPhase_ * 2.f * pi * 43.f) * .8f + noise * .35f
            : std::sin(doorPhase_ * 2.f * pi * (175.f + 35.f * std::sin(doorPhase_ * 6.3f)))
                * (.45f + .55f * noise);
        const float door = doorEnvelope * doorAmp_ * .15f * doorTone;
        doorPan_ += .004f * (doorPan - doorPan_);

        whisperPhase_ += dt;
        whisperAmp_ *= std::exp(-dt / 2.6f);
        whisperNoise_ += .05f * (noise - whisperNoise_);
        const float whisperEnvelope = std::sin(std::clamp(whisperPhase_ / 2.4f, 0.f, 1.f) * pi);
        const float knockEnvelope=std::exp(-std::abs(whisperPhase_-.18f)*65.f)+.65f*std::exp(-std::abs(whisperPhase_-.61f)*65.f);
        const float distantKnock=whisperAmp_*knockEnvelope*(.065f*std::sin(whisperPhase_*2.f*pi*78.f)+noise*.025f);
        const float whisper = distantKnock + whisperAmp_ * whisperEnvelope * whisperNoise_
            * (.55f + .45f * std::sin(time_ * 2.f * pi * 2.7f)) * .13f;
        whisperPan_ += .004f * (whisperPan - whisperPan_);
        hunterPan_ += .0015f * (hunterPan - hunterPan_);
        shiftingPan_ += .0015f * (shiftingPan - shiftingPan_);
        if (frame.hunterMoving)
        {
            hunterPhase_ += dt;
            const float unevenPeriod=hunterPeriod*(1.f+.11f*std::sin(time_*3.7f)+.05f*std::sin(time_*7.1f));
            if (hunterPhase_ >= unevenPeriod) hunterPhase_ -= unevenPeriod;
        }
        else hunterPhase_ = 0.f;
        const float hunterStep = frame.hunterMoving ? std::exp(-hunterPhase_ * 12.f) : 0.f;
        const float breathRate = frame.hunterChasing ? 1.3f : .58f;
        breathPhase_ += dt * breathRate;
        if (breathPhase_ >= 1.f) breathPhase_ -= 1.f;
        const float breathEnvelope = std::pow(std::max(0.f, std::sin(breathPhase_ * 2.f * pi)), 1.5f);
        const float roughness = .70f + .30f * std::sin(time_ * 29.f);
        const float vocalFrequency = (frame.hunterChasing ? 91.f : 72.f) + 7.f * std::sin(time_ * 2.3f);
        vocalPhase_ += dt * vocalFrequency;
        if (vocalPhase_ >= 1.f) vocalPhase_ -= 1.f;
        const float growl = std::sin(vocalPhase_ * 2.f * pi) * .52f
            + std::sin(vocalPhase_ * 4.f * pi) * .25f + std::sin(vocalPhase_ * 6.f * pi) * .11f;
        const float voice = breathEnvelope * (.14f * breathNoise_ + .075f * growl * roughness)
            + roar_ * (.18f * growl + .13f * breathNoise_);
        roar_ *= std::exp(-dt / .75f);
        const float creature = threat * (hunterStep * (.12f * std::sin(hunterPhase_ * 2.f * pi * 51.f)
            + .12f * rumble_) + .045f * rumble_ + voice);
        const float branches = shifting * .18f * rumble_;
        slotPhase_ += dt; if (slotPhase_ > 10.f) slotPhase_ -= 10.f;
        const float click=std::exp(-std::fmod(slotPhase_,1.f/14.f)*110.f);
        const float motor=frame.slotSpinning ? .025f*std::sin(slotPhase_*2.f*pi*138.f)+click*.055f*noise : 0.f;
        slotPull_*=std::exp(-dt/ .12f);
        slotResultTime_+=dt; slotResult_*=std::exp(-dt/(slotWin_?1.5f:.25f));
        const float resultTone=slotWin_ ?
            std::sin(slotResultTime_*2.f*pi*523.25f)+.65f*std::sin(slotResultTime_*2.f*pi*659.25f)+.5f*std::sin(slotResultTime_*2.f*pi*783.99f)
            : std::sin(slotResultTime_*2.f*pi*87.f);
        const float machine=motor+slotPull_*.12f*(noise*.5f+std::sin(time_*2.f*pi*63.f))+
            slotResult_*.055f*resultTone;
        glassPhase_+=dt; glass_*=std::exp(-dt/.35f);
        const float glass=glass_*.055f*(std::sin(glassPhase_*2.f*pi*1830.f)+.45f*std::sin(glassPhase_*2.f*pi*2917.f));
        // Uneven subharmonics and a rasp keep the pursuer less human-sounding.
        const float rasp=threat*.025f*breathEnvelope*(noise-breathNoise_)*roughness;
        sample += glass;
        // Five short paper scrapes and muted taps, spread over the deal.
        cardTime_+=dt;
        const float cardBeat=std::fmod(cardTime_,.095f);
        const float cards=cardTime_<.475f ? std::exp(-cardBeat*65.f)*(.05f*(noise-breathNoise_)+.025f*std::sin(cardBeat*2.f*pi*160.f))*std::clamp(frame.pokerGain,0.f,1.f):0.f;
        const float cutoff=350.f+std::clamp(frame.hunterClarity,0.f,1.f)*6000.f;
        const float filterAlpha=1.f-std::exp(-2.f*pi*cutoff*dt);
        occludedHunter_+=(creature*1.12f+rasp-occludedHunter_)*filterAlpha;
        const float heardHunter=occludedHunter_*std::clamp(frame.hunterGain,0.f,1.f);
        if (thunderDelay_ > 0.f)
        {
            thunderDelay_ -= dt;
            if (thunderDelay_ <= 0.f) thunder_ = 1.f;
        }
        sample += thunder_ * .48f * wind_;
        thunder_ *= std::exp(-dt / 1.8f);
        gain_ += .002f * ((frame.active ? 1.f : 0.f) - gain_);
        const float left = sample + heardHunter * (1.f - hunterPan_) + branches * (1.f - shiftingPan_)
            + door * (1.f - doorPan_) + whisper * (1.f - whisperPan_) + machine*(1.f-std::clamp(frame.slotPan,-1.f,1.f)) + cards*(1.f-std::clamp(frame.pokerPan,-1.f,1.f));
        const float right = sample + heardHunter * (1.f + hunterPan_) + branches * (1.f + shiftingPan_)
            + door * (1.f + doorPan_) + whisper * (1.f + whisperPan_) + machine*(1.f+std::clamp(frame.slotPan,-1.f,1.f)) + cards*(1.f+std::clamp(frame.pokerPan,-1.f,1.f));
        output[2 * i] = static_cast<std::int16_t>(std::clamp(left * gain_, -1.f, 1.f) * 32767.f);
        output[2 * i + 1] = static_cast<std::int16_t>(std::clamp(right * gain_, -1.f, 1.f) * 32767.f);
        time_ += dt;
        if (time_ >= 1000.f) time_ -= 1000.f;
    }
}

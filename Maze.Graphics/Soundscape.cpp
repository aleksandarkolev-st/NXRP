#include "Soundscape.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>

struct Soundscape::Impl
{
    std::atomic<bool> stop{ false }, muted{ false }, active{ false }, moving{ false }, running{ false }, hunterMoving{ false }, hunterChasing{ false };
    std::atomic<bool> doorSlam{ false }, slotSpinning{ false }, slotJackpot{ false };
    std::atomic<float> slotPan{ 0.f }, hunterGain{1.f}, hunterClarity{1.f}, ambienceHush{0.f};
    std::atomic<std::uint32_t> slotEvent{ 0 }, drinkEvent{ 0 };
    std::atomic<std::uint32_t> pokerEvent{0};
    std::atomic<float> pokerGain{0.f},pokerPan{0.f};
    std::atomic<float> threat{ 0.f }, shifting{ 0.f }, hunterPan{ 0.f }, shiftingPan{ 0.f }, hunterStepSeconds{ 1.f };
    std::atomic<float> fear{ 0.f }, doorPan{ 0.f }, whisperPan{ 0.f };
    std::atomic<float> doorGain{1.f};
    std::atomic<int> status{ 0 };
    std::atomic<std::uint32_t> thunderEvent{ 0 }, doorEvent{ 0 }, whisperEvent{ 0 };
    std::thread worker;

    SoundscapeFrame frame() const noexcept
    {
        SoundscapeFrame result;
        result.active = active.load() && !muted.load();
        result.moving = moving.load(); result.running = running.load();
        result.threat = threat.load(); result.shifting = shifting.load();
        result.hunterPan = hunterPan.load(); result.shiftingPan = shiftingPan.load();
        result.hunterMoving = hunterMoving.load(); result.hunterStepSeconds = hunterStepSeconds.load();
        result.hunterChasing = hunterChasing.load();
        result.thunderEvent = thunderEvent.load();
        result.slotSpinning=slotSpinning.load(); result.slotJackpot=slotJackpot.load(); result.slotPan=slotPan.load();
        result.slotEvent=slotEvent.load(); result.drinkEvent=drinkEvent.load();
        result.pokerEvent=pokerEvent.load();result.pokerGain=pokerGain.load();result.pokerPan=pokerPan.load();
        result.hunterGain=hunterGain.load(); result.hunterClarity=hunterClarity.load(); result.ambienceHush=ambienceHush.load();
        result.fear = fear.load();
        result.doorEvent = doorEvent.load(); result.doorPan = doorPan.load(); result.doorSlam = doorSlam.load();
        result.doorGain=doorGain.load();
        result.whisperEvent = whisperEvent.load(); result.whisperPan = whisperPan.load();
        return result;
    }

    void run()
    {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = 2;
        format.nSamplesPerSec = 22050; format.wBitsPerSample = 16;
        format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        HWAVEOUT device = nullptr;
        if (waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            // Some older devices accept only mono. Downmix rather than losing audio.
            format.nChannels = 1; format.nBlockAlign = 2;
            format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
            if (waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
            { status.store(-1); return; }
        }
        // Four short blocks keep movement cues responsive while tolerating scheduling jitter.
        constexpr std::size_t blockSize = 512;
        std::array<std::array<std::int16_t, blockSize * 2>, 4> samples{};
        std::array<WAVEHDR, 4> headers{};
        SoundscapeSynth synth(format.nSamplesPerSec);
        const auto fill = [&](std::size_t index)
        {
            synth.render(samples[index].data(), blockSize, frame());
            if (format.nChannels == 1)
                for (std::size_t i = 0; i < blockSize; ++i)
                    samples[index][i] = static_cast<std::int16_t>((static_cast<int>(samples[index][2*i])
                        + static_cast<int>(samples[index][2*i+1])) / 2);
        };
        int queued = 0;
        for (std::size_t i = 0; i < headers.size(); ++i)
        {
            headers[i].lpData = reinterpret_cast<LPSTR>(samples[i].data());
            headers[i].dwBufferLength = static_cast<DWORD>(blockSize * format.nBlockAlign);
            if (waveOutPrepareHeader(device, &headers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) break;
            fill(i);
            if (waveOutWrite(device, &headers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) break;
            ++queued;
        }
        status.store(queued > 0 ? 1 : -1);
        bool playbackFailed = false;
        while (queued > 0 && !stop.load() && !playbackFailed)
        {
            for (std::size_t i = 0; i < headers.size(); ++i)
                if ((headers[i].dwFlags & (WHDR_PREPARED | WHDR_DONE)) == (WHDR_PREPARED | WHDR_DONE))
                {
                    fill(i);
                    if (waveOutWrite(device, &headers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                    { status.store(-1); playbackFailed = true; break; }
                }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        waveOutReset(device);
        for (auto& header : headers)
            if (header.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(device, &header, sizeof(WAVEHDR));
        waveOutClose(device);
    }
};

Soundscape::Soundscape() : impl_(std::make_unique<Impl>())
{
    try { impl_->worker = std::thread([state = impl_.get()] { state->run(); }); }
    catch (const std::system_error&) { impl_->status.store(-1); }
}
Soundscape::~Soundscape()
{
    impl_->stop.store(true);
    if (impl_->worker.joinable()) impl_->worker.join();
}
void Soundscape::setFrame(const SoundscapeFrame& frame) noexcept
{
    impl_->moving.store(frame.moving); impl_->running.store(frame.running);
    impl_->threat.store(frame.threat); impl_->shifting.store(frame.shifting);
    impl_->hunterPan.store(frame.hunterPan); impl_->shiftingPan.store(frame.shiftingPan);
    impl_->hunterMoving.store(frame.hunterMoving); impl_->hunterChasing.store(frame.hunterChasing);
    impl_->hunterStepSeconds.store(frame.hunterStepSeconds);
    impl_->thunderEvent.store(frame.thunderEvent); impl_->active.store(frame.active);
    impl_->hunterGain.store(frame.hunterGain); impl_->hunterClarity.store(frame.hunterClarity);impl_->ambienceHush.store(frame.ambienceHush);
    impl_->fear.store(frame.fear);
    impl_->slotSpinning.store(frame.slotSpinning); impl_->slotJackpot.store(frame.slotJackpot); impl_->slotPan.store(frame.slotPan);
    impl_->pokerEvent.store(frame.pokerEvent);impl_->pokerGain.store(frame.pokerGain);impl_->pokerPan.store(frame.pokerPan);
    impl_->doorGain.store(frame.doorGain);
    impl_->slotEvent.store(frame.slotEvent); impl_->drinkEvent.store(frame.drinkEvent); impl_->doorEvent.store(frame.doorEvent);
    impl_->doorPan.store(frame.doorPan); impl_->doorSlam.store(frame.doorSlam);
    impl_->whisperEvent.store(frame.whisperEvent); impl_->whisperPan.store(frame.whisperPan);
}
bool Soundscape::muted() const noexcept { return impl_->muted.load(); }
int Soundscape::status() const noexcept { return impl_->status.load(); }
void Soundscape::setMuted(bool muted) noexcept { impl_->muted.store(muted); }

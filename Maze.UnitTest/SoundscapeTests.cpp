#include "pch.h"
#include "CppUnitTest.h"
#include "Maze.Soundscape.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MazeUnitTest
{
    TEST_CLASS(SoundscapeTests)
    {
    public:
        TEST_METHOD(DistantDoorEventsCanBeSilentWithoutMutingAmbience)
        {
            SoundscapeSynth baseline,distant,nearby;
            SoundscapeFrame frame;frame.active=true;
            std::array<std::int16_t,8192> a{},b{},c{};
            baseline.render(a.data(),a.size()/2,frame);
            frame.doorEvent=1;frame.doorSlam=true;frame.doorGain=0.f;
            distant.render(b.data(),b.size()/2,frame);
            frame.doorGain=1.f;nearby.render(c.data(),c.size()/2,frame);
            Assert::IsTrue(a==b);Assert::IsFalse(a==c);
        }
        TEST_METHOD(OcclusionRemovesMonsterHighFrequencyDetail)
        {
            SoundscapeSynth baseline,clear,muffled;
            SoundscapeFrame frame;frame.active=true;frame.threat=1.f;frame.hunterChasing=true;frame.hunterMoving=true;
            std::array<std::int16_t,8192> silence{},bright{},dark{};
            frame.hunterGain=0.f;baseline.render(silence.data(),silence.size()/2,frame);
            frame.hunterGain=1.f;frame.hunterClarity=1.f;clear.render(bright.data(),bright.size()/2,frame);
            frame.hunterClarity=0.f;muffled.render(dark.data(),dark.size()/2,frame);
            double clearRoughness=0.,muffledRoughness=0.;
            for(std::size_t i=4;i<bright.size();i+=2)
            {
                const double a=(bright[i]-silence[i])-(bright[i-2]-silence[i-2]);
                const double b=(dark[i]-silence[i])-(dark[i-2]-silence[i-2]);
                clearRoughness+=a*a;muffledRoughness+=b*b;
            }
            Assert::IsTrue(clearRoughness>1000. && muffledRoughness<clearRoughness*.8);
        }
        TEST_METHOD(SlotAndDrinkSoundsAreDistinctAndPauseFadesThem)
        {
            SoundscapeFrame frame; frame.active=true;
            SoundscapeSynth base, slot, drink;
            std::array<std::int16_t,8192> quiet{}, reels{}, glass{};
            base.render(quiet.data(),quiet.size()/2,frame);
            frame.slotEvent=1; frame.slotSpinning=true; frame.slotPan=1.f;
            slot.render(reels.data(),reels.size()/2,frame);
            frame.slotEvent=0; frame.slotSpinning=false; frame.drinkEvent=1;
            drink.render(glass.data(),glass.size()/2,frame);
            Assert::IsTrue(quiet!=reels && quiet!=glass && reels!=glass);
            frame.active=false;
            std::vector<std::int16_t> fade(88200);
            slot.render(fade.data(),fade.size()/2,frame);
            Assert::IsTrue(std::all_of(fade.end()-512,fade.end(),[](auto s){return s==0;}));
        }
        TEST_METHOD(ChasingCreatureVocalizesEvenWhenNotStepping)
        {
            SoundscapeSynth calm, chasing;
            SoundscapeFrame frame; frame.active = true; frame.threat = .8f; frame.hunterPan = 1.f;
            SoundscapeFrame chase = frame; chase.hunterChasing = true;
            std::array<std::int16_t, 8192> quiet{}, alarm{};
            calm.render(quiet.data(), quiet.size()/2, frame);
            chasing.render(alarm.data(), alarm.size()/2, chase);
            long long difference = 0, stereoDifference = 0;
            for (std::size_t i = 0; i < quiet.size(); i += 2)
            {
                difference += std::abs(static_cast<int>(alarm[i+1]) - static_cast<int>(quiet[i+1]));
                stereoDifference += std::abs(static_cast<int>(alarm[i+1]) - static_cast<int>(alarm[i]));
            }
            Assert::IsTrue(difference > 50000);
            Assert::IsTrue(stereoDifference > 50000);
        }
        TEST_METHOD(SilenceAndPauseFadeDoNotLeaveResidualOutput)
        {
            SoundscapeSynth synth;
            SoundscapeFrame frame;
            std::vector<std::int16_t> samples(88200);
            synth.render(samples.data(), samples.size() / 2, frame);
            Assert::IsTrue(std::all_of(samples.begin(), samples.end(), [](auto s) { return s == 0; }));
            frame.active = true; frame.moving = true;
            synth.render(samples.data(), samples.size() / 2, frame);
            Assert::IsTrue(std::any_of(samples.begin(), samples.end(), [](auto s) { return s != 0; }));
            frame.active = false;
            synth.render(samples.data(), samples.size() / 2, frame);
            Assert::IsTrue(std::all_of(samples.end() - 512, samples.end(), [](auto s) { return s == 0; }));
        }

        TEST_METHOD(ContinuousBlocksMatchOneRender)
        {
            SoundscapeFrame frame; frame.active = true; frame.moving = true;
            frame.running = true; frame.threat = .7f; frame.shifting = .4f; frame.thunderEvent = 1;
            frame.slotEvent=2; frame.slotSpinning=true; frame.drinkEvent=1;
            SoundscapeSynth one, split;
            std::array<std::int16_t, 4096> full{}, parts{};
            one.render(full.data(), full.size() / 2, frame);
            split.render(parts.data(), 512, frame);
            split.render(parts.data() + 1024, parts.size() / 2 - 512, frame);
            Assert::IsTrue(full == parts);
        }

        TEST_METHOD(ThreatAndWeatherChangeTheOutput)
        {
            SoundscapeSynth quiet, danger;
            SoundscapeFrame base; base.active = true;
            SoundscapeFrame alarm = base; alarm.threat = 1.f; alarm.shifting = 1.f; alarm.thunderEvent = 1;
            std::array<std::int16_t, 4096> first{}, second{};
            quiet.render(first.data(), first.size() / 2, base);
            danger.render(second.data(), second.size() / 2, alarm);
            Assert::IsFalse(first == second);
        }

        TEST_METHOD(DoorAndWhisperEventsAddDirectionalAudio)
        {
            SoundscapeSynth calm, door, whisper;
            SoundscapeFrame base; base.active = true;
            SoundscapeFrame doorFrame = base;
            doorFrame.doorEvent = 1; doorFrame.doorSlam = true; doorFrame.doorPan = -1.f;
            SoundscapeFrame whisperFrame = base;
            whisperFrame.whisperEvent = 1; whisperFrame.whisperPan = 1.f;
            std::array<std::int16_t, 4096> a{}, b{}, c{};
            calm.render(a.data(), a.size() / 2, base);
            door.render(b.data(), b.size() / 2, doorFrame);
            whisper.render(c.data(), c.size() / 2, whisperFrame);
            long long doorDifference = 0, whisperDifference = 0;
            for (std::size_t i = 0; i < a.size(); i += 2)
            {
                doorDifference += std::abs(static_cast<int>(b[i]) - static_cast<int>(a[i]));
                doorDifference += std::abs(static_cast<int>(b[i+1]) - static_cast<int>(a[i+1]));
                whisperDifference += std::abs(static_cast<int>(c[i]) - static_cast<int>(a[i]));
                whisperDifference += std::abs(static_cast<int>(c[i+1]) - static_cast<int>(a[i+1]));
            }
            Assert::IsTrue(doorDifference > 2000);
            Assert::IsTrue(whisperDifference > 500);
        }

        TEST_METHOD(DirectionalSoundsPanWithoutChangingTheCenteredAmbience)
        {
            SoundscapeSynth leftSynth, rightSynth, centerSynth;
            SoundscapeFrame left; left.active = true; left.threat = .8f;
            left.hunterMoving = true; left.hunterPan = -1.f;
            left.shifting = .6f; left.shiftingPan = -1.f;
            SoundscapeFrame right = left; right.hunterPan = right.shiftingPan = 1.f;
            SoundscapeFrame center = left; center.hunterPan = center.shiftingPan = 0.f;
            std::array<std::int16_t, 8192> l{}, r{}, c{};
            leftSynth.render(l.data(), l.size() / 2, left);
            rightSynth.render(r.data(), r.size() / 2, right);
            centerSynth.render(c.data(), c.size() / 2, center);
            bool separated = false;
            for (std::size_t i = 0; i < l.size(); i += 2)
            {
                Assert::AreEqual(l[i], r[i + 1]);
                Assert::AreEqual(l[i + 1], r[i]);
                Assert::AreEqual(c[i], c[i + 1]);
                separated = separated || l[i] != l[i + 1];
            }
            Assert::IsTrue(separated);
        }
    };
}

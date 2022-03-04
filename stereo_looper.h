#pragma once

#include "head.h"
#include "looper.h"
#include "envelope_follower.h"
#include "Utility/dsp.h"
#include "Filters/svf.h"
#include "dev/sdram.h"
#include <cmath>
#include <stddef.h>

namespace wreath
{
    using namespace daisysp;

    constexpr int32_t kSampleRate{48000};
    // constexpr int kBufferSeconds{150}; // 2:30 minutes max, with 2 buffers
    constexpr int kBufferSeconds{80}; // 1:20 minutes, max with 4 buffers
    // constexpr int kBufferSeconds{1};
    const int32_t kBufferSamples{kSampleRate * kBufferSeconds};
    // constexpr float kParamSlewCoeff{0.0002f}; // 1.0 / (time_sec * sample_rate) > 100ms @ 48K
    constexpr float kParamSlewCoeff{1.f}; // 1.0 / (time_sec * sample_rate) > 100ms @ 48K

    float DSY_SDRAM_BSS leftBuffer_[kBufferSamples];
    float DSY_SDRAM_BSS rightBuffer_[kBufferSamples];

    float DSY_SDRAM_BSS leftFreezeBuffer_[kBufferSamples];
    float DSY_SDRAM_BSS rightFreezeBuffer_[kBufferSamples];

    class StereoLooper
    {
    public:
        StereoLooper() {}
        ~StereoLooper() {}

        enum
        {
            LEFT,
            RIGHT,
            BOTH,
            NONE,
        };

        enum State
        {
            STARTUP,
            BUFFERING,
            READY,
            RECORDING,
            FROZEN,
        };

        enum Mode
        {
            MONO,
            CROSS,
            DUAL,
            LAST_MODE,
        };

        enum FilterType
        {
            LP,
            BP,
            HP,
        };

        enum NoteMode
        {
            NO_MODE,
            NOTE,
            FLANGER,
        };

        struct Conf
        {
            Mode mode;
            Looper::TriggerMode triggerMode;
            Movement movement;
            Direction direction;
            float rate;
        };

        bool mustResetLooper{};
        bool mustClearBuffer{};
        bool mustStopBuffering{};

        float inputGain{1.f};
        float outputGain{1.f};
        float dryWetMix{0.5f};
        float feedback{0.f};
        float feedbackLevel{1.f};
        float filterLevel{0.3f};
        float rateSlew{0.f};
        float stereoWidth{1.f};
        float dryLevel{1.f};
        FilterType filterType{FilterType::BP};

        NoteMode noteModeLeft{};
        NoteMode noteModeRight{};

        int32_t nextLeftLoopStart{};
        int32_t nextRightLoopStart{};

        Direction leftDirection{};
        Direction rightDirection{};

        int32_t nextLeftLoopLength{};
        int32_t nextRightLoopLength{};

        float nextLeftReadRate{};
        float nextRightReadRate{};

        float nextLeftWriteRate{};
        float nextRightWriteRate{};

        float nextLeftFreeze{};
        float nextRightFreeze{};

        Looper::TriggerMode leftTriggerMode{};
        Looper::TriggerMode rightTriggerMode{};

        bool mustStart{};
        bool mustStop{};
        bool mustRetrigger{};

        inline int32_t GetBufferSamples(int channel) { return loopers_[channel].GetBufferSamples(); }
        inline float GetBufferSeconds(int channel) { return loopers_[channel].GetBufferSeconds(); }
        inline float GetLoopStartSeconds(int channel) { return loopers_[channel].GetLoopStartSeconds(); }
        inline float GetLoopLengthSeconds(int channel) { return loopers_[channel].GetLoopLengthSeconds(); }
        inline float GetReadPosSeconds(int channel) { return loopers_[channel].GetReadPosSeconds(); }
        inline float GetLoopStart(int channel) { return loopers_[channel].GetLoopStart(); }
        inline float GetLoopEnd(int channel) { return loopers_[channel].GetLoopEnd(); }
        inline float GetLoopLength(int channel) { return loopers_[channel].GetLoopLength(); }
        inline float GetReadPos(int channel) { return loopers_[channel].GetReadPos(); }
        inline float GetWritePos(int channel) { return loopers_[channel].GetWritePos(); }
        inline float GetReadRate(int channel) { return loopers_[channel].GetReadRate(); }
        inline Movement GetMovement(int channel) { return loopers_[channel].GetMovement(); }
        inline bool IsGoingForward(int channel) { return loopers_[channel].IsGoingForward(); }
        inline int32_t GetCrossPoint(int channel) { return loopers_[channel].GetCrossPoint(); }
        inline int32_t GetHeadsDistance(int channel) { return loopers_[channel].GetHeadsDistance(); }

        inline bool IsStartingUp() { return State::STARTUP == state_; }
        inline bool IsBuffering() { return State::BUFFERING == state_; }
        inline bool IsRecording() { return State::RECORDING == state_; }
        inline bool IsFrozen() { return State::FROZEN == state_; }
        inline bool IsRunning() { return State::RECORDING == state_ || State::FROZEN == state_; }
        inline bool IsReady() { return State::READY == state_; }
        inline bool IsMonoMode() { return Mode::MONO == conf_.mode; }
        inline bool IsCrossMode() { return Mode::CROSS == conf_.mode; }
        inline bool IsDualMode() { return Mode::DUAL == conf_.mode; }
        inline Mode GetMode() { return conf_.mode; }
        inline Looper::TriggerMode GetTriggerMode() { return leftTriggerMode; }
        inline bool IsGateMode() { return Looper::TriggerMode::GATE == leftTriggerMode; }

        void Init(int32_t sampleRate, Conf conf)
        {
            sampleRate_ = sampleRate;
            loopers_[LEFT].Init(sampleRate_, leftBuffer_, leftFreezeBuffer_, kBufferSamples);
            loopers_[RIGHT].Init(sampleRate_, rightBuffer_, rightFreezeBuffer_, kBufferSamples);
            state_ = State::STARTUP;
            feedbackFilter_.Init(sampleRate_);

            // Process configuration and reset the looper.
            conf_ = conf;
            loopers_[LEFT].Reset();
            loopers_[RIGHT].Reset();
        }

        bool loopSync_{};
        bool GetLoopSync()
        {
            return loopSync_;
        }
        void SetLoopSync(bool loopSync)
        {
            loopSync_ = loopSync;
            loopers_[LEFT].SetLoopSync(loopSync);
            loopers_[RIGHT].SetLoopSync(loopSync);
        }

        bool HasLoopSync()
        {
            return loopSync_;
        }

        void ToggleFreeze()
        {
            bool frozen = IsFrozen();
            SetFreeze(BOTH, !frozen);
        }

        void SetSamplesToFade(float samples)
        {
            loopers_[LEFT].SetSamplesToFade(samples);
            loopers_[RIGHT].SetSamplesToFade(samples);
        }

        float GetFilterValue()
        {
            return filterValue_;
        }

        void SetFilterValue(float value)
        {
            filterValue_ = value;
            feedbackFilter_.SetFreq(filterValue_);
            feedbackFilter_.SetDrive(0.75f);
            feedbackFilter_.SetRes(fmap(1.f - feedback, 0.05f, 0.2f + (freeze_ * 0.2f)));
        }

        void OffsetLoopers(float value)
        {
            float pos = fclamp(loopers_[LEFT].GetReadPos() + value, 0, loopers_[RIGHT].GetLoopEnd());
            loopers_[RIGHT].SetReadPos(pos);
        }

        void SetTriggerMode(Looper::TriggerMode mode)
        {
            leftTriggerMode = mode;
            rightTriggerMode = mode;
            switch (mode)
            {
            case Looper::TriggerMode::GATE:
                dryLevel = 0.f;
                mustStart = true;
                break;
            case Looper::TriggerMode::TRIGGER:
                dryLevel = 1.f;
                mustStop = true;
                break;
            case Looper::TriggerMode::LOOP:
                dryLevel = 1.f;
                mustStart = true;
                break;
            }
            conf_.triggerMode = mode;
        }

        void SetMovement(int channel, Movement movement)
        {
            if (BOTH == channel)
            {
                loopers_[LEFT].SetMovement(movement);
                loopers_[RIGHT].SetMovement(movement);
                conf_.movement = movement;
            }
            else
            {
                loopers_[channel].SetMovement(movement);
            }
        }

        void SetDirection(int channel, Direction direction)
        {
            if (LEFT == channel || BOTH == channel)
            {
                leftDirection = direction;
            }
            if (RIGHT == channel || BOTH == channel)
            {
                rightDirection = direction;
            }
            if (BOTH == channel)
            {
                conf_.direction = direction;
            }
            // Before the looper starts, if the direction is backwards set the
            // read head at the end of the loop.
            if (State::READY == state_ && Direction::BACKWARDS == direction)
            {
                loopers_[LEFT].SetReadPos(loopers_[LEFT].GetLoopEnd());
                loopers_[RIGHT].SetReadPos(loopers_[RIGHT].GetLoopEnd());
            }
        }

        void SetLoopStart(int channel, float value)
        {
            if (LEFT == channel || BOTH == channel)
            {
                nextLeftLoopStart = std::min(std::max(value, 0.f), loopers_[LEFT].GetBufferSamples() - 1.f);
            }
            if (RIGHT == channel || BOTH == channel)
            {
                nextRightLoopStart = std::min(std::max(value, 0.f), loopers_[RIGHT].GetBufferSamples() - 1.f);
            }
        }

        void SetFreeze(int channel, float amount)
        {
            if (LEFT == channel || BOTH == channel)
            {
                nextLeftFreeze = amount;
            }
            if (RIGHT == channel || BOTH == channel)
            {
                nextRightFreeze = amount;
            }
            freeze_ = amount;
            if (State::READY != state_)
            {
                state_ = amount == 1.f ? State::FROZEN : State::RECORDING;
            }
        }

        void SetReadRate(int channel, float rate)
        {
            if (LEFT == channel || BOTH == channel)
            {
                nextLeftReadRate = rate;
            }
            if (RIGHT == channel || BOTH == channel)
            {
                nextRightReadRate = rate;
            }
            conf_.rate = rate;
        }

        void SetWriteRate(int channel, float rate)
        {
            if (LEFT == channel || BOTH == channel)
            {
                nextLeftWriteRate = rate;
            }
            if (RIGHT == channel || BOTH == channel)
            {
                nextRightWriteRate = rate;
            }
        }

        void SetLoopLength(int channel, float length)
        {
            if (LEFT == channel || BOTH == channel)
            {
                nextLeftLoopLength = std::min(std::max(length, kMinLoopLengthSamples), static_cast<float>(loopers_[LEFT].GetBufferSamples()));
                noteModeLeft = NoteMode::NO_MODE;
                if (length <= kMinLoopLengthSamples)
                {
                    noteModeLeft = NoteMode::NOTE;
                }
                else if (length >= kMinSamplesForTone && length <= kMinSamplesForFlanger)
                {
                    noteModeLeft = NoteMode::FLANGER;
                }
            }
            if (RIGHT == channel || BOTH == channel)
            {
                nextRightLoopLength = std::min(std::max(length, kMinLoopLengthSamples), static_cast<float>(loopers_[RIGHT].GetBufferSamples()));
                noteModeRight = NoteMode::NO_MODE;
                if (length <= kMinLoopLengthSamples)
                {
                    noteModeRight = NoteMode::NOTE;
                }
                else if (length >= kMinSamplesForTone && length <= kMinSamplesForFlanger)
                {
                    noteModeRight = NoteMode::FLANGER;
                }
            }
        }

        void Start()
        {
            if (State::READY == state_)
            {
                loopers_[LEFT].Start(true);
                loopers_[RIGHT].Start(true);
                state_ = freeze_ == 1.f ? State::FROZEN : State::RECORDING;
            }
        }

        void Process(const float leftIn, const float rightIn, float &leftOut, float &rightOut)
        {
            // Input gain stage.
            float leftDry = SoftClip(leftIn * inputGain);
            float rightDry = SoftClip(rightIn * inputGain);

            float leftWet{};
            float rightWet{};

            switch (state_)
            {
            case State::STARTUP:
            {
                static int32_t fadeIndex{0};
                if (fadeIndex > sampleRate_)
                {
                    fadeIndex = 0;
                    state_ = State::BUFFERING;
                }
                fadeIndex++;

                // Return now, so we don't emit any sound.
                return;
            }
            case State::BUFFERING:
            {
                bool doneLeft{loopers_[LEFT].Buffer(leftDry)};
                bool doneRight{loopers_[RIGHT].Buffer(rightDry)};
                if ((doneLeft && doneRight) || mustStopBuffering)
                {
                    mustStopBuffering = false;
                    loopers_[LEFT].StopBuffering();
                    loopers_[RIGHT].StopBuffering();

                    state_ = State::READY;
                }

                // Pass the audio through.
                leftWet = leftDry;
                rightWet = rightDry;

                break;
            }
            case State::READY:
            {
                nextLeftLoopLength = loopers_[LEFT].GetLoopLength();
                nextRightLoopLength = loopers_[RIGHT].GetLoopLength();
                nextLeftLoopStart = loopers_[LEFT].GetLoopStart();
                nextRightLoopStart = loopers_[RIGHT].GetLoopStart();
                nextLeftReadRate = 1.f;
                nextRightReadRate = 1.f;
                nextLeftWriteRate = 1.f;
                nextRightWriteRate = 1.f;
                nextLeftFreeze = 0.f;
                nextRightFreeze = 0.f;

                break;
            }
            case State::RECORDING:
            case State::FROZEN:
            {
                UpdateParameters();

                leftDry *= dryLevel;
                rightDry *= dryLevel;

                if (mustClearBuffer)
                {
                    mustClearBuffer = false;
                    loopers_[LEFT].ClearBuffer();
                    loopers_[RIGHT].ClearBuffer();
                }

                if (mustResetLooper)
                {
                    mustResetLooper = false;
                    loopers_[LEFT].Stop(true);
                    loopers_[RIGHT].Stop(true);
                    Reset();
                    state_ = State::BUFFERING;

                    break;
                }

                if (mustRetrigger)
                {
                    loopers_[LEFT].Trigger();
                    loopers_[RIGHT].Trigger();
                    mustRetrigger = false;
                }

                if (mustStart)
                {
                    bool doneLeft{loopers_[LEFT].Start(false)};
                    bool doneRight{loopers_[RIGHT].Start(false)};
                    if (doneLeft && doneRight)
                    {
                        mustStart = false;
                    }
                }

                if (mustStop)
                {
                    bool doneLeft{loopers_[LEFT].Stop(false)};
                    bool doneRight{loopers_[RIGHT].Stop(false)};
                    if (doneLeft && doneRight)
                    {
                        mustStop = false;
                    }
                }

                loopers_[LEFT].HandleFade();
                loopers_[RIGHT].HandleFade();

                leftWet = loopers_[LEFT].Read(leftDry);
                rightWet = loopers_[RIGHT].Read(rightDry);

                // Feedback path.
                float leftFeedback = leftWet * feedback;
                float rightFeedback = rightWet * feedback;
                leftFeedback = Mix(leftFeedback, filterLevel * Filter(leftFeedback));
                rightFeedback = Mix(rightFeedback, filterLevel * Filter(rightFeedback));
                leftFeedback *= (feedbackLevel - filterEnvelope_.GetEnv(leftFeedback));
                rightFeedback *= (feedbackLevel - filterEnvelope_.GetEnv(rightFeedback));

                leftWet = Mix(leftWet, Filter(leftFeedback) * freeze_ * 0.5f);
                rightWet = Mix(rightWet, Filter(rightFeedback) * freeze_ * 0.5f);

                loopers_[LEFT].Write(Mix(leftDry, leftFeedback));
                loopers_[RIGHT].Write(Mix(rightDry, rightFeedback));

                loopers_[LEFT].UpdateWritePos();
                loopers_[RIGHT].UpdateWritePos();

                loopers_[LEFT].UpdateReadPos();
                loopers_[RIGHT].UpdateReadPos();

                /*
                // When drunk there's a small probability of changing direction.
                bool toggleDir{rand() % loopers_[LEFT].GetSampleRateSpeed() == 1};
                if (loopers_[LEFT].IsDrunkMovement())
                {
                    if ((IsDualMode() && (rand() % loopers_[LEFT].GetSampleRateSpeed()) == 1) || toggleDir)
                    {
                        loopers_[LEFT].ToggleDirection();
                    }
                }
                if (loopers_[RIGHT].IsDrunkMovement())
                {
                    if ((IsDualMode() && (rand() % loopers_[RIGHT].GetSampleRateSpeed()) == 1) || toggleDir)
                    {
                        loopers_[RIGHT].ToggleDirection();
                    }
                }
                */
            }
            default:
                break;
            }

            // Mid-side processing for stereo widening.
            float mid = ((leftWet + rightWet) / fastroot(2, 10));
            float side = ((leftWet - rightWet) / fastroot(2, 10)) * stereoWidth;
            float stereoLeft = (mid + side) / fastroot(2, 10);
            float stereoRight = (mid - side) / fastroot(2, 10);

            // Output gain stage.
            leftOut = SoftClip(Fader::EqualCrossFade(leftDry, stereoLeft, dryWetMix) * outputGain);
            rightOut = SoftClip(Fader::EqualCrossFade(rightDry, stereoRight, dryWetMix) * outputGain);
        }

    private:
        Looper loopers_[2];
        State state_{}; // The current state of the looper
        EnvFollow filterEnvelope_{};
        Svf feedbackFilter_;
        int32_t sampleRate_{};
        float freeze_{};
        float filterValue_{};
        Conf conf_{};

        void Reset()
        {
            loopers_[LEFT].Reset();
            loopers_[RIGHT].Reset();

            // SetMode(conf_.mode);
            SetTriggerMode(conf_.triggerMode);
            SetMovement(BOTH, conf_.movement);
            SetDirection(BOTH, conf_.direction);
            SetReadRate(BOTH, conf_.rate);
            SetWriteRate(BOTH, conf_.rate);
        }

        float Mix(float a, float b)
        {
            return SoftClip(a + b);
        }

        float Filter(float value)
        {
            feedbackFilter_.Process(value);
            switch (filterType)
            {
            case FilterType::BP:
                return feedbackFilter_.Band();
            case FilterType::HP:
                return feedbackFilter_.High();
            case FilterType::LP:
                return feedbackFilter_.Low();
            default:
                return feedbackFilter_.Band();
            }
        }

        void UpdateParameters()
        {
            if (leftTriggerMode != loopers_[LEFT].GetTriggerMode())
            {
                loopers_[LEFT].SetTriggerMode(leftTriggerMode);
            }
            if (rightTriggerMode != loopers_[RIGHT].GetTriggerMode())
            {
                loopers_[RIGHT].SetTriggerMode(rightTriggerMode);
            }

            if (leftDirection != loopers_[LEFT].GetDirection())
            {
                loopers_[LEFT].SetDirection(leftDirection);
            }
            if (rightDirection != loopers_[RIGHT].GetDirection())
            {
                loopers_[RIGHT].SetDirection(rightDirection);
            }

            float leftLoopLength = loopers_[LEFT].GetLoopLength();
            if (leftLoopLength != nextLeftLoopLength)
            {
                loopers_[LEFT].SetLoopLength(nextLeftLoopLength);
            }
            float rightLoopLength = loopers_[RIGHT].GetLoopLength();
            if (rightLoopLength != nextRightLoopLength)
            {
                loopers_[RIGHT].SetLoopLength(nextLeftLoopLength);
            }

            float leftLoopStart = loopers_[LEFT].GetLoopStart();
            if (leftLoopStart != nextLeftLoopStart)
            {
                loopers_[LEFT].SetLoopStart(nextLeftLoopStart);
            }
            float rightLoopStart = loopers_[RIGHT].GetLoopStart();
            if (rightLoopStart != nextRightLoopStart)
            {
                loopers_[RIGHT].SetLoopStart(nextRightLoopStart);
            }

            float leftReadRate = loopers_[LEFT].GetReadRate();
            if (leftReadRate != nextLeftReadRate)
            {
                float coeff = rateSlew > 0 ? 1.f / (rateSlew * sampleRate_) : 1.f;
                fonepole(leftReadRate, nextLeftReadRate, coeff);
                loopers_[LEFT].SetReadRate(leftReadRate);
            }
            float rightReadRate = loopers_[RIGHT].GetReadRate();
            if (rightReadRate != nextRightReadRate)
            {
                float coeff = rateSlew > 0 ? 1.f / (rateSlew * sampleRate_) : 1.f;
                fonepole(rightReadRate, nextRightReadRate, coeff);
                loopers_[RIGHT].SetReadRate(rightReadRate);
            }

            float leftWriteRate = loopers_[LEFT].GetWriteRate();
            if (leftWriteRate != nextLeftWriteRate)
            {
                float coeff = rateSlew > 0 ? 1.f / (rateSlew * sampleRate_) : 1.f;
                fonepole(leftWriteRate, nextLeftWriteRate, coeff);
                loopers_[LEFT].SetWriteRate(leftWriteRate);
            }
            float rightWriteRate = loopers_[RIGHT].GetWriteRate();
            if (rightWriteRate != nextRightWriteRate)
            {
                float coeff = rateSlew > 0 ? 1.f / (rateSlew * sampleRate_) : 1.f;
                fonepole(rightWriteRate, nextRightWriteRate, coeff);
                loopers_[RIGHT].SetWriteRate(rightWriteRate);
            }

            float leftFreeze = loopers_[LEFT].GetFreeze();
            if (leftFreeze != nextLeftFreeze)
            {
                loopers_[LEFT].SetFreeze(nextLeftFreeze);
            }

            float rightFreeze = loopers_[RIGHT].GetFreeze();
            if (rightFreeze != nextRightFreeze)
            {
                loopers_[RIGHT].SetFreeze(nextRightFreeze);
            }
        }
    };

}
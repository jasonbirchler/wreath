#pragma once

#include "head.h"
#include <cstdint>

namespace wreath
{
    class Looper
    {

    public:
        Looper() {}
        ~Looper() {}

        void Init(int32_t sampleRate, float *buffer, int32_t maxBufferSamples);
        void Reset();
        void ClearBuffer();
        void StopBuffering();
        void SetReadRate(float rate);
        void SetLoopLength(int32_t length);
        void SetMovement(Movement movement);
        void SetLooping(bool looping);
        bool Buffer(float value);
        void SetReadPosition(float position);
        float Read();
        void Write(float value, float bufferedValue);
        void UpdateReadPos();
        void UpdateWritePos();
        bool HandleFade();
        bool Restart(bool triggerRestart);
        void SetLoopStart(int32_t pos);
        int32_t GetRandomPosition();
        void SetLoopEnd(int32_t pos);
        void SetDirection(Direction direction);
        void ToggleDirection();
        void SetWriting(bool active);
        void ToggleWriting();
        void ToggleReading();

        bool isRestarting{};
        bool isStopping{};
        bool isStarting{};

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }

        inline int32_t GetLoopStart() { return loopStart_; }
        inline float GetLoopStartSeconds() { return loopStartSeconds_; }

        inline int32_t GetLoopEnd() { return loopEnd_; }

        inline int32_t GetLoopLength() { return loopLength_; }
        inline float GetLoopLengthSeconds() { return loopLengthSeconds_; }

        inline float GetReadPos() { return readPos_; }
        inline float GetReadPosSeconds() { return readPosSeconds_; }
        inline float GetNextReadPos() { return nextReadPos_; }

        inline int32_t GetWritePos() { return writePos_; }

        inline float GetReadRate() { return readRate_; }
        inline int32_t GetSampleRateSpeed() { return sampleRateSpeed_; }

        inline Movement GetMovement() { return movement_; }
        inline Direction GetDirection() { return direction_; }
        inline bool IsDrunkMovement() { return Movement::DRUNK == movement_; }
        inline bool IsGoingForward() { return Direction::FORWARD == direction_; }

        inline void SetReading(bool active) { readingActive_ = active; }

        inline int32_t GetCrossPoint() { return crossPoint_; }
        inline bool CrossPointFound() { return crossPointFound_; }

    private:
        void CalculateHeadsDistance();
        void CalculateCrossPoint();
        void SetUpFade(Fade fade);

        float *buffer_{};           // The buffer
        float bufferSeconds_{};     // Written buffer length in seconds
        float readPos_{};           // The read position
        float readPosSeconds_{};    // Read position in seconds
        float nextReadPos_{};       // Next read position
        float loopStartSeconds_{};  // Start of the loop in seconds
        float loopLengthSeconds_{}; // Length of the loop in seconds
        float readRate_{};         // Speed multiplier
        float writeRate_{};         // Speed multiplier
        float readSpeed_{};         // Actual read speed
        float writeSpeed_{};        // Actual write speed
        int32_t bufferSamples_{};    // The written buffer length in samples
        int32_t writePos_{};         // The write position
        int32_t loopStart_{};        // Loop start position
        int32_t loopEnd_{};          // Loop end position
        int32_t loopLength_{};       // Length of the loop in samples
        int32_t headsDistance_{};
        int32_t sampleRate_{}; // The sample rate
        Direction direction_{};
        int32_t crossPoint_{};
        bool crossPointFound_{};
        bool readingActive_{true};
        bool writingActive_{true};
        int32_t sampleRateSpeed_{};
        bool looping_{true};

        Head heads_[2]{{Type::READ}, {Type::WRITE}};

        Movement movement_{}; // The current movement type of the looper
    };
} // namespace wreath
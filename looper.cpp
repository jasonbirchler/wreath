#include "looper.h"
#include "Utility/dsp.h"

using namespace wreath;
using namespace daisysp;

/**
 * @brief Initializes the looper.
 *
 * @param sampleRate
 * @param mem
 * @param maxBufferSeconds
 */
void Looper::Init(size_t sampleRate, float *mem, int maxBufferSeconds)
{
    sampleRate_ = sampleRate;
    buffer_ = mem;
    initBufferSamples_ = sampleRate * maxBufferSeconds;
    ResetBuffer();

    movement_ = Movement::FORWARD;
    forward_ = Movement::FORWARD == movement_;

    cf_.Init(CROSSFADE_CPOW);
}

/**
 * @brief Sets the speed, clamping its value just in case.
 *
 * @param speed
 */
void Looper::SetSpeed(float speed)
{
    speed_ = fclamp(speed, 0.f, 2.f);
}

/**
 * @brief Sets the movement type.
 *
 * @param movement
 */
void Looper::SetMovement(Movement movement)
{
    if (Movement::FORWARD == movement && !forward_)
    {
        forward_ = true;
    }
    else if (Movement::BACKWARDS == movement && forward_)
    {
        forward_ = false;
    }

    movement_ = movement;
}

/**
 * @brief Resets the buffer and the looper state.
 */
void Looper::ResetBuffer()
{
    std::fill(&buffer_[0], &buffer_[initBufferSamples_ - 1], 0.f);
    bufferSamples_ = 0;
    bufferSeconds_ = 0.f;
    readPos_ = 0.f;
    readPosSeconds_ = 0.f;
    nextReadPos_ = 0.f;
    fadePos_ = 0;
    loopLengthSeconds_ = 0.f;
    writePos_ = 0;
    loopStart_ = 0;
    loopEnd_ = 0;
    loopLength_ = 0;
    speed_ = 1.f;
    fadeIndex_ = 0;
}

bool Looper::Buffer(float value)
{
    // Handle end of buffer.
    if (writePos_ > initBufferSamples_ - 1)
    {
        return true;
    }

    Write(value);
    writePos_++;
    bufferSamples_ = writePos_;
    bufferSeconds_ = bufferSamples_ / static_cast<float>(sampleRate_);

    return false;
}

void Looper::MustRestart()
{
    mustRestart_ = true;
}

void Looper::Restart()
{
    if (Movement::RANDOM == movement_)
    {
        nextReadPos_ = GetRandomPosition();
        forward_ = nextReadPos_ > readPos_;
    }
    else
    {
        if (forward_)
        {
            SetReadPosAtStart();
        }
        else
        {
            SetReadPosAtEnd();
        }
    }
}

/**
 * @brief Stops the buffering.
 */
void Looper::StopBuffering()
{
    loopStart_ = 0;
    writePos_ = 0;
    ResetLoopLength();
    SetReadPos(forward_ ? loopStart_ : loopEnd_);
}

/**
 * @brief Increments the loop length by the given samples.
 *
 * @param samples
 */
void Looper::IncrementLoopLength(size_t samples)
{
    SetLoopLength((loopLength_ + samples < bufferSamples_) ? loopLength_ + samples : bufferSamples_);
}

/**
 * @brief Decrements the loop length by the given samples.
 *
 * @param samples
 */
void Looper::DecrementLoopLength(size_t samples)
{
    SetLoopLength((loopLength_ > samples) ? loopLength_ - samples : kMinSamples);
}

/**
 * @brief Sets the loop length.
 *
 * @param length
 */
void Looper::SetLoopLength(size_t length)
{
    loopLength_ = length;
    loopEnd_ = loopLength_ - 1;
    loopLengthSeconds_ = loopLength_ / static_cast<float>(sampleRate_);
}

/**
 * @brief Sets the loop length to that of the written buffer.
 */
void Looper::ResetLoopLength()
{
    SetLoopLength(bufferSamples_);
}

/**
 * @brief Reads from a specified point in the delay line using linear
 *        interpolation. Also applies a fade in and out at the beginning and end
 *        of the loop.
 *
 * @param pos
 * @return float
 */
float Looper::Read(float pos)
{
    // 1) get the value at position.

    // Integer position.
    uint32_t i_idx{static_cast<uint32_t>(pos)};
    // Value at the integer position.
    float val{buffer_[i_idx]};
    // Position decimal part.
    float frac{pos - i_idx};
    // If the position is not an integer number we need to interpolate the value.
    if (frac != 0.f)
    {
        // Value at the position after or before, depending on the direction
        // we're going.
        float val2{buffer_[static_cast<uint32_t>(fclamp(i_idx + (forward_ ? 1 : -1), 0, bufferSamples_ - 1))]};
        // Interpolated value.
        val = val + (val2 - val) * frac;
    }

    // 2) fade the value if needed.
    if (mustFadeIn_ || mustFadeOut_)
    {
        // The number of samples we need to fade.
        float samples{GetFadeSamples()};
        float from{mustFadeIn_ ? 0.f : 1.f};
        float to{mustFadeIn_ ? 1.f : 0.f};
        cf_.SetPos(fadeIndex_ * (1.f / samples));
        val *= cf_.Process(from, to);
        fadeIndex_++;
        if (fadeIndex_ > samples)
        {
            mustFadeIn_ = mustFadeOut_ = false;
        }
    }

    return val;
}

/**
 * @brief
 *
 * @param pos
 */
void Looper::SetWritePos(float pos)
{
    writePos_ = (pos > loopEnd_) ? loopStart_ : pos;
}

/**
 * @brief Updates the given position depending on the loop boundaries and the
 *        current movement type.
 *
 * @param pos
 * @param isReadPos
 */
void Looper::HandlePosBoundaries(float &pos, bool isReadPos)
{
    // Handle normal loop boundaries.
    if (loopEnd_ > loopStart_)
    {
        // Forward direction.
        if (forward_ && pos > loopEnd_)
        {
            pos = loopStart_;
            // Invert direction when in pendulum.
            if (Movement::PENDULUM == movement_)
            {
                // Switch direction only if we're handling the read position.
                if (isReadPos) {
                    forward_ = !forward_;
                }
                pos = loopEnd_;
            }
        }
        // Backwards direction.
        else if (!forward_ && pos < loopStart_)
        {
            pos = loopEnd_;
            // Invert direction when in pendulum.
            if (Movement::PENDULUM == movement_)
            {
                // Switch direction only if we're handling the read position.
                if (isReadPos) {
                    forward_ = !forward_;
                }
                pos = loopStart_;
            }
        }
    }
    // Handle inverted loop boundaries (end point comes before start point).
    else
    {
        if (forward_)
        {
            if (pos > bufferSamples_)
            {
                // Wrap-around.
                pos = 0;
            }
            else if (pos > loopEnd_ && pos < loopStart_)
            {
                pos = loopStart_;
                // Invert direction when in pendulum.
                if (Movement::PENDULUM == movement_)
                {
                    // Switch direction only if we're handling the read position.
                    if (isReadPos) {
                        forward_ = !forward_;
                    }
                    pos = loopEnd_;
                }
            }
        }
        else
        {
            if (pos < 0)
            {
                // Wrap-around.
                pos = bufferSamples_ - 1;
            }
            else if (pos > loopEnd_ && pos < loopStart_)
            {
                pos = loopEnd_;
                // Invert direction when in pendulum.
                if (Movement::PENDULUM == movement_)
                {
                    // Switch direction only if we're handling the read position.
                    if (isReadPos) {
                        forward_ = !forward_;
                    }
                    pos = loopStart_;
                }
            }
        }
    }
}

/**
 * @brief Sets the read position. Also, sets a fade in/out if needed.
 *
 * @param pos
 */
void Looper::SetReadPos(float pos)
{
    uint32_t i_idx{static_cast<uint32_t>(pos)};
    float samples{GetFadeSamples()};

    // Set up a fade in if:
    // - we're going forward and are at the beginning of the loop;
    // - we're going backwards and are at the end of the loop;
    // - we've been instructed to reset the starting position.
    if ((forward_ && i_idx == loopStart_) || (!forward_ && i_idx == loopEnd_) || mustRestart_)
    {
        fadePos_ = pos;
        fadeIndex_ = 0;
        mustFadeIn_ = true;
        mustRestart_ = false;
    }

    // Set up a fade out if:
    // - we're going forward and are at the end of the loop;
    // - we're going backwards and are at the beginning of the loop;
    // - we're about to cross the write position when going backwards.
    if ((forward_ && i_idx + samples == loopEnd_) || (!forward_ && i_idx - samples == loopStart_) || (!forward_ && i_idx - samples == writePos_))
    {
        fadePos_ = pos;
        fadeIndex_ = 0;
        mustFadeOut_ = true;
    }

    readPos_ = pos;
    readPosSeconds_ = readPos_ / sampleRate_;
}

/**
 * @brief Returns a random position within the loop.
 *
 * @return size_t
 */
size_t Looper::GetRandomPosition()
{
    size_t pos{loopStart_ + rand() % (loopLength_ - 1)};
    if (forward_ && pos > loopEnd_)
    {
        pos = loopEnd_;
    }
    else if (!forward_ && pos < loopStart_)
    {
        pos = loopStart_;
    }

    return pos;
}

/**
 * @brief Sets the read position at the beginning of the loop.
 */
void Looper::SetReadPosAtStart()
{
    SetReadPos(loopStart_);
    // Invert direction when in pendulum.
    if (Movement::PENDULUM == movement_)
    {
        forward_ = !forward_;
        SetReadPos(loopEnd_);
    }
}

/**
 * @brief Sets the read position at the end of the loop.
 */
void Looper::SetReadPosAtEnd()
{
    SetReadPos(loopEnd_);
    // Invert direction when in pendulum.
    if (Movement::PENDULUM == movement_)
    {
        forward_ = !forward_;
        SetReadPos(loopStart_);
    }
}
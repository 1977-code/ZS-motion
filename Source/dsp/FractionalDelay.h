#pragma once

#include "../utils/MathUtils.h"
#include <vector>
#include <cmath>

namespace zs
{

/**
    A single-channel modulated delay line with 4-point (3rd-order) Hermite
    interpolation.

    This is the part the spec calls out as the main technical risk: a chorus or
    vibrato sweeps the read pointer continuously, so the interpolator has to stay
    smooth and artefact-free as the fractional delay glides. Cubic Hermite is the
    sweet spot — clearly cleaner than linear under deep modulation, far cheaper
    than a windowed-sinc.

    All memory is taken in prepare(); push()/readHermite() are allocation-free and
    safe to call from the audio thread.

    Convention: readHermite(d) returns the sample written d samples before the
    most recent push(). The minimum readable delay is 2 samples, which is what the
    interpolation stencil needs; callers keep their base delay well above that.
*/
class FractionalDelay
{
public:
    /** @param maxDelaySamples largest delay (in samples) that will ever be read. */
    void prepare (int maxDelaySamples)
    {
        maxDelay = std::max (4, maxDelaySamples);
        size     = maxDelay + 4;                 // stencil margin
        buffer.assign ((size_t) size, 0.0f);
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        if (++writePos >= size)
            writePos = 0;
    }

    float readHermite (float delaySamples) const noexcept
    {
        delaySamples = math::clamp (delaySamples, 2.0f, (float) maxDelay);

        const float readPos = (float) writePos - 1.0f - delaySamples;
        const int   i1   = (int) std::floor (readPos);
        const float frac = readPos - (float) i1;

        const float ym1 = buffer[(size_t) wrap (i1 - 1)];
        const float y0  = buffer[(size_t) wrap (i1)];
        const float y1  = buffer[(size_t) wrap (i1 + 1)];
        const float y2  = buffer[(size_t) wrap (i1 + 2)];

        // Catmull-Rom / Hermite: interpolate between y0 and y1 using ym1, y2.
        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    /** Push then read — the common per-sample idiom. */
    float processSample (float x, float delaySamples) noexcept
    {
        push (x);
        return readHermite (delaySamples);
    }

    int getMaxDelay() const noexcept { return maxDelay; }

private:
    int wrap (int i) const noexcept
    {
        i %= size;
        return i < 0 ? i + size : i;
    }

    std::vector<float> buffer;
    int size     = 0;
    int maxDelay = 0;
    int writePos = 0;
};

} // namespace zs

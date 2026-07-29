#pragma once

#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    Mid/Side width control.

        M = (L + R) / 2      S = (L − R) / 2
        S' = S · width
        L' = M + S'          R' = M − S'

    width = 0 collapses to mono, 1 is unchanged, 2 exaggerates the sides. The
    factor is smoothed so automating it never zips.
*/
class StereoWidth
{
public:
    void prepare (double sampleRate) noexcept
    {
        widthSmoothed.prepare (sampleRate, 0.02f, widthSmoothed.getTarget());
    }

    void reset() noexcept {}

    void setWidth (float w) noexcept { widthSmoothed.setTarget (math::clamp (w, 0.0f, 2.0f)); }

    void processSample (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float width = widthSmoothed.getNextValue();

        const float mid  = 0.5f * (inL + inR);
        const float side = 0.5f * (inL - inR) * width;

        outL = mid + side;
        outR = mid - side;
    }

private:
    SmoothedParameter widthSmoothed;
};

} // namespace zs

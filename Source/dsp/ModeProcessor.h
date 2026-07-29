#pragma once

namespace zs
{

/**
    Common interface for the four modulation modes (chorus, vibrato, tremolo,
    rotary). The engine owns one of each, keeps them all prepared, and
    crossfades between the outgoing and incoming mode when MODE changes so the
    switch is click-free.

    Every mode returns *wet only* — the global equal-power Dry/Wet stage adds the
    dry signal back at the end of the chain. That is what makes a vibrato turn
    into a chorus below 100 % wet, exactly as the reference plug-in behaves.

    processSample() is handed the shared LFO already sampled for this sample, once
    per channel (lfoL / lfoR), both bipolar in [-1, 1]. The stereo-phase offset is
    baked into lfoR by the engine. Rotary keeps its own faster/slower phases and
    ignores these, driving itself from the rate set via setRateHz().
*/
class ModeProcessor
{
public:
    virtual ~ModeProcessor() = default;

    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;

    /** Global depth, 0..1. Each mode maps it to its own natural range. */
    virtual void setDepth (float depth01) noexcept = 0;

    /** The global LFO rate in Hz. Only rotary needs it (for its own phases);
        the others read the shared LFO passed into processSample(). */
    virtual void setRateHz (float) noexcept {}

    virtual void processSample (float inL, float inR,
                                float lfoL, float lfoR,
                                float& outL, float& outR) noexcept = 0;
};

} // namespace zs

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Layout.h"

namespace zs
{

/**
    Everything behind the controls: the base wash, the gold glow under the rotor,
    the triangle texture, the still brand waves, the header and footer, and the
    hairlines that divide the panel up.

    All of it is drawn once into an image on resize, because none of it changes —
    only the drifting waveform behind the header is repainted live, in its own
    narrow strip.
*/
class BrandBackground final : public juce::Component,
                              private juce::Timer
{
public:
    explicit BrandBackground (juce::String versionText);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void renderStaticLayer();

    juce::String version;
    juce::Image  staticLayer;
    float        wavePhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrandBackground)
};

} // namespace zs

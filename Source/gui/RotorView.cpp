#include "RotorView.h"

namespace zs
{

using namespace juce;

namespace
{
    /** Wrap an error into [-0.5, 0.5) so a phase lock always takes the short way. */
    inline float shortestArc (float err) noexcept
    {
        return err - std::floor (err + 0.5f);
    }

    constexpr int   liveFrames  = 6;      // frames of no movement before we free-run
    constexpr float lockStrength = 0.25f;
}

//==============================================================================
RotorView::RotorView (ZsMotionAudioProcessor& p)
    : processor (p)
{
    math::Xorshift rng { 0x5EEDu };
    for (auto& n : randomNodes)
        n = rng.nextBipolar();

    setTooltip (String::fromUTF8 (
        "\xd0\xa4\xd0\xbe\xd1\x80\xd0\xbc\xd0\xb0 \xd1\x80\xd0\xbe\xd1\x82\xd0\xbe\xd1\x80\xd0\xb0 \xe2\x80\x94 "
        "\xd1\x8d\xd1\x82\xd0\xbe \xd0\xb8 \xd0\xb5\xd1\x81\xd1\x82\xd1\x8c \xd1\x84\xd0\xbe\xd1\x80\xd0\xbc\xd0\xb0 LFO, "
        "\xd0\xb0 \xd1\x81\xd0\xba\xd0\xbe\xd1\x80\xd0\xbe\xd1\x81\xd1\x82\xd1\x8c \xd0\xb2\xd1\x80\xd0\xb0\xd1\x89\xd0\xb5\xd0\xbd\xd0\xb8\xd1\x8f \xe2\x80\x94 Rate. "
        "\xd0\xa2\xd1\x8f\xd0\xbd\xd0\xb8 \xd0\xb2\xd0\xb2\xd0\xb5\xd1\x80\xd1\x85/\xd0\xb2\xd0\xbd\xd0\xb8\xd0\xb7 \xe2\x80\x94 Depth, "
        "\xd0\xb2\xd0\xbb\xd0\xb5\xd0\xb2\xd0\xbe/\xd0\xb2\xd0\xbf\xd1\x80\xd0\xb0\xd0\xb2\xd0\xbe \xe2\x80\x94 Rate. "
        "Shift \xe2\x80\x94 \xd1\x82\xd0\xbe\xd1\x87\xd0\xbd\xd0\xb5\xd0\xb5, \xd0\xb4\xd0\xb2\xd0\xbe\xd0\xb9\xd0\xbd\xd0\xbe\xd0\xb9 \xd0\xba\xd0\xbb\xd0\xb8\xd0\xba \xe2\x80\x94 \xd1\x81\xd0\xb1\xd1\x80\xd0\xbe\xd1\x81."));

    startTimerHz ((int) refreshHz);
}

//==============================================================================
float RotorView::param (const char* id) const noexcept
{
    if (auto* v = processor.apvts.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

void RotorView::setParam (const char* id, float plainValue) const
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

//==============================================================================
void RotorView::timerCallback()
{
    const float dt   = 1.0f / refreshHz;
    const float rate = processor.getEffectiveRateHz();

    // Is the engine actually running? If the phase has not moved for a few frames
    // the host is idle, and we free-run so the rotor still shows the Rate.
    const float dsp = processor.getUiPhase();

    if (std::abs (dsp - lastDspPhase) > 1.0e-7f)
    {
        lastDspPhase = dsp;
        staleFrames  = 0;
    }
    else if (staleFrames <= liveFrames)
    {
        ++staleFrames;
    }

    const bool live = staleFrames <= liveFrames;

    const float previous = displayPhase;
    displayPhase = math::wrap01 (displayPhase + rate * dt);

    if (live)
        displayPhase = math::wrap01 (displayPhase + lockStrength * shortestArc (dsp - displayPhase));

    // A fresh smooth-random node each revolution, exactly as the LFO does.
    if (displayPhase < previous)
    {
        cycleIndex = (cycleIndex + 1) % traceCycles;

        math::Xorshift rng { (uint32_t) (1u + (uint32_t) (previous * 100000.0f)) };
        for (size_t i = randomNodes.size() - 1; i > 0; --i)
            randomNodes[i] = randomNodes[i - 1];
        randomNodes[0] = rng.nextBipolar();
    }

    // Fan carrier.
    const float fanRate = param (params::fanRate);
    fanPhase = math::wrap01 (fanPhase + fanRate * dt);
    if (live)
        fanPhase = math::wrap01 (fanPhase + lockStrength * shortestArc (processor.getUiFanPhase() - fanPhase));

    // Rotary rotors: the DSP's own, so their inertia is visible; free-run if idle.
    if (live)
    {
        hornPhase = processor.getUiHornPhase();
        drumPhase = processor.getUiDrumPhase();
    }
    else
    {
        hornPhase = math::wrap01 (hornPhase + rate * dt);
        drumPhase = math::wrap01 (drumPhase + rate * 0.75f * dt);
    }

    smoothedLevel += 0.30f * (processor.getUiOutputLevel() - smoothedLevel);

    repaint();
}

//==============================================================================
float RotorView::profileAt (Waveform wave, float t, int lobes) const noexcept
{
    if (wave == Waveform::SmoothRandom)
    {
        // Nodes indexed modulo the lobe count, so the figure closes on itself.
        const float x  = t * (float) lobes;
        const int   i0 = (int) std::floor (x);
        const float f  = x - (float) i0;

        const int a = ((i0     % lobes) + lobes) % lobes;
        const int b = ((i0 + 1) % lobes + lobes) % lobes;

        return math::lerp (randomNodes[(size_t) a], randomNodes[(size_t) b], math::smoothstep (f));
    }

    return waveformShape (wave, math::wrap01 (t * (float) lobes));
}

Path RotorView::rotorPath (Point<float> centre, float radius, float depth,
                           Waveform wave, int lobes, float rotation) const
{
    Path p;

    for (int i = 0; i <= pathSteps; ++i)
    {
        const float t = (float) i / (float) pathSteps;
        const float r = radius * (1.0f + depth * 0.40f * profileAt (wave, t, lobes));
        const float a = math::twoPi * (t + rotation) - math::halfPi;

        const Point<float> pt { centre.x + std::cos (a) * r,
                                centre.y + std::sin (a) * r };

        if (i == 0) p.startNewSubPath (pt);
        else        p.lineTo (pt);
    }

    p.closeSubPath();
    return p;
}

//==============================================================================
void RotorView::paintPanel (Graphics& g, Rectangle<float> area) const
{
    g.setColour (theme::panelDeep);
    g.fillRoundedRectangle (area, 8.0f);
}

void RotorView::paintGraticule (Graphics& g, Rectangle<float> area,
                                Point<float> centre, float radius, float glow) const
{
    // Heat bloom behind the sculpture.
    g.setGradientFill (ColourGradient (theme::gold.withAlpha (0.10f * glow), centre,
                                       Colours::transparentBlack,
                                       centre.translated (radius * 2.1f, 0.0f), true));
    g.fillRect (area);

    // Guide rings — the instrument's own graticule.
    for (float k : { 0.44f, 0.80f, 1.22f })
    {
        g.setColour (theme::border.withAlpha (k > 1.0f ? 0.75f : 0.45f));
        g.drawEllipse (Rectangle<float> (radius * 2.0f * k, radius * 2.0f * k).withCentre (centre), 1.0f);
    }

    // Ticks every 15°, longer every 90°.
    for (int i = 0; i < 24; ++i)
    {
        const float a = math::twoPi * (float) i / 24.0f - math::halfPi;
        const bool  major = (i % 6) == 0;
        const float r0 = radius * 1.22f;
        const float r1 = r0 + (major ? 7.0f : 3.5f);

        g.setColour (theme::border.withAlpha (major ? 0.9f : 0.5f));
        g.drawLine (centre.x + std::cos (a) * r0, centre.y + std::sin (a) * r0,
                    centre.x + std::cos (a) * r1, centre.y + std::sin (a) * r1,
                    major ? 1.2f : 0.8f);
    }
}

/**
    The same waveform again, but laid out flat: two cycles as a reference curve with
    the second channel's offset copy behind it, and a sweep marking where the LFO is
    right now. Seeing the shape both ways makes the stereo offset obvious.
*/
void RotorView::paintTrace (Graphics& g, Rectangle<float> strip,
                            Waveform wave, float stereoOffset, Colour colour) const
{
    const float midY = strip.getCentreY();
    const float amp  = strip.getHeight() * 0.44f;

    // Just the zero line — rails would sit exactly under a square wave and read
    // as part of the trace.
    g.setColour (theme::border.withAlpha (0.55f));
    g.fillRect (strip.getX(), midY, strip.getWidth(), 1.0f);

    // Cycle boundaries.
    for (int c = 1; c < traceCycles; ++c)
    {
        const float x = strip.getX() + strip.getWidth() * (float) c / (float) traceCycles;
        g.setColour (theme::border.withAlpha (0.35f));
        g.fillRect (x, strip.getY(), 1.0f, strip.getHeight());
    }

    const int steps = jmax (64, (int) strip.getWidth());

    const auto buildTrace = [&] (float offset)
    {
        Path p;

        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float v = waveformShape (wave, math::wrap01 (t * (float) traceCycles + offset),
                                          randomNodes[0], randomNodes[1]);

            const Point<float> pt { strip.getX() + strip.getWidth() * t, midY - v * amp };

            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }

        return p;
    };

    // The right channel sits behind, shifted by the stereo phase.
    g.setColour (colour.withAlpha (0.30f));
    g.strokePath (buildTrace (stereoOffset), PathStrokeType (1.2f));

    g.setColour (colour.withAlpha (0.85f));
    g.strokePath (buildTrace (0.0f), PathStrokeType (1.5f));

    // The sweep: where the LFO is at this instant.
    const float sweep01 = ((float) cycleIndex + displayPhase) / (float) traceCycles;
    const float sweepX  = strip.getX() + strip.getWidth() * sweep01;

    g.setColour (theme::gold.withAlpha (0.35f));
    g.fillRect (sweepX, strip.getY(), 1.0f, strip.getHeight());

    const float v = waveformShape (wave, displayPhase, randomNodes[0], randomNodes[1]);
    const Point<float> dot { sweepX, midY - v * amp };

    g.setColour (theme::goldLight);
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (dot));
}

void RotorView::paintBody (Graphics& g, Point<float> centre, float radius,
                           float depth, Waveform wave, int lobes, float rotation,
                           Colour colour, float trail) const
{
    // Motion trail: a few ghosts dropped behind, longer the faster it turns.
    for (int k = trailSteps; k >= 1; --k)
    {
        const float back = 0.016f * trail * (float) k;
        const float a = 0.10f * (1.0f - (float) k / (float) (trailSteps + 1)) * (0.25f + trail);

        if (a <= 0.004f)
            continue;

        g.setColour (colour.withAlpha (a));
        g.strokePath (rotorPath (centre, radius, depth, wave, lobes, rotation - back),
                      PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const auto body = rotorPath (centre, radius, depth, wave, lobes, rotation);

    g.setColour (colour.withAlpha (0.07f));
    g.fillPath (body);

    g.setColour (colour.withAlpha (0.22f));
    g.strokePath (body, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (colour);
    g.strokePath (body, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
}

void RotorView::paintFan (Graphics& g, Point<float> centre, float radius) const
{
    const float amount = param (params::fanAmount);
    const int   blades = jlimit (2, 8, (int) param (params::fanBlades));
    const float res    = param (params::fanResonance);

    if (amount <= 0.001f)
        return;

    const float inner = radius * 0.20f;
    const float outer = radius * 1.16f;

    for (int i = 0; i < blades; ++i)
    {
        const float centreAngle = math::twoPi * (fanPhase + (float) i / (float) blades) - math::halfPi;
        const float halfSpan    = math::pi / (float) blades * 0.30f;

        Path wedge;
        wedge.startNewSubPath (centre.x + std::cos (centreAngle - halfSpan) * inner,
                               centre.y + std::sin (centreAngle - halfSpan) * inner);
        wedge.lineTo (centre.x + std::cos (centreAngle - halfSpan) * outer,
                      centre.y + std::sin (centreAngle - halfSpan) * outer);
        wedge.lineTo (centre.x + std::cos (centreAngle + halfSpan) * outer,
                      centre.y + std::sin (centreAngle + halfSpan) * outer);
        wedge.lineTo (centre.x + std::cos (centreAngle + halfSpan) * inner,
                      centre.y + std::sin (centreAngle + halfSpan) * inner);
        wedge.closeSubPath();

        g.setColour (theme::background.withAlpha (0.55f * amount));
        g.fillPath (wedge);

        g.setColour (theme::goldLight.withAlpha ((0.16f + 0.34f * res) * amount));
        g.strokePath (wedge, PathStrokeType (1.0f));
    }
}

void RotorView::paintHub (Graphics& g, Point<float> centre, float radius,
                          float level, Colour colour) const
{
    const float r = radius * 0.155f * (1.0f + 0.12f * jlimit (0.0f, 1.0f, level));
    const auto  hub = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);

    g.setGradientFill (ColourGradient (Colour (0xff17150f), centre.x, hub.getY(),
                                       Colour (0xff050505), centre.x, hub.getBottom(), false));
    g.fillEllipse (hub);

    g.setColour (colour.withAlpha (0.55f + 0.45f * jlimit (0.0f, 1.0f, level)));
    g.drawEllipse (hub.reduced (0.5f), 1.4f);

    g.setColour (colour.withAlpha (0.35f + 0.65f * jlimit (0.0f, 1.0f, level)));
    g.fillEllipse (Rectangle<float> (r * 0.44f, r * 0.44f).withCentre (centre));
}

void RotorView::paintReadout (Graphics& g, Rectangle<float> area) const
{
    const int mode = jlimit (0, 3, (int) param (params::mode));

    auto caption = theme::display (10.0f);
    caption.setExtraKerningFactor (0.32f);
    g.setFont (caption);

    g.setColour (theme::textMuted);
    g.drawText (params::modeChoices()[mode].toUpperCase(),
                area.reduced (12.0f, 9.0f), Justification::topLeft, false);

    // What the LFO is actually running at, in the terms the user set it.
    String rateText;

    if (param (params::syncEnabled) > 0.5f)
    {
        const int div = jlimit (0, 5, (int) param (params::syncDivision));
        const int mod = jlimit (0, 2, (int) param (params::syncModifier));

        rateText = params::divisionChoices()[div];
        if (mod == 1) rateText += " DOT";
        if (mod == 2) rateText += " TRI";
    }
    else
    {
        rateText = String (processor.getEffectiveRateHz(), 2) + " Hz";
    }

    g.setFont (theme::value (13.0f));
    g.setColour (theme::text);
    g.drawText (rateText, area.reduced (12.0f, 8.0f), Justification::topRight, false);
}

//==============================================================================
void RotorView::paint (Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    const int   mode   = jlimit (0, 3, (int) param (params::mode));
    const auto  wave   = (Waveform) jlimit (0, (int) Waveform::numWaveforms - 1,
                                            (int) param (params::waveform));
    const float depth  = jlimit (0.0f, 1.0f, param (params::depth));
    const float sat    = jlimit (0.0f, 1.0f, param (params::saturation));
    const float width  = jlimit (0.0f, 2.0f, param (params::width));
    const float stereo = param (params::stereoPhase) / 360.0f;
    const bool  rotary = mode == 3;

    // The sculpture takes the upper stage; the flat trace runs along the bottom.
    auto stage = area;
    auto strip = stage.removeFromBottom ((float) traceHeight).reduced (16.0f, 6.0f);

    // Kept clear of the trace even at full depth, where the lobes reach 1.4 r.
    const auto  centre = stage.getCentre();
    const float radius = jmin (stage.getWidth() * 0.20f, stage.getHeight() * 0.34f);

    paintPanel (g, area);

    const float glow = jlimit (0.0f, 1.0f, (0.30f + 0.70f * sat) * (0.45f + smoothedLevel));
    paintGraticule (g, area, centre, radius, glow);

    // The metal runs hot as it is driven.
    const auto hot  = theme::gold.interpolatedWith (Colour (0xfffff0d2), sat * 0.80f);
    const auto cool = theme::goldLight.interpolatedWith (Colour (0xffe8d6ff), sat * 0.35f);

    const float trail = jlimit (0.0f, 1.0f, processor.getEffectiveRateHz() / 8.0f);

    if (rotary)
    {
        // Horn outside, drum inside, turning at their own speeds.
        paintBody (g, centre, radius,         depth, wave, rotaryLobes,  hornPhase, hot,  trail);
        paintBody (g, centre, radius * 0.58f, depth, wave, rotaryLobes, -drumPhase,
                   cool.withAlpha (0.85f), trail * 0.6f);
    }
    else
    {
        const float spread = radius * 0.17f * width;

        paintBody (g, centre.translated (-spread, 0.0f), radius, depth, wave, lobeCount,
                   displayPhase, hot, trail);
        paintBody (g, centre.translated (spread, 0.0f), radius, depth, wave, lobeCount,
                   displayPhase + stereo, cool.withAlpha (0.80f), trail);
    }

    if (param (params::fanEnabled) > 0.5f)
        paintFan (g, centre, radius);

    paintHub (g, centre, radius, smoothedLevel, hot);

    // Phase marker riding the outer graticule.
    {
        const float a = math::twoPi * displayPhase - math::halfPi;
        const Point<float> pt { centre.x + std::cos (a) * radius * 1.22f,
                                centre.y + std::sin (a) * radius * 1.22f };

        g.setColour (theme::gold.withAlpha (0.30f));
        g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (pt));
        g.setColour (theme::goldLight);
        g.fillEllipse (Rectangle<float> (4.4f, 4.4f).withCentre (pt));
    }

    // The same motion, laid out flat. Rotary drives itself, so it has no LFO trace.
    if (! rotary)
        paintTrace (g, strip, wave, stereo, cool);

    paintReadout (g, area);

    g.setColour (theme::border);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);
}

//==============================================================================
void RotorView::mouseDown (const MouseEvent&)
{
    dragging = true;
    dragStartDepth = param (params::depth);
    dragStartRate  = param (params::rate);

    if (auto* p = processor.apvts.getParameter (params::depth)) p->beginChangeGesture();
    if (auto* p = processor.apvts.getParameter (params::rate))  p->beginChangeGesture();
}

void RotorView::mouseDrag (const MouseEvent& e)
{
    if (! dragging)
        return;

    const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;

    // Vertical → Depth.
    const float dDepth = -(float) e.getDistanceFromDragStartY() / 200.0f * fine;
    setParam (params::depth, jlimit (0.0f, 1.0f, dragStartDepth + dDepth));

    // Horizontal → Rate, geometrically, so it feels the same at any speed.
    // Only when the LFO is free-running; when synced the division owns the rate.
    if (param (params::syncEnabled) <= 0.5f)
    {
        const float factor = std::exp ((float) e.getDistanceFromDragStartX() / 220.0f * fine);
        setParam (params::rate, jlimit (params::rateMinHz, params::rateMaxHz,
                                        dragStartRate * factor));
    }
}

void RotorView::mouseUp (const MouseEvent&)
{
    dragging = false;

    if (auto* p = processor.apvts.getParameter (params::depth)) p->endChangeGesture();
    if (auto* p = processor.apvts.getParameter (params::rate))  p->endChangeGesture();
}

void RotorView::mouseDoubleClick (const MouseEvent&)
{
    if (auto* p = processor.apvts.getParameter (params::depth))
        p->setValueNotifyingHost (p->getDefaultValue());

    if (auto* p = processor.apvts.getParameter (params::rate))
        p->setValueNotifyingHost (p->getDefaultValue());
}

void RotorView::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel)
{
    if (param (params::syncEnabled) > 0.5f)
        return;

    const float factor = std::exp (wheel.deltaY * 1.6f);
    setParam (params::rate, jlimit (params::rateMinHz, params::rateMaxHz,
                                    param (params::rate) * factor));
}

} // namespace zs

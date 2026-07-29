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

    inline Point<float> polar (Point<float> centre, float angle, float radius) noexcept
    {
        return { centre.x + std::cos (angle) * radius,
                 centre.y + std::sin (angle) * radius };
    }

    constexpr int   liveFrames   = 6;      // frames of no movement before we free-run
    constexpr float lockStrength = 0.25f;
}

//==============================================================================
RotorView::RotorView (ZsMotionAudioProcessor& p)
    : processor (p)
{
    for (auto& n : randomNodes)
        n = rng.nextBipolar();

    setTooltip (String::fromUTF8 (
        "Форма ротора — это форма LFO, прогнанная через тот же сатуратор, что и звук: "
        "на Hard кончики лепестков сплющиваются, на Asymmetric фигура становится "
        "несимметричной. Скорость вращения — Rate, размах — Depth, поток искр — Mix, "
        "круг вокруг — сухой сигнал, эхо — Feedback, лопасти — FAN.\n\n"
        "Тяни вверх/вниз — Depth, влево/вправо — Rate. Shift — точнее, "
        "двойной клик — сброс."));

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

RotorView::State RotorView::readState() const
{
    State s;

    s.mode       = jlimit (0, 3, (int) param (params::mode));
    s.wave       = (Waveform) jlimit (0, (int) Waveform::numWaveforms - 1,
                                      (int) param (params::waveform));
    s.depth      = jlimit (0.0f, 1.0f, param (params::depth));
    s.rate       = processor.getEffectiveRateHz();
    s.mix        = jlimit (0.0f, 1.0f, param (params::mix));
    s.width      = jlimit (0.0f, 2.0f, param (params::width));
    s.stereo     = param (params::stereoPhase) / 360.0f;
    s.phase      = param (params::phase) / 360.0f;
    s.feedback   = param (params::chorusFeedback);
    s.saturation = jlimit (0.0f, 1.0f, param (params::saturation));
    s.character  = (Saturator::Character) jlimit (0, 2, (int) param (params::satCharacter));
    s.fanOn      = param (params::fanEnabled) > 0.5f;
    s.fanAmount  = jlimit (0.0f, 1.0f, param (params::fanAmount));
    s.fanRes     = jlimit (0.0f, 1.0f, param (params::fanResonance));
    s.fanBlades  = jlimit (2, 8, (int) param (params::fanBlades));
    s.fanShape   = jlimit (0, 2, (int) param (params::fanShape));
    s.synced     = param (params::syncEnabled) > 0.5f;
    s.division   = jlimit (0, 5, (int) param (params::syncDivision));
    s.modifier   = jlimit (0, 2, (int) param (params::syncModifier));
    s.rotary     = s.mode == 3;

    // Output nudges the whole figure's size: 0 dB is unity, quiet shrinks it.
    s.outputScale = jlimit (0.78f, 1.14f, 1.0f + param (params::output) / 12.0f * 0.10f);

    return s;
}

//==============================================================================
void RotorView::timerCallback()
{
    const float dt = 1.0f / refreshHz;
    const auto  s  = readState();

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
    displayPhase = math::wrap01 (displayPhase + s.rate * dt);

    if (live)
        displayPhase = math::wrap01 (displayPhase + lockStrength * shortestArc (dsp - displayPhase));

    // A fresh smooth-random node each revolution, exactly as the LFO does.
    if (displayPhase < previous)
    {
        cycleIndex = (cycleIndex + 1) % traceCycles;

        for (size_t i = randomNodes.size() - 1; i > 0; --i)
            randomNodes[i] = randomNodes[i - 1];

        randomNodes[0] = rng.nextBipolar();
    }

    // Fan carrier.
    fanPhase = math::wrap01 (fanPhase + param (params::fanRate) * dt);
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
        hornPhase = math::wrap01 (hornPhase + s.rate * dt);
        drumPhase = math::wrap01 (drumPhase + s.rate * 0.75f * dt);
    }

    smoothedLevel += 0.30f * (processor.getUiOutputLevel() - smoothedLevel);

    // The wake: one frame of the past per tick.
    history[(size_t) historyWrite] = { s.rotary ? hornPhase : displayPhase, s.depth };
    historyWrite = (historyWrite + 1) % historySize;
    historyCount = jmin (historyCount + 1, historySize);

    updateFlow (s, dt);

    // Flash on any parameter move, so a change always registers.
    const float signature = s.depth * 1.7f + s.rate * 0.31f + s.mix * 2.3f + s.width * 3.1f
                          + s.saturation * 4.7f + s.feedback * 5.3f + s.stereo * 6.1f
                          + s.phase * 7.3f + s.fanAmount * 8.7f + s.fanRes * 9.1f
                          + (float) s.fanBlades * 0.7f + (float) s.fanShape * 1.3f
                          + (float) s.mode * 2.9f + (float) s.wave * 3.7f
                          + (float) s.character * 4.1f + s.outputScale * 11.3f
                          + (s.fanOn ? 0.53f : 0.0f) + (s.synced ? 0.97f : 0.0f);

    if (std::abs (signature - paramSignature) > 1.0e-5f)
    {
        paramSignature = signature;
        highlight = 1.0f;
    }

    highlight = jmax (0.0f, highlight - dt * 2.4f);

    repaint();
}

//==============================================================================
void RotorView::spawnParticle (int index) noexcept
{
    auto& p = particles[(size_t) index];

    p.radius01 = 0.15f + 0.05f * rng.nextFloat();
    p.angle    = rng.nextFloat() * math::twoPi;
    p.speed    = 0.22f + 0.30f * rng.nextFloat();
    p.spin     = (rng.nextFloat() - 0.5f) * 0.7f;
    p.life     = 1.0f;
}

void RotorView::updateFlow (const State& s, float dt)
{
    // Mix decides how much of the signal is actually going through the effect, so
    // it decides how much flow there is to see.
    const float density = 0.12f + 0.88f * s.mix;
    const int   wanted  = (int) ((float) maxParticles * jlimit (0.05f, 1.0f, density));

    int alive = 0;
    for (auto& p : particles)
        if (p.life > 0.0f)
            ++alive;

    // Faster rate throws the flow out harder; depth makes it swirl.
    const float rateNorm = jlimit (0.0f, 1.0f, s.rate / 10.0f);
    const float outward  = 0.45f + 1.15f * rateNorm;
    const float swirl    = (0.5f + 2.6f * rateNorm) * (0.35f + s.depth);

    for (int i = 0; i < (int) particles.size(); ++i)
    {
        auto& p = particles[(size_t) i];

        if (p.life <= 0.0f)
        {
            if (alive < wanted)
            {
                spawnParticle (i);
                ++alive;
            }
            continue;
        }

        p.radius01 += p.speed * outward * dt;
        p.angle    += (p.spin + swirl) * dt;
        p.life     -= dt * 0.62f;

        if (p.radius01 > 1.28f)
            p.life = 0.0f;
    }
}

float RotorView::bladeShadow (const State& s, float angle) const noexcept
{
    if (! s.fanOn || s.fanAmount <= 0.001f)
        return 0.0f;

    // How wide a blade is depends on the carrier shape, exactly as it does in the
    // audio: Pulse is a narrow chop, Sine a broad sweep.
    const float widthScale = s.fanShape == 2 ? 0.34f : (s.fanShape == 1 ? 0.55f : 0.80f);
    const float halfSpan   = math::pi / (float) s.fanBlades * widthScale;

    // Distance to the nearest blade centre.
    const float spacing = math::twoPi / (float) s.fanBlades;
    const float base    = math::twoPi * fanPhase - math::halfPi;

    float delta = std::fmod (angle - base, spacing);
    if (delta < 0.0f) delta += spacing;
    if (delta > spacing * 0.5f) delta -= spacing;

    const float t = jlimit (0.0f, 1.0f, 1.0f - std::abs (delta) / halfSpan);

    return math::smoothstep (t) * s.fanAmount;
}

//==============================================================================
float RotorView::profileAt (const State& s, float t, int lobes) const noexcept
{
    float v;

    if (s.wave == Waveform::SmoothRandom)
    {
        // Nodes indexed modulo the lobe count, so the figure closes on itself.
        const float x  = t * (float) lobes;
        const int   i0 = (int) std::floor (x);
        const float f  = x - (float) i0;

        const int a = ((i0     % lobes) + lobes) % lobes;
        const int b = ((i0 + 1) % lobes + lobes) % lobes;

        v = math::lerp (randomNodes[(size_t) a], randomNodes[(size_t) b], math::smoothstep (f));
    }
    else
    {
        v = waveformShape (s.wave, math::wrap01 (t * (float) lobes));
    }

    // Push it through the very same curve the audio takes. Hard flattens the lobe
    // tips into a stepped rim, Asymmetric makes one side bulge — you can see the
    // character of the drive in the metal.
    if (s.saturation > 0.001f)
    {
        const float driven = Saturator::curve (v, Saturator::driveGainFor (s.saturation), s.character);
        v = math::lerp (v, driven, s.saturation);
    }

    return v;
}

Path RotorView::rotorPath (const State& s, Point<float> centre, float radius, float depth,
                           int lobes, float rotation, int steps) const
{
    Path p;

    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float r = radius * (1.0f + depth * 0.40f * profileAt (s, t, lobes));
        const float a = math::twoPi * (t + rotation) - math::halfPi;

        const auto pt = polar (centre, a, r);

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

void RotorView::paintGraticule (Graphics& g, const State& s, Rectangle<float> area,
                                Point<float> centre, float radius, float glow) const
{
    // Heat bloom behind the sculpture, brighter under drive and on a change.
    g.setGradientFill (ColourGradient (theme::gold.withAlpha (0.11f * glow), centre,
                                       Colours::transparentBlack,
                                       centre.translated (radius * 2.1f, 0.0f), true));
    g.fillRect (area);

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
        g.drawLine (polar (centre, a, r0).x, polar (centre, a, r0).y,
                    polar (centre, a, r1).x, polar (centre, a, r1).y,
                    major ? 1.2f : 0.8f);
    }

    // When synced, mark the beat divisions on the rim so the grid is visible.
    if (s.synced)
    {
        const int beats = jlimit (1, 16, (int) std::round (4.0f / params::divisionInQuarters (s.division)));

        for (int i = 0; i < beats; ++i)
        {
            const float a = math::twoPi * (float) i / (float) beats - math::halfPi;
            g.setColour (theme::gold.withAlpha (0.5f));
            g.fillEllipse (Rectangle<float> (3.4f, 3.4f).withCentre (polar (centre, a, radius * 1.30f)));
        }
    }

    // The index mark: where the global Phase offset puts zero.
    {
        const float a = math::twoPi * s.phase - math::halfPi;
        const auto  from = polar (centre, a, radius * 1.26f);
        const auto  to   = polar (centre, a, radius * 1.40f);

        g.setColour (theme::textMuted.withAlpha (0.8f));
        g.drawLine (from.x, from.y, to.x, to.y, 1.4f);
    }
}

void RotorView::paintWake (Graphics& g, const State& s, Point<float> centre,
                           float radius, int lobes, Colour colour) const
{
    if (historyCount < 4)
        return;

    const int stride = jmax (1, historySize / wakeContours);

    for (int c = 1; c <= wakeContours; ++c)
    {
        const int back = c * stride;

        if (back >= historyCount)
            break;

        const int index = ((historyWrite - 1 - back) % historySize + historySize) % historySize;
        const auto& snap = history[(size_t) index];

        const float age = (float) c / (float) wakeContours;

        // The past recedes toward the hub, so the figure sits in its own tunnel.
        const float scale = 1.0f - 0.45f * age;
        const float alpha = 0.17f * (1.0f - age) * (1.0f - age);

        if (alpha < 0.004f)
            continue;

        g.setColour (colour.withAlpha (alpha));
        g.strokePath (rotorPath (s, centre, radius * scale, snap.depth, lobes, snap.phase, wakeSteps),
                      PathStrokeType (1.0f));
    }
}

void RotorView::paintFlow (Graphics& g, const State& s, Point<float> centre,
                           float radius, Colour colour) const
{
    const float rateNorm = jlimit (0.0f, 1.0f, s.rate / 10.0f);
    const float swirl    = (0.5f + 2.6f * rateNorm) * (0.35f + s.depth);
    const float outward  = 0.45f + 1.15f * rateNorm;

    for (const auto& p : particles)
    {
        if (p.life <= 0.0f)
            continue;

        const float r = radius * p.radius01;
        const auto  pos = polar (centre, p.angle, r);

        // A short streak backwards along its own velocity — motion, not dots.
        const float dtBack = 0.085f;
        const float rPrev = jmax (radius * 0.10f, r - radius * p.speed * outward * dtBack);
        const auto  prev = polar (centre, p.angle - (p.spin + swirl) * dtBack, rPrev);

        // Bright in the middle of its life, and dimmed as a blade passes over it.
        const float fade   = std::sin (jlimit (0.0f, 1.0f, p.life) * math::pi);
        const float shadow = 1.0f - 0.85f * bladeShadow (s, p.angle);
        const float alpha  = 0.58f * fade * shadow;

        if (alpha < 0.01f)
            continue;

        g.setColour (colour.withAlpha (alpha));
        g.drawLine (prev.x, prev.y, pos.x, pos.y, 1.0f);
    }
}

void RotorView::paintDryRing (Graphics& g, const State& s, Point<float> centre, float radius) const
{
    // What the signal would be with the effect out of the way. It fades up as Mix
    // comes down, so at 0 % you are looking at a plain circle — which is the truth.
    const float alpha = 0.10f + 0.42f * (1.0f - s.mix);

    Path ring, dashed;
    ring.addEllipse (Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

    const float dashes[] { 5.0f, 5.0f };
    PathStrokeType (1.0f).createDashedStroke (dashed, ring, dashes, 2);

    g.setColour (theme::textFaint.withAlpha (alpha));
    g.strokePath (dashed, PathStrokeType (1.0f));
}

void RotorView::paintEchoes (Graphics& g, const State& s, Point<float> centre, float radius,
                             int lobes, float rotation, Colour colour) const
{
    const float amount = std::abs (s.feedback);

    if (amount < 0.02f)
        return;

    // Feedback is repeats, so it draws repeats: each one a little wider and fainter.
    for (int i = 1; i <= 3; ++i)
    {
        const float scale = 1.0f + 0.10f * (float) i * amount;
        const float alpha = 0.30f * amount / (float) i;

        // Negative feedback inverts, so the copies turn the other way.
        const float turn = rotation - (s.feedback < 0.0f ? -1.0f : 1.0f) * 0.035f * (float) i;

        g.setColour (colour.withAlpha (alpha));
        g.strokePath (rotorPath (s, centre, radius * scale, s.depth, lobes, turn, wakeSteps),
                      PathStrokeType (1.0f));
    }
}

void RotorView::paintBody (Graphics& g, const State& s, Point<float> centre, float radius,
                           int lobes, float rotation, Colour colour, float trail, float alpha) const
{
    for (int k = trailSteps; k >= 1; --k)
    {
        const float back = 0.016f * trail * (float) k;
        const float a = 0.10f * (1.0f - (float) k / (float) (trailSteps + 1)) * (0.25f + trail) * alpha;

        if (a <= 0.004f)
            continue;

        g.setColour (colour.withAlpha (a));
        g.strokePath (rotorPath (s, centre, radius, s.depth, lobes, rotation - back, wakeSteps),
                      PathStrokeType (1.2f));
    }

    const auto body = rotorPath (s, centre, radius, s.depth, lobes, rotation, pathSteps);

    g.setColour (colour.withAlpha (0.07f * alpha));
    g.fillPath (body);

    g.setColour (colour.withAlpha (0.22f * alpha));
    g.strokePath (body, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (colour.withAlpha (alpha));
    g.strokePath (body, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
}

void RotorView::paintFan (Graphics& g, const State& s, Point<float> centre, float radius) const
{
    if (! s.fanOn || s.fanAmount <= 0.001f)
        return;

    const float inner = radius * 0.20f;
    const float outer = radius * 1.16f;

    const float widthScale = s.fanShape == 2 ? 0.34f : (s.fanShape == 1 ? 0.55f : 0.80f);

    for (int i = 0; i < s.fanBlades; ++i)
    {
        const float centreAngle = math::twoPi * (fanPhase + (float) i / (float) s.fanBlades) - math::halfPi;
        const float halfSpan    = math::pi / (float) s.fanBlades * widthScale;

        Path wedge;
        wedge.startNewSubPath (polar (centre, centreAngle - halfSpan, inner));
        wedge.lineTo          (polar (centre, centreAngle - halfSpan, outer));
        wedge.lineTo          (polar (centre, centreAngle + halfSpan, outer));
        wedge.lineTo          (polar (centre, centreAngle + halfSpan, inner));
        wedge.closeSubPath();

        g.setColour (theme::background.withAlpha (0.55f * s.fanAmount));
        g.fillPath (wedge);

        g.setColour (theme::goldLight.withAlpha ((0.16f + 0.34f * s.fanRes) * s.fanAmount));
        g.strokePath (wedge, PathStrokeType (1.0f));

        // Resonance rings the blade edge — the metallic edge you hear.
        if (s.fanRes > 0.02f)
        {
            g.setColour (theme::goldLight.withAlpha (0.30f * s.fanRes * s.fanAmount));
            const auto a = polar (centre, centreAngle, inner);
            const auto b = polar (centre, centreAngle, outer);
            g.drawLine (a.x, a.y, b.x, b.y, 0.9f);
        }
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

/**
    The same waveform again, but laid out flat: two cycles as a reference curve with
    the second channel's offset copy behind it, and a sweep marking where the LFO is
    right now. Seeing the shape both ways makes the stereo offset obvious.
*/
void RotorView::paintTrace (Graphics& g, const State& s, Rectangle<float> strip, Colour colour) const
{
    const float midY = strip.getCentreY();
    const float amp  = strip.getHeight() * 0.44f;

    g.setColour (theme::border.withAlpha (0.55f));
    g.fillRect (strip.getX(), midY, strip.getWidth(), 1.0f);

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

            // Drawn through the drive too, so the flat view agrees with the rotor.
            float v = waveformShape (s.wave, math::wrap01 (t * (float) traceCycles + offset),
                                     randomNodes[0], randomNodes[1]);

            if (s.saturation > 0.001f)
                v = math::lerp (v, Saturator::curve (v, Saturator::driveGainFor (s.saturation),
                                                     s.character), s.saturation);

            const Point<float> pt { strip.getX() + strip.getWidth() * t, midY - v * amp };

            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }

        return p;
    };

    g.setColour (colour.withAlpha (0.30f));
    g.strokePath (buildTrace (s.stereo), PathStrokeType (1.2f));

    g.setColour (colour.withAlpha (0.85f));
    g.strokePath (buildTrace (0.0f), PathStrokeType (1.5f));

    const float sweep01 = ((float) cycleIndex + displayPhase) / (float) traceCycles;
    const float sweepX  = strip.getX() + strip.getWidth() * sweep01;

    g.setColour (theme::gold.withAlpha (0.35f));
    g.fillRect (sweepX, strip.getY(), 1.0f, strip.getHeight());

    float v = waveformShape (s.wave, displayPhase, randomNodes[0], randomNodes[1]);

    if (s.saturation > 0.001f)
        v = math::lerp (v, Saturator::curve (v, Saturator::driveGainFor (s.saturation),
                                             s.character), s.saturation);

    g.setColour (theme::goldLight);
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ sweepX, midY - v * amp }));
}

void RotorView::paintReadout (Graphics& g, const State& s, Rectangle<float> area) const
{
    auto caption = theme::display (10.0f);
    caption.setExtraKerningFactor (0.32f);
    g.setFont (caption);

    g.setColour (theme::textMuted);
    g.drawText (params::modeChoices()[s.mode].toUpperCase(),
                area.reduced (12.0f, 9.0f), Justification::topLeft, false);

    String rateText;

    if (s.synced)
    {
        rateText = params::divisionChoices()[s.division];
        if (s.modifier == 1) rateText += " DOT";
        if (s.modifier == 2) rateText += " TRI";
    }
    else
    {
        rateText = String (s.rate, 2) + " Hz";
    }

    g.setFont (theme::value (13.0f));
    g.setColour (theme::text);
    g.drawText (rateText, area.reduced (12.0f, 8.0f), Justification::topRight, false);
}

//==============================================================================
void RotorView::paint (Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    const auto s = readState();

    auto stage = area;
    auto strip = stage.removeFromBottom ((float) traceHeight).reduced (16.0f, 6.0f);

    const auto centre = stage.getCentre();

    // The graticule is the instrument's fixed scale; Output grows the figure
    // against it, so turning Output up visibly fills more of the dial.
    const float baseRadius = jmin (stage.getWidth() * 0.20f, stage.getHeight() * 0.34f);
    const float radius     = baseRadius * s.outputScale;

    paintPanel (g, area);

    const float flash = highlight * highlight;      // short, sharp
    const float glow  = jlimit (0.0f, 1.4f, (0.30f + 0.70f * s.saturation)
                                            * (0.45f + smoothedLevel) + 0.35f * flash);

    paintGraticule (g, s, area, centre, baseRadius, glow);

    // The metal runs hot as it is driven; a change briefly brightens everything.
    const auto hot  = theme::gold.interpolatedWith (Colour (0xfffff0d2),
                                                    jlimit (0.0f, 1.0f, s.saturation * 0.80f + 0.35f * flash));
    const auto cool = theme::goldLight.interpolatedWith (Colour (0xffe8d6ff), s.saturation * 0.35f);

    const float trail = jlimit (0.0f, 1.0f, s.rate / 8.0f);
    const int   lobes = s.rotary ? rotaryLobes : lobeCount;
    const float bodyAlpha = 0.35f + 0.65f * s.mix;   // Mix decides how present it is

    paintFlow (g, s, centre, radius, hot);
    paintDryRing (g, s, centre, radius);

    if (s.rotary)
    {
        paintWake (g, s, centre, radius, lobes, hot);

        paintBody (g, s, centre, radius, lobes, hornPhase, hot, trail, bodyAlpha);
        paintBody (g, s, centre, radius * 0.58f, lobes, -drumPhase,
                   cool.withAlpha (0.85f), trail * 0.6f, bodyAlpha);
    }
    else
    {
        const float spread = radius * 0.17f * s.width;
        const auto  left   = centre.translated (-spread, 0.0f);
        const auto  right  = centre.translated (spread, 0.0f);

        paintWake (g, s, left, radius, lobes, hot);

        if (s.mode == 0)
            paintEchoes (g, s, left, radius, lobes, displayPhase, hot);

        paintBody (g, s, left, radius, lobes, displayPhase, hot, trail, bodyAlpha);
        paintBody (g, s, right, radius, lobes, displayPhase + s.stereo,
                   cool.withAlpha (0.80f), trail, bodyAlpha * 0.9f);
    }

    paintFan (g, s, centre, radius);
    paintHub (g, centre, baseRadius, smoothedLevel + 0.35f * flash, hot);

    // Phase marker riding the outer graticule.
    {
        const float a = math::twoPi * displayPhase - math::halfPi;
        const auto  pt = polar (centre, a, baseRadius * 1.22f);

        g.setColour (theme::gold.withAlpha (0.30f));
        g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (pt));
        g.setColour (theme::goldLight);
        g.fillEllipse (Rectangle<float> (4.4f, 4.4f).withCentre (pt));
    }

    if (! s.rotary)
        paintTrace (g, s, strip, cool);

    paintReadout (g, s, area);

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

    const float dDepth = -(float) e.getDistanceFromDragStartY() / 200.0f * fine;
    setParam (params::depth, jlimit (0.0f, 1.0f, dragStartDepth + dDepth));

    // Horizontal → Rate, geometrically, so it feels the same at any speed. Only
    // when the LFO is free-running; when synced the division owns the rate.
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

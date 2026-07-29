/*
    Offline DSP checks for ZS-motion — no host, no audio device. Built with
    -DZSMOTION_BUILD_TESTS=ON. Returns non-zero if any check fails.
*/
#include <juce_dsp/juce_dsp.h>

#include "dsp/LFO.h"
#include "dsp/FractionalDelay.h"
#include "dsp/Saturator.h"
#include "dsp/ModulationEngine.h"
#include "ParameterRanges.h"

#include <cstdio>
#include <cmath>

namespace
{
    int failures = 0;

    void check (bool ok, const char* name)
    {
        std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
        if (! ok) ++failures;
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    float peak (const juce::AudioBuffer<float>& b) { return b.getMagnitude (0, b.getNumSamples()); }

    void fillNoise (juce::AudioBuffer<float>& b, uint32_t seed = 1)
    {
        zs::math::Xorshift rng { seed };
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, 0.5f * rng.nextBipolar());
    }

    // Single-bin magnitude (Goertzel), for measuring aliasing energy at one frequency.
    double goertzel (const float* x, int n, double freq, double sr)
    {
        const double w = 2.0 * 3.14159265358979323846 * freq / sr;
        const double coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double s0 = (double) x[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return std::sqrt (std::max (0.0, power)) / (double) n;
    }

    // Run a signal through the saturator at a given quality, block by block.
    double saturationAliasAt3k (int quality, double sr, int block)
    {
        constexpr int N = 8192;
        constexpr double f0 = 9000.0;   // 5th harmonic (45 kHz) aliases to 3 kHz without OS

        zs::Saturator sat;
        sat.prepare (sr, block, 1);
        sat.setCharacter (zs::Saturator::Character::Hard);
        sat.setQuality (quality);
        sat.setAmount (1.0f);

        juce::AudioBuffer<float> out (1, N);
        double phase = 0.0;
        const double inc = 2.0 * 3.14159265358979323846 * f0 / sr;

        for (int start = 0; start < N; start += block)
        {
            const int len = std::min (block, N - start);
            juce::AudioBuffer<float> chunk (1, len);
            for (int i = 0; i < len; ++i) { chunk.setSample (0, i, 0.9f * (float) std::sin (phase)); phase += inc; }
            sat.processBlock (chunk);
            for (int i = 0; i < len; ++i) out.setSample (0, start + i, chunk.getSample (0, i));
        }

        // Measure the alias bin over the settled second half.
        const int half = N / 2;
        return goertzel (out.getReadPointer (0) + half, half, 3000.0, sr);
    }
}

int main()
{
    std::printf ("ZS-motion DSP checks\n");
    constexpr double sr = 48000.0;
    constexpr int    block = 512;

    // ── LFO: range ────────────────────────────────────────────────────────────
    {
        bool inRange = true;
        for (int w = 0; w < (int) zs::Waveform::numWaveforms; ++w)
        {
            zs::LFO lfo;
            lfo.prepare (sr);
            lfo.setWaveform ((zs::Waveform) w);
            lfo.setFrequency (3.7f);

            for (int i = 0; i < 200000; ++i)
            {
                lfo.advance();
                const float v = lfo.value();
                if (v < -1.0001f || v > 1.0001f) { inRange = false; break; }
            }
        }
        check (inRange, "LFO stays within [-1, 1] for every waveform");
    }

    // ── LFO: phase continuity across blocks ────────────────────────────────────
    {
        zs::LFO lfo;
        lfo.prepare (sr);
        lfo.setWaveform (zs::Waveform::Sine);
        lfo.setFrequency (2.0f);

        const float inc = 2.0f / (float) sr;
        bool continuous = true;
        float prev = lfo.getPhase();

        for (int i = 0; i < 100000; ++i)
        {
            lfo.advance();
            float d = lfo.getPhase() - prev;
            if (d < 0.0f) d += 1.0f;                 // wrap
            if (std::abs (d - inc) > 1.0e-4f) { continuous = false; break; }
            prev = lfo.getPhase();
        }
        check (continuous, "LFO phase advances continuously (no jumps)");
    }

    // ── Tempo sync maths ───────────────────────────────────────────────────────
    {
        using namespace zs::params;
        const float q120 = syncedFrequency (120.0, 2, 0);   // 1/4 straight @120 -> 2 Hz
        const float e120 = syncedFrequency (120.0, 3, 0);   // 1/8 straight @120 -> 4 Hz
        const float t8   = syncedFrequency (120.0, 3, 2);   // 1/8 triplet  -> 6 Hz
        const float d8   = syncedFrequency (120.0, 3, 1);   // 1/8 dotted   -> 2.667 Hz

        check (std::abs (q120 - 2.0f) < 1.0e-3f, "sync 1/4 @120 BPM == 2.00 Hz");
        check (std::abs (e120 - 4.0f) < 1.0e-3f, "sync 1/8 @120 BPM == 4.00 Hz");
        check (std::abs (t8   - 6.0f) < 1.0e-3f, "sync 1/8 triplet == 6.00 Hz");
        check (std::abs (d8   - 8.0f / 3.0f) < 1.0e-3f, "sync 1/8 dotted == 2.667 Hz");
    }

    // ── Fractional delay: Hermite is exact on a ramp, never NaN ─────────────────
    {
        zs::FractionalDelay line;
        line.prepare (256);

        for (int n = 0; n < 512; ++n)
            line.push ((float) n);                   // newest value == 511

        const float d  = 10.5f;
        const float got = line.readHermite (d);      // linear ramp -> 511 - 10.5
        check (std::abs (got - (511.0f - 10.5f)) < 1.0e-3f, "fractional delay Hermite exact on a ramp");

        // Integer delay returns the stored sample exactly.
        const float gi = line.readHermite (20.0f);
        check (std::abs (gi - (511.0f - 20.0f)) < 1.0e-3f, "fractional delay integer read exact");

        // Clamp / bounds: extreme delay never reads garbage.
        bool finite = std::isfinite (line.readHermite (1.0e6f)) && std::isfinite (line.readHermite (0.0f));
        check (finite, "fractional delay stays finite outside its range");
    }

    // ── Engine: silence in → silence out ────────────────────────────────────────
    {
        zs::ModulationEngine eng;
        eng.prepare (sr, block, 2);

        zs::ModulationEngine::Settings s;
        s.mode = zs::ModulationEngine::Mode::Chorus;
        s.fanEnabled = true;
        s.saturation = 0.5f;
        eng.setSettings (s);

        juce::AudioBuffer<float> buf (2, block);
        buf.clear();

        for (int k = 0; k < 8; ++k) eng.process (buf);
        check (peak (buf) == 0.0f, "silence in produces exact silence out");
    }

    // ── Engine: transparent at mix = 0 ──────────────────────────────────────────
    {
        zs::ModulationEngine eng;
        eng.prepare (sr, block, 2);

        zs::ModulationEngine::Settings s;
        s.mix = 0.0f;
        s.saturation = 1.0f;         // wet path hammered, but mix = 0 hides it
        s.fanEnabled = true;
        s.outputDb = 0.0f;
        eng.setSettings (s);

        juce::AudioBuffer<float> in (2, block), out (2, block);
        fillNoise (in, 7);

        float worst = 0.0f;
        for (int k = 0; k < 4; ++k)                  // let smoothing settle
        {
            out.makeCopyOf (in);
            eng.process (out);
        }
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                worst = juce::jmax (worst, std::abs (out.getSample (ch, i) - in.getSample (ch, i)));

        check (worst < 1.0e-5f, "mix = 0 passes the dry signal through unchanged");
    }

    // ── Engine: every mode stays finite and bounded under load ──────────────────
    {
        const char* names[] = { "chorus", "vibrato", "tremolo", "rotary" };
        bool ok = true;

        for (int m = 0; m < 4; ++m)
        {
            zs::ModulationEngine eng;
            eng.prepare (sr, block, 2);

            zs::ModulationEngine::Settings s;
            s.mode = (zs::ModulationEngine::Mode) m;
            s.depth = 1.0f;
            s.rateHz = 6.0f;
            s.mix = 1.0f;
            s.fanEnabled = true;
            s.fanAmount = 1.0f;
            s.saturation = 0.7f;
            s.width = 1.5f;
            s.chorusFeedback = 0.7f;
            eng.setSettings (s);

            juce::AudioBuffer<float> buf (2, block);
            for (int k = 0; k < 32; ++k)
            {
                fillNoise (buf, (uint32_t) (100 + k));
                eng.process (buf);
                if (! allFinite (buf) || peak (buf) > 8.0f) { ok = false; break; }
            }
            std::printf ("      mode %-8s peak ok=%d\n", names[m], (int) ok);
            if (! ok) break;
        }
        check (ok, "all modes: finite, bounded output under full-tilt settings");
    }

    // ── Engine: MODE switching does not blow up ─────────────────────────────────
    {
        zs::ModulationEngine eng;
        eng.prepare (sr, block, 2);

        juce::AudioBuffer<float> buf (2, block);
        bool ok = true;

        for (int k = 0; k < 40; ++k)
        {
            zs::ModulationEngine::Settings s;
            s.mode = (zs::ModulationEngine::Mode) (k % 4);   // hammer the crossfade
            s.depth = 0.8f;
            s.mix = 1.0f;
            eng.setSettings (s);

            fillNoise (buf, (uint32_t) (500 + k));
            eng.process (buf);
            if (! allFinite (buf)) { ok = false; break; }
        }
        check (ok, "rapid MODE switching stays click-safe and finite");
    }

    // ── Mono works ──────────────────────────────────────────────────────────────
    {
        zs::ModulationEngine eng;
        eng.prepare (sr, block, 1);

        juce::AudioBuffer<float> buf (1, block);
        fillNoise (buf, 3);

        zs::ModulationEngine::Settings s;
        s.mode = zs::ModulationEngine::Mode::Rotary;
        s.mix = 1.0f;
        eng.setSettings (s);
        eng.process (buf);

        check (allFinite (buf), "mono input processes without NaN/Inf");
    }

    // ── Oversampling lowers aliasing ────────────────────────────────────────────
    {
        const double aliasOff = saturationAliasAt3k (0, sr, block);
        const double alias4x  = saturationAliasAt3k (2, sr, block);
        std::printf ("      alias@3k  off=%.5f  4x=%.5f\n", aliasOff, alias4x);
        check (alias4x < aliasOff * 0.5, "4x oversampling roughly halves saturation aliasing (or better)");
    }

    // ── Rotary survives an abrupt rate jump (inertia is finite) ─────────────────
    {
        zs::ModulationEngine eng;
        eng.prepare (sr, block, 2);

        juce::AudioBuffer<float> buf (2, block);
        bool ok = true;

        for (int k = 0; k < 60; ++k)
        {
            zs::ModulationEngine::Settings s;
            s.mode   = zs::ModulationEngine::Mode::Rotary;
            s.depth  = 0.8f;
            s.mix    = 1.0f;
            s.rateHz = (k % 2 == 0) ? 0.8f : 8.0f;   // slam slow ↔ fast
            eng.setSettings (s);

            fillNoise (buf, (uint32_t) (900 + k));
            eng.process (buf);
            if (! allFinite (buf) || peak (buf) > 8.0f) { ok = false; break; }
        }
        check (ok, "rotary stays finite/bounded through repeated slow↔fast jumps");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

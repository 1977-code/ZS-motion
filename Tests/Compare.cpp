/*
    A measuring bench for modulation plug-ins.

    Built with -DZSMOTION_BUILD_COMPARE=ON:

        ZSmotionCompare <path-to-plugin.vst3 | path-to-plugin.component> [more...]

    Loads each plug-in, lists what it exposes, and measures its behaviour purely
    from the audio it produces — the black-box pass that was missing from this
    project. Nothing here inspects, patches or disassembles a binary; it feeds
    signals in and reads signals out, the way any user could with a DAW and an
    analyser. That is the whole point: it is a fair comparison, not an extraction.

    What it reports
    ---------------
      parameters      every exposed control, its range and default, so two
                      plug-ins' control surfaces can be compared directly
      latency         from an impulse, against what the plug-in claims
      modulation      the output envelope over a few seconds: rate, depth, and the
                      shape of the LFO, recovered from a steady tone
      carrier         the spectrum around a test tone. This is the decisive test
                      for a "fan" effect: true ring modulation removes the carrier
                      and leaves f0 ± fc, while amplitude modulation keeps f0 and
                      puts sidebands either side of it
      harmonics       H2 / H3 / H5 of a 1 kHz tone, for the saturation character

    Trial builds that mute themselves periodically (FAN's demo goes silent for
    five seconds in every sixty) would poison the averages, so every measurement
    window is checked for signal first and reported as skipped if it is silent.
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <cstdio>
#include <cmath>

using namespace juce;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int    blockSize  = 512;

    //==========================================================================
    /** Single-bin magnitude (Goertzel) — cheap and exact for one frequency. */
    double binMagnitude (const float* x, int n, double freq, double sr)
    {
        const double w = 2.0 * MathConstants<double>::pi * freq / sr;
        const double coeff = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double s0 = (double) x[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) * 2.0 / (double) n;
    }

    double rms (const float* x, int n)
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i) sum += (double) x[i] * x[i];
        return std::sqrt (sum / (double) std::max (1, n));
    }

    String dB (double linear)
    {
        if (linear <= 1.0e-9) return "-inf";
        return String (20.0 * std::log10 (linear), 1) + " dB";
    }

    //==========================================================================
    /** Runs a plug-in over a prepared input buffer and returns its output. */
    AudioBuffer<float> run (AudioPluginInstance& plugin, const AudioBuffer<float>& input)
    {
        AudioBuffer<float> output;
        output.makeCopyOf (input);

        MidiBuffer midi;

        for (int start = 0; start < output.getNumSamples(); start += blockSize)
        {
            const int len = jmin (blockSize, output.getNumSamples() - start);
            AudioBuffer<float> chunk (output.getArrayOfWritePointers(),
                                      output.getNumChannels(), start, len);
            plugin.processBlock (chunk, midi);
        }

        return output;
    }

    AudioBuffer<float> makeTone (double freq, int numSamples, float amplitude = 0.5f)
    {
        AudioBuffer<float> buffer (2, numSamples);
        const double inc = 2.0 * MathConstants<double>::pi * freq / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto v = (float) (amplitude * std::sin (inc * i));
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }

        return buffer;
    }

    //==========================================================================
    /** Set a parameter by a fragment of its name, with a normalised 0..1 value.

        Two plug-ins never name things identically, so matching on a fragment is
        what lets both be put into comparable states from one command line. */
    bool setByName (AudioPluginInstance& plugin, const String& fragment, float normalised)
    {
        for (auto* p : plugin.getParameters())
        {
            if (p->getName (64).containsIgnoreCase (fragment))
            {
                p->setValueNotifyingHost (jlimit (0.0f, 1.0f, normalised));
                std::printf ("  set  %-28s -> %s\n",
                             p->getName (28).toRawUTF8(), p->getCurrentValueAsText().toRawUTF8());
                return true;
            }
        }

        std::printf ("  set  %-28s -> NO SUCH PARAMETER\n", fragment.toRawUTF8());
        return false;
    }

    void listParameters (AudioPluginInstance& plugin)
    {
        std::printf ("  parameters (%d):\n", plugin.getParameters().size());

        for (auto* p : plugin.getParameters())
        {
            const auto name = p->getName (28);
            const auto value = p->getCurrentValueAsText();
            const int steps = p->getNumSteps();

            std::printf ("    %-30s = %-14s  default %.3f  %s\n",
                         name.toRawUTF8(),
                         value.isNotEmpty() ? value.toRawUTF8() : "-",
                         p->getDefaultValue(),
                         steps > 0 && steps < 1000 ? (String (steps) + " steps").toRawUTF8() : "continuous");
        }
    }

    void measureLatency (AudioPluginInstance& plugin)
    {
        constexpr int n = 8192;
        constexpr int at = 128;

        AudioBuffer<float> input (2, n);
        input.clear();
        input.setSample (0, at, 1.0f);
        input.setSample (1, at, 1.0f);

        plugin.reset();
        const auto out = run (plugin, input);

        int peak = 0;
        float best = 0.0f;

        for (int i = 0; i < n; ++i)
            if (std::abs (out.getSample (0, i)) > best)
            {
                best = std::abs (out.getSample (0, i));
                peak = i;
            }

        if (best < 1.0e-6f)
        {
            std::printf ("  latency: no output (silent — trial mute?)\n");
            return;
        }

        std::printf ("  latency: reported %d samples, impulse peak at %+d\n",
                     plugin.getLatencySamples(), peak - at);
    }

    /** Recover the modulation envelope of a steady tone: rate, depth, and shape. */
    void measureModulation (AudioPluginInstance& plugin, double toneHz = 1000.0,
                            double seconds = 4.0)
    {
        const int n = (int) (seconds * sampleRate);
        const auto input = makeTone (toneHz, n);

        plugin.reset();
        const auto out = run (plugin, input);

        if (rms (out.getReadPointer (0), n) < 1.0e-5)
        {
            std::printf ("  modulation: silent output — skipped\n");
            return;
        }

        // Envelope by peak-per-window, at 1 ms resolution.
        const int hop = (int) (sampleRate / 1000.0);
        const int frames = n / hop;

        std::vector<float> envelope ((size_t) frames, 0.0f);

        for (int f = 0; f < frames; ++f)
        {
            float peak = 0.0f;
            for (int i = 0; i < hop; ++i)
                peak = jmax (peak, std::abs (out.getSample (0, f * hop + i)));
            envelope[(size_t) f] = peak;
        }

        // Ignore the first 250 ms so smoothing has settled.
        const int from = jmin (250, frames / 4);

        float lo = 1.0e9f, hi = 0.0f;
        for (int f = from; f < frames; ++f)
        {
            lo = jmin (lo, envelope[(size_t) f]);
            hi = jmax (hi, envelope[(size_t) f]);
        }

        // Rate from zero crossings of the envelope about its midpoint.
        const float mid = 0.5f * (lo + hi);
        int crossings = 0;
        bool above = envelope[(size_t) from] > mid;

        for (int f = from + 1; f < frames; ++f)
        {
            const bool now = envelope[(size_t) f] > mid;
            if (now != above) { ++crossings; above = now; }
        }

        const double span = (double) (frames - from) / 1000.0;
        const double rate = span > 0.0 ? crossings / (2.0 * span) : 0.0;

        std::printf ("  modulation: envelope %.4f … %.4f (%s of swing), rate ~%.2f Hz\n",
                     lo, hi, dB (hi > 0.0f ? lo / hi : 0.0).toRawUTF8(), rate);
    }

    /** Ring modulation removes the carrier; amplitude modulation keeps it. */
    void measureCarrier (AudioPluginInstance& plugin, double toneHz = 1000.0)
    {
        const int n = (int) (2.0 * sampleRate);
        const auto input = makeTone (toneHz, n);

        plugin.reset();
        const auto out = run (plugin, input);

        const auto* data = out.getReadPointer (0) + n / 4;      // skip the onset
        const int len = n / 2;

        if (rms (data, len) < 1.0e-5)
        {
            std::printf ("  carrier: silent output — skipped\n");
            return;
        }

        const double atTone = binMagnitude (data, len, toneHz, sampleRate);

        std::printf ("  carrier: level at %.0f Hz = %s\n", toneHz, dB (atTone).toRawUTF8());
        std::printf ("           sidebands:");

        for (double offset : { 3.0, 6.0, 12.0, 25.0, 50.0 })
        {
            const double up = binMagnitude (data, len, toneHz + offset, sampleRate);
            const double dn = binMagnitude (data, len, toneHz - offset, sampleRate);
            std::printf ("  ±%.0f: %s", offset, dB (0.5 * (up + dn)).toRawUTF8());
        }

        std::printf ("\n");
    }

    void measureHarmonics (AudioPluginInstance& plugin, double toneHz = 1000.0)
    {
        const int n = (int) (2.0 * sampleRate);
        const auto input = makeTone (toneHz, n, 0.8f);

        plugin.reset();
        const auto out = run (plugin, input);

        const auto* data = out.getReadPointer (0) + n / 4;
        const int len = n / 2;

        if (rms (data, len) < 1.0e-5)
        {
            std::printf ("  harmonics: silent output — skipped\n");
            return;
        }

        const double f0 = binMagnitude (data, len, toneHz, sampleRate);

        std::printf ("  harmonics (relative to fundamental):");

        for (int h : { 2, 3, 4, 5, 7 })
        {
            const double m = binMagnitude (data, len, toneHz * h, sampleRate);
            std::printf ("  H%d %s", h, dB (f0 > 0.0 ? m / f0 : 0.0).toRawUTF8());
        }

        std::printf ("\n");
    }

    //==========================================================================
    void examine (const String& path, const StringPairArray& assignments)
    {
        std::printf ("\n================================================================\n");
        std::printf ("%s\n", path.toRawUTF8());
        std::printf ("================================================================\n");

        const File file (path);

        if (! file.exists())
        {
            std::printf ("  not found\n");
            return;
        }

        // Registered explicitly rather than through addDefaultFormats(), which the
        // headless build of juce_audio_processors deletes — and we only ever want
        // these two anyway.
        AudioPluginFormatManager formats;

       #if JUCE_PLUGINHOST_VST3
        formats.addFormat (new VST3PluginFormat());
       #endif
       #if JUCE_PLUGINHOST_AU && JUCE_MAC
        formats.addFormat (new AudioUnitPluginFormat());
       #endif

        OwnedArray<PluginDescription> found;

        for (int i = 0; i < formats.getNumFormats(); ++i)
        {
            auto* format = formats.getFormat (i);

            if (format->fileMightContainThisPluginType (path))
                format->findAllTypesForFile (found, path);
        }

        if (found.isEmpty())
        {
            std::printf ("  no plug-in could be loaded from this path\n");
            return;
        }

        for (auto* description : found)
        {
            String error;
            auto plugin = formats.createPluginInstance (*description, sampleRate, blockSize, error);

            if (plugin == nullptr)
            {
                std::printf ("  %s: could not instantiate — %s\n",
                             description->pluginFormatName.toRawUTF8(), error.toRawUTF8());
                continue;
            }

            std::printf ("\n  %s  %s  by %s  (%s)\n",
                         description->name.toRawUTF8(),
                         description->version.toRawUTF8(),
                         description->manufacturerName.toRawUTF8(),
                         description->pluginFormatName.toRawUTF8());

            plugin->enableAllBuses();
            plugin->setPlayConfigDetails (2, 2, sampleRate, blockSize);
            plugin->prepareToPlay (sampleRate, blockSize);

            listParameters (*plugin);
            std::printf ("\n");

            if (assignments.size() > 0)
            {
                for (const auto& key : assignments.getAllKeys())
                    setByName (*plugin, key, assignments[key].getFloatValue());

                std::printf ("\n");
            }

            measureLatency (*plugin);
            measureModulation (*plugin);
            measureCarrier (*plugin);
            measureHarmonics (*plugin);

            plugin->releaseResources();
        }
    }
}

//==============================================================================
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juceInit;

    StringArray paths;
    StringPairArray assignments;

    for (int i = 1; i < argc; ++i)
    {
        const String argument (argv[i]);

        // --set "Name Fragment=0.75"  — normalised value, name matched loosely.
        if (argument.startsWith ("--set") && i + 1 < argc)
        {
            const String pair (argv[++i]);
            assignments.set (pair.upToFirstOccurrenceOf ("=", false, false).trim(),
                             pair.fromFirstOccurrenceOf ("=", false, false).trim());
        }
        else
        {
            paths.add (argument);
        }
    }

    if (paths.isEmpty())
    {
        std::printf ("usage: ZSmotionCompare [--set \"Param=0.5\"]... <plugin path> [more paths...]\n\n"
                     "Measures modulation plug-ins from their audio alone. Run it once with no\n"
                     "--set to see the parameter list, then again with --set to put the plug-in\n"
                     "into the state you want to measure.\n\n"
                     "Values are normalised 0..1. Names match on a fragment, case-insensitive.\n\n"
                     "Typical macOS locations:\n"
                     "  /Library/Audio/Plug-Ins/VST3/FAN.vst3\n"
                     "  /Library/Audio/Plug-Ins/Components/FAN.component\n");
        return 1;
    }

    std::printf ("ZS-motion measuring bench — %.0f Hz, %d-sample blocks\n",
                 sampleRate, blockSize);
    std::printf ("Note: the envelope rate below is only meaningful for amplitude\n"
                 "modulation — put the plug-in in a tremolo mode to read the LFO.\n");

    for (const auto& path : paths)
        examine (path, assignments);

    std::printf ("\nNote: a plug-in running in trial mode may mute itself periodically;\n"
                 "any window that came out silent is reported as skipped above.\n");

    return 0;
}

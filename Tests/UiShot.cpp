/*
    Renders the editor straight to PNG, with no window and no screen involved.

    Built with -DZSMOTION_BUILD_SHOT=ON. It spins the plug-in up, feeds it a little
    noise so the meters and the rotor are in a lifelike state, pumps the message
    loop so the interface's timers run, then snapshots the editor at its logical
    size. Handy for eyeballing a layout change without loading a host, and for
    keeping a visual record of the interface next to the code.

        ZSmotionShot <output-directory>
*/
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "utils/MathUtils.h"

namespace
{
    void setParam (ZsMotionAudioProcessor& p, const char* id, float plain)
    {
        if (auto* param = p.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    }

    /** Run the plug-in and the interface together for a while. */
    void pump (ZsMotionAudioProcessor& processor, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        zs::math::Xorshift rng { 0x51D5u };

        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample (ch, i, 0.34f * rng.nextBipolar());

            processor.processBlock (buffer, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (14);
        }
    }

    // Long enough for the wake to fill and the flow to reach the rim.
    void shoot (const juce::File& outputDirectory, const juce::String& name,
                const std::function<void (ZsMotionAudioProcessor&)>& configure,
                int blocks = 110)
    {
        ZsMotionAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        configure (processor);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setSize (zs::layout::width, zs::layout::height);

        pump (processor, blocks);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true);

        const auto file = outputDirectory.getChildFile (name + ".png");
        file.deleteFile();

        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
        {
            juce::PNGImageFormat png;
            png.writeImageToStream (image, *stream);
        }

        std::printf ("  wrote %s\n", file.getFullPathName().toRawUTF8());

        processor.releaseResources();
    }
}

namespace
{
    int failures = 0;

    void expect (bool ok, const char* what)
    {
        std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok) ++failures;
    }

    /** The processor only exists in this tool, so the state and preset round-trips
        are checked here rather than in the DSP-only test binary. */
    void checkStateAndPresets()
    {
        std::printf ("Integration checks\n");

        namespace ids = zs::params;

        // ── Plug-in state survives a save and restore ───────────────────────
        {
            ZsMotionAudioProcessor a;
            setParam (a, ids::depth, 0.77f);
            setParam (a, ids::rate, 4.25f);
            setParam (a, ids::mode, 2);
            setParam (a, ids::satQuality, 2);

            juce::MemoryBlock blob;
            a.getStateInformation (blob);

            ZsMotionAudioProcessor b;
            b.setStateInformation (blob.getData(), (int) blob.getSize());

            const auto get = [] (ZsMotionAudioProcessor& p, const char* id)
            {
                return p.apvts.getRawParameterValue (id)->load();
            };

            const bool same = std::abs (get (b, ids::depth) - 0.77f) < 1.0e-4f
                           && std::abs (get (b, ids::rate) - 4.25f) < 1.0e-3f
                           && (int) get (b, ids::mode) == 2
                           && (int) get (b, ids::satQuality) == 2;

            expect (same, "plug-in state restores every parameter");
        }

        // ── A user preset round-trips through disk ──────────────────────────
        {
            ZsMotionAudioProcessor p;

            const juce::String name = "__zsmotion_selfcheck__";
            const auto file = zs::PresetManager::getUserDirectory()
                                .getChildFile (juce::File::createLegalFileName (name) + ".xml");
            file.deleteFile();

            setParam (p, ids::depth, 0.31f);
            setParam (p, ids::width, 1.75f);
            setParam (p, ids::fanEnabled, 1.0f);

            const int saved = p.presets.saveUserPreset (name);
            expect (saved >= 0 && file.existsAsFile(), "user preset writes to disk");
            expect (! p.presets.isDirty(), "saving clears the modified marker");

            // Move away, then come back to it.
            p.presets.load (0);
            expect (std::abs (p.apvts.getRawParameterValue (ids::depth)->load() - 0.31f) > 0.01f,
                    "loading another preset actually changes the settings");

            p.presets.load (saved);

            const bool restored = std::abs (p.apvts.getRawParameterValue (ids::depth)->load() - 0.31f) < 1.0e-4f
                               && std::abs (p.apvts.getRawParameterValue (ids::width)->load() - 1.75f) < 1.0e-4f
                               && p.apvts.getRawParameterValue (ids::fanEnabled)->load() > 0.5f;

            expect (restored, "user preset restores what was saved");
            expect (p.presets.isUserPreset (saved), "a saved preset is reported as a user preset");

            // Touching anything marks it altered.
            setParam (p, ids::mix, 0.123f);
            expect (p.presets.isDirty(), "changing a parameter marks the preset modified");

            expect (p.presets.deleteUserPreset (saved) && ! file.existsAsFile(),
                    "user preset can be deleted again");
        }

        std::printf ("\n");
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outputDirectory = argc > 1
        ? juce::File (juce::String (argv[1])).getFullPathName()
        : juce::File::getCurrentWorkingDirectory();

    outputDirectory.createDirectory();

    checkStateAndPresets();

    std::printf ("ZS-motion interface snapshots\n");

    using P = ZsMotionAudioProcessor;
    namespace ids = zs::params;

    // The face of the plug-in as it opens.
    shoot (outputDirectory, "ui-default", [] (P&) {});

    // A square LFO at full depth — the rotor should be a stepped cog.
    shoot (outputDirectory, "ui-square", [] (P& p)
    {
        setParam (p, ids::waveform, 4);
        setParam (p, ids::depth, 0.9f);
        setParam (p, ids::rate, 3.0f);
        setParam (p, ids::width, 1.6f);
        setParam (p, ids::mix, 1.0f);
    });

    // Rotary: two rotors, horn outside and drum inside.
    shoot (outputDirectory, "ui-rotary", [] (P& p)
    {
        setParam (p, ids::mode, 3);
        setParam (p, ids::depth, 0.8f);
        setParam (p, ids::rate, 6.0f);
        setParam (p, ids::mix, 1.0f);
    });

    // The Fan, driven hot, with the blades strobing over the body and the flow.
    shoot (outputDirectory, "ui-fan", [] (P& p)
    {
        setParam (p, ids::waveform, 1);
        setParam (p, ids::depth, 0.6f);
        setParam (p, ids::fanEnabled, 1.0f);
        setParam (p, ids::fanAmount, 0.75f);
        setParam (p, ids::fanBlades, 6);
        setParam (p, ids::fanShape, 2);
        setParam (p, ids::fanResonance, 0.6f);
        setParam (p, ids::saturation, 0.8f);
        setParam (p, ids::satQuality, 2);
        setParam (p, ids::syncEnabled, 1.0f);
    });

    // Hard clipping should visibly flatten the lobe tips into a stepped rim.
    shoot (outputDirectory, "ui-drive-hard", [] (P& p)
    {
        setParam (p, ids::depth, 0.85f);
        setParam (p, ids::saturation, 1.0f);
        setParam (p, ids::satCharacter, 2);       // Hard
        setParam (p, ids::satQuality, 2);
        setParam (p, ids::mix, 1.0f);
        setParam (p, ids::rate, 2.0f);
    });

    // Asymmetric should make the figure lopsided.
    shoot (outputDirectory, "ui-drive-asym", [] (P& p)
    {
        setParam (p, ids::depth, 0.85f);
        setParam (p, ids::saturation, 0.95f);
        setParam (p, ids::satCharacter, 1);       // Asymmetric
        setParam (p, ids::mix, 1.0f);
        setParam (p, ids::rate, 2.0f);
    });

    // Mix almost dry: the dashed reference circle should dominate and the body fade.
    shoot (outputDirectory, "ui-mix-dry", [] (P& p)
    {
        setParam (p, ids::mix, 0.04f);
        setParam (p, ids::depth, 0.7f);
    });

    // Heavy chorus feedback: echo copies around the body.
    shoot (outputDirectory, "ui-feedback", [] (P& p)
    {
        setParam (p, ids::chorusFeedback, 0.85f);
        setParam (p, ids::depth, 0.6f);
        setParam (p, ids::mix, 1.0f);
    });

    // Fast and deep: the flow should be dense and strongly swirled.
    shoot (outputDirectory, "ui-fast", [] (P& p)
    {
        setParam (p, ids::rate, 9.0f);
        setParam (p, ids::depth, 1.0f);
        setParam (p, ids::mix, 1.0f);
        setParam (p, ids::width, 1.8f);
        setParam (p, ids::output, 9.0f);          // figure grows against the dial
    });

    if (failures > 0)
        std::printf ("\n%d integration check%s FAILED\n", failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}

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

    void shoot (const juce::File& outputDirectory, const juce::String& name,
                const std::function<void (ZsMotionAudioProcessor&)>& configure,
                int blocks = 26)
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

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outputDirectory = argc > 1
        ? juce::File (juce::String (argv[1])).getFullPathName()
        : juce::File::getCurrentWorkingDirectory();

    outputDirectory.createDirectory();

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

    // The Fan, driven hot, with the blades strobing over the body.
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

    return 0;
}

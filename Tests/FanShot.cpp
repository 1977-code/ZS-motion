/*
    Renders the ZS-MOTION-FAN editor straight to PNG, with no window and no screen.

    Built with -DZSMOTION_BUILD_SHOT=ON:

        ZSmotionFanShot <output-directory>

    A separate target from ZSmotionShot on purpose: each plug-in defines its own
    createPluginFilter(), so the two cannot be linked into one console app.

    Pass an ASCII output path — a non-ASCII one is mangled through argv on this
    machine.
*/
#include <juce_audio_utils/juce_audio_utils.h>

#include "fan/FanProcessor.h"
#include "fan/FanEditor.h"
#include "utils/MathUtils.h"

namespace
{
    void setParam (ZsFanAudioProcessor& p, const char* id, float plain)
    {
        if (auto* param = p.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    }

    /** Run plug-in and interface together, so the sweep and level are lifelike. */
    void pump (ZsFanAudioProcessor& processor, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        zs::math::Xorshift rng { 0x7A11u };

        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample (ch, i, 0.34f * rng.nextBipolar());

            processor.processBlock (buffer, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (14);
        }
    }

    void shoot (const juce::File& directory, const juce::String& name,
                const std::function<void (ZsFanAudioProcessor&)>& configure,
                int blocks = 70)
    {
        ZsFanAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        configure (processor);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setSize (zs::fanlayout::width, zs::fanlayout::height);

        pump (processor, blocks);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true);
        const auto file = directory.getChildFile (name + ".png");
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

    const juce::File directory = argc > 1 ? juce::File (juce::String (argv[1])).getFullPathName()
                                         : juce::File::getCurrentWorkingDirectory();
    directory.createDirectory();

    std::printf ("ZS-MOTION-FAN interface snapshots\n");

    namespace ids = zs::fanparams;
    using P = ZsFanAudioProcessor;

    // As it opens: chorus, fan off, so the carrier trace sits dim.
    shoot (directory, "fan-default", [] (P&) {});

    // The fan running on its own — mode None, which is what the reference can only
    // do by un-lighting its mode button.
    shoot (directory, "fan-alone", [] (P& p)
    {
        setParam (p, ids::mode, 0);          // None
        setParam (p, ids::fanOn, 1.0f);
        setParam (p, ids::depth, 0.7f);
        setParam (p, ids::rateHz, 6.0f);
        setParam (p, ids::mix, 0.5f);
    });

    // Morph up into the hard end of the shape chain, driven, wide.
    shoot (directory, "fan-square", [] (P& p)
    {
        setParam (p, ids::mode, 4);          // Tremolo
        setParam (p, ids::morph, 1.0f);
        setParam (p, ids::depth, 0.9f);
        setParam (p, ids::rateHz, 3.0f);
        setParam (p, ids::saturation, 0.6f);
        setParam (p, ids::width, 1.6f);
        setParam (p, ids::syncOn, 1.0f);
    });

    return 0;
}

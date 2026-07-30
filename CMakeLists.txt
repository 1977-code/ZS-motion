cmake_minimum_required(VERSION 3.22)

# ─── macOS build settings (must be set before project()) ────────────────────
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
endif ()

project(ZSmotion VERSION 0.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ZSMOTION_COPY_AFTER_BUILD
       "Copy the built plug-ins into the user plug-in folders after building" ON)

add_subdirectory(JUCE)

# ─── The plug-in ────────────────────────────────────────────────────────────
juce_add_plugin(ZSmotion
    COMPANY_NAME              "ZS Records"
    COMPANY_WEBSITE           "https://zsr.artspace1977.ru"
    BUNDLE_ID                 "ru.zsrecords.zsmotion"
    PLUGIN_MANUFACTURER_CODE  ZSRc
    PLUGIN_CODE               Zmot
    FORMATS                   AU VST3 Standalone
    PRODUCT_NAME              "ZS-motion"
    DESCRIPTION               "Kinetic modulation effect"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   ${ZSMOTION_COPY_AFTER_BUILD}
    VST3_CATEGORIES           Fx Modulation
    AU_MAIN_TYPE              kAudioUnitType_Effect
    NEEDS_WEB_BROWSER         FALSE
    NEEDS_CURL                FALSE
    # JUCE turns this into the .icns and .ico the bundles need. Generated from the
    # brand theme by the ZSmotionArt target.
    ICON_BIG                  "Resources/brand/icon.png")

# ─── Embedded brand fonts (shared with the rest of the ZS Records line) ──────
juce_add_binary_data(ZSmotionAssets
    NAMESPACE ZsAssets
    SOURCES
        Resources/Fonts/MontserratBold.ttf
        Resources/Fonts/MontserratMedium.ttf
        Resources/Fonts/InterMedium.ttf)

set_target_properties(ZSmotionAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(ZSmotion
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/dsp/ModulationEngine.cpp
        Source/gui/Theme.cpp
        Source/gui/ZsLookAndFeel.cpp
        Source/gui/BrandBackground.cpp
        Source/gui/RotorView.cpp
        Source/gui/ZsKnob.cpp)

target_include_directories(ZSmotion PRIVATE Source)

target_compile_definitions(ZSmotion
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
        # Snap denormals to zero inside the juce_dsp filters we call per sample.
        JUCE_DSP_ENABLE_SNAP_TO_ZERO=1)

target_link_libraries(ZSmotion
    PRIVATE
        ZSmotionAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ─── ZS-MOTION-FAN: the short-reach sibling ─────────────────────────────────
# Same code base, opposite intent: ten controls, a genuine ring-modulating fan and
# a tight chorus, all built to what the reference plug-in measures rather than to
# its documentation. Ships in the same installer as ZS-motion.
juce_add_plugin(ZSmotionFan
    COMPANY_NAME              "ZS Records"
    COMPANY_WEBSITE           "https://zsr.artspace1977.ru"
    BUNDLE_ID                 "ru.zsrecords.zsmotionfan"
    PLUGIN_MANUFACTURER_CODE  ZSRc
    PLUGIN_CODE               Zmfn
    FORMATS                   AU VST3 Standalone
    PRODUCT_NAME              "ZS-MOTION-FAN"
    DESCRIPTION               "Ring-modulation fan"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   ${ZSMOTION_COPY_AFTER_BUILD}
    VST3_CATEGORIES           Fx Modulation
    AU_MAIN_TYPE              kAudioUnitType_Effect
    NEEDS_WEB_BROWSER         FALSE
    NEEDS_CURL                FALSE
    ICON_BIG                  "Resources/brand/icon.png")

target_sources(ZSmotionFan
    PRIVATE
        Source/fan/FanProcessor.cpp
        Source/fan/FanEditor.cpp
        Source/fan/FanEngine.cpp
        Source/gui/Theme.cpp
        Source/gui/ZsLookAndFeel.cpp
        Source/gui/ZsKnob.cpp)

target_include_directories(ZSmotionFan PRIVATE Source)

target_compile_definitions(ZSmotionFan
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_DSP_ENABLE_SNAP_TO_ZERO=1)

target_link_libraries(ZSmotionFan
    PRIVATE
        ZSmotionAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ─── Offline DSP checks ─────────────────────────────────────────────────────
option(ZSMOTION_BUILD_TESTS "Build the offline DSP checks" OFF)

if (ZSMOTION_BUILD_TESTS)
    juce_add_console_app(ZSmotionTests PRODUCT_NAME "ZS-motion Tests")

    target_sources(ZSmotionTests
        PRIVATE
            Tests/DspTests.cpp
            Source/dsp/ModulationEngine.cpp)

    target_include_directories(ZSmotionTests PRIVATE Source)

    target_compile_definitions(ZSmotionTests
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1
            JUCE_DSP_ENABLE_SNAP_TO_ZERO=1)

    target_link_libraries(ZSmotionTests
        PRIVATE
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME dsp COMMAND ZSmotionTests)
endif ()

# ─── Offscreen interface snapshots ──────────────────────────────────────────
# Renders the editor to PNG without a window, so a layout can be checked without
# loading a host. Off by default; it pulls the GUI modules into a console app.
# ─── Brand artwork generator ────────────────────────────────────────────────
# Renders the icon and the installer graphics from the same theme code the
# interface uses, so they cannot drift from the plug-in. Off by default; the
# generated files are committed, so a normal build never needs it.
option(ZSMOTION_BUILD_ART "Build the brand artwork generator" OFF)

if (ZSMOTION_BUILD_ART)
    juce_add_console_app(ZSmotionArt PRODUCT_NAME "ZS-motion Art")

    target_sources(ZSmotionArt
        PRIVATE
            Tests/BrandArt.cpp
            Source/gui/Theme.cpp)

    target_include_directories(ZSmotionArt PRIVATE Source)

    target_compile_definitions(ZSmotionArt
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1)

    target_link_libraries(ZSmotionArt
        PRIVATE
            ZSmotionAssets
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif ()

# ─── Measuring bench ────────────────────────────────────────────────────────
# Hosts any VST3/AU and measures it from its audio alone, so this plug-in can be
# compared against others fairly. Off by default.
option(ZSMOTION_BUILD_COMPARE "Build the plug-in measuring bench" OFF)

if (ZSMOTION_BUILD_COMPARE)
    juce_add_console_app(ZSmotionCompare PRODUCT_NAME "ZS-motion Compare")

    target_sources(ZSmotionCompare PRIVATE Tests/Compare.cpp)

    target_compile_definitions(ZSmotionCompare
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1
            JUCE_PLUGINHOST_VST3=1
            JUCE_PLUGINHOST_AU=1)

    target_link_libraries(ZSmotionCompare
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif ()

option(ZSMOTION_BUILD_SHOT "Build the offscreen interface snapshot tool" OFF)

if (ZSMOTION_BUILD_SHOT)
    juce_add_console_app(ZSmotionShot PRODUCT_NAME "ZS-motion Shot")

    target_sources(ZSmotionShot
        PRIVATE
            Tests/UiShot.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/dsp/ModulationEngine.cpp
            Source/gui/Theme.cpp
            Source/gui/ZsLookAndFeel.cpp
            Source/gui/BrandBackground.cpp
            Source/gui/RotorView.cpp
            Source/gui/ZsKnob.cpp)

    target_include_directories(ZSmotionShot PRIVATE Source)

    target_compile_definitions(ZSmotionShot
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            JUCE_DSP_ENABLE_SNAP_TO_ZERO=1
            # The plug-in wrapper normally supplies these.
            JucePlugin_Name="ZS-motion"
            JucePlugin_VersionString="${PROJECT_VERSION}")

    target_link_libraries(ZSmotionShot
        PRIVATE
            ZSmotionAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif ()

cmake_minimum_required(VERSION 3.22)

project(Sonder VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Статический рантайм: плагину не нужен установленный VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(external/JUCE)

juce_add_plugin(Sonder
    COMPANY_NAME "Sonder"
    PRODUCT_NAME "Sonder"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Sndr
    PLUGIN_CODE Snd1
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone)

target_sources(Sonder
    PRIVATE
        Source/Parameters.cpp
        Source/Parameters.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/ScopeBuffer.h
        Source/dsp/AnalogEnvelope.h
        Source/dsp/FastRandom.h
        Source/dsp/LadderFilter.h
        Source/dsp/Lfo.h
        Source/dsp/PolyBlepOscillator.h
        Source/dsp/Saturation.h
        Source/dsp/SmoothNoise.h
        Source/fx/Chorus.h
        Source/fx/TapeDelay.h
        Source/presets/FactoryPresets.cpp
        Source/presets/FactoryPresets.h
        Source/presets/PresetManager.cpp
        Source/presets/PresetManager.h
        Source/synth/SynthParams.h
        Source/synth/Voice.cpp
        Source/synth/Voice.h
        Source/synth/VoiceManager.cpp
        Source/synth/VoiceManager.h
        Source/ui/Controls.cpp
        Source/ui/Controls.h
        Source/ui/MainView.cpp
        Source/ui/MainView.h
        Source/ui/PresetBar.cpp
        Source/ui/PresetBar.h
        Source/ui/ScopeView.cpp
        Source/ui/ScopeView.h
        Source/ui/SonderLookAndFeel.cpp
        Source/ui/SonderLookAndFeel.h)

target_include_directories(Sonder PRIVATE Source)

target_compile_definitions(Sonder
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Sonder
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Sonder_Standalone)

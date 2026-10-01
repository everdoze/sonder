cmake_minimum_required(VERSION 3.22)

project(Sonder VERSION 1.1.0 LANGUAGES C CXX)

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
        Source/dsp/Distortion.h
        Source/dsp/FastRandom.h
        Source/dsp/FormantFilter.h
        Source/dsp/LadderFilter.h
        Source/dsp/LfoShapes.cpp
        Source/dsp/LfoShapes.h
        Source/dsp/NoiseGenerator.h
        Source/dsp/PolyBlepOscillator.h
        Source/dsp/Saturation.h
        Source/dsp/SmoothNoise.h
        Source/dsp/Wavetable.cpp
        Source/dsp/Wavetable.h
        Source/dsp/WavetableBank.cpp
        Source/dsp/WavetableBank.h
        Source/fx/Chorus.h
        Source/fx/Compressor.h
        Source/fx/Equalizer.h
        Source/fx/FilterFx.h
        Source/fx/Flanger.h
        Source/fx/FreqShifter.h
        Source/fx/FxChain.h
        Source/fx/FxRack.h
        Source/fx/FxRackController.cpp
        Source/fx/FxRackController.h
        Source/fx/FxTypes.cpp
        Source/fx/FxTypes.h
        Source/fx/Limiter.h
        Source/fx/MasterDistortion.h
        Source/fx/Multiband.h
        Source/fx/Phaser.h
        Source/fx/Reverb.h
        Source/fx/TapeDelay.h
        Source/fx/Tremolo.h
        Source/fx/Widener.h
        Source/presets/FactoryPresets.cpp
        Source/presets/FactoryPresets.h
        Source/presets/PresetManager.cpp
        Source/presets/PresetManager.h
        Source/synth/Arpeggiator.cpp
        Source/synth/Arpeggiator.h
        Source/synth/ModRouting.h
        Source/synth/SynthParams.h
        Source/synth/Tuning.cpp
        Source/synth/Tuning.h
        Source/synth/Voice.cpp
        Source/synth/Voice.h
        Source/synth/VoiceManager.cpp
        Source/synth/VoiceManager.h
        Source/ui/Controls.cpp
        Source/ui/Controls.h
        Source/ui/EffectView.cpp
        Source/ui/EffectView.h
        Source/ui/FilterPanel.cpp
        Source/ui/FilterPanel.h
        Source/ui/FilterView.cpp
        Source/ui/FilterView.h
        Source/ui/FxRackView.cpp
        Source/ui/FxRackView.h
        Source/ui/LfoEditor.cpp
        Source/ui/LfoEditor.h
        Source/ui/LfoPanel.cpp
        Source/ui/LfoPanel.h
        Source/ui/MainView.cpp
        Source/ui/OffscreenGl.cpp
        Source/ui/OffscreenGl.h
        Source/ui/MainView.h
        Source/ui/OscPanel.cpp
        Source/ui/OscPanel.h
        Source/ui/OscillatorView.cpp
        Source/ui/OscillatorView.h
        Source/ui/PlayPanels.cpp
        Source/ui/PlayPanels.h
        Source/ui/PresetBar.cpp
        Source/ui/PresetBar.h
        Source/ui/ScopeView.cpp
        Source/ui/ScopeView.h
        Source/ui/SonderLookAndFeel.cpp
        Source/ui/ShaderStage.cpp
        Source/ui/ShaderStage.h
        Source/ui/SonderLookAndFeel.h
        Source/ui/Theme.cpp
        Source/ui/Theme.h
        Source/ui/Visuals.cpp
        Source/ui/Visuals.h)

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
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Sonder_Standalone)

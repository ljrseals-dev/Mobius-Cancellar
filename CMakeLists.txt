cmake_minimum_required(VERSION 3.22)

project(MobiusSpectralCanceller VERSION 1.0.0)

# --------------------------------------------------------------------
# JUCE
# --------------------------------------------------------------------
# Option A – system / already-fetched JUCE
# find_package(JUCE CONFIG REQUIRED)

# Option B – FetchContent (recommended for a self-contained repo)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4          # or latest 8.x
)
FetchContent_MakeAvailable(JUCE)

# --------------------------------------------------------------------
# Plugin target
# --------------------------------------------------------------------
juce_add_plugin(MobiusSpectralCanceller
    COMPANY_NAME            "Modular Waveform Mechanics"
    PLUGIN_MANUFACTURER_CODE MWM1
    PLUGIN_CODE             Msc1
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "Möbius Spectral Canceller"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(MobiusSpectralCanceller
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(MobiusSpectralCanceller
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MobiusSpectralCanceller
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Make the Möbius table header visible
target_include_directories(MobiusSpectralCanceller
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

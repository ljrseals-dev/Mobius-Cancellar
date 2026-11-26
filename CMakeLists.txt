cmake_minimum_required(VERSION 3.22)

project(MobiusSpectralCanceller VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MobiusSpectralCanceller
    COMPANY_NAME             "Modular Waveform Mechanics"
    PLUGIN_MANUFACTURER_CODE MWM1
    PLUGIN_CODE              Msc1
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "Möbius Spectral Canceller"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
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

target_include_directories(MobiusSpectralCanceller PRIVATE Source)

target_link_libraries(MobiusSpectralCanceller
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(MSC_BUILD_TESTS "Build the MSC DSP regression tests" ON)
if (MSC_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(MobiusDspTests PRODUCT_NAME "Mobius DSP Tests")
    target_sources(MobiusDspTests PRIVATE
        tests/MobiusDspTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )
    target_include_directories(MobiusDspTests PRIVATE Source)
    target_compile_definitions(MobiusDspTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
    target_link_libraries(MobiusDspTests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME msc_dsp_identity_and_dry COMMAND MobiusDspTests)
endif()

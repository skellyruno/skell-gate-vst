cmake_minimum_required(VERSION 3.22)
project(SkellGlitch VERSION 1.4.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(SKELL_DSP_ONLY "Only build the dependency-free DSP tests" OFF)
option(SKELL_BUILD_TESTS "Build DSP regression tests" ON)
if(SKELL_BUILD_TESTS)
    enable_testing()
    add_executable(skell_dsp_tests tests/DSPTests.cpp)
    target_include_directories(skell_dsp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME skell_dsp_tests COMMAND skell_dsp_tests)
endif()
if(SKELL_DSP_ONLY)
    return()
endif()

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, an installed JUCE,
# or download a pinned release at configuration time.
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
else()
    find_package(JUCE 8 CONFIG QUIET)
    if(NOT JUCE_FOUND)
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

juce_add_plugin(SkellGlitch
    COMPANY_NAME "Skelly"
    BUNDLE_ID "uno.skellyr.skellglitch"
    PLUGIN_MANUFACTURER_CODE SkLy
    PLUGIN_CODE SkGl
    PRODUCT_NAME "Skell-Glitch"
    PLUGIN_NAME "Skell-Glitch"
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SkellGlitch PRIVATE
    PluginProcessor.h PluginProcessor.cpp PluginEditor.h PluginEditor.cpp
    ui/Theme.h ui/LookAndFeel.h ui/LookAndFeel.cpp
    ui/TopBar.h ui/TopBar.cpp
    ui/LoopRow.h ui/LoopRow.cpp
    ui/GlitchKnob.h ui/GlitchKnob.cpp
    ui/WaveDisplay.h ui/WaveDisplay.cpp
    ui/GateGrid.h ui/GateGrid.cpp
    ui/ModeRow.h ui/ModeRow.cpp
    ui/BandSlider.h ui/BandSlider.cpp
    ui/PresetBar.h ui/PresetBar.cpp
    dsp/TempoClock.h dsp/SliceCapture.h dsp/Reverser.h
    dsp/GateSequencer.h dsp/GateShaper.h dsp/BandSplitter.h
    dsp/Crossfader.h dsp/TriggerRouter.h dsp/GlitchRandomizer.h)

target_compile_definitions(SkellGlitch PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SkellGlitch PRIVATE
    juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
# Original vector mark is embedded; no third-party or redistributable font files are included.
juce_add_binary_data(SkellGlitchAssets SOURCES assets/original_mark.svg)
target_link_libraries(SkellGlitch PRIVATE SkellGlitchAssets)
juce_generate_juce_header(SkellGlitch)

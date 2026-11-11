cmake_minimum_required(VERSION 3.22)
project(SkellGlitch VERSION 1.7.2 LANGUAGES CXX)
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

# SkellCore Studio skin, shared appearance storage and official skull geometry.
# Pin to a reviewed immutable commit, so identical builds use identical assets.
set(SKELLCORE_COMMIT "d02d50849123f28bbc2e32cded0e78aced15afb3")
if(DEFINED SKELLCORE_DIR AND EXISTS "${SKELLCORE_DIR}/SkellCore/CMakeLists.txt")
    add_subdirectory("${SKELLCORE_DIR}/SkellCore" "${CMAKE_BINARY_DIR}/skellcore" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(SkellCoreRoot
        GIT_REPOSITORY https://github.com/skellyruno/skellcore-vst-template.git
        GIT_TAG ${SKELLCORE_COMMIT}
        SOURCE_SUBDIR SkellCore)
    FetchContent_MakeAvailable(SkellCoreRoot)
endif()

juce_add_plugin(SkellGlitch
    COMPANY_NAME "Skelly"
    BUNDLE_ID "uno.skellyr.skellglitch"
    PLUGIN_MANUFACTURER_CODE SkLy
    PLUGIN_CODE SkGl
    PRODUCT_NAME "SkellGlitch"
    PLUGIN_NAME "SkellGlitch"
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
    juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra SkellCore::UI
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
# Original vector mark is embedded; no third-party or redistributable font files are included.
juce_add_binary_data(SkellGlitchAssets SOURCES assets/original_mark.svg)
target_link_libraries(SkellGlitch PRIVATE SkellGlitchAssets)
juce_generate_juce_header(SkellGlitch)

# Build a real offscreen JUCE editor render test on the Windows runner.
# Reuses the plugin's existing source/library; no additional runtime dependencies.
option(SKELL_BUILD_UI_SMOKE "Build offscreen UI snapshot smoke test" OFF)
if(SKELL_BUILD_UI_SMOKE)
    juce_add_console_app(SkellGlitchUISmoke PRODUCT_NAME "Skell-Glitch UI Smoke")
    target_sources(SkellGlitchUISmoke PRIVATE tests/UISmoke.cpp)
    target_link_libraries(SkellGlitchUISmoke PRIVATE SkellGlitch juce::juce_audio_utils juce::juce_gui_extra)
    target_compile_definitions(SkellGlitchUISmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
endif()

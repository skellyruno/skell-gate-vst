cmake_minimum_required(VERSION 3.22)

project(Skellgate VERSION 1.1.1)

option(SKELLGATE_BUILD_TESTS     "Build the headless DSP test executable (ctest)" OFF)
option(SKELLGATE_ALLOW_NON_WINDOWS "Allow configuring on non-Windows platforms (untested)" OFF)

if(NOT WIN32 AND NOT SKELLGATE_ALLOW_NON_WINDOWS)
    message(FATAL_ERROR "Skellgate is configured to build exclusively for Windows. "
                        "Pass -DSKELLGATE_ALLOW_NON_WINDOWS=ON to try anyway.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE framework automatically (pinned so the build doesn't break when master changes)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Define the JUCE plugin target
juce_add_plugin(Skellgate
    COMPANY_NAME "Skell"
    PLUGIN_MANUFACTURER_CODE Skel
    PLUGIN_CODE Skgl
    FORMATS VST3 Standalone
    PRODUCT_NAME "Skellgate"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE        # lets the host route MIDI notes to the effect to switch presets
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Skellgate
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# Strip bloat and bypass VST3 parameter warnings
target_compile_definitions(Skellgate
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
)

# Link necessary JUCE modules
target_link_libraries(Skellgate
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Headless DSP tests: cmake -B build -DSKELLGATE_BUILD_TESTS=ON && ctest --test-dir build -C Release
if(SKELLGATE_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(SkellgateTests PRODUCT_NAME "SkellgateTests")

    target_sources(SkellgateTests
        PRIVATE
            Tests/DspTests.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
    )
    target_include_directories(SkellgateTests PRIVATE Source)
    target_compile_definitions(SkellgateTests
        PRIVATE
            JucePlugin_Name="Skellgate"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=0
    )
    target_link_libraries(SkellgateTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_gui_basics
            juce::juce_audio_processors
            juce::juce_recommended_config_flags
    )

    add_test(NAME dsp COMMAND SkellgateTests)
endif()

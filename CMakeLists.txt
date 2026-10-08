cmake_minimum_required(VERSION 3.22)

project(Skellgate VERSION 1.0.0)

if(NOT WIN32)
    message(FATAL_ERROR "Skellgate is configured to build exclusively for Windows.")
endif()

# Fetch JUCE framework automatically
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(JUCE)

# Define the JUCE plugin target
juce_add_plugin(Skellgate
    COMPANY_NAME "Skell"
    PLUGIN_MANUFACTURER_CODE Skel
    PLUGIN_CODE Skgl
    FORMATS VST3 Standalone
    PRODUCT_NAME "Skellgate"
)

# Add your plugin source files from the Source directory
target_sources(Skellgate
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# Compile definitions to strip bloat and bypass VST3 parameter warnings
target_compile_definitions(Skellgate
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
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

cmake_minimum_required(VERSION 3.22)
project(nine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mac ARM / universal binary support
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS deployment target" FORCE)
endif()

include(FetchContent)

# Download JUCE
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Plugin target
juce_add_plugin(nine
    VERSION                     1.0.0
    COMPANY_NAME                "nine"
    PLUGIN_MANUFACTURER_CODE    Nine
    PLUGIN_CODE                 Tr99
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "nine"
    BUNDLE_ID                   "com.nine.tr909"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Instrument" "Drums"
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
)

# Source files
target_sources(nine PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/circuits/TR909BassDrum.cpp
    Source/circuits/TR909SnareDrum.cpp
    Source/circuits/TR909Tom.cpp
    Source/circuits/TR909RimShot.cpp
    Source/circuits/TR909Clap.cpp
)

target_include_directories(nine PRIVATE Source)

target_compile_definitions(nine PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

juce_generate_juce_header(nine)

target_link_libraries(nine
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

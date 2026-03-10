cmake_minimum_required(VERSION 3.22)
project(nine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mac ARM / universal binary support
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES     "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0"          CACHE STRING "macOS deployment target" FORCE)
endif()

# ---------------------------------------------------------------------------
# VST3 SDK – fetched at configure time; no JUCE dependency
# Correct option names discovered from the SDK's own CMakeLists.txt
# ---------------------------------------------------------------------------
set(SMTG_ENABLE_VSTGUI_SUPPORT        OFF CACHE BOOL "" FORCE)  # no GUI toolkit
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES  OFF CACHE BOOL "" FORCE)  # skip sample plug-ins
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)  # skip hosting examples
set(SMTG_ENABLE_USE_OF_JACK           OFF CACHE BOOL "" FORCE)  # no JACK
set(SMTG_ADD_UNIT_TESTS               OFF CACHE BOOL "" FORCE)  # no SDK unit tests

include(FetchContent)
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.7.14_build_55
    GIT_SHALLOW    TRUE
    # Only fetch the required submodules; exclude vstgui4, doc, tutorials
    GIT_SUBMODULES "base cmake pluginterfaces public.sdk"
)
FetchContent_MakeAvailable(vst3sdk)

# ---------------------------------------------------------------------------
# Plugin target – uses the SDK's smtg_add_vst3plugin helper which sets up
# the correct bundle layout on macOS and the correct export/suffix elsewhere.
# ---------------------------------------------------------------------------
smtg_add_vst3plugin(nine
    SOURCES_LIST
        Source/NineFactory.cpp
        Source/NineProcessor.cpp
        Source/NineController.cpp
        Source/circuits/TR909BassDrum.cpp
        Source/circuits/TR909SnareDrum.cpp
        Source/circuits/TR909Tom.cpp
        Source/circuits/TR909RimShot.cpp
        Source/circuits/TR909Clap.cpp
)

# Circuit-model and plugin headers live under Source/
target_include_directories(nine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# Link against the Steinberg VST3 SDK core (provides public include paths)
target_link_libraries(nine PRIVATE sdk)

# Platform-specific entry-point glue
# - Linux: ModuleEntry / ModuleExit exported by linuxmain.cpp
# - macOS: bundleEntry / bundleExit via macmain.cpp
# - Windows: DLL entry-point via dllmain.cpp (already in sdk)
if(APPLE)
    target_sources(nine PRIVATE
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/macmain.cpp)
elseif(UNIX)
    target_sources(nine PRIVATE
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/linuxmain.cpp)
endif()

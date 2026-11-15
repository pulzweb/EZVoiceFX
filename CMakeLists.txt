cmake_minimum_required(VERSION 3.22)

project(EZVoiceFX VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Static MSVC runtime: the plugin works without installing the VC++ redistributable
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# ---------------------------------------------------------------- JUCE framework
set(EZ_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/JUCE" CACHE PATH "Path to JUCE")
if(EXISTS "${EZ_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${EZ_JUCE_DIR}" JUCE)
else()
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# ------------------------------------------- RNNoise neural network (BSD licence)
# Uses the RNNoise copy maintained in werman/noise-suppression-for-voice, which ships
# the generated model weights and a CMake build that works with MSVC.
set(EZ_RNNOISE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/nsv/external/rnnoise" CACHE PATH "Path to RNNoise")
if(EXISTS "${EZ_RNNOISE_DIR}/CMakeLists.txt")
    add_subdirectory("${EZ_RNNOISE_DIR}" rnnoise EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(nsv
        GIT_REPOSITORY https://github.com/werman/noise-suppression-for-voice.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR external/rnnoise)
    FetchContent_MakeAvailable(nsv)
endif()

# ------------------------------------------------------------------ the plugin
juce_add_plugin(EZVoiceFX
    COMPANY_NAME "EZVibe"
    PRODUCT_NAME "EZ Voice FX"
    PLUGIN_MANUFACTURER_CODE Ezvb
    PLUGIN_CODE Ezvf
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Restoration
    BUNDLE_ID com.ezvibe.ezvoicefx)

target_sources(EZVoiceFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/ui/Widgets.cpp
    Source/ui/Pages.cpp
    Source/ui/Magic.cpp)

target_include_directories(EZVoiceFX PRIVATE Source)

target_compile_definitions(EZVoiceFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

if(MSVC)
    target_compile_options(EZVoiceFX PRIVATE /utf-8 /bigobj)
endif()

target_link_libraries(EZVoiceFX
    PRIVATE
        RnNoise
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)

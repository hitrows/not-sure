cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Identity. Change these five lines to rename the plugin.
# PLUGIN_CODE and MANUFACTURER_CODE must be exactly 4 chars, at least one upper.
# ---------------------------------------------------------------------------
set(PROJECT_NAME        "NotSure")
set(PRODUCT_NAME        "Not Sure")
set(COMPANY_NAME        "Hitrows")
set(BUNDLE_ID           "com.hitrows.notsure")
set(MANUFACTURER_CODE   Htrw)
set(PLUGIN_CODE         Nsur)

project(${PROJECT_NAME} VERSION 0.8.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary so the plugin runs native on both Apple Silicon and Intel.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")

# ---------------------------------------------------------------------------
# JUCE. FetchContent clones it into the build dir, so nothing is vendored
# into git. Pin an explicit tag - never track a moving branch.
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME                "${COMPANY_NAME}"
    BUNDLE_ID                   "${BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE    ${MANUFACTURER_CODE}
    PLUGIN_CODE                 ${PLUGIN_CODE}
    PRODUCT_NAME                "${PRODUCT_NAME}"

    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Drops the built .component / .vst3 straight into ~/Library/Audio/Plug-Ins
    COPY_PLUGIN_AFTER_BUILD     TRUE

    # Standalone is here for the tuning phase - it avoids a DAW rescan on
    # every rebuild. Drop it before release if you want AU only.
    FORMATS                     AU VST3 Standalone)

# ---------------------------------------------------------------------------
# Binary assets
#
# The panel artwork is embedded rather than loaded from disk, so the plugin has
# no external file dependencies. Files become BinaryData::<name>_<ext> - here
# BinaryData::panel_png.
#
# POSITION_INDEPENDENT_CODE is required: the plugin is a shared library, and
# linking a static archive built without -fPIC into it fails at link time on
# some toolchains. This is the usual first thing to break.
# ---------------------------------------------------------------------------
juce_add_binary_data(NotSureAssets
    SOURCES
        Resources/main.png
        Resources/off.png
        Resources/allleft.png
        Resources/allcenter.png
        Resources/allright.png)

set_target_properties(NotSureAssets PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

target_sources(${PROJECT_NAME} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/LimiterCore.cpp
    Source/dsp/Oversampler.cpp)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        NotSureAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# notsure-render
#
# Offline wav renderer. Links only LimiterCore - no JUCE, no plugin code - so
# it builds in seconds and the tune-listen loop never touches a DAW.
# ---------------------------------------------------------------------------
add_executable(notsure-render
    tools/render.cpp
    Source/dsp/LimiterCore.cpp
    Source/dsp/Oversampler.cpp)

target_include_directories(notsure-render PRIVATE Source/dsp)

set_target_properties(notsure-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tools"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

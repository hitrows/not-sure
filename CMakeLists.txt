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

# Deployment target and architectures MUST be set BEFORE project(): project()
# initialises these as cache variables, and a later set(... CACHE ...) without
# FORCE is ignored. Setting them after left the cache entry empty and the build
# arm64-only - Intel testers could not run it at all.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")

project(${PROJECT_NAME} VERSION 0.9.3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Windows build (WINDOWS-SPEC.md). AU does not exist off Apple platforms, and
# Standalone exists only for local auditioning on the Mac, so neither belongs
# in a Windows build. Presets and the installer are macOS-only for now too -
# see README-WINDOWS.md.
# ---------------------------------------------------------------------------
if (APPLE)
    set(NOTSURE_FORMATS AU VST3 Standalone)
else()
    set(NOTSURE_FORMATS VST3)
endif()

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
    # for local auditioning. On Windows CI this would instead try (and likely
    # fail without elevation) to write into Program Files for an artifact
    # nobody needs copied anywhere - the workflow zips the build output itself.
    COPY_PLUGIN_AFTER_BUILD     ${APPLE}

    FORMATS                     ${NOTSURE_FORMATS})

if (WIN32)
    # std::min/std::max collide with the min/max macros windows.h defines
    # otherwise; WIN32_LEAN_AND_MEAN keeps windows.h from pulling in the rest
    # of the stuff nothing here needs.
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN)
endif()

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
        Resources/allright.png
        Resources/newver.png)

set_target_properties(NotSureAssets PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

target_sources(${PROJECT_NAME} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UpdateChecker.cpp
    Source/Licence.cpp
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
        juce::juce_cryptography
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

# ---------------------------------------------------------------------------
# notsure-state-test
#
# Feeds NotSureProcessor::setStateInformation malformed/foreign/future blocks
# for real (STATE-VERSION-SPEC.md part 1, acceptance 1-4) instead of reasoning
# about it. Needs the real processor + APVTS, so unlike notsure-render it can't
# stay JUCE-free - it recompiles the same sources into a console executable
# (no plugin-format wrapper, our own main()). juce_add_plugin() sets every
# JucePlugin_* macro as COMPILE_DEFINITIONS on ${PROJECT_NAME}; copy that
# property here rather than hand-picking the subset we think we need, which
# would silently drift from the real plugin build.
# ---------------------------------------------------------------------------
add_executable(notsure-state-test
    tools/test-state.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UpdateChecker.cpp
    Source/Licence.cpp
    Source/dsp/LimiterCore.cpp
    Source/dsp/Oversampler.cpp)

target_compile_definitions(notsure-state-test PRIVATE
    $<TARGET_PROPERTY:${PROJECT_NAME},COMPILE_DEFINITIONS>)

target_link_libraries(notsure-state-test
    PRIVATE
        NotSureAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set_target_properties(notsure-state-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tools"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

# ---------------------------------------------------------------------------
# make-licence
#
# Generates the signing key pair (once) and issues licence.txt files
# (LICENCE-SPEC.md). Cryptography-only - no audio_processors, no GUI - so it
# builds fast like notsure-render.
# ---------------------------------------------------------------------------
add_executable(make-licence
    tools/make-licence.cpp
    Source/Licence.cpp)

target_link_libraries(make-licence PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_cryptography)

set_target_properties(make-licence PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tools"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

# ---------------------------------------------------------------------------
# notsure-licence-test
#
# Feeds signLicence/verifyLicence the cases LICENCE-SPEC.md acceptance 1-5
# ask for, for real, with throwaway keys - never the real embedded public key
# or the real private key. Cryptography-only, same spirit as make-licence.
# ---------------------------------------------------------------------------
add_executable(notsure-licence-test
    tools/notsure-licence-test.cpp
    Source/Licence.cpp)

target_link_libraries(notsure-licence-test PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_cryptography)

set_target_properties(notsure-licence-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tools"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

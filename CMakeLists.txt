cmake_minimum_required(VERSION 3.22)
project(Nacre VERSION 1.0.0 LANGUAGES C CXX)

# A synth must be built optimised: an unoptimised build makes the plug-in far too slow to load in a host.
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo" CACHE STRING "" FORCE)
elseif(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # static runtime: no VC++ redistributable needed
endif()

# ---------------------------------------------------------------- JUCE
# Point NACRE_JUCE_DIR at a local JUCE checkout, or let CMake download JUCE 8.0.15.
set(NACRE_JUCE_DIR "" CACHE PATH "Path to a JUCE checkout (optional)")
if(NACRE_JUCE_DIR)
    add_subdirectory(${NACRE_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- plugin
set(NACRE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NACRE_FORMATS AU)
endif()

option(NACRE_COPY_AFTER_BUILD "Copy the VST3 into the system plug-in folder after building" OFF)

juce_add_plugin(Nacre
    COMPANY_NAME "Nacre Audio"
    COMPANY_WEBSITE "https://github.com/"
    BUNDLE_ID com.nacreaudio.nacre
    PLUGIN_MANUFACTURER_CODE Ncra
    PLUGIN_CODE Ncr1
    FORMATS ${NACRE_FORMATS}
    PRODUCT_NAME "Nacre"
    DESCRIPTION "Wavetable synthesizer"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${NACRE_COPY_AFTER_BUILD})

juce_generate_juce_header(Nacre)

juce_add_binary_data(NacreFonts SOURCES
    plugin/fonts/Inter-Regular.otf
    plugin/fonts/Inter-Medium.otf
    plugin/fonts/Inter-SemiBold.otf
    plugin/fonts/Inter-Bold.otf)

target_sources(Nacre PRIVATE
    engine/Synth.cpp
    engine/Wavetables.cpp
    engine/FactoryPresets.cpp
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    plugin/PresetManager.cpp
    plugin/Pages.cpp)

target_include_directories(Nacre PRIVATE engine plugin)

target_compile_definitions(Nacre PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

if(UNIX AND NOT APPLE)
    # Minimal Linux dependencies (only libx11, freetype and fontconfig headers needed)
    target_compile_definitions(Nacre PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0
        JUCE_USE_XCURSOR=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

if(MSVC)
    target_compile_options(Nacre PRIVATE /O2 /W3 /bigobj /utf-8 /MP)
else()
    target_compile_options(Nacre PRIVATE -O3 -ffast-math -fno-finite-math-only -Wall -Wno-unused-parameter -Wno-float-equal)
endif()

target_link_libraries(Nacre
    PRIVATE
        NacreFonts
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- engine tests (no JUCE needed)
add_executable(nacre_render tools/nacre_render.cpp engine/Synth.cpp engine/Wavetables.cpp)
target_include_directories(nacre_render PRIVATE engine)
enable_testing()
add_test(NAME engine_tests COMMAND nacre_render test)

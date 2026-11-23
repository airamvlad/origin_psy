cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(OriginPsy VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ORIGINPSY_BUILD_PLUGIN "Build the plugin (downloads JUCE)" ON)
option(ORIGINPSY_BUILD_TESTS "Build the tests" ON)
option(ORIGINPSY_COPY_AFTER_BUILD "Copy the plugin into the system plugin folder after building" OFF)

# DSP core: plain C++, no JUCE, so it can be tested without a DAW.
add_library(originpsy_dsp INTERFACE)
target_include_directories(originpsy_dsp INTERFACE src)

if(ORIGINPSY_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)

    set(ORIGINPSY_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND ORIGINPSY_FORMATS AU)
    endif()

    juce_add_plugin(OriginPsy
        PRODUCT_NAME "OriginPsy"
        COMPANY_NAME "AMV"
        BUNDLE_ID "com.amv.OriginPsy"
        PLUGIN_MANUFACTURER_CODE Amvx
        PLUGIN_CODE Opsy
        FORMATS ${ORIGINPSY_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Fx Delay Filter
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD ${ORIGINPSY_COPY_AFTER_BUILD})

    target_sources(OriginPsy PRIVATE
        src/plugin/Parameters.cpp
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/Presets.cpp)

    target_compile_definitions(OriginPsy PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    # Source files are UTF-8 (the UI uses a few symbols such as arrows).
    if(MSVC)
        target_compile_options(OriginPsy PRIVATE /utf-8)
    endif()

    target_link_libraries(OriginPsy
        PRIVATE
            originpsy_dsp
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(ORIGINPSY_BUILD_TESTS)
    enable_testing()
    add_executable(originpsy_dsp_tests tests/DspTests.cpp)
    target_link_libraries(originpsy_dsp_tests PRIVATE originpsy_dsp)
    add_test(NAME originpsy_dsp_tests COMMAND originpsy_dsp_tests)

    if(ORIGINPSY_BUILD_PLUGIN)
        # Runs the real plugin processor offline: state, automation, sample rates.
        add_executable(originpsy_plugin_tests tests/PluginTests.cpp)
        target_include_directories(originpsy_plugin_tests PRIVATE src
            $<TARGET_PROPERTY:OriginPsy,INCLUDE_DIRECTORIES>)
        target_compile_definitions(originpsy_plugin_tests PRIVATE
            $<TARGET_PROPERTY:OriginPsy,COMPILE_DEFINITIONS>)
        target_link_libraries(originpsy_plugin_tests PRIVATE OriginPsy)
        add_test(NAME originpsy_plugin_tests COMMAND originpsy_plugin_tests)
    endif()
endif()

cmake_minimum_required(VERSION 3.22)

project(ToneRig VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

#===============================================================================
# Options
#===============================================================================
option(TONERIG_BUILD_AS_INSTRUMENT "Classify the plugin as a VST instrument (VSTi)" ON)
option(TONERIG_BUILD_STANDALONE   "Also build a standalone application"             OFF)
option(TONERIG_BUILD_TESTS        "Build the offline DSP smoke test"                ON)
option(TONERIG_COPY_PLUGIN        "Copy the plugin into the user plugin folder"      OFF)

#===============================================================================
# JUCE
#===============================================================================
add_subdirectory(third_party/JUCE)

set(TONERIG_FORMATS VST3)
if(TONERIG_BUILD_STANDALONE)
    list(APPEND TONERIG_FORMATS Standalone)
endif()

juce_add_plugin(ToneRig
    COMPANY_NAME              "FreebuffAudio"
    BUNDLE_ID                 "audio.freebuff.tonerig"
    PLUGIN_MANUFACTURER_CODE  Frba
    PLUGIN_CODE               Tnr1
    PRODUCT_NAME              "ToneRig"
    DESCRIPTION               "Self contained guitar amp, cab and effects rig"
    IS_SYNTH                  ${TONERIG_BUILD_AS_INSTRUMENT}
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   ${TONERIG_COPY_PLUGIN}
    FORMATS                   ${TONERIG_FORMATS}
    AU_MAIN_TYPE              "kAudioUnitType_Effect")

target_sources(ToneRig PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/RigChain.cpp)

target_compile_definitions(ToneRig PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_USE_WINRT_MIDI=0)

target_link_libraries(ToneRig
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)

#===============================================================================
# Offline DSP smoke test (no GUI, no plugin host required)
#===============================================================================
if(TONERIG_BUILD_TESTS)
    juce_add_console_app(ToneRigDspTest PRODUCT_NAME "ToneRig DSP Test")

    target_sources(ToneRigDspTest PRIVATE
        Tests/SmokeTest.cpp
        Source/dsp/RigChain.cpp)

    target_include_directories(ToneRigDspTest PRIVATE Source)

    target_link_libraries(ToneRigDspTest
        PRIVATE
            juce::juce_dsp
            juce::juce_audio_basics
        PUBLIC
            juce::juce_recommended_config_flags)
endif()

#===============================================================================
# VST3 host load test: opens the built bundle through the VST3 ABI and runs
# audio through it, the same way a DAW does.  (Linux; macOS needs the inner
# binary path, Windows would need LoadLibrary.)
#===============================================================================
if(TONERIG_BUILD_TESTS AND UNIX AND NOT APPLE)
    set(TONERIG_VST3_SDK
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK")

    add_executable(ToneRigVst3HostTest
        Tests/Vst3HostTest.cpp
        "${TONERIG_VST3_SDK}/pluginterfaces/base/funknown.cpp"
        "${TONERIG_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
        "${TONERIG_VST3_SDK}/pluginterfaces/base/ustring.cpp"
        "${TONERIG_VST3_SDK}/pluginterfaces/base/conststringtable.cpp"
        "${TONERIG_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")

    target_include_directories(ToneRigVst3HostTest PRIVATE "${TONERIG_VST3_SDK}")
    target_compile_definitions(ToneRigVst3HostTest PRIVATE
        TONERIG_VST3_PATH="$<TARGET_FILE:ToneRig_VST3>")
    set_target_properties(ToneRigVst3HostTest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_link_libraries(ToneRigVst3HostTest PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(ToneRigVst3HostTest ToneRig_VST3)
endif()



# The top-level CMakeLists.txt file for a project must contain a literal, direct call to the
# `project()` command. `project()` sets up some helpful variables that describe source/binary
# directories, and the current project version. This is a standard CMake command.

# AAT2: this file works in two ways
#  - standalone (self-contained repository): cmake -S . -B build
#    uses JUCE from the git submodule JUCE/ (see README, section AAT2)
#  - as a subdirectory of a bigger project that already added JUCE (e.g. AudioDev)
cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR) # standalone
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON) # needed on Linux for binary data (e.g. factory presets)
endif()

set(TARGET_NAME DrumFeatureDemonstrator)

project(${TARGET_NAME} VERSION 0.5.0)

if(NOT TARGET juce::juce_core) # JUCE not added by a parent project: use the submodule
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        message(FATAL_ERROR "JUCE not found: run 'git submodule update --init' (see README, section AAT2)")
    endif()
    add_subdirectory(JUCE)
endif()

configure_file(Versioning.h.in Versioning.h)

# `VST3_CATEGORIES`
#  - Should be one or more, separated by spaces, of the following: `Fx`, `Instrument`, `Analyzer`,
#    `Delay`, `Distortion`, `Drum`, `Dynamics`, `EQ`, `External`, `Filter`, `Generator`, `Mastering`,
#    `Modulation`, `Mono`, `Network`, `NoOfflineProcess`, `OnlyOfflineProcess`, `OnlyRT`,
#    `Pitch Shift`, `Restoration`, `Reverb`, `Sampler`, `Spatial`, `Stereo`, `Surround`, `Synth`,
#    `Tools`, `Up-Downmix`

#- `AU_MAIN_TYPE`
#  - Should be one of: `kAudioUnitType_MusicDevice`, `kAudioUnitType_FormatConverter`,
#    `kAudioUnitType_Generator`, `kAudioUnitType_MIDIProcessor`, `kAudioUnitType_Mixer`,
#    `kAudioUnitType_MusicDevice`, `kAudioUnitType_MusicEffect`, `kAudioUnitType_OfflineEffect`,
#    `kAudioUnitType_Output`, `kAudioUnitType_Panner`

add_compile_definitions(FACTORY_PRESETS) # factory presets from res/presets (made by tools/make_factory_presets.py, see binary files below)
add_compile_definitions(WITH_MIDIKEYBOARD) # MIDI keyboard in the GUI: plays the sample
add_compile_definitions(WITH_PRESETHANDLERGUI)
add_compile_definitions(WITH_DAYNIGHT) # day/night theme button in the preset bar (needs WITH_PRESETHANDLERGUI)

get_directory_property(ALL_DEFINITIONS COMPILE_DEFINITIONS)
if("WITH_DAYNIGHT" IN_LIST ALL_DEFINITIONS AND NOT "WITH_PRESETHANDLERGUI" IN_LIST ALL_DEFINITIONS)
    message(FATAL_ERROR "WITH_DAYNIGHT needs WITH_PRESETHANDLERGUI: the day/night button sits in the preset bar")
endif()

juce_add_plugin(${TARGET_NAME}
    # VERSION ...                               # Set this if the plugin version is different to the project version
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
    # ICON_SMALL ...
    # COMPANY_NAME ...                          # Specify the name of the plugin's author
    COMPANY_NAME  "Jade_Hochschule"             # Specify the name of the plugin's author
    IS_SYNTH TRUE                        # Is this a synth or an effect?
    NEEDS_MIDI_INPUT TRUE                # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE              # Does the plugin need midi output?
    # IS_MIDI_EFFECT TRUE/FALSE                 # Is this plugin a MIDI effect?
    # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    # Does the editor need keyboard focus?
    VST3_CATEGORIES "Instrument" "Sampler" "Drum"                      
    AU_MAIN_TYPE "kAudioUnitType_MusicDevice"
    COPY_PLUGIN_AFTER_BUILD FALSE               # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE IHAJ               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE DFD1                            # A unique four-character plugin id with at least one upper-case character
    FORMATS AU VST3 Standalone                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME "DrumFeatureDemonstrator")     # The name of the final executable, which can differ from the target name
#
# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.

# juce_generate_juce_header(AudioPluginExample)
juce_generate_juce_header(${TARGET_NAME})

# `target_sources` adds source files to a target. We pass the target that needs the sources as the
# first argument, then a visibility parameter for the sources which should normally be PRIVATE.
# Finally, we supply a list of source files that will be built into the target. This is a standard
# CMake command.


target_sources(${TARGET_NAME}
    PRIVATE
        PluginEditor.cpp
        PluginProcessor.cpp
        DrumFeatureDemonstrator.cpp
        tools/DayNightLookAndFeel.cpp
        tools/MidiModPitchState.cpp
        tools/PresetHandler.cpp
        tools/SynchronBlockProcessor.cpp
        src/StepScheduler.cpp
        src/Sequencer.cpp
        src/StepParameters.cpp
        src/SampleData.cpp
        src/SampleVoice.cpp
        src/VoiceBank.cpp
        src/SampleHolder.cpp
        src/SampleLoader.cpp
        src/SampleStore.cpp
        gui/WaveformView.cpp
        gui/StepGrid.cpp
        )




target_compile_definitions(${TARGET_NAME}
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)

# for binaries you can add them directly or recursevly (examples for both methods below, dont forget to 
# to add the binaries in target_link_libraries below)        
# If your target needs extra binary assets, you can add them here. The first argument is the name of
# a new static library target that will include all the binary resources. There is an optional
# `NAMESPACE` argument that can specify the namespace of the generated binary data class. Finally,
# the SOURCES argument should be followed by a list of source files that should be built into the
# static library. These source files can be of any kind (wav data, images, fonts, icons etc.).
# Conversion to binary-data will happen when your target is built.

# juce_add_binary_data(${TARGET_NAME}-binary SOURCES ...)

# `target_link_libraries` links libraries and JUCE modules to other libraries or executables. Here,
# we're linking our executable target to the `juce::juce_audio_utils` module. Inter-module
# dependencies are resolved automatically, so `juce_core`, `juce_events` and so on will also be
# linked automatically. If we'd generated a binary data target above, we would need to link to it
# here too. This is a standard CMake command.



# use this code for binaries (e.g. presets)
file(GLOB TEMPLATE_RESOURCES_GLOB
  res/presets/*.xml
  )

juce_add_binary_data( ${TARGET_NAME}-binary
  SOURCES ${TEMPLATE_RESOURCES_GLOB}
)
if( UNIX AND NOT APPLE )
set_target_properties(${TARGET_NAME}-binary PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)
endif()



target_link_libraries(${TARGET_NAME}
    PRIVATE
        ${TARGET_NAME}-binary       # the factory presets (res/presets)
        # AudioPluginDrumFeatureDemonstrator-binary # or here if we used the recursice method
        juce::juce_audio_utils
        # juce::juce_opengl  # if we want to use opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

target_include_directories(${TARGET_NAME} PUBLIC
        "${PROJECT_BINARY_DIR}"
        )        

# AAT2: test tool (render a wav through the plugin, take a GUI snapshot), see
# HowToTestYourPlugin.md. Off by default: cmake ... -DAAT_BUILD_TESTER=ON
option(AAT_BUILD_TESTER "Build the test tool ${TARGET_NAME}_Tester" OFF)
if(AAT_BUILD_TESTER)
    add_executable(${TARGET_NAME}_Tester tools/tester/Tester.cpp)
    # same settings as the plugin code (JucePlugin_* macros, JUCE options), and the plugin code itself;
    # modal loops: the snapshot mode runs the message loop for a while (runDispatchLoopUntil)
    target_compile_definitions(${TARGET_NAME}_Tester PRIVATE $<TARGET_PROPERTY:${TARGET_NAME},COMPILE_DEFINITIONS>
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(${TARGET_NAME}_Tester PRIVATE ${TARGET_NAME} juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

# Unit tests of the pure code (scheduler, no GUI), run with CTest:
#   cmake ... -DDFD_BUILD_UNIT_TESTS=ON ; cmake --build build --target DrumFeatureDemonstrator_UnitTests ; ctest --test-dir build
option(DFD_BUILD_UNIT_TESTS "Build the unit tests of the pure code" OFF)
if(DFD_BUILD_UNIT_TESTS)
    enable_testing()
    juce_add_console_app(${TARGET_NAME}_UnitTests PRODUCT_NAME "${TARGET_NAME}_UnitTests")
    target_sources(${TARGET_NAME}_UnitTests PRIVATE
        tests/UnitTestMain.cpp
        tests/StepSchedulerTests.cpp
        tests/SampleEngineTests.cpp
        tests/SequencerTests.cpp
        tests/SampleStoreTests.cpp
        src/StepScheduler.cpp
        src/Sequencer.cpp
        src/SampleData.cpp
        src/SampleVoice.cpp
        src/VoiceBank.cpp
        src/SampleHolder.cpp
        src/SampleStore.cpp)
    target_compile_definitions(${TARGET_NAME}_UnitTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(${TARGET_NAME}_UnitTests PRIVATE juce::juce_core
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME StepSchedulerTests COMMAND ${TARGET_NAME}_UnitTests)
endif()

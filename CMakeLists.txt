# FmLibPlug
cmake_minimum_required(VERSION 3.22)

project(FmLibPlug VERSION 1.2.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FMLIBPLUG_BUILD_CLAP "Build CLAP via clap-juce-extensions" ON)
option(FMLIBPLUG_BUILD_TESTS "Build unit tests, SysEx smoke tool, and hardware MIDI harness" ON)
option(FMLIBPLUG_BUILD_AUV3 "Request AUv3 format (only if the toolchain supports it)" OFF)
option(FMLIBPLUG_COPY_PLUGIN_AFTER_BUILD "Copy plugins into system plug-in folders after each format build" OFF)

include(FetchContent)

FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.14
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# AUv3 is opt-in: many macOS Command Line Tools setups never emit FmLibPlug_AUv3.
# AU / AUv3 are Apple-only — do not request them on Windows/Linux.
set(FMLIBPLUG_FORMATS VST3 LV2 Standalone)
if(APPLE)
  list(APPEND FMLIBPLUG_FORMATS AU)
  if(FMLIBPLUG_BUILD_AUV3)
    list(APPEND FMLIBPLUG_FORMATS AUv3)
  endif()
endif()

if(DEFINED ENV{AAX_SDK_PATH} OR DEFINED AAX_SDK_PATH)
  list(APPEND FMLIBPLUG_FORMATS AAX)
endif()

juce_add_plugin(FmLibPlug
  COMPANY_NAME "mywonderland808"
  BUNDLE_ID "com.mywonderland808.fmlibplug"
  PLUGIN_MANUFACTURER_CODE Mwld
  PLUGIN_CODE FmLp
  FORMATS ${FMLIBPLUG_FORMATS}
  VERSION ${PROJECT_VERSION}
  BUILD_VERSION ${PROJECT_VERSION}
  PRODUCT_NAME "FmLibPlug"
  PLUGIN_NAME "FmLibPlug"
  DESCRIPTION "FM SysEx librarian for DX7 / TX7"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD ${FMLIBPLUG_COPY_PLUGIN_AFTER_BUILD}
  VST3_CATEGORIES Fx
  AU_MAIN_TYPE kAudioUnitType_Effect
  LV2URI "https://github.com/mywonderland808/FmLibPlug"
)

target_sources(FmLibPlug
  PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/library/PatchLibrary.cpp
    Source/library/FolderScanner.cpp
    Source/library/FavoritesStore.cpp
    Source/library/PatchWriter.cpp
    Source/library/LibraryFilter.cpp
    Source/library/BrowserList.cpp
    Source/library/TagStore.cpp
    Source/library/AutoTagger.cpp
    Source/library/RecentStore.cpp
    Source/library/MorphPresetStore.cpp
    Source/library/PatchEntry.cpp
    Source/library/DeviceBuffer.cpp
    Source/sysex/Dx7Formats.cpp
    Source/sysex/SysexParser.cpp
    Source/sysex/SysexMessages.cpp
    Source/sysex/FormatDetect.cpp
    Source/sysex/VoiceMorpher.cpp
    Source/sysex/MorphLocks.cpp
    Source/midi/MidiDeviceManager.cpp
    Source/midi/MorphTransport.cpp
    Source/ui/PatchBrowser.cpp
    Source/ui/DeviceBufferPanel.cpp
    Source/ui/SettingsPanel.cpp
    Source/ui/LookAndFeel_FmLibPlug.cpp
    Source/ui/MorpherPanel.cpp
    Source/prefs/AppPreferences.cpp
)

# Build stamp lives out of tree so `make build` does not dirty Source/.
set(FMLIBPLUG_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(FMLIBPLUG_BUILD_ID_CPP "${FMLIBPLUG_GENERATED_DIR}/BuildId.cpp")
file(MAKE_DIRECTORY "${FMLIBPLUG_GENERATED_DIR}")
if(NOT EXISTS "${FMLIBPLUG_BUILD_ID_CPP}")
  file(WRITE "${FMLIBPLUG_BUILD_ID_CPP}"
"#include \"BuildId.h\"
namespace fmlib {
const char* buildId() { return \"dev\"; }
}
")
endif()
target_sources(FmLibPlug PRIVATE "${FMLIBPLUG_BUILD_ID_CPP}")

target_include_directories(FmLibPlug
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(FmLibPlug
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(FmLibPlug
  PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(FMLIBPLUG_BUILD_CLAP)
  FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(clap-juce-extensions)
  clap_juce_extensions_plugin(
    TARGET FmLibPlug
    CLAP_ID "com.mywonderland808.fmlibplug"
    CLAP_FEATURES utility midi
  )
endif()

# Aggregate target for every host-loadable format (JUCE *_All + optional CLAP).
add_custom_target(FmLibPlug_Plugins
  DEPENDS FmLibPlug_All
  COMMENT "Building all JUCE plugin formats (FmLibPlug_All)"
)
if(TARGET FmLibPlug_CLAP)
  add_dependencies(FmLibPlug_Plugins FmLibPlug_CLAP)
endif()

if(FMLIBPLUG_BUILD_TESTS)
  enable_testing()

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Catch2)

  set(FMLIBPLUG_FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures")

  add_executable(FmLibPlugGenFixtures
    Tools/GenTestFixtures.cpp
    Source/sysex/Dx7Formats.cpp
    Source/sysex/SysexMessages.cpp
  )
  target_include_directories(FmLibPlugGenFixtures PRIVATE Source)
  target_compile_features(FmLibPlugGenFixtures PRIVATE cxx_std_17)

  add_custom_command(
    OUTPUT
      "${FMLIBPLUG_FIXTURES_DIR}/single_163.syx"
      "${FMLIBPLUG_FIXTURES_DIR}/bank_4104.syx"
      "${FMLIBPLUG_FIXTURES_DIR}/bank_headerless_4096.syx"
      "${FMLIBPLUG_FIXTURES_DIR}/bad_checksum_163.syx"
      "${FMLIBPLUG_FIXTURES_DIR}/dexed_bank.dx7"
      "${FMLIBPLUG_FIXTURES_DIR}/dx7ii_skip.syx"
    COMMAND FmLibPlugGenFixtures "${FMLIBPLUG_FIXTURES_DIR}"
    DEPENDS FmLibPlugGenFixtures
    COMMENT "Generating Tests/fixtures SysEx blobs"
    VERBATIM
  )
  add_custom_target(FmLibPlugFixtures DEPENDS
    "${FMLIBPLUG_FIXTURES_DIR}/single_163.syx"
    "${FMLIBPLUG_FIXTURES_DIR}/bank_4104.syx"
    "${FMLIBPLUG_FIXTURES_DIR}/bank_headerless_4096.syx"
    "${FMLIBPLUG_FIXTURES_DIR}/bad_checksum_163.syx"
    "${FMLIBPLUG_FIXTURES_DIR}/dexed_bank.dx7"
    "${FMLIBPLUG_FIXTURES_DIR}/dx7ii_skip.syx"
  )

  add_executable(FmLibPlugUnitTests
    Tests/unit/SysexFormatsTests.cpp
    Tests/unit/SysexParserTests.cpp
    Tests/unit/SysexMessagesTests.cpp
    Tests/unit/PatchWriterTests.cpp
    Tests/unit/DeviceBufferTests.cpp
    Tests/unit/FolderScannerTests.cpp
    Tests/unit/LibraryFilterTests.cpp
    Tests/unit/FavoritesStoreTests.cpp
    Tests/unit/AppPreferencesTests.cpp
    Tests/unit/BrowserListTests.cpp
    Tests/unit/BrowserStatsTests.cpp
    Tests/unit/TagStoreTests.cpp
    Tests/unit/FormatDetectTests.cpp
    Tests/unit/AutoTagDupeTests.cpp
    Tests/unit/RecentFilterTests.cpp
    Tests/unit/VoiceMorpherTests.cpp
    Tests/unit/MorphTransportTests.cpp
    Tests/unit/MorphPresetStoreTests.cpp
    Tests/unit/AuditionHoldInputsTests.cpp
    Source/sysex/Dx7Formats.cpp
    Source/sysex/SysexParser.cpp
    Source/sysex/SysexMessages.cpp
    Source/sysex/FormatDetect.cpp
    Source/sysex/VoiceMorpher.cpp
    Source/sysex/MorphLocks.cpp
    Source/midi/MorphTransport.cpp
    Source/library/FolderScanner.cpp
    Source/library/PatchWriter.cpp
    Source/library/FavoritesStore.cpp
    Source/library/LibraryFilter.cpp
    Source/library/BrowserList.cpp
    Source/library/TagStore.cpp
    Source/library/AutoTagger.cpp
    Source/library/RecentStore.cpp
    Source/library/PatchEntry.cpp
    Source/library/MorphPresetStore.cpp
    Source/library/DeviceBuffer.cpp
    Source/prefs/AppPreferences.cpp
  )
  add_dependencies(FmLibPlugUnitTests FmLibPlugFixtures)
  target_include_directories(FmLibPlugUnitTests PRIVATE Source Tests/unit)
  target_compile_features(FmLibPlugUnitTests PRIVATE cxx_std_17)
  target_compile_definitions(FmLibPlugUnitTests
    PRIVATE
      FMLIBPLUG_FIXTURES_DIR="${FMLIBPLUG_FIXTURES_DIR}"
      JUCE_WEB_BROWSER=0
      JUCE_USE_CURL=0
  )
  target_link_libraries(FmLibPlugUnitTests
    PRIVATE
      Catch2::Catch2WithMain
      juce::juce_core
  )

  add_test(NAME FmLibPlugUnit COMMAND FmLibPlugUnitTests)
  set_tests_properties(FmLibPlugUnit PROPERTIES LABELS "unit")

  add_executable(FmLibPlugSysexSmoke
    Tools/SysexSmokeTest.cpp
    Source/sysex/Dx7Formats.cpp
    Source/sysex/SysexParser.cpp
    Source/sysex/SysexMessages.cpp
  )
  target_include_directories(FmLibPlugSysexSmoke PRIVATE Source)
  target_compile_features(FmLibPlugSysexSmoke PRIVATE cxx_std_17)

  add_test(
    NAME FmLibPlugSysexSmoke
    COMMAND FmLibPlugSysexSmoke "${CMAKE_CURRENT_SOURCE_DIR}/Presets"
  )
  set_tests_properties(FmLibPlugSysexSmoke PROPERTIES
    LABELS "smoke"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )

  add_executable(FmLibPlugHwMidiTest
    Tools/HwMidiTest.cpp
    Source/midi/MidiDeviceManager.cpp
    Source/midi/MorphTransport.cpp
    Source/sysex/Dx7Formats.cpp
    Source/sysex/SysexParser.cpp
    Source/sysex/SysexMessages.cpp
  )
  target_include_directories(FmLibPlugHwMidiTest PRIVATE Source)
  target_compile_features(FmLibPlugHwMidiTest PRIVATE cxx_std_17)
  target_compile_definitions(FmLibPlugHwMidiTest
    PRIVATE
      JUCE_WEB_BROWSER=0
      JUCE_USE_CURL=0
      FMLIBPLUG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_link_libraries(FmLibPlugHwMidiTest
    PRIVATE
      juce::juce_audio_devices
      juce::juce_events
      juce::juce_core
  )

  add_test(NAME FmLibPlugHwMidi COMMAND FmLibPlugHwMidiTest)
  set_tests_properties(FmLibPlugHwMidi PROPERTIES
    LABELS "hw"
    DISABLED TRUE
  )

  # Convenience targets: cmake --build build --target check / check-smoke / check-hw
  # VERBATIM is required so label regexes like unit|smoke are not interpreted as shell pipes.
  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L unit --output-on-failure
    DEPENDS FmLibPlugUnitTests
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running unit tests (ctest -L unit)"
  )

  add_custom_target(check-smoke
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L smoke --output-on-failure
    DEPENDS FmLibPlugSysexSmoke
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running Presets SysEx smoke (ctest -L smoke)"
  )

  add_custom_target(check-all
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L unit --output-on-failure
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L smoke --output-on-failure
    DEPENDS FmLibPlugUnitTests FmLibPlugSysexSmoke
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running unit + smoke tests"
  )

  add_custom_target(check-hw
    COMMAND "$<TARGET_FILE:FmLibPlugHwMidiTest>"
    DEPENDS FmLibPlugHwMidiTest
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running hardware MIDI harness (Tests/hw-midi.local.json)"
  )

  add_custom_target(check-hw-list
    COMMAND "$<TARGET_FILE:FmLibPlugHwMidiTest>" --list-devices
    DEPENDS FmLibPlugHwMidiTest
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Listing MIDI devices for hardware tests"
  )
endif()

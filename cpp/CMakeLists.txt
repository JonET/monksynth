cmake_minimum_required(VERSION 3.20)
project(MonkSynth VERSION 0.2.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fetch VST3 SDK ---
include(FetchContent)
FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.8.0_build_66
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
if(APPLE)
    set(SMTG_ENABLE_AUV2_BUILDS ON CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(vst3sdk)

# Ensure dllmain.cpp is found by smtg_target_add_library_main
set(public_sdk_SOURCE_DIR "${vst3sdk_SOURCE_DIR}/public.sdk")

# --- Version header ---
set(MONKSYNTH_VERSION_LABEL "dev" CACHE STRING "Version label (e.g. beta.1, rc.1, release)")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
)

# --- DSP engine (plain C) ---
set(DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dsp")

# --- VST3 Plugin ---
smtg_add_vst3plugin(MonkSynth
    src/plugin_cids.h
    src/plugin_entry.cpp
    src/processor.h
    src/processor.cpp
    src/controller.h
    src/controller.cpp
    src/monk_view.h
    src/monk_view.cpp
    src/xy_pad.h
    src/xy_pad.cpp
    src/theme_manager.h
    src/theme_manager.cpp
    src/dll_extractor.h
    src/dll_extractor.cpp
    src/setup_view.h
    src/setup_view.cpp
    src/stb_impl.c
    ${DSP_DIR}/voice.c
    ${DSP_DIR}/delay.c
    ${DSP_DIR}/synth.c
)

target_include_directories(MonkSynth PRIVATE "${DSP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(MonkSynth PRIVATE sdk vstgui_support)

smtg_target_add_plugin_resources(MonkSynth
    RESOURCES
        resources/editor.uidesc
        resources/monk-base.png
        resources/monk-strip.png
        resources/knob-left.png
        resources/knob-right.png
        resources/fader-down-large.png
        resources/fader-down-sm.png
        resources/fader-right-sm.png
        resources/info.png
)

if(UNIX)
    target_link_libraries(MonkSynth PRIVATE m)
endif()

# --- macOS Audio Unit wrapper ---
if(APPLE AND SMTG_ENABLE_AUV2_BUILDS)
    include(SMTG_AddVST3AuV2)
    # AUv2 type codes: 'aumu' = instrument, 'aufx' = effect
    # 4-char codes: subtype 'MkSy', manufacturer 'ODLC'
    smtg_target_add_auv2(MonkSynth-au
        BUNDLE_NAME "MonkSynth"
        BUNDLE_IDENTIFIER "com.monksynth.audiounit"
        INFO_PLIST_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/au-info.plist"
        VST3_PLUGIN_TARGET MonkSynth
    )
endif()

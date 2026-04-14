cmake_minimum_required(VERSION 3.20)
project(MonkSynth VERSION 0.0.1)

# Default to Release if no build type specified (the VST3 SDK requires one of
# DEVELOPMENT, RELEASE, _DEBUG, or NDEBUG to be defined).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    # VSTGUI uses deprecated wstring_convert; suppress the error on newer SDKs
    add_compile_options(-Wno-deprecated-declarations)
endif()

# --- Linux static linking option ---
option(MONKSYNTH_STATIC_LINUX_DEPS "Statically link Linux GUI dependencies" OFF)
set(MONKSYNTH_STATIC_PREFIX "" CACHE PATH "Install prefix for static dependencies")

# --- Fetch VST3 SDK ---
include(FetchContent)

# Patch VSTGUI to support static linking of Linux GUI dependencies.
# The patch adds a conditional block guarded by MONKSYNTH_STATIC_LINUX_DEPS,
# so it is harmless on macOS/Windows where that option is never set.
# We use cmake -E env to ensure cross-platform shell compatibility, and
# check first whether the patch has already been applied.
FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.8.0_build_66
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES_RECURSE TRUE
    PATCH_COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/patches/apply-patch.cmake"
)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
if(APPLE)
    set(SMTG_ENABLE_AUV2_BUILDS ON CACHE BOOL "" FORCE)
endif()
if(MONKSYNTH_STATIC_LINUX_DEPS)
    set(SMTG_ENABLE_WAYLAND_SUPPORT OFF CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(vst3sdk)

# Ensure dllmain.cpp is found by smtg_target_add_library_main
set(public_sdk_SOURCE_DIR "${vst3sdk_SOURCE_DIR}/public.sdk")

# --- Version header ---
set(MONKSYNTH_VERSION_LABEL "dev" CACHE STRING "Version label (e.g. beta.1, rc.1, release)")
set(MONKSYNTH_VERSION_NUMBER "" CACHE STRING "Override version (e.g. 0.2.0), derived from git tag in CI")
if(MONKSYNTH_VERSION_NUMBER)
    set(MONKSYNTH_VERSION "${MONKSYNTH_VERSION_NUMBER}")
else()
    set(MONKSYNTH_VERSION "${PROJECT_VERSION}")
endif()
string(REPLACE "." ";" _ver_parts "${MONKSYNTH_VERSION}")
list(GET _ver_parts 0 MONKSYNTH_VERSION_MAJOR)
list(GET _ver_parts 1 MONKSYNTH_VERSION_MINOR)
list(GET _ver_parts 2 MONKSYNTH_VERSION_PATCH)
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
    src/info_view.h
    src/info_view.cpp
    src/info_button.h
    src/info_button.cpp
    src/open_url.h
    src/open_url.cpp
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

if(APPLE)
    smtg_target_set_bundle(MonkSynth
        BUNDLE_IDENTIFIER "com.monksynth.vst3"
        COMPANY_NAME "MonkSynth"
    )
endif()

# --- Install presets to the system VST3 preset directory ---
file(GLOB MONKSYNTH_PRESETS "${CMAKE_CURRENT_SOURCE_DIR}/../presets/*.vstpreset")
if(WIN32)
    set(PRESET_DIR "$ENV{APPDATA}/VST3 Presets/MonkSynth/MonkSynth")
elseif(APPLE)
    set(PRESET_DIR "$ENV{HOME}/Library/Audio/Presets/MonkSynth/MonkSynth")
else()
    set(PRESET_DIR "$ENV{HOME}/.vst3/presets/MonkSynth/MonkSynth")
endif()
add_custom_command(TARGET MonkSynth POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PRESET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MONKSYNTH_PRESETS} "${PRESET_DIR}"
    COMMENT "[SMTG] Installing presets to ${PRESET_DIR}"
)

if(UNIX)
    target_link_libraries(MonkSynth PRIVATE m)
endif()

# Hide all symbols except VST3 entry points when statically linking on Linux.
# This prevents bundled cairo/pango/glib symbols from conflicting with
# other plugins or the host DAW.
if(MONKSYNTH_STATIC_LINUX_DEPS)
    target_link_options(MonkSynth PRIVATE
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/resources/linux-exports.map
    )
endif()

# --- macOS Audio Unit wrapper ---
if(APPLE AND SMTG_ENABLE_AUV2_BUILDS)
    list(APPEND CMAKE_MODULE_PATH "${vst3sdk_SOURCE_DIR}/cmake/modules")
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

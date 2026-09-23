cmake_minimum_required(VERSION 3.20)

# The release version comes from the git tag in CI. It has to be known before
# project() because the VST3 SDK reads PROJECT_VERSION for the bundle plists
# and moduleinfo.json.
set(MONKSYNTH_VERSION_NUMBER "" CACHE STRING "Override version (e.g. 0.2.0), derived from git tag in CI")
if(MONKSYNTH_VERSION_NUMBER)
    set(_monksynth_project_version "${MONKSYNTH_VERSION_NUMBER}")
else()
    set(_monksynth_project_version "0.0.1")
endif()

# The deployment target must be set before project(). CMake's Darwin platform
# init creates this cache entry during project(), defaulting to the host OS
# version under the Xcode generator, and a later set(CACHE) is then a no-op.
# That is how 1.0.0 shipped requiring macOS 26.5.
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(MonkSynth VERSION ${_monksynth_project_version})

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
set(MONKSYNTH_VERSION "${PROJECT_VERSION}")
set(MONKSYNTH_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(MONKSYNTH_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(MONKSYNTH_VERSION_PATCH ${PROJECT_VERSION_PATCH})
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
)
# AudioComponents "version" is a packed integer, shown by auval as major.minor.patch.
math(EXPR MONKSYNTH_AU_VERSION
    "(${MONKSYNTH_VERSION_MAJOR} << 16) | (${MONKSYNTH_VERSION_MINOR} << 8) | ${MONKSYNTH_VERSION_PATCH}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/au-info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/au-info.plist"
    @ONLY
)

# --- DSP engine (plain C, standalone static library) ---
# Built as its own target so the VST3 plugin and the unit-test executables can
# both link against the same objects without duplicating the source list.
set(DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dsp")

add_library(monk_dsp STATIC
    ${DSP_DIR}/voice.c
    ${DSP_DIR}/delay.c
    ${DSP_DIR}/synth.c
)
target_include_directories(monk_dsp PUBLIC "${DSP_DIR}")
if(UNIX)
    target_link_libraries(monk_dsp PRIVATE m)
endif()
# Match the plugin's arch policy so universal builds link cleanly.
if(SMTG_MAC)
    smtg_target_setup_universal_binary(monk_dsp)
endif()

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
    src/draw_utils.h
    src/overlay_view.h
    src/overlay_view.cpp
    src/info_view.h
    src/info_view.cpp
    src/theme_info_view.h
    src/theme_info_view.cpp
    src/info_button.h
    src/info_button.cpp
    src/open_url.h
    src/plugin_paths.h
    src/plugin_paths.cpp
    src/open_url.cpp
    src/i18n.h
    src/i18n.cpp
    src/strings_en.h
    src/strings_ja.h
    src/strings_ko.h
    src/stb_impl.c
    src/http_fetch.h
    src/theme_gallery.h
    src/theme_gallery.cpp
    src/theme_browser_view.h
    src/theme_browser_view.cpp
    src/theme_gallery_view.h
    src/theme_gallery_view.cpp
    src/gallery_ui.h
)

# Theme gallery downloads: NSURLSession on macOS, WinHTTP on Windows, the
# system curl (a child process, nothing linked) elsewhere.
if(APPLE)
    target_sources(MonkSynth PRIVATE src/http_fetch_mac.mm)
    target_link_libraries(MonkSynth PRIVATE "-framework Foundation")
else()
    target_sources(MonkSynth PRIVATE src/http_fetch.cpp)
endif()
if(WIN32)
    target_link_libraries(MonkSynth PRIVATE winhttp)
endif()
# The gallery index is parsed with the rapidjson copy that ships with VSTGUI.
target_include_directories(MonkSynth PRIVATE
    "${vst3sdk_SOURCE_DIR}/vstgui4/vstgui/thirdparty/rapidjson/include")

target_include_directories(MonkSynth PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(MonkSynth PRIVATE sdk vstgui_support monk_dsp)

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

# Themes shipped inside the plugin bundle (Contents/Resources/themes/<name>).
# Each entry is a folder under ../themes. The plugin lists them in its theme
# menu and offers one on the setup screen (the first by display name).
set(MONKSYNTH_BUNDLED_THEMES smileyface)
foreach(theme ${MONKSYNTH_BUNDLED_THEMES})
    file(GLOB theme_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/../themes/${theme}/*.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/../themes/${theme}/*.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../themes/${theme}/*.txt"
    )
    smtg_target_add_plugin_resources(MonkSynth
        RESOURCES ${theme_files}
        OUTPUT_SUBDIRECTORY "themes/${theme}"
    )
endforeach()

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

# --- DSP unit tests (opt-in) ---
option(MONKSYNTH_BUILD_TESTS "Build DSP unit tests" OFF)
if(MONKSYNTH_BUILD_TESTS)
    enable_testing()
    foreach(t test_voice test_synth test_delay)
        add_executable(${t} tests/${t}.c)
        target_link_libraries(${t} PRIVATE monk_dsp)
        add_test(NAME ${t} COMMAND ${t})
    endforeach()
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
        INFO_PLIST_TEMPLATE "${CMAKE_CURRENT_BINARY_DIR}/generated/au-info.plist"
        VST3_PLUGIN_TARGET MonkSynth
    )
endif()

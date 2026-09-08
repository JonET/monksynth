#pragma once

// VSTGUI's DirectComposition backend presents through a D3D11 composition
// swapchain.  Under Wine that swapchain never reaches the host's child window
// and the editor stays black, so this fork never uses it: the bundled VSTGUI is
// patched to skip the DirectComposition factory entirely and paint through a
// Direct2D DC render target (GDI BitBlt) instead.
//
// disableDirectComposition() here is a second belt in case the SDK patch did
// not apply.  It must run before the first CFrame is created, because
// Win32Frame captures the DirectComposition factory in its constructor.
#ifdef _WIN32
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"

#include <cstdio>
#include <cstdlib>
#include <windows.h>

// Records whether the SDK patch is in the loaded binary.  Written once to
// %APPDATA%\MonkSynth\gui-debug.log; the editor is otherwise a black box with
// no way to tell which present path VSTGUI chose.
inline void monksynthLogGuiPath(const char *stage, bool directCompositionActive) {
    const char *appData = std::getenv("APPDATA");
    if (!appData)
        return;

    char path[MAX_PATH];
    if (std::snprintf(path, sizeof(path), "%s\\MonkSynth", appData) < 0)
        return;
    CreateDirectoryA(path, nullptr);
    if (std::snprintf(path, sizeof(path), "%s\\MonkSynth\\gui-debug.log", appData) < 0)
        return;

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    using WineGetVersion = const char *(__cdecl *)(void);
    auto wineGetVersion =
        ntdll ? reinterpret_cast<WineGetVersion>(GetProcAddress(ntdll, "wine_get_version"))
              : nullptr;

    if (FILE *f = std::fopen(path, "a")) {
        std::fprintf(f, "%s: wine=%s directComposition=%s\n", stage,
                     wineGetVersion ? wineGetVersion() : "no",
                     directCompositionActive ? "ACTIVE (patch missing)" : "off");
        std::fclose(f);
    }
}

inline void monksynthForceWineSafeGui(const char *stage = "init") {
    auto *win32 = VSTGUI::getPlatformFactory().asWin32Factory();
    if (!win32)
        return;
    const bool wasActive = win32->getDirectCompositionFactory() != nullptr;
    win32->disableDirectComposition();
    monksynthLogGuiPath(stage, wasActive);
}
#else
inline void monksynthForceWineSafeGui(const char * = "init") {}
#endif

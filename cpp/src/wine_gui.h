#pragma once

// Under Wine, VSTGUI's DirectComposition/D3D11 swapchain paints a black
// editor. Force the HWND software Direct2D path (BitBlt) instead.
#ifdef _WIN32
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"

#include <windows.h>

inline void monksynthForceWineSafeGui() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!(ntdll && GetProcAddress(ntdll, "wine_get_version")))
        return;
    if (auto *win32 = VSTGUI::getPlatformFactory().asWin32Factory())
        win32->disableDirectComposition();
}
#else
inline void monksynthForceWineSafeGui() {}
#endif

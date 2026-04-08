#include "open_url.h"

#if _WIN32
#include <windows.h>
#include <shellapi.h>
#elif __APPLE__
#include <cstdlib>
#else
#include <cstdlib>
#endif

namespace MonkSynth {

void openURL(const std::string &url) {
#if _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    std::string cmd = "open \"" + url + "\"";
    if (system(cmd.c_str())) {} // best-effort
#else
    std::string cmd = "xdg-open \"" + url + "\" &";
    if (system(cmd.c_str())) {} // best-effort
#endif
}

} // namespace MonkSynth

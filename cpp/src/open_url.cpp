#include "open_url.h"

#include <system_error>

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

void openFolder(const std::filesystem::path &path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    // Proceed even if create_directories failed — the folder may already exist
    // and succeeded in a prior run, or a parent permission may block creation.

#if _WIN32
    // ShellExecuteW correctly handles non-ASCII paths (e.g. Japanese user
    // folder names) which the ANSI variant would mangle.
    ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
#elif __APPLE__
    std::string cmd = "open \"" + path.string() + "\"";
    if (system(cmd.c_str())) {}
#else
    std::string cmd = "xdg-open \"" + path.string() + "\" &";
    if (system(cmd.c_str())) {}
#endif
}

} // namespace MonkSynth

#pragma once

#include <filesystem>
#include <string>

namespace MonkSynth {

// Open a URL in the user's default browser. Platform-specific implementation.
void openURL(const std::string &url);

// Reveal a folder in the system file browser (Explorer/Finder/xdg-open).
// Creates the directory if it doesn't exist.
void openFolder(const std::filesystem::path &path);

} // namespace MonkSynth

#pragma once

#include <filesystem>
#include <string>

namespace MonkSynth {

struct ExtractionResult {
    bool success;
    std::string error;
    std::filesystem::path themeDir;
};

// Extract classic Delay Lama theme from the original Windows DLL.
// Reads BMP data at known file offsets — works on all platforms.
// Validates the DLL by file size and CRC32 checksum.
ExtractionResult extractClassicTheme(const std::filesystem::path &dllPath,
                                     const std::filesystem::path &configDir);

} // namespace MonkSynth

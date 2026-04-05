#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace MonkSynth {

class ThemeManager {
  public:
    ThemeManager();

    void loadConfig();
    void saveConfig() const;

    void setThemePath(const std::filesystem::path &path);
    void resetTheme();

    bool hasTheme() const { return !themePath_.empty(); }
    const std::filesystem::path &themePath() const { return themePath_; }

    // Returns absolute path to a themed bitmap file, or nullopt if not present.
    // |bitmapName| is the UIDESC logical name (e.g. "background", "monk_strip").
    std::optional<std::filesystem::path> resolveThemeBitmap(const std::string &bitmapName) const;

    // Read theme.json metadata from the active theme folder.
    std::string getThemeName() const;

    // Logical bitmap name -> expected filename in theme folder.
    static const std::unordered_map<std::string, std::string> &bitmapFileMap();

    // Classic theme support
    std::filesystem::path getClassicThemeDir() const;
    bool hasClassicTheme() const;
    void autoDetectClassicTheme();

  private:
    static std::filesystem::path getConfigDir();
    static std::filesystem::path getConfigPath();

    std::filesystem::path themePath_;
};

} // namespace MonkSynth

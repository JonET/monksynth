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

    // Language preference stored in config.json. Values: "auto", "en", "ja",
    // "ko". Empty string is treated as "auto" and falls back to OS detection.
    const std::string &languagePref() const { return languagePref_; }
    void setLanguagePref(const std::string &pref);

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

    // Platform-specific user config/themes paths. Exposed so the UI can
    // reveal the themes folder from the right-click menu without duplicating
    // the path logic.
    static std::filesystem::path getConfigDir();
    static std::filesystem::path getThemesDir();

  private:
    static std::filesystem::path getConfigPath();

    std::filesystem::path themePath_;
    std::string languagePref_;
};

} // namespace MonkSynth

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace MonkSynth {

class ThemeManager {
  public:
    ThemeManager();

    void loadConfig();
    void saveConfig() const;

    // |bundled| marks a theme that lives inside the plugin bundle; it is
    // persisted as "bundled:<folder>" so the choice survives plugin updates
    // and is shared between the VST3 and AU copies.
    void setThemePath(const std::filesystem::path &path, bool bundled = false);
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

    // Metadata from a theme.json manifest. Every field is optional in the
    // file; |name| falls back to the folder name.
    struct ThemeInfo {
        std::string name;
        std::string author;
        std::string version;
        std::string description;
        std::string url;
    };

    // Read theme.json metadata from the active theme folder.
    std::string getThemeName() const;
    ThemeInfo getThemeInfo() const;

    // Read theme.json metadata from any theme folder.
    static ThemeInfo readThemeInfo(const std::filesystem::path &themeDir);

    // A theme folder found under getThemesDir() or getBundledThemesDir(),
    // for the context menu's theme switcher. |name| comes from theme.json,
    // falling back to the folder name. |bundled| is true for themes shipped
    // inside the plugin bundle (read-only).
    struct InstalledTheme {
        std::string name;
        std::filesystem::path path;
        bool bundled = false;
    };

    // Scans both theme folders for subfolders containing a theme.json,
    // sorted by display name. Never throws; missing folders are skipped.
    static std::vector<InstalledTheme> listInstalledThemes();

    // Logical bitmap name -> expected filename in theme folder.
    static const std::unordered_map<std::string, std::string> &bitmapFileMap();

    // Classic theme support
    static constexpr const char *kClassicThemeName = "Classic Delay Lama";
    static constexpr const char *kClassicThemeAuthor = "AudioNerdz";
    static constexpr const char *kClassicThemeDescription =
        "Artwork extracted from the original Delay Lama VST plugin (2002).";
    static constexpr const char *kClassicThemeUrl = "http://www.audionerdz.nl/";
    std::filesystem::path getClassicThemeDir() const;
    bool hasClassicTheme() const;
    void autoDetectClassicTheme();

    // Platform-specific user config/themes paths. Exposed so the UI can
    // reveal the themes folder from the right-click menu without duplicating
    // the path logic.
    static std::filesystem::path getConfigDir();
    static std::filesystem::path getThemesDir();

    // Themes shipped inside the plugin bundle (Contents/Resources/themes).
    // Empty if the bundle has none.
    static std::filesystem::path getBundledThemesDir();

  private:
    static std::filesystem::path getConfigPath();

    std::filesystem::path themePath_;
    std::string bundledName_; // non-empty when themePath_ is a bundled theme
    std::string languagePref_;
};

} // namespace MonkSynth

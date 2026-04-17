#include "theme_manager.h"

#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <shlobj.h>
#elif defined(__APPLE__)
#include <pwd.h>
#include <unistd.h>
#else
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#endif

namespace MonkSynth {

namespace fs = std::filesystem;

// --- Bitmap name -> theme filename mapping ---

const std::unordered_map<std::string, std::string> &ThemeManager::bitmapFileMap() {
    static const std::unordered_map<std::string, std::string> map = {
        {"background", "background.png"},
        {"monk_strip", "monk-strip.png"},
        {"knob_left", "knob-left.png"},
        {"knob_right", "knob-right.png"},
        {"fader_handle", "fader-down-large.png"},
        {"fader_sm_down", "fader-down-sm.png"},
        {"fader_sm_right", "fader-right-sm.png"},
        {"info_overlay", "info.png"},
    };
    return map;
}

// --- Platform config paths ---

fs::path ThemeManager::getConfigDir() {
#ifdef _WIN32
    wchar_t *appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        fs::path dir = fs::path(appData) / "MonkSynth";
        CoTaskMemFree(appData);
        return dir;
    }
    return fs::path(".");
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    if (!home)
        home = getpwuid(getuid())->pw_dir;
    return fs::path(home) / "Library" / "Application Support" / "MonkSynth";
#else
    const char *configHome = getenv("XDG_CONFIG_HOME");
    if (configHome && configHome[0] != '\0')
        return fs::path(configHome) / "MonkSynth";
    const char *home = getenv("HOME");
    if (!home)
        home = getpwuid(getuid())->pw_dir;
    return fs::path(home) / ".config" / "MonkSynth";
#endif
}

fs::path ThemeManager::getConfigPath() { return getConfigDir() / "config.json"; }

fs::path ThemeManager::getThemesDir() { return getConfigDir() / "themes"; }

// --- Constructor ---

ThemeManager::ThemeManager() = default;

// --- Minimal JSON helpers (avoid adding a dependency for one field) ---

// Extract a string value for a given key from a simple JSON object.
static std::string jsonGetString(const std::string &json, const std::string &key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        return {};

    // Find the colon after the key.
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return {};

    // Find the opening quote of the value.
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos)
        return {};

    auto end = json.find('"', pos + 1);
    if (end == std::string::npos)
        return {};

    return json.substr(pos + 1, end - pos - 1);
}

// --- Config I/O ---

void ThemeManager::loadConfig() {
    auto path = getConfigPath();
    std::ifstream f(path);
    if (!f.is_open())
        return;

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    std::string tp = jsonGetString(json, "themePath");
    if (!tp.empty()) {
        fs::path candidate(tp);
        if (fs::is_directory(candidate))
            themePath_ = candidate;
    }

    std::string lang = jsonGetString(json, "language");
    if (!lang.empty())
        languagePref_ = lang;

    std::string adv = jsonGetString(json, "advanced");
    advancedMode_ = (adv == "true");
}

void ThemeManager::saveConfig() const {
    auto dir = getConfigDir();
    std::error_code ec;
    fs::create_directories(dir, ec);

    auto path = getConfigPath();
    std::ofstream f(path);
    if (!f.is_open())
        return;

    f << "{\n";
    if (!themePath_.empty()) {
        // Write path with forward slashes for cross-platform readability.
        std::string pathStr = themePath_.generic_string();
        f << "  \"themePath\": \"" << pathStr << "\",\n";
    } else {
        f << "  \"themePath\": \"\",\n";
    }
    f << "  \"language\": \"" << languagePref_ << "\",\n";
    f << "  \"advanced\": \"" << (advancedMode_ ? "true" : "false") << "\"\n";
    f << "}\n";
}

// --- Theme path management ---

void ThemeManager::setThemePath(const fs::path &path) {
    themePath_ = path;
    saveConfig();
}

void ThemeManager::resetTheme() {
    themePath_.clear();
    saveConfig();
}

void ThemeManager::setLanguagePref(const std::string &pref) {
    languagePref_ = pref;
    saveConfig();
}

void ThemeManager::setAdvancedMode(bool v) {
    advancedMode_ = v;
    saveConfig();
}

// --- Bitmap resolution ---

std::optional<fs::path> ThemeManager::resolveThemeBitmap(const std::string &bitmapName) const {
    if (themePath_.empty())
        return std::nullopt;

    auto &map = bitmapFileMap();
    auto it = map.find(bitmapName);
    if (it == map.end())
        return std::nullopt;

    fs::path candidate = themePath_ / it->second;
    if (fs::is_regular_file(candidate))
        return candidate;

    return std::nullopt;
}

// --- Theme metadata ---

std::string ThemeManager::getThemeName() const {
    if (themePath_.empty())
        return "Default";

    fs::path manifest = themePath_ / "theme.json";
    std::ifstream f(manifest);
    if (!f.is_open())
        return themePath_.filename().string();

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string name = jsonGetString(ss.str(), "name");
    return name.empty() ? themePath_.filename().string() : name;
}

// --- Classic theme support ---

fs::path ThemeManager::getClassicThemeDir() const { return getConfigDir() / "themes" / "classic"; }

bool ThemeManager::hasClassicTheme() const {
    fs::path dir = getClassicThemeDir();
    if (!fs::is_directory(dir))
        return false;

    // Check that all expected theme files exist
    for (auto &[name, filename] : bitmapFileMap()) {
        if (!fs::is_regular_file(dir / filename))
            return false;
    }
    return true;
}

void ThemeManager::autoDetectClassicTheme() {
    if (hasTheme())
        return;
    if (hasClassicTheme())
        setThemePath(getClassicThemeDir());
}

} // namespace MonkSynth

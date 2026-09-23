#include "theme_manager.h"
#include "plugin_paths.h"

#include <algorithm>
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

fs::path ThemeManager::getBundledThemesDir() {
    fs::path res = getPluginResourcesDir();
    if (res.empty())
        return {};
    std::error_code ec;
    fs::path dir = res / "themes";
    return fs::is_directory(dir, ec) ? dir : fs::path{};
}

// Bundled themes are stored in config.json as "bundled:<folder>" rather than
// an absolute path, so the choice survives plugin updates and is shared
// between the VST3 and AU copies of the bundle.
static const char *kBundledPrefix = "bundled:";

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
        fs::path candidate;
        std::string bundledName;
        if (tp.rfind(kBundledPrefix, 0) == 0) {
            bundledName = tp.substr(std::string(kBundledPrefix).size());
            fs::path dir = getBundledThemesDir();
            if (!dir.empty() && !bundledName.empty())
                candidate = dir / bundledName;
        } else {
            candidate = fs::path(tp);
        }
        std::error_code ec;
        if (!candidate.empty() && fs::is_directory(candidate, ec)) {
            themePath_ = candidate;
            bundledName_ = bundledName;
        }
    }

    std::string lang = jsonGetString(json, "language");
    if (!lang.empty())
        languagePref_ = lang;
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
    if (!bundledName_.empty()) {
        f << "  \"themePath\": \"" << kBundledPrefix << bundledName_ << "\",\n";
    } else if (!themePath_.empty()) {
        // Write path with forward slashes for cross-platform readability.
        std::string pathStr = themePath_.generic_string();
        f << "  \"themePath\": \"" << pathStr << "\",\n";
    } else {
        f << "  \"themePath\": \"\",\n";
    }
    f << "  \"language\": \"" << languagePref_ << "\"\n";
    f << "}\n";
}

// --- Theme path management ---

void ThemeManager::setThemePath(const fs::path &path, bool bundled) {
    themePath_ = path;
    bundledName_ = bundled ? path.filename().string() : std::string();
    saveConfig();
}

void ThemeManager::resetTheme() {
    themePath_.clear();
    bundledName_.clear();
    saveConfig();
}

void ThemeManager::setLanguagePref(const std::string &pref) {
    languagePref_ = pref;
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

// Parse a theme folder's theme.json. Missing manifest or fields leave the
// corresponding strings empty, except |name| which falls back to the folder
// name so every theme has something to display.
static ThemeManager::ThemeInfo readThemeManifest(const fs::path &themeDir) {
    ThemeManager::ThemeInfo info;
    info.name = themeDir.filename().string();

    std::ifstream f(themeDir / "theme.json");
    if (!f.is_open())
        return info;

    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string json = ss.str();

    std::string name = jsonGetString(json, "name");
    if (!name.empty())
        info.name = name;
    info.author = jsonGetString(json, "author");
    info.version = jsonGetString(json, "version");
    info.description = jsonGetString(json, "description");
    info.url = jsonGetString(json, "url");
    return info;
}

static std::string readThemeName(const fs::path &themeDir) {
    return readThemeManifest(themeDir).name;
}

ThemeManager::ThemeInfo ThemeManager::readThemeInfo(const fs::path &themeDir) {
    return readThemeManifest(themeDir);
}

std::string ThemeManager::getThemeName() const { return getThemeInfo().name; }

ThemeManager::ThemeInfo ThemeManager::getThemeInfo() const {
    if (themePath_.empty())
        return {"Default", {}, {}, {}, {}};

    ThemeInfo info = readThemeManifest(themePath_);

    // Classic themes imported before the extractor wrote credits have only a
    // name and version; fill in the AudioNerdz attribution so existing
    // installs don't need to re-import.
    std::error_code ec;
    if (info.author.empty() && fs::equivalent(themePath_, getClassicThemeDir(), ec)) {
        info.author = kClassicThemeAuthor;
        if (info.description.empty())
            info.description = kClassicThemeDescription;
        if (info.url.empty())
            info.url = kClassicThemeUrl;
    }
    return info;
}

static void scanThemesDir(const fs::path &dir, bool bundled,
                          std::vector<ThemeManager::InstalledTheme> &out) {
    if (dir.empty())
        return;
    std::error_code ec;
    fs::directory_iterator it(dir, ec);
    if (ec)
        return;

    for (const auto &entry : it) {
        if (!entry.is_directory(ec) || ec)
            continue;
        if (!fs::is_regular_file(entry.path() / "theme.json", ec))
            continue;
        out.push_back({readThemeName(entry.path()), entry.path(), bundled});
    }
}

std::vector<ThemeManager::InstalledTheme> ThemeManager::listInstalledThemes() {
    std::vector<InstalledTheme> themes;
    scanThemesDir(getThemesDir(), false, themes);
    scanThemesDir(getBundledThemesDir(), true, themes);

    std::sort(themes.begin(), themes.end(),
              [](const InstalledTheme &a, const InstalledTheme &b) { return a.name < b.name; });
    return themes;
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

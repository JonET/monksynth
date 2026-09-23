#include "theme_gallery.h"
#include "http_fetch.h"
#include "theme_manager.h"

#include "rapidjson/document.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <set>

namespace MonkSynth {

namespace fs = std::filesystem;

// Keep in sync with scripts/build_index.py in the themes repo.
static constexpr std::uint64_t kMaxFileBytes = 8ull * 1024 * 1024;
static constexpr std::uint64_t kMaxThemeBytes = 24ull * 1024 * 1024;
static constexpr std::size_t kMaxIndexBytes = 2 * 1024 * 1024;
static constexpr int kIndexSchema = 1;

// --- Index parsing ---

static bool validId(const std::string &id) {
    static const std::regex re("^[a-z0-9][a-z0-9-]{0,39}$");
    return std::regex_match(id, re);
}

// Everything a theme may contain. Anything else in the index is refused, so
// a file name can never carry a path.
static const std::set<std::string> &allowedFiles() {
    static const std::set<std::string> names = [] {
        std::set<std::string> s{"theme.json", "credits.txt"};
        for (auto &[key, file] : ThemeManager::bitmapFileMap())
            s.insert(file);
        return s;
    }();
    return names;
}

static std::string getString(const rapidjson::Value &obj, const char *key) {
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsString())
        return {};
    return std::string(it->value.GetString(), it->value.GetStringLength());
}

static std::optional<GalleryFile> parseFile(const rapidjson::Value &v) {
    if (!v.IsObject())
        return std::nullopt;
    GalleryFile f;
    f.name = getString(v, "name");
    f.sha256 = getString(v, "sha256");
    auto size = v.FindMember("size");
    if (size == v.MemberEnd() || !size->value.IsUint64())
        return std::nullopt;
    f.size = size->value.GetUint64();
    if (f.name.empty() || f.size == 0 || f.size > kMaxFileBytes)
        return std::nullopt;
    if (f.sha256.size() != 64 ||
        !std::all_of(f.sha256.begin(), f.sha256.end(),
                     [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
        f.sha256.clear();
    return f;
}

std::optional<std::vector<GalleryTheme>> parseGalleryIndex(const std::string &json) {
    rapidjson::Document doc;
    doc.Parse(json.c_str(), json.size());
    if (doc.HasParseError() || !doc.IsObject())
        return std::nullopt;
    auto schema = doc.FindMember("schema");
    if (schema == doc.MemberEnd() || !schema->value.IsInt() ||
        schema->value.GetInt() != kIndexSchema)
        return std::nullopt;
    auto list = doc.FindMember("themes");
    if (list == doc.MemberEnd() || !list->value.IsArray())
        return std::nullopt;

    std::vector<GalleryTheme> themes;
    std::set<std::string> seen;
    for (auto &v : list->value.GetArray()) {
        if (!v.IsObject())
            continue;
        GalleryTheme t;
        t.id = getString(v, "id");
        t.name = getString(v, "name");
        if (!validId(t.id) || t.name.empty() || !seen.insert(t.id).second)
            continue;
        t.author = getString(v, "author");
        t.version = getString(v, "version");
        t.description = getString(v, "description");
        t.url = getString(v, "url");

        auto thumb = v.FindMember("thumb");
        if (thumb != v.MemberEnd()) {
            if (auto f = parseFile(thumb->value); f && f->name == "thumb.png")
                t.thumb = *f;
        }
        auto preview = v.FindMember("preview");
        if (preview != v.MemberEnd()) {
            if (auto f = parseFile(preview->value); f && f->name == "preview.png")
                t.preview = *f;
        }

        auto files = v.FindMember("files");
        if (files == v.MemberEnd() || !files->value.IsArray())
            continue;
        bool ok = true;
        std::set<std::string> names;
        for (auto &fv : files->value.GetArray()) {
            auto f = parseFile(fv);
            if (!f || !allowedFiles().count(f->name) || !names.insert(f->name).second) {
                ok = false;
                break;
            }
            t.size += f->size;
            t.files.push_back(*f);
        }
        if (!ok || !names.count("theme.json") || t.size > kMaxThemeBytes)
            continue;
        themes.push_back(std::move(t));
    }
    return themes;
}

// --- Paths ---

fs::path ThemeGallery::installDir(const std::string &id) { return ThemeManager::getThemesDir() / id; }

std::string ThemeGallery::baseUrl() {
    const char *env = std::getenv("MONKSYNTH_GALLERY_URL");
    std::string url = (env && env[0]) ? env
                                      : "https://raw.githubusercontent.com/JonET/monksynth-themes/main/";
    if (url.back() != '/')
        url += '/';
    return url;
}

ThemeGallery::InstalledCopy ThemeGallery::installedCopy(const std::string &id) {
    InstalledCopy c;
    std::error_code ec;
    fs::path user = installDir(id);
    fs::path bundledRoot = ThemeManager::getBundledThemesDir();
    fs::path bundled = bundledRoot.empty() ? fs::path() : bundledRoot / id;
    c.user = fs::is_regular_file(user / "theme.json", ec);
    c.bundled = !bundled.empty() && fs::is_regular_file(bundled / "theme.json", ec);
    if (c.user)
        c.path = user;
    else if (c.bundled)
        c.path = bundled;
    if (c.installed())
        c.version = ThemeManager::readThemeInfo(c.path).version;
    return c;
}

bool ThemeGallery::isActive(const InstalledCopy &copy, const fs::path &activeTheme) {
    if (!copy.installed() || activeTheme.empty())
        return false;
    std::error_code ec;
    if (copy.user && fs::equivalent(copy.path, activeTheme, ec))
        return true;
    fs::path bundledRoot = ThemeManager::getBundledThemesDir();
    return copy.bundled && !bundledRoot.empty() &&
           fs::equivalent(bundledRoot / copy.path.filename(), activeTheme, ec);
}

static fs::path cacheDir() { return ThemeManager::getConfigDir() / "cache" / "gallery"; }
static fs::path stagingDir() { return ThemeManager::getConfigDir() / "cache" / "staging"; }

static std::string fileUrl(const std::string &id, const std::string &name) {
    return ThemeGallery::baseUrl() + "themes/" + id + "/" + name;
}

static bool writeFile(const fs::path &path, const std::string &data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open())
        return false;
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

// --- Worker ---

ThemeGallery::ThemeGallery() : worker_([this] { run(); }) {}

ThemeGallery::~ThemeGallery() {
    stop_ = true;
    queueCv_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

void ThemeGallery::post(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push_back(std::move(job));
    }
    queueCv_.notify_one();
}

void ThemeGallery::run() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_)
                return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        try {
            job();
        } catch (...) {
            // A filesystem or allocation failure must not take the host down;
            // the job's own error state is the best we can report.
        }
    }
}

ThemeGallery::Snapshot ThemeGallery::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snap_;
}

void ThemeGallery::refresh() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snap_.state == IndexState::Loading)
            return;
        snap_.state = IndexState::Loading;
        snap_.indexError.clear();
    }
    bump();
    post([this] { fetchIndex(); });
}

void ThemeGallery::fetchIndex() {
    HttpResult r = httpGet(baseUrl() + "index.json", kMaxIndexBytes);
    std::optional<std::vector<GalleryTheme>> themes;
    if (r.ok)
        themes = parseGalleryIndex(r.body);

    std::vector<std::pair<GalleryTheme, Image>> needImages;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!themes) {
            snap_.state = IndexState::Failed;
            snap_.indexError = r.ok ? "The theme list couldn't be read." : r.error;
        } else {
            snap_.state = IndexState::Ready;
            snap_.themes = std::move(*themes);
            for (auto &t : snap_.themes) {
                auto &st = snap_.status[t.id];
                if (st.thumbPath.empty() && t.thumb.size > 0)
                    needImages.push_back({t, Image::Thumb});
                if (st.previewPath.empty() && t.preview.size > 0)
                    needImages.push_back({t, Image::Preview});
            }
        }
    }
    bump();
    for (auto &[t, which] : needImages)
        post([this, t = t, which = which] { fetchImage(t, which); });
}

void ThemeGallery::fetchImage(const GalleryTheme &theme, Image which) {
    const GalleryFile &file = which == Image::Thumb ? theme.thumb : theme.preview;
    // Keyed by content hash so an updated image replaces the cached one.
    std::string key = file.sha256.empty() ? std::to_string(file.size) : file.sha256.substr(0, 16);
    std::string kind = which == Image::Thumb ? "thumb" : "preview";
    fs::path path = cacheDir() / (theme.id + "-" + kind + "-" + key + ".png");
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        HttpResult r = httpGet(fileUrl(theme.id, file.name), kMaxFileBytes);
        if (!r.ok || r.body.size() != file.size)
            return; // the view keeps its placeholder
        fs::create_directories(cacheDir(), ec);
        if (!writeFile(path, r.body))
            return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &st = snap_.status[theme.id];
        (which == Image::Thumb ? st.thumbPath : st.previewPath) = path;
    }
    bump();
}

void ThemeGallery::install(const std::string &id) {
    GalleryTheme theme;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(snap_.themes.begin(), snap_.themes.end(),
                               [&](const GalleryTheme &t) { return t.id == id; });
        if (it == snap_.themes.end())
            return;
        auto &st = snap_.status[id];
        if (st.installing)
            return;
        st.installing = true;
        st.progress = 0.0f;
        st.error.clear();
        theme = *it;
    }
    bump();
    post([this, theme] { doInstall(theme); });
}

void ThemeGallery::doInstall(const GalleryTheme &theme) {
    auto finish = [&](const std::string &error) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto &st = snap_.status[theme.id];
            st.installing = false;
            st.progress = error.empty() ? 1.0f : 0.0f;
            st.error = error;
            if (error.empty())
                finished_.push_back(theme.id);
        }
        bump();
    };

    // Download everything into a staging folder first, so a failed or
    // interrupted download never leaves a half-written theme in the menu.
    std::error_code ec;
    fs::path staging = stagingDir() / theme.id;
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    if (ec)
        return finish("Couldn't create a download folder.");

    std::uint64_t done = 0;
    for (const auto &file : theme.files) {
        if (stop_)
            return;
        HttpResult r = httpGet(fileUrl(theme.id, file.name), file.size);
        if (!r.ok)
            return finish(r.error);
        if (r.body.size() != file.size)
            return finish("A file didn't download completely.");
        if (!writeFile(staging / file.name, r.body))
            return finish("Couldn't write the theme files.");
        done += file.size;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snap_.status[theme.id].progress =
                theme.size ? static_cast<float>(done) / static_cast<float>(theme.size) : 1.0f;
        }
        bump();
    }

    // Swap the new copy in. The old one is moved aside rather than deleted
    // first, so it can be restored if the move fails.
    fs::path dest = installDir(theme.id);
    fs::path old = stagingDir() / (theme.id + ".old");
    fs::create_directories(dest.parent_path(), ec);
    fs::remove_all(old, ec);
    bool hadOld = fs::exists(dest, ec);
    if (hadOld) {
        fs::rename(dest, old, ec);
        if (ec)
            return finish("The installed copy is in use. Close the plugin window and try again.");
    }
    fs::rename(staging, dest, ec);
    if (ec) {
        if (hadOld)
            fs::rename(old, dest, ec);
        return finish("Couldn't move the theme into the themes folder.");
    }
    fs::remove_all(old, ec);
    finish({});
}

std::vector<std::string> ThemeGallery::takeFinishedInstalls() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    out.swap(finished_);
    return out;
}

} // namespace MonkSynth

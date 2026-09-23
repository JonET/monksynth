#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace MonkSynth {

// One file of a gallery theme, as listed in the repo's index.json.
struct GalleryFile {
    std::string name;
    std::uint64_t size = 0;
    std::string sha256; // hex; used only to key the thumbnail cache
};

struct GalleryTheme {
    std::string id; // folder name in the repo and in the local themes dir
    std::string name;
    std::string author;
    std::string version;
    std::string description;
    std::string url;
    GalleryFile thumb;   // 112x112 face crop for list rows
    GalleryFile preview; // the whole 360x510 editor, idle
    std::uint64_t size = 0; // total of |files|
    std::vector<GalleryFile> files;
};

// Parses the gallery's index.json. Returns nullopt if the document itself is
// unusable (bad JSON, unknown schema). Individual entries with an invalid id,
// a file name outside the theme file list, or sizes over the limits are
// dropped, so a bad entry can't be used to write outside its theme folder.
std::optional<std::vector<GalleryTheme>> parseGalleryIndex(const std::string &json);

// Client for the community theme repo (github.com/JonET/monksynth-themes).
// Network and disk work runs on one background thread; the UI reads a
// snapshot and polls revision() to know when to redraw. Owned by the
// Controller so a download keeps going if the editor is closed.
class ThemeGallery {
  public:
    static constexpr const char *kRepoUrl = "https://github.com/JonET/monksynth-themes";

    // Where index.json and the theme folders are fetched from: the repo's
    // main branch, or $MONKSYNTH_GALLERY_URL if set (for trying a fork).
    static std::string baseUrl();

    ThemeGallery();
    // Stops the worker, waiting for a request in flight to finish or time out.
    ~ThemeGallery();

    enum class IndexState { Idle, Loading, Ready, Failed };

    struct ThemeStatus {
        bool installing = false;
        float progress = 0.0f; // 0..1 while installing
        std::string error;     // last install error, cleared on retry
        std::filesystem::path thumbPath;   // cached images once downloaded
        std::filesystem::path previewPath;
    };

    struct Snapshot {
        IndexState state = IndexState::Idle;
        std::string indexError;
        std::vector<GalleryTheme> themes;
        std::map<std::string, ThemeStatus> status;
    };

    // Bumped on every change to the snapshot.
    std::uint64_t revision() const { return revision_.load(); }
    Snapshot snapshot() const;

    // Fetches the index (then any missing thumbnails). Ignored while a fetch
    // is already running.
    void refresh();
    // Downloads |id| into the user themes folder, replacing an older copy.
    // Ignored if unknown or already installing.
    void install(const std::string &id);

    // Ids whose install finished since the last call, so the UI can re-apply
    // a theme that was updated while in use.
    std::vector<std::string> takeFinishedInstalls();

    // Folder a gallery theme installs to.
    static std::filesystem::path installDir(const std::string &id);

    // What's on disk for a gallery id: a user copy in the themes folder
    // (preferred) and/or a copy shipped in the plugin bundle.
    struct InstalledCopy {
        bool user = false;
        bool bundled = false;
        std::string version;
        std::filesystem::path path;
        bool installed() const { return user || bundled; }
    };
    static InstalledCopy installedCopy(const std::string &id);
    // Whether |copy| is the theme at |activeTheme|.
    static bool isActive(const InstalledCopy &copy, const std::filesystem::path &activeTheme);

  private:
    void post(std::function<void()> job);
    void run();
    void bump() { revision_.fetch_add(1); }
    void fetchIndex();
    enum class Image { Thumb, Preview };
    void fetchImage(const GalleryTheme &theme, Image which);
    void doInstall(const GalleryTheme &theme);

    mutable std::mutex mutex_; // guards everything below except the queue
    Snapshot snap_;
    std::vector<std::string> finished_;
    std::atomic<std::uint64_t> revision_{1};

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<std::function<void()>> queue_;
    std::atomic<bool> stop_{false};
    std::thread worker_;
};

} // namespace MonkSynth

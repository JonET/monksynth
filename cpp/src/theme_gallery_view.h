#pragma once

#include "gallery_ui.h"
#include "theme_gallery.h"

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/cvstguitimer.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace MonkSynth {

// The wide theme gallery: filter chips and a grid of theme cards on the left,
// a large preview with the full description and actions on the right. It is
// the whole editor while open (the "gallery" template, 1120x720); Done swaps
// back to the synth. Applying a theme keeps the gallery open, and the synth
// picks it up when it comes back.
class ThemeGalleryView : public VSTGUI::CView {
  public:
    using ActiveThemeFn = std::function<std::filesystem::path()>;
    using ApplyFn = std::function<void(const std::filesystem::path &, bool bundled)>;
    using CloseFn = std::function<void()>;

    ThemeGalleryView(const VSTGUI::CRect &size, ThemeGallery *gallery, ActiveThemeFn activeTheme,
                     ApplyFn onApply, CloseFn onClose);
    ~ThemeGalleryView() override;

    void draw(VSTGUI::CDrawContext *ctx) override;
    bool removed(VSTGUI::CView *parent) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent &event) override;

  private:
    enum class Filter { All, Installed, Available };
    enum class Action {
        None, Close, OpenFolder, Refresh, FilterAll, FilterInstalled, FilterAvailable,
        Select, Download, Apply, Remove, Spec, Submit, AuthorLink
    };
    struct Hit {
        VSTGUI::CRect rect; // relative to the view
        Action action;
        std::string id;
    };

    void poll();
    void sync();
    bool passesFilter(const GalleryTheme &t) const;
    VSTGUI::CBitmap *previewFor(const std::string &id);
    const GalleryTheme *selectedTheme() const;
    const Hit *hitAt(const VSTGUI::CPoint &local) const;
    void perform(const Hit &hit);
    void addHit(VSTGUI::CRect r, Action a, const std::string &id = {});

    void drawHeader(VSTGUI::CDrawContext *ctx);
    void drawBanner(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &r);
    void drawChips(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &row);
    void drawGrid(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &area);
    void drawCard(VSTGUI::CDrawContext *ctx, const GalleryTheme &t, const VSTGUI::CRect &card);
    void drawMakeYourOwn(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &card,
                         const VSTGUI::CRect &clip);
    void drawDetail(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &panel);

    ThemeGallery *gallery_;
    ActiveThemeFn activeTheme_;
    ApplyFn onApply_;
    CloseFn onClose_;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer_;

    std::uint64_t seenRevision_ = 0;
    ThemeGallery::Snapshot snap_;
    std::map<std::string, gallery_ui::ThemeState> states_; // rebuilt by sync()
    std::map<std::string, VSTGUI::SharedPointer<VSTGUI::CBitmap>> previews_;
    std::string selectedId_;
    Filter filter_ = Filter::All;
    VSTGUI::CCoord scrollY_ = 0;
    VSTGUI::CCoord maxScroll_ = 0;
    VSTGUI::CRect gridArea_; // relative to the view
    std::vector<Hit> hits_;  // rebuilt on every draw
    Action hoverAction_ = Action::None;
    std::string hoverId_;

    // Refresh feedback: the icon spins while the index loads (and for a
    // moment after, so a fast fetch is still visible), then "Up to date"
    // shows briefly.
    using Clock = std::chrono::steady_clock;
    bool wasLoading_ = false;
    Clock::time_point spinStart_{};
    Clock::time_point spinUntil_{};
    Clock::time_point upToDateUntil_{};
    bool spinning(Clock::time_point now) const;
};

} // namespace MonkSynth

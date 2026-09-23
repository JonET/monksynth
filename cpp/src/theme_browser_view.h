#pragma once

#include "gallery_ui.h"
#include "overlay_view.h"
#include "theme_gallery.h"

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cvstguitimer.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace MonkSynth {

// Compact theme gallery: a scrolling list of the community themes with a
// details-and-actions footer, sized to the 360x510 editor. The fallback for
// hosts that won't grow the window for the wide ThemeGalleryView.
class ThemeBrowserView : public OverlayView {
  public:
    // |activeTheme| returns the folder of the theme in use; |onApply|
    // switches to a theme folder (which rebuilds the editor and closes this
    // view).
    using ActiveThemeFn = std::function<std::filesystem::path()>;
    using ApplyFn = std::function<void(const std::filesystem::path &, bool bundled)>;

    ThemeBrowserView(const VSTGUI::CRect &size, ThemeGallery *gallery, ActiveThemeFn activeTheme,
                     ApplyFn onApply);
    ~ThemeBrowserView() override;

    bool removed(VSTGUI::CView *parent) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent &event) override;

  protected:
    void drawBody(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &bounds) override;
    bool hitLink(const VSTGUI::CPoint &local, bool click) override;

  private:
    enum class Action { None, Close, OpenFolder, Retry, Select, Download, Apply, Remove, Spec, Submit };
    struct Hit {
        VSTGUI::CRect rect; // relative to the view
        Action action;
        std::string id;
    };

    void poll();
    void sync();
    VSTGUI::CBitmap *thumbFor(const std::string &id);
    const GalleryTheme *selectedTheme() const;
    const Hit *hitAt(const VSTGUI::CPoint &local) const;
    void perform(const Hit &hit);

    void drawHeader(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &bounds);
    void drawList(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &area);
    void drawFooter(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &area);

    ThemeGallery *gallery_;
    ActiveThemeFn activeTheme_;
    ApplyFn onApply_;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer_;

    std::uint64_t seenRevision_ = 0;
    ThemeGallery::Snapshot snap_;
    std::map<std::string, gallery_ui::ThemeState> states_; // rebuilt by sync()
    std::map<std::string, VSTGUI::SharedPointer<VSTGUI::CBitmap>> thumbs_;
    std::string selectedId_;
    VSTGUI::CCoord scrollY_ = 0;
    VSTGUI::CCoord maxScroll_ = 0;
    VSTGUI::CRect listArea_;
    std::vector<Hit> hits_; // rebuilt on every draw
    Action hoverAction_ = Action::None;
    std::string hoverId_;
};

} // namespace MonkSynth

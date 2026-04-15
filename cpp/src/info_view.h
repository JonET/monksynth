#pragma once

#include "vstgui/lib/cviewcontainer.h"

#include <functional>

namespace MonkSynth {

// Info overlay shown when the user clicks the "?" button.
// Displays project info, license, creator credit, and links.
class InfoView : public VSTGUI::CViewContainer {
  public:
    InfoView(const VSTGUI::CRect &size);

    using CloseCallback = std::function<void()>;
    void setCloseCallback(CloseCallback cb) { closeCb_ = std::move(cb); }

    void drawBackgroundRect(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &rect) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;

  private:
    VSTGUI::CRect closeBtnRect_;
    VSTGUI::CRect githubLinkRect_;
    VSTGUI::CRect openFolderRect_;
    CloseCallback closeCb_;
};

} // namespace MonkSynth

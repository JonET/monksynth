#pragma once

#include "vstgui/lib/cviewcontainer.h"

#include <functional>

namespace MonkSynth {

// Full-frame modal panel with the shared dark chrome (background, accent
// line, Close button). Subclasses draw their body and report link hits.
// With |standardChrome| off, drawBody paints the whole view and the subclass
// handles its own clicks, calling requestClose() to dismiss.
class OverlayView : public VSTGUI::CViewContainer {
  public:
    explicit OverlayView(const VSTGUI::CRect &size, bool standardChrome = true);

    using CloseCallback = std::function<void()>;
    void setCloseCallback(CloseCallback cb) { closeCb_ = std::move(cb); }

    void drawBackgroundRect(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &rect) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;

  protected:
    // Draw everything between the accent line and the Close button.
    // |bounds| is the view's absolute rect; body content must stay above
    // bounds.top + closeButtonTop().
    virtual void drawBody(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &bounds) = 0;

    // |local| is relative to the view. Return true if it's over a link; when
    // |click| is set, also perform the link's action.
    virtual bool hitLink(const VSTGUI::CPoint &local, bool click) = 0;

    // Top of the Close button, relative to the view.
    VSTGUI::CCoord closeButtonTop() const { return closeBtnRect_.top; }

    // Runs the close callback; the owner removes the view later.
    void requestClose();

  private:
    bool standardChrome_ = true;
    VSTGUI::CRect closeBtnRect_;
    CloseCallback closeCb_;
};

} // namespace MonkSynth

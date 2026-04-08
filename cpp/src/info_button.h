#pragma once

#include "vstgui/lib/cview.h"

#include <functional>

namespace MonkSynth {

// Invisible hit zone over the skin's "?" area.
class InfoButton : public VSTGUI::CView {
  public:
    using ClickCallback = std::function<void()>;

    InfoButton(const VSTGUI::CRect &size);

    void setClickCallback(ClickCallback cb) { clickCb_ = std::move(cb); }

    void draw(VSTGUI::CDrawContext *ctx) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;

  private:
    ClickCallback clickCb_;
};

} // namespace MonkSynth

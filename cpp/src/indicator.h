#pragma once

#include "vstgui/vstgui.h"

namespace MonkSynth {

// Read-only slider indicator. Draws a handle bitmap at a normalized
// position (0-1) without accepting any mouse input. Used for the
// XY pad faders that display smoothed pitch/vowel state.
class Indicator : public VSTGUI::CControl {
  public:
    Indicator(const VSTGUI::CRect &size, VSTGUI::CBitmap *handle, bool vertical)
        : CControl(size, nullptr, -1), handle_(handle), vertical_(vertical) {
        if (handle_)
            handle_->remember();
        setMouseEnabled(false);
        setWantsFocus(false);
    }

    ~Indicator() override {
        if (handle_)
            handle_->forget();
    }

    void draw(VSTGUI::CDrawContext *context) override {
        if (!handle_)
            return;

        VSTGUI::CRect r = getViewSize();
        float val = getValueNormalized();

        VSTGUI::CCoord hw = handle_->getWidth() / 2.0;
        VSTGUI::CCoord hh = handle_->getHeight() / 2.0;
        VSTGUI::CCoord x, y;

        if (vertical_) {
            x = r.left + r.getWidth() / 2.0 - hw;
            y = r.bottom - hh - val * (r.getHeight() - handle_->getHeight());
        } else {
            x = r.left + val * (r.getWidth() - handle_->getWidth());
            y = r.top + r.getHeight() / 2.0 - hh;
        }

        handle_->draw(context, VSTGUI::CRect(x, y, x + handle_->getWidth(), y + handle_->getHeight()));
        setDirty(false);
    }

    CLASS_METHODS(Indicator, CControl)

  private:
    VSTGUI::CBitmap *handle_ = nullptr;
    bool vertical_ = false;
};

} // namespace MonkSynth

#pragma once

#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/controls/ccontrol.h"

namespace MonkSynth {

class ArcKnob : public VSTGUI::CControl {
  public:
    ArcKnob(const VSTGUI::CRect &size, VSTGUI::IControlListener *listener, int32_t tag);

    void draw(VSTGUI::CDrawContext *pContext) override;

    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint &where,
                                        const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseCancel() override;
    bool onWheel(const VSTGUI::CPoint &where, const VSTGUI::CMouseWheelAxis &axis,
                 const float &distance, const VSTGUI::CButtonState &buttons) override;

    void setTrackColor(const VSTGUI::CColor &c) { trackColor_ = c; }
    void setFillColor(const VSTGUI::CColor &c) { fillColor_ = c; }
    void setTrackWidth(VSTGUI::CCoord w) { trackWidth_ = w; }
    void setArcWidth(VSTGUI::CCoord w) { arcWidth_ = w; }

    CLASS_METHODS(ArcKnob, CControl)

  private:
    static constexpr float kStartAngleDeg = 135.f;
    static constexpr float kAngleRangeDeg = 270.f;

    VSTGUI::CColor trackColor_{0, 0, 0, 128};
    VSTGUI::CColor fillColor_{255, 255, 255, 217};
    VSTGUI::CCoord trackWidth_ = 4.0;
    VSTGUI::CCoord arcWidth_ = 3.0;

    float startValue_ = 0.f;
    VSTGUI::CPoint anchorPoint_;
    bool dragging_ = false;
};

} // namespace MonkSynth

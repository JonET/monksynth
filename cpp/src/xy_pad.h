#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/vstgui.h"

namespace MonkSynth {

// XY pad overlay on the Tibetan flag area. Clicking and dragging
// controls pitch (X) and vowel (Y), while mouse down/up triggers
// note on/off via private parameters routed to the processor.
class XYPadView : public VSTGUI::CControl {
  public:
    XYPadView(const VSTGUI::CRect &size, VSTGUI::IControlListener *listener,
              Steinberg::Vst::EditController *controller);

    void draw(VSTGUI::CDrawContext *context) override;

    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint &where,
                                        const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseCancel() override;

    CLASS_METHODS(XYPadView, CControl)

  private:
    void updateFromMouse(const VSTGUI::CPoint &where);

    Steinberg::Vst::EditController *controller_ = nullptr;
    bool tracking_ = false;
    float xNorm_ = 0.5f;
    float yNorm_ = 0.5f;
};

} // namespace MonkSynth

#pragma once

#include "vstgui/lib/cfont.h"
#include "vstgui/lib/controls/ccontrol.h"

namespace MonkSynth {

// Read-only value display for an ADSR knob. Auto-formats based on its tag:
// Attack/Decay/Release → "250ms" or "1.25s" (uses same quadratic taper
// applied in the processor). Sustain → "75%". Updates live via VSTGUI's
// normal param-listener machinery.
class AdsrValueReadout : public VSTGUI::CControl {
  public:
    AdsrValueReadout(const VSTGUI::CRect &size, VSTGUI::IControlListener *listener, int32_t tag);

    void draw(VSTGUI::CDrawContext *ctx) override;

    CLASS_METHODS(AdsrValueReadout, CControl)

  private:
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> font_;
};

} // namespace MonkSynth

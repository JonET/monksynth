#pragma once

#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cview.h"

namespace MonkSynth {

// Non-interactive ADSR envelope visualization. Draws four linear segments
// (attack up, decay down, sustain hold, release down) matching the DSP's
// linear ramps. The host container keeps us in sync with the ADSR params
// via setAttack/setDecay/setSustain/setRelease.
class AdsrCurve : public VSTGUI::CView {
  public:
    AdsrCurve(const VSTGUI::CRect &size);

    void draw(VSTGUI::CDrawContext *ctx) override;

    void setAttack(float v) { attack_ = v; invalid(); }
    void setDecay(float v) { decay_ = v; invalid(); }
    void setSustain(float v) { sustain_ = v; invalid(); }
    void setRelease(float v) { release_ = v; invalid(); }

    CLASS_METHODS(AdsrCurve, CView)

  private:
    float attack_ = 0.3f;
    float decay_ = 0.3f;
    float sustain_ = 0.7f;
    float release_ = 0.3f;

    VSTGUI::CColor bg_{0, 0, 0, 51};              // rgba(0,0,0,0.2)
    VSTGUI::CColor border_{0, 0, 0, 102};         // rgba(0,0,0,0.4)
    VSTGUI::CColor line_{255, 255, 255, 179};     // rgba(255,255,255,0.7)
    VSTGUI::CColor fill_{255, 255, 255, 15};      // rgba(255,255,255,0.06)
    VSTGUI::CColor dot_{255, 255, 255, 230};      // rgba(255,255,255,0.9)
};

} // namespace MonkSynth

#include "value_readout.h"

#include "plugin_cids.h"
#include "vstgui/lib/cdrawcontext.h"

#include <cmath>
#include <cstdio>

using namespace VSTGUI;

namespace MonkSynth {

AdsrValueReadout::AdsrValueReadout(const CRect &size, IControlListener *listener, int32_t tag)
    : CControl(size, listener, tag) {
    font_ = makeOwned<CFontDesc>("Arial", 9);
    setWantsFocus(false);
    setMouseEnabled(false);
}

void AdsrValueReadout::draw(CDrawContext *ctx) {
    float norm = getValueNormalized();

    char buf[32];
    switch (getTag()) {
        case kAttack:
        case kDecay:
        case kRelease: {
            // Matches processor's quadratic taper and 3s cap.
            double sec = static_cast<double>(norm) * norm * 3.0;
            if (sec < 1.0) {
                int ms = static_cast<int>(std::round(sec * 1000.0));
                std::snprintf(buf, sizeof(buf), "%dms", ms);
            } else {
                std::snprintf(buf, sizeof(buf), "%.2fs", sec);
            }
            break;
        }
        case kSustain: {
            int pct = static_cast<int>(std::round(norm * 100.0));
            std::snprintf(buf, sizeof(buf), "%d%%", pct);
            break;
        }
        default:
            std::snprintf(buf, sizeof(buf), "%.2f", norm);
            break;
    }

    ctx->setFontColor(CColor(255, 255, 255, 170));
    ctx->setFont(font_);
    ctx->drawString(buf, getViewSize(), kCenterText, true);
    setDirty(false);
}

} // namespace MonkSynth

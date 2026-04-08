#include "info_button.h"

#include "vstgui/lib/cframe.h"

using namespace VSTGUI;

namespace MonkSynth {

InfoButton::InfoButton(const CRect &size) : CView(size) {
    setTransparency(true);
}

void InfoButton::draw(CDrawContext * /*ctx*/) {
    // Invisible — the skin provides the visual "?" element
}

CMouseEventResult InfoButton::onMouseMoved(CPoint & /*where*/, const CButtonState & /*buttons*/) {
    auto *frame = getFrame();
    if (frame)
        frame->setCursor(kCursorHand);
    return kMouseEventHandled;
}

CMouseEventResult InfoButton::onMouseExited(CPoint & /*where*/, const CButtonState & /*buttons*/) {
    auto *frame = getFrame();
    if (frame)
        frame->setCursor(kCursorDefault);
    return kMouseEventHandled;
}

CMouseEventResult InfoButton::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;

    if (getViewSize().pointInside(where)) {
        if (clickCb_)
            clickCb_();
        return kMouseEventHandled;
    }

    return kMouseEventNotHandled;
}

} // namespace MonkSynth

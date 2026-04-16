#include "xy_pad.h"
#include "plugin_cids.h"

using namespace VSTGUI;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace MonkSynth {

XYPadView::XYPadView(const CRect &size, IControlListener *listener, EditController *controller)
    : CControl(size, listener, kVowel), controller_(controller) {
    setTransparency(true);
}

void XYPadView::draw(CDrawContext *context) {
    if (!tracking_) {
        setDirty(false);
        return;
    }

    CRect r = getViewSize();
    CCoord cx = r.left + xNorm_ * r.getWidth();
    CCoord cy = r.top + (1.0f - yNorm_) * r.getHeight(); // yNorm_ is inverted, convert back to screen

    context->setLineWidth(1.0);
    context->setFrameColor(CColor(255, 255, 255, 160));
    context->drawLine(CPoint(r.left, cy), CPoint(r.right, cy));
    context->drawLine(CPoint(cx, r.top), CPoint(cx, r.bottom));

    CRect circle(cx - 4, cy - 4, cx + 4, cy + 4);
    context->setFillColor(CColor(255, 255, 255, 200));
    context->drawEllipse(circle, kDrawFilled);

    setDirty(false);
}

void XYPadView::updateFromMouse(const CPoint &where) {
    CRect r = getViewSize();
    float x = static_cast<float>((where.x - r.left) / r.getWidth());
    float y = static_cast<float>((where.y - r.top) / r.getHeight());

    if (x < 0.0f)
        x = 0.0f;
    if (x > 1.0f)
        x = 1.0f;
    if (y < 0.0f)
        y = 0.0f;
    if (y > 1.0f)
        y = 1.0f;

    xNorm_ = x;
    yNorm_ = 1.0f - y; // invert: screen top = high vowel, bottom = low

    // Y axis -> vowel target (processor smooths and writes back to kVowel)
    setValue(yNorm_);
    if (controller_) {
        controller_->performEdit(kXYVowel, yNorm_);
        controller_->setParamNormalized(kXYVowel, yNorm_);
        // X axis -> pitch target
        controller_->performEdit(kXYPitchTarget, xNorm_);
        controller_->setParamNormalized(kXYPitchTarget, xNorm_);
    }

    invalid();
}

CMouseEventResult XYPadView::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;

    tracking_ = true;

    beginEdit();
    if (controller_) {
        controller_->beginEdit(kXYPitchTarget);
        controller_->beginEdit(kXYVowel);
        // Note on — also set normalized so controller updates monk view
        controller_->beginEdit(kXYNoteOn);
        controller_->performEdit(kXYNoteOn, 1.0);
        controller_->setParamNormalized(kXYNoteOn, 1.0);
        controller_->endEdit(kXYNoteOn);
    }

    updateFromMouse(where);
    return kMouseEventHandled;
}

CMouseEventResult XYPadView::onMouseUp(CPoint &where, const CButtonState & /*buttons*/) {
    if (!tracking_)
        return kMouseEventNotHandled;

    tracking_ = false;

    // Note off
    if (controller_) {
        controller_->beginEdit(kXYNoteOn);
        controller_->performEdit(kXYNoteOn, 0.0);
        controller_->setParamNormalized(kXYNoteOn, 0.0);
        controller_->endEdit(kXYNoteOn);
        controller_->endEdit(kXYVowel);
        controller_->endEdit(kXYPitchTarget);
    }
    endEdit();

    invalid();
    return kMouseEventHandled;
}

CMouseEventResult XYPadView::onMouseMoved(CPoint &where, const CButtonState & /*buttons*/) {
    if (!tracking_)
        return kMouseEventNotHandled;

    updateFromMouse(where);
    return kMouseEventHandled;
}

CMouseEventResult XYPadView::onMouseCancel() {
    if (tracking_) {
        tracking_ = false;
        if (controller_) {
            controller_->beginEdit(kXYNoteOn);
            controller_->performEdit(kXYNoteOn, 0.0);
            controller_->setParamNormalized(kXYNoteOn, 0.0);
            controller_->endEdit(kXYNoteOn);
            controller_->endEdit(kXYVowel);
            controller_->endEdit(kXYPitchTarget);
        }
        endEdit();
        invalid();
    }
    return kMouseEventHandled;
}

} // namespace MonkSynth

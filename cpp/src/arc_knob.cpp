#include "arc_knob.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"

#include <cmath>

using namespace VSTGUI;

namespace MonkSynth {

namespace {
constexpr double kPi = 3.14159265358979323846;

double deg2rad(double deg) { return deg * kPi / 180.0; }

CPoint pointOnArc(CCoord cx, CCoord cy, CCoord r, double angleDeg) {
    double rad = deg2rad(angleDeg);
    return {cx + std::cos(rad) * r, cy + std::sin(rad) * r};
}
} // namespace

ArcKnob::ArcKnob(const CRect &size, IControlListener *listener, int32_t tag)
    : CControl(size, listener, tag) {
    setWantsFocus(false);
}

void ArcKnob::draw(CDrawContext *ctx) {
    ctx->setDrawMode(kAntiAliasing);

    CRect r = getViewSize();
    CCoord cx = r.left + r.getWidth() * 0.5;
    CCoord cy = r.top + r.getHeight() * 0.5;
    CCoord maxW = std::max(trackWidth_, arcWidth_);
    CCoord radius = std::min(r.getWidth(), r.getHeight()) * 0.5 - maxW * 0.5 - 3.0;

    // VSTGUI arc angles: 0° = right (3 o'clock), clockwise.
    double startAngle = kStartAngleDeg;
    double endAngle = kStartAngleDeg + kAngleRangeDeg;

    // --- Track arc (full sweep, semi-transparent) ---
    {
        auto *path = ctx->createGraphicsPath();
        if (path) {
            path->addArc(CRect(cx - radius, cy - radius, cx + radius, cy + radius), startAngle,
                         endAngle, true);
            ctx->setFrameColor(trackColor_);
            ctx->setLineWidth(trackWidth_);
            ctx->setLineStyle(kLineSolid);
            ctx->drawGraphicsPath(path, CDrawContext::kPathStroked);
            path->forget();
        }
    }

    // --- Value arc ---
    float val = getValueNormalized();
    if (val > 0.001f) {
        double valueAngle = startAngle + val * kAngleRangeDeg;
        auto *path = ctx->createGraphicsPath();
        if (path) {
            path->addArc(CRect(cx - radius, cy - radius, cx + radius, cy + radius), startAngle,
                         valueAngle, true);
            ctx->setFrameColor(fillColor_);
            ctx->setLineWidth(arcWidth_);
            ctx->setLineStyle(kLineSolid);
            ctx->drawGraphicsPath(path, CDrawContext::kPathStroked);
            path->forget();
        }
    }

    // --- Handle dot at value endpoint ---
    {
        double valueAngle = startAngle + val * kAngleRangeDeg;
        CPoint dot = pointOnArc(cx, cy, radius, valueAngle);
        CCoord dotR = arcWidth_ * 0.9;
        CRect dotRect(dot.x - dotR, dot.y - dotR, dot.x + dotR, dot.y + dotR);
        ctx->setFillColor(fillColor_);
        ctx->drawEllipse(dotRect, kDrawFilled);
    }

    setDirty(false);
}

CMouseEventResult ArcKnob::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;

    beginEdit();
    startValue_ = getValueNormalized();
    anchorPoint_ = where;
    dragging_ = true;
    return kMouseEventHandled;
}

CMouseEventResult ArcKnob::onMouseUp(CPoint &where, const CButtonState &buttons) {
    if (!dragging_)
        return kMouseEventNotHandled;
    dragging_ = false;
    endEdit();
    return kMouseEventHandled;
}

CMouseEventResult ArcKnob::onMouseMoved(CPoint &where, const CButtonState &buttons) {
    if (!dragging_)
        return kMouseEventNotHandled;

    // Vertical drag: up = increase, down = decrease.
    CCoord delta = anchorPoint_.y - where.y;
    float sensitivity = 1.0f / 200.0f;
    if (buttons & kShift)
        sensitivity *= 0.1f;

    float newVal = startValue_ + static_cast<float>(delta) * sensitivity;
    if (newVal < 0.f)
        newVal = 0.f;
    if (newVal > 1.f)
        newVal = 1.f;

    setValueNormalized(newVal);
    if (isDirty())
        invalid();
    valueChanged();
    return kMouseEventHandled;
}

CMouseEventResult ArcKnob::onMouseCancel() {
    if (dragging_) {
        dragging_ = false;
        setValueNormalized(startValue_);
        if (isDirty())
            invalid();
        valueChanged();
        endEdit();
    }
    return kMouseEventHandled;
}

bool ArcKnob::onWheel(const CPoint & /*where*/, const CMouseWheelAxis &axis, const float &distance,
                       const CButtonState &buttons) {
    if (axis != kMouseWheelAxisY)
        return false;

    float inc = 0.01f;
    if (buttons & kShift)
        inc *= 0.1f;

    beginEdit();
    float newVal = getValueNormalized() + distance * inc;
    if (newVal < 0.f)
        newVal = 0.f;
    if (newVal > 1.f)
        newVal = 1.f;
    setValueNormalized(newVal);
    if (isDirty())
        invalid();
    valueChanged();
    endEdit();
    return true;
}

} // namespace MonkSynth

#include "overlay_view.h"
#include "draw_utils.h"
#include "i18n.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cvstguitimer.h"

using namespace VSTGUI;

namespace MonkSynth {

OverlayView::OverlayView(const CRect &size, bool standardChrome)
    : CViewContainer(size), standardChrome_(standardChrome) {
    setTransparency(false);

    double bw = 120, bh = 36;
    double bx = (size.getWidth() - bw) / 2;
    double by = size.getHeight() - 60;
    closeBtnRect_ = CRect(bx, by, bx + bw, by + bh);
}

void OverlayView::drawBackgroundRect(CDrawContext *ctx, const CRect & /*rect*/) {
    CRect bounds = getViewSize();

    ctx->setDrawMode(kAntiAliasing | kNonIntegralMode);

    if (!standardChrome_) {
        drawBody(ctx, bounds);
        return;
    }

    // Dark background
    ctx->setFillColor(CColor(30, 30, 35, 255));
    ctx->drawRect(bounds, kDrawFilled);

    // Accent line
    CRect accent(bounds.left + 30, bounds.top + 40, bounds.right - 30, bounds.top + 42);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    ctx->drawRect(accent, kDrawFilled);

    drawBody(ctx, bounds);

    // Close button
    CRect btn = closeBtnRect_;
    btn.offset(bounds.left, bounds.top);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    drawRoundRect(ctx, btn, 6);

    auto *btnFont = new CFontDesc(i18n::uiFont(), 14, kBoldFace);
    ctx->setFont(btnFont);
    ctx->setFontColor(CColor(30, 30, 35, 255));
    ctx->drawString(i18n::str(i18n::StringId::InfoClose), btn, kCenterText);
    btnFont->forget();
}

CMouseEventResult OverlayView::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;

    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    if (closeBtnRect_.pointInside(local)) {
        requestClose();
        return kMouseEventHandled;
    }

    hitLink(local, true);
    return kMouseEventHandled; // consume all clicks so they don't pass through
}

void OverlayView::requestClose() {
    // The owner removes the view; it defers that itself, since removing a
    // view from inside its own click handler frees it mid-dispatch. Copy the
    // callback in case running it replaces it.
    if (closeCb_) {
        auto cb = closeCb_;
        cb();
    }
}

CMouseEventResult OverlayView::onMouseMoved(CPoint &where, const CButtonState & /*buttons*/) {
    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    auto *frame = getFrame();
    if (frame) {
        if (closeBtnRect_.pointInside(local) || hitLink(local, false))
            frame->setCursor(kCursorHand);
        else
            frame->setCursor(kCursorDefault);
    }
    return kMouseEventHandled;
}

CMouseEventResult OverlayView::onMouseExited(CPoint & /*where*/, const CButtonState & /*buttons*/) {
    auto *frame = getFrame();
    if (frame)
        frame->setCursor(kCursorDefault);
    return kMouseEventHandled;
}

} // namespace MonkSynth

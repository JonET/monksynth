#include "info_view.h"
#include "open_url.h"
#include "version.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cstring.h"
#include "vstgui/lib/cvstguitimer.h"

#if __linux__
#define MONKSYNTH_FONT "DejaVu Sans"
#elif __APPLE__
#define MONKSYNTH_FONT "Helvetica"
#else
#define MONKSYNTH_FONT "Arial"
#endif

using namespace VSTGUI;

namespace MonkSynth {

InfoView::InfoView(const CRect &size) : CViewContainer(size) {
    setTransparency(false);

    double bw = 120, bh = 36;
    double bx = (size.getWidth() - bw) / 2;
    double by = size.getHeight() - 60;
    closeBtnRect_ = CRect(bx, by, bx + bw, by + bh);
}

static void drawRoundRect(CDrawContext *ctx, const CRect &r, CCoord radius, bool fill) {
    auto *path = ctx->createRoundRectGraphicsPath(r, radius);
    if (!path)
        return;
    if (fill)
        ctx->drawGraphicsPath(path, CDrawContext::kPathFilled);
    else
        ctx->drawGraphicsPath(path, CDrawContext::kPathStroked);
    path->forget();
}

void InfoView::drawBackgroundRect(CDrawContext *ctx, const CRect & /*rect*/) {
    CRect bounds = getViewSize();

    ctx->setDrawMode(kAntiAliasing | kNonIntegralMode);

    // Dark background
    ctx->setFillColor(CColor(30, 30, 35, 255));
    ctx->drawRect(bounds, kDrawFilled);

    // Accent line
    CRect accent(bounds.left + 30, bounds.top + 40, bounds.right - 30, bounds.top + 42);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    ctx->drawRect(accent, kDrawFilled);

    auto *titleFont = new CFontDesc(MONKSYNTH_FONT, 24, kBoldFace);
    auto *bodyFont = new CFontDesc(MONKSYNTH_FONT, 13);
    auto *smallFont = new CFontDesc(MONKSYNTH_FONT, 11);
    auto *btnFont = new CFontDesc(MONKSYNTH_FONT, 14, kBoldFace);
    auto *linkFont = new CFontDesc(MONKSYNTH_FONT, 13, kUnderlineFace);

    // Title
    ctx->setFont(titleFont);
    ctx->setFontColor(CColor(230, 230, 230, 255));
    CRect titleRect(bounds.left, bounds.top + 55, bounds.right, bounds.top + 85);
    ctx->drawString("MonkSynth", titleRect, kCenterText);

    // Version
    ctx->setFont(smallFont);
    ctx->setFontColor(CColor(140, 140, 140, 255));
    CRect verRect(bounds.left, bounds.top + 90, bounds.right, bounds.top + 108);
    ctx->drawString(MONKSYNTH_VERSION " " MONKSYNTH_VERSION_LABEL, verRect, kCenterText);

    // Creator
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    double y = bounds.top + 140;
    CRect creatorRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("Created by Jonathan Taylor", creatorRect, kCenterText);

    // License
    y += 40;
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect licenseHeaderRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("License", licenseHeaderRect, kCenterText);

    y += 24;
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    CRect licenseRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("MIT License \xC2\xA9 2026 Jonathan Taylor", licenseRect, kCenterText);

    // Source code section
    y += 45;
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect srcHeaderRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("Source Code", srcHeaderRect, kCenterText);

    y += 24;
    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect linkRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("github.com/JonET/monksynth", linkRect, kCenterText);
    // Store the link rect in local coordinates for hit testing
    githubLinkRect_ = CRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    githubLinkRect_.offset(-bounds.left, -bounds.top);

    // Description
    y += 40;
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(150, 150, 155, 255));
    const char *descLines[] = {
        "A vocal synthesizer inspired by Delay Lama.",
        "Open source and free forever.",
    };
    for (const char *line : descLines) {
        CRect lr(bounds.left + 20, y, bounds.right - 20, y + 18);
        ctx->drawString(line, lr, kCenterText);
        y += 19;
    }

    // Close button
    CRect btn = closeBtnRect_;
    btn.offset(bounds.left, bounds.top);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    drawRoundRect(ctx, btn, 6, true);

    ctx->setFont(btnFont);
    ctx->setFontColor(CColor(30, 30, 35, 255));
    ctx->drawString("Close", btn, kCenterText);

    titleFont->forget();
    bodyFont->forget();
    smallFont->forget();
    btnFont->forget();
    linkFont->forget();
}

CMouseEventResult InfoView::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;

    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    if (closeBtnRect_.pointInside(local)) {
        if (closeCb_) {
            auto cb = closeCb_;
            Call::later([cb]() { cb(); });
        }
        return kMouseEventHandled;
    }

    if (githubLinkRect_.pointInside(local)) {
        openURL("https://github.com/JonET/monksynth");
        return kMouseEventHandled;
    }

    return kMouseEventHandled; // consume all clicks so they don't pass through
}

CMouseEventResult InfoView::onMouseMoved(CPoint &where, const CButtonState & /*buttons*/) {
    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    auto *frame = getFrame();
    if (frame) {
        if (closeBtnRect_.pointInside(local) || githubLinkRect_.pointInside(local))
            frame->setCursor(kCursorHand);
        else
            frame->setCursor(kCursorDefault);
    }
    return kMouseEventHandled;
}

CMouseEventResult InfoView::onMouseExited(CPoint & /*where*/, const CButtonState & /*buttons*/) {
    auto *frame = getFrame();
    if (frame)
        frame->setCursor(kCursorDefault);
    return kMouseEventHandled;
}

} // namespace MonkSynth

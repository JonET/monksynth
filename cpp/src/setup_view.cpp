#include "setup_view.h"
#include "open_url.h"
#include "version.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cstring.h"

#if __linux__
#define MONKSYNTH_FONT "DejaVu Sans"
#elif __APPLE__
#define MONKSYNTH_FONT "Helvetica"
#else
#define MONKSYNTH_FONT "Arial"
#endif

using namespace VSTGUI;

namespace MonkSynth {

SetupView::SetupView(const CRect &size) : CViewContainer(size) {
    setTransparency(false);

    // Import button position (relative to view)
    double bw = 220, bh = 36;
    double bx = (size.getWidth() - bw) / 2;
    double by = 345;
    importBtnRect_ = CRect(bx, by, bx + bw, by + bh);
}

void SetupView::setStatusText(const std::string &text) {
    statusText_ = text;
    setDirty(true);
}

// Draw a rounded rectangle using anti-aliased lines and fills.
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

void SetupView::drawBackgroundRect(CDrawContext *ctx, const CRect & /*rect*/) {
    CRect bounds = getViewSize();

    // Enable anti-aliasing for sharp rendering on HiDPI
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
    auto *linkFont = new CFontDesc(MONKSYNTH_FONT, 13, kUnderlineFace);
    auto *smallFont = new CFontDesc(MONKSYNTH_FONT, 11);
    auto *btnFont = new CFontDesc(MONKSYNTH_FONT, 14, kBoldFace);

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

    // Body text
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));

    const char *lines[] = {
        "MonkSynth needs a skin to display its GUI.",
        "",
        "You can import the classic look from the",
        "original Delay Lama v1.1 plugin (Windows).",
        "",
        "Download it for free from:",
    };
    double lineY = bounds.top + 130;
    for (const char *line : lines) {
        CRect lr(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
        ctx->drawString(line, lr, kCenterText);
        lineY += 19;
    }

    // Clickable URL link
    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect linkRect(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
    ctx->drawString("www.audionerdz.nl/download.htm", linkRect, kCenterText);
    urlLinkRect_ = linkRect;
    urlLinkRect_.offset(-bounds.left, -bounds.top);
    lineY += 19;

    // Remaining body text
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    const char *lines2[] = {
        "",
        "Then click below and select the",
        "\"Delay Lama.dll\" file.",
    };
    for (const char *line : lines2) {
        CRect lr(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
        ctx->drawString(line, lr, kCenterText);
        lineY += 19;
    }

    // Import button (rounded rect)
    CRect btn = importBtnRect_;
    btn.offset(bounds.left, bounds.top);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    drawRoundRect(ctx, btn, 6, true);

    ctx->setFont(btnFont);
    ctx->setFontColor(CColor(30, 30, 35, 255));
    ctx->drawString("Import Classic Skin...", btn, kCenterText);

    // Status text
    if (!statusText_.empty()) {
        ctx->setFont(smallFont);
        ctx->setFontColor(CColor(220, 180, 100, 255));
        CRect statusRect(bounds.left + 20, btn.bottom + 15, bounds.right - 20, btn.bottom + 55);
        ctx->drawString(statusText_.c_str(), statusRect, kCenterText);
    }

    // Hint at bottom
    ctx->setFont(smallFont);
    ctx->setFontColor(CColor(100, 100, 110, 255));
    CRect hintRect(bounds.left + 20, bounds.bottom - 35, bounds.right - 20, bounds.bottom - 15);
    ctx->drawString("You can also load custom themes via right-click.", hintRect, kCenterText);

    titleFont->forget();
    bodyFont->forget();
    linkFont->forget();
    smallFont->forget();
    btnFont->forget();
}

CMouseEventResult SetupView::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;

    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    if (importBtnRect_.pointInside(local)) {
        if (importCb_)
            importCb_();
        return kMouseEventHandled;
    }

    if (urlLinkRect_.pointInside(local)) {
        openURL("http://www.audionerdz.nl/download.htm");
        return kMouseEventHandled;
    }

    return kMouseEventNotHandled;
}

CMouseEventResult SetupView::onMouseMoved(CPoint &where, const CButtonState & /*buttons*/) {
    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    auto *frame = getFrame();
    if (frame) {
        if (importBtnRect_.pointInside(local) || urlLinkRect_.pointInside(local))
            frame->setCursor(kCursorHand);
        else
            frame->setCursor(kCursorDefault);
    }
    return kMouseEventHandled;
}

CMouseEventResult SetupView::onMouseExited(CPoint & /*where*/, const CButtonState & /*buttons*/) {
    auto *frame = getFrame();
    if (frame)
        frame->setCursor(kCursorDefault);
    return kMouseEventHandled;
}

} // namespace MonkSynth

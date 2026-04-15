#include "info_view.h"
#include "i18n.h"
#include "open_url.h"
#include "theme_manager.h"
#include "version.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cstring.h"
#include "vstgui/lib/cvstguitimer.h"

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
    const char *font = i18n::uiFont();

    ctx->setDrawMode(kAntiAliasing | kNonIntegralMode);

    // Dark background
    ctx->setFillColor(CColor(30, 30, 35, 255));
    ctx->drawRect(bounds, kDrawFilled);

    // Accent line
    CRect accent(bounds.left + 30, bounds.top + 40, bounds.right - 30, bounds.top + 42);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    ctx->drawRect(accent, kDrawFilled);

    auto *titleFont = new CFontDesc(font, 24, kBoldFace);
    auto *bodyFont = new CFontDesc(font, 13);
    auto *smallFont = new CFontDesc(font, 11);
    auto *btnFont = new CFontDesc(font, 14, kBoldFace);
    auto *linkFont = new CFontDesc(font, 13, kUnderlineFace);
    auto *sectionFont = new CFontDesc(font, 14, kBoldFace);

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
    double y = bounds.top + 135;
    CRect creatorRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString(i18n::str(i18n::StringId::InfoCreatedBy), creatorRect, kCenterText);

    // License
    y += 36;
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect licenseHeaderRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString(i18n::str(i18n::StringId::InfoLicenseHeader), licenseHeaderRect, kCenterText);

    y += 22;
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    CRect licenseRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("MIT License \xC2\xA9 2026 Jonathan Taylor", licenseRect, kCenterText);

    // Source code section
    y += 36;
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect srcHeaderRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString(i18n::str(i18n::StringId::InfoSourceCodeHeader), srcHeaderRect, kCenterText);

    y += 22;
    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect linkRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("github.com/JonET/monksynth", linkRect, kCenterText);
    githubLinkRect_ = CRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    githubLinkRect_.offset(-bounds.left, -bounds.top);

    // Description
    y += 32;
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(150, 150, 155, 255));
    const char *descLines[] = {
        i18n::str(i18n::StringId::InfoTagline1),
        i18n::str(i18n::StringId::InfoTagline2),
    };
    for (const char *line : descLines) {
        CRect lr(bounds.left + 20, y, bounds.right - 20, y + 18);
        ctx->drawString(line, lr, kCenterText);
        y += 19;
    }

    // ---- Contribute section ----
    double cy = bounds.top + 355;
    CRect sepRect(bounds.left + 30, cy, bounds.right - 30, cy + 1);
    ctx->setFillColor(CColor(200, 150, 50, 120));
    ctx->drawRect(sepRect, kDrawFilled);
    cy += 8;

    ctx->setFont(sectionFont);
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect headerRect(bounds.left + 20, cy, bounds.right - 20, cy + 20);
    ctx->drawString(i18n::str(i18n::StringId::ContributeHeader), headerRect, kCenterText);
    cy += 22;

    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    CRect shareRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeShare), shareRect, kCenterText);
    cy += 18;

    ctx->setFontColor(CColor(150, 150, 155, 255));
    CRect lookingRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeLookingFor), lookingRect, kCenterText);
    cy += 20;

    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect folderRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeOpenFolder), folderRect, kCenterText);
    openFolderRect_ = folderRect;
    openFolderRect_.offset(-bounds.left, -bounds.top);

    // Close button
    CRect btn = closeBtnRect_;
    btn.offset(bounds.left, bounds.top);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    drawRoundRect(ctx, btn, 6, true);

    ctx->setFont(btnFont);
    ctx->setFontColor(CColor(30, 30, 35, 255));
    ctx->drawString(i18n::str(i18n::StringId::InfoClose), btn, kCenterText);

    titleFont->forget();
    bodyFont->forget();
    smallFont->forget();
    btnFont->forget();
    linkFont->forget();
    sectionFont->forget();
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

    if (openFolderRect_.pointInside(local)) {
        openFolder(ThemeManager::getThemesDir());
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
        if (closeBtnRect_.pointInside(local) || githubLinkRect_.pointInside(local) ||
            openFolderRect_.pointInside(local))
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

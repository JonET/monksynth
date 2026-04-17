#include "section_panel.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cstring.h"

using namespace VSTGUI;

namespace MonkSynth {

SectionPanel::SectionPanel(const CRect &size) : CViewContainer(size) {
    setTransparency(true);
    titleFont_ = makeOwned<CFontDesc>("Arial", 11);
}

void SectionPanel::setTitle(const std::string &t) {
    title_ = t;
    invalid();
}

static SharedPointer<CGraphicsPath> makeTopRoundedRect(CDrawContext *ctx, const CRect &r,
                                                       CCoord radius) {
    auto path = owned(ctx->createGraphicsPath());
    if (!path)
        return nullptr;
    CCoord left = r.left;
    CCoord right = r.right;
    CCoord top = r.top;
    CCoord bottom = r.bottom;
    path->beginSubpath(CPoint(left + radius, top));
    path->addLine(CPoint(right - radius, top));
    path->addBezierCurve(CPoint(right, top), CPoint(right, top), CPoint(right, top + radius));
    path->addLine(CPoint(right, bottom));
    path->addLine(CPoint(left, bottom));
    path->addLine(CPoint(left, top + radius));
    path->addBezierCurve(CPoint(left, top), CPoint(left, top), CPoint(left + radius, top));
    path->closeSubpath();
    return path;
}

void SectionPanel::drawBackgroundRect(CDrawContext *ctx, const CRect & /*updateRect*/) {
    ctx->setDrawMode(kAntiAliasing);

    // Full rounded rect in content color.
    CRect viewRect(0, 0, getViewSize().getWidth(), getViewSize().getHeight());
    {
        auto path = owned(ctx->createRoundRectGraphicsPath(viewRect, cornerRadius_));
        if (path) {
            ctx->setFillColor(contentBg_);
            ctx->drawGraphicsPath(path, CDrawContext::kPathFilled);
        }
    }

    // Title strip: rounded only at top.
    if (titleHeight_ > 0 && !title_.empty()) {
        CRect titleRect(0, 0, viewRect.getWidth(), titleHeight_);
        auto titlePath = makeTopRoundedRect(ctx, titleRect, cornerRadius_);
        if (titlePath) {
            ctx->setFillColor(titleBg_);
            ctx->drawGraphicsPath(titlePath, CDrawContext::kPathFilled);
        }

        // Title text.
        ctx->setFontColor(titleText_);
        ctx->setFont(titleFont_);
        CRect textRect = titleRect;
        textRect.inset(10, 0);
        ctx->drawString(title_.c_str(), textRect, kLeftText, true);
    }
}

} // namespace MonkSynth

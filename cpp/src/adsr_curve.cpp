#include "adsr_curve.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"

#include <cmath>

using namespace VSTGUI;

namespace MonkSynth {

AdsrCurve::AdsrCurve(const CRect &size) : CView(size) {
    setTransparency(true);
    setMouseEnabled(false);
}

void AdsrCurve::draw(CDrawContext *ctx) {
    ctx->setDrawMode(kAntiAliasing);

    CRect r = getViewSize();
    CCoord w = r.getWidth();
    CCoord h = r.getHeight();

    // Background + 2px border, rounded 2px.
    {
        CRect inset = r;
        inset.inset(1, 1); // keep border inside view
        auto path = owned(ctx->createRoundRectGraphicsPath(inset, 2.0));
        if (path) {
            ctx->setFillColor(bg_);
            ctx->drawGraphicsPath(path, CDrawContext::kPathFilled);
            ctx->setFrameColor(border_);
            ctx->setLineWidth(2.0);
            ctx->setLineStyle(kLineSolid);
            ctx->drawGraphicsPath(path, CDrawContext::kPathStroked);
        }
    }

    // Envelope breakpoint math. Segment times are computed in absolute
    // seconds (matches DSP's quadratic taper and 3s cap). The sustain-hold
    // is a fixed "virtual" duration on the same axis — anchoring the other
    // segments so short times read as short even when they're the only
    // non-zero phase. Log-compression keeps long tails readable without
    // squishing short phases to invisibility.
    const CCoord padX = 4.0;
    const CCoord padTop = 4.0;
    const double kMaxSec = 3.0;
    const double kHoldSec = 1.0; // virtual held-note duration

    double aSec = attack_ * attack_ * kMaxSec;
    double dSec = decay_ * decay_ * kMaxSec;
    double rSec = release_ * release_ * kMaxSec;

    double aLog = std::log(1.0 + aSec);
    double dLog = std::log(1.0 + dSec);
    double holdLog = std::log(1.0 + kHoldSec);
    double rLog = std::log(1.0 + rSec);
    double totalLog = aLog + dLog + holdLog + rLog;

    double aFrac = aLog / totalLog;
    double dFrac = dLog / totalLog;
    double holdFrac = holdLog / totalLog;
    double rFrac = rLog / totalLog;

    CCoord inner = w - 2 * padX;
    CCoord top = r.top + padTop;
    CCoord bot = r.bottom; // reach all the way to the bottom edge
    CCoord range = bot - top;
    CCoord susY = bot - sustain_ * range;

    CCoord x0 = r.left + padX;
    CCoord x1 = x0 + aFrac * inner;
    CCoord x2 = x1 + dFrac * inner;
    CCoord x3 = x2 + holdFrac * inner;
    CCoord x4 = x3 + rFrac * inner;

    // Filled area under the envelope.
    {
        auto path = owned(ctx->createGraphicsPath());
        if (path) {
            path->beginSubpath(CPoint(x0, bot));
            path->addLine(CPoint(x1, top));
            path->addLine(CPoint(x2, susY));
            path->addLine(CPoint(x3, susY));
            path->addLine(CPoint(x4, bot));
            path->closeSubpath();
            ctx->setFillColor(fill_);
            ctx->drawGraphicsPath(path, CDrawContext::kPathFilled);
        }
    }

    // Envelope line.
    {
        auto path = owned(ctx->createGraphicsPath());
        if (path) {
            path->beginSubpath(CPoint(x0, bot));
            path->addLine(CPoint(x1, top));
            path->addLine(CPoint(x2, susY));
            path->addLine(CPoint(x3, susY));
            path->addLine(CPoint(x4, bot));
            ctx->setFrameColor(line_);
            ctx->setLineWidth(1.5);
            ctx->setLineStyle(kLineSolid);
            ctx->drawGraphicsPath(path, CDrawContext::kPathStroked);
        }
    }

    // Breakpoint dots: peak (end of attack), start of sustain, end of sustain hold.
    auto dot = [&](CCoord x, CCoord y) {
        CCoord dr = 2.5;
        CRect dotRect(x - dr, y - dr, x + dr, y + dr);
        ctx->setFillColor(dot_);
        ctx->drawEllipse(dotRect, kDrawFilled);
    };
    dot(x1, top);
    dot(x2, susY);
    dot(x3, susY);

    setDirty(false);
}

} // namespace MonkSynth

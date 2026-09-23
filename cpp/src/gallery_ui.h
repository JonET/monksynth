#pragma once

// Drawing helpers and per-theme state shared by the two gallery views (the
// wide ThemeGalleryView and the compact ThemeBrowserView fallback).

#include "draw_utils.h"
#include "i18n.h"
#include "theme_gallery.h"

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/clinestyle.h"
#include "vstgui/lib/platform/platformfactory.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace MonkSynth {
namespace gallery_ui {

// Warm charcoal with the saffron accent, matching the gallery mockup.
inline const VSTGUI::CColor kBg(19, 17, 15);
inline const VSTGUI::CColor kPanel(26, 24, 21);
inline const VSTGUI::CColor kLine(37, 34, 30);
inline const VSTGUI::CColor kText(244, 239, 231);
inline const VSTGUI::CColor kBody(201, 193, 181);
inline const VSTGUI::CColor kMuted(168, 160, 149);
inline const VSTGUI::CColor kAccent(242, 163, 58);
inline const VSTGUI::CColor kAccentHover(247, 181, 92);
inline const VSTGUI::CColor kAccentSoft(242, 163, 58, 36);
inline const VSTGUI::CColor kOnAccent(26, 18, 6);
inline const VSTGUI::CColor kCard(30, 27, 24);
inline const VSTGUI::CColor kRowSelected(34, 30, 25);
inline const VSTGUI::CColor kQuiet(34, 31, 27);
inline const VSTGUI::CColor kPillGrey(42, 38, 34);
inline const VSTGUI::CColor kBorder(58, 53, 47);
inline const VSTGUI::CColor kBorderHover(90, 83, 74);
inline const VSTGUI::CColor kStage(14, 13, 11);
inline const VSTGUI::CColor kError(233, 185, 110);

inline VSTGUI::SharedPointer<VSTGUI::CFontDesc> font(VSTGUI::CCoord size, bool bold = false) {
    return VSTGUI::makeOwned<VSTGUI::CFontDesc>(i18n::uiFont(), size,
                                                bold ? VSTGUI::kBoldFace : VSTGUI::kNormalFace);
}

inline void text(VSTGUI::CDrawContext *ctx, const std::string &s, const VSTGUI::CRect &r,
                 const VSTGUI::CColor &color, VSTGUI::CFontDesc *f,
                 VSTGUI::CHoriTxtAlign align = VSTGUI::kLeftText) {
    ctx->setFont(f);
    ctx->setFontColor(color);
    ctx->drawString(s.c_str(), r, align);
}

inline VSTGUI::CCoord textWidth(VSTGUI::CDrawContext *ctx, const std::string &s,
                                VSTGUI::CFontDesc *f) {
    ctx->setFont(f);
    return std::ceil(ctx->getStringWidth(s.c_str()));
}

inline void fillRound(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &r, VSTGUI::CCoord radius,
                      const VSTGUI::CColor &color) {
    ctx->setFillColor(color);
    drawRoundRect(ctx, r, radius);
}

inline void strokeRound(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &r, VSTGUI::CCoord radius,
                        const VSTGUI::CColor &color, bool dashed = false) {
    using VSTGUI::CLineStyle;
    VSTGUI::CRect inset(r);
    inset.inset(0.5, 0.5);
    auto *path = ctx->createRoundRectGraphicsPath(inset, radius);
    if (!path)
        return;
    ctx->setFrameColor(color);
    ctx->setLineWidth(1);
    const VSTGUI::CCoord dashes[] = {4, 4};
    ctx->setLineStyle(dashed ? CLineStyle(CLineStyle::kLineCapButt, CLineStyle::kLineJoinMiter, 0,
                                          2, dashes)
                             : VSTGUI::kLineSolid);
    ctx->drawGraphicsPath(path, VSTGUI::CDrawContext::kPathStroked);
    ctx->setLineStyle(VSTGUI::kLineSolid);
    path->forget();
}

inline std::string formatSize(std::uint64_t bytes) {
    char buf[32];
    if (bytes >= 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    else
        std::snprintf(buf, sizeof(buf), "%.0f KB", std::ceil(bytes / 1024.0));
    return buf;
}

enum class Style { Primary, Ghost, Quiet };

inline void button(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &r, const std::string &label,
                   Style style, bool hover) {
    auto f = font(r.getHeight() >= 44 ? 14 : 13, true);
    switch (style) {
    case Style::Primary:
        fillRound(ctx, r, 10, hover ? kAccentHover : kAccent);
        text(ctx, label, r, kOnAccent, f, VSTGUI::kCenterText);
        break;
    case Style::Ghost:
        if (hover)
            fillRound(ctx, r, 10, kQuiet);
        strokeRound(ctx, r, 10, hover ? kBorderHover : kBorder);
        text(ctx, label, r, kText, f, VSTGUI::kCenterText);
        break;
    case Style::Quiet:
        fillRound(ctx, r, 10, kQuiet);
        text(ctx, label, r, kMuted, f, VSTGUI::kCenterText);
        break;
    }
}

inline VSTGUI::CCoord buttonWidth(VSTGUI::CDrawContext *ctx, const std::string &label,
                                  VSTGUI::CCoord minWidth) {
    auto f = font(14, true);
    return std::max(minWidth, textWidth(ctx, label, f) + 36);
}

// Download progress in a button-shaped bar.
inline void progressBar(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &r, float progress) {
    fillRound(ctx, r, 10, kQuiet);
    VSTGUI::CRect bar(r);
    bar.right = bar.left + std::max<VSTGUI::CCoord>(20, r.getWidth() * progress);
    fillRound(ctx, bar, 10, VSTGUI::CColor(242, 163, 58, 70));
    std::string label = std::string(i18n::str(i18n::StringId::GalleryDownloading)) + " " +
                        std::to_string(static_cast<int>(progress * 100)) + "%";
    text(ctx, label, r, kAccent, font(14, true), VSTGUI::kCenterText);
}

// Loads a cached gallery image drawn at |scale| points per pixel (0.5 draws a
// 112 px thumbnail at 56 pt).
inline VSTGUI::SharedPointer<VSTGUI::CBitmap> loadImage(const std::filesystem::path &path,
                                                       double pixelsPerPoint) {
    if (path.empty())
        return nullptr;
    auto platform =
        VSTGUI::getPlatformFactory().createBitmapFromPath(path.generic_u8string().c_str());
    if (!platform)
        return nullptr;
    platform->setScaleFactor(pixelsPerPoint);
    return VSTGUI::makeOwned<VSTGUI::CBitmap>(platform);
}

// Draws |bmp| scaled by |scale| with its top-left at |origin|, clipped to |clip|.
inline void drawScaled(VSTGUI::CDrawContext *ctx, VSTGUI::CBitmap *bmp,
                       const VSTGUI::CPoint &origin, double scale, const VSTGUI::CRect &clip) {
    if (!bmp)
        return;
    VSTGUI::ConcatClip c(*ctx, clip);
    VSTGUI::CDrawContext::Transform t(
        *ctx, VSTGUI::CGraphicsTransform(scale, 0, 0, scale, origin.x, origin.y));
    auto quality = ctx->getBitmapInterpolationQuality();
    ctx->setBitmapInterpolationQuality(VSTGUI::BitmapInterpolationQuality::kHigh);
    ctx->drawBitmap(bmp, VSTGUI::CRect(0, 0, bmp->getWidth(), bmp->getHeight()));
    ctx->setBitmapInterpolationQuality(quality);
}

// Everything the views need to know about one gallery theme right now.
struct ThemeState {
    ThemeGallery::InstalledCopy copy;
    bool active = false;
    bool update = false; // installed, but the gallery has another version
    bool installing = false;
    float progress = 0.0f;
    std::string error;
};

inline ThemeState themeState(const GalleryTheme &theme, const ThemeGallery::ThemeStatus &status,
                             const std::filesystem::path &activeTheme) {
    ThemeState s;
    s.copy = ThemeGallery::installedCopy(theme.id);
    s.active = ThemeGallery::isActive(s.copy, activeTheme);
    s.update = s.copy.installed() && !theme.version.empty() && s.copy.version != theme.version;
    s.installing = status.installing;
    s.progress = status.progress;
    s.error = status.error;
    return s;
}

struct Pill {
    std::string label;
    VSTGUI::CColor bg, fg;
};

inline Pill pillFor(const ThemeState &s) {
    using i18n::StringId;
    if (s.installing)
        return {std::to_string(static_cast<int>(s.progress * 100)) + "%", kPillGrey, kAccent};
    if (s.active && !s.update)
        return {i18n::str(StringId::GalleryPillInUse), kAccentSoft, kAccent};
    if (s.update)
        return {i18n::str(StringId::GalleryPillUpdate), kAccent, kOnAccent};
    if (s.copy.installed())
        return {i18n::str(StringId::GalleryPillInstalled), kPillGrey, kBody};
    return {i18n::str(StringId::GalleryPillGet), kText, kBg};
}

// Draws |pill| right-aligned to |right|, vertically centred on |centerY|,
// at most |maxWidth| wide. Returns its rect.
inline VSTGUI::CRect drawPill(VSTGUI::CDrawContext *ctx, const Pill &pill, VSTGUI::CCoord right,
                              VSTGUI::CCoord centerY, VSTGUI::CCoord maxWidth = 1000) {
    auto f = font(12, true);
    ctx->setFont(f);
    std::string label = ellipsize(ctx, pill.label, maxWidth - 24);
    VSTGUI::CCoord w = textWidth(ctx, label, f) + 24;
    VSTGUI::CRect r(right - w, centerY - 14, right, centerY + 14);
    fillRound(ctx, r, 14, pill.bg);
    text(ctx, label, r, pill.fg, f, VSTGUI::kCenterText);
    return r;
}

} // namespace gallery_ui
} // namespace MonkSynth

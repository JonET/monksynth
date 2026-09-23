#include "theme_gallery_view.h"
#include "open_url.h"
#include "theme_manager.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/events.h"

#include <cmath>

using namespace VSTGUI;
using namespace MonkSynth::gallery_ui;
namespace fs = std::filesystem;

namespace MonkSynth {

namespace {

// Layout of the 1120x720 gallery, in view coordinates.
constexpr CCoord kMargin = 24;
constexpr CCoord kHeaderH = 72;
constexpr CCoord kBannerH = 44;
constexpr CCoord kGridW = 656;
constexpr CCoord kGap = 16;
constexpr CCoord kChipH = 32;
constexpr CCoord kCardW = (kGridW - 2 * kGap) / 3;
constexpr CCoord kThumbH = 132;
constexpr CCoord kCardH = kThumbH + 72;
constexpr CCoord kPanelPad = 20;
constexpr CCoord kStageH = 300;

// The theme preview is the 360x510 editor. Cards show its top, the detail
// panel the whole thing.
constexpr CCoord kEditorW = 360;
constexpr CCoord kEditorH = 510;
constexpr double kCardScale = 0.58;
constexpr double kStageScale = 0.56;

const CColor kIcon(201, 193, 181);
const CColor kBannerBg(34, 28, 20);
const CColor kBannerBorder(74, 58, 34);

void strokeIcon(CDrawContext *ctx, const std::function<void(CGraphicsPath *)> &build) {
    auto *path = ctx->createGraphicsPath();
    if (!path)
        return;
    build(path);
    ctx->setFrameColor(kIcon);
    ctx->setLineWidth(2);
    ctx->setLineStyle(CLineStyle(CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
    ctx->drawGraphicsPath(path, CDrawContext::kPathStroked);
    ctx->setLineStyle(kLineSolid);
    ctx->setLineWidth(1);
    path->forget();
}

} // namespace

ThemeGalleryView::ThemeGalleryView(const CRect &size, ThemeGallery *gallery,
                                   ActiveThemeFn activeTheme, ApplyFn onApply, CloseFn onClose)
    : CView(size), gallery_(gallery), activeTheme_(std::move(activeTheme)),
      onApply_(std::move(onApply)), onClose_(std::move(onClose)) {
    setTransparency(false);
    sync();
    if (snap_.state != ThemeGallery::IndexState::Loading)
        gallery_->refresh();
    // Fast enough to animate the refresh icon; otherwise it only compares a
    // revision counter.
    timer_ = makeOwned<CVSTGUITimer>([this](CVSTGUITimer *) { poll(); }, 40);
}

ThemeGalleryView::~ThemeGalleryView() {
    if (timer_)
        timer_->stop();
}

bool ThemeGalleryView::removed(CView *parent) {
    if (timer_) {
        timer_->stop();
        timer_ = nullptr;
    }
    return CView::removed(parent);
}

// --- State ---

void ThemeGalleryView::poll() {
    // A theme updated while in use: load the new files.
    fs::path active = activeTheme_ ? activeTheme_() : fs::path();
    for (const auto &id : gallery_->takeFinishedInstalls()) {
        fs::path dir = ThemeGallery::installDir(id);
        std::error_code ec;
        if (!active.empty() && fs::equivalent(dir, active, ec) && onApply_)
            onApply_(dir, false);
    }
    bool changed = gallery_->revision() != seenRevision_;
    if (changed)
        sync();

    auto now = Clock::now();
    bool loading = snap_.state == ThemeGallery::IndexState::Loading;
    if (loading && !wasLoading_) {
        spinStart_ = now;
        spinUntil_ = now + std::chrono::milliseconds(800);
    }
    if (!loading && wasLoading_ && snap_.state == ThemeGallery::IndexState::Ready)
        upToDateUntil_ = std::max(now, spinUntil_) + std::chrono::milliseconds(2500);
    wasLoading_ = loading;

    // Keep redrawing while something is animating, and once more after.
    if (changed || spinning(now) || now < upToDateUntil_ + std::chrono::milliseconds(100))
        invalid();
}

bool ThemeGalleryView::spinning(Clock::time_point now) const {
    return snap_.state == ThemeGallery::IndexState::Loading || now < spinUntil_;
}

void ThemeGalleryView::sync() {
    seenRevision_ = gallery_->revision();
    snap_ = gallery_->snapshot();
    fs::path active = activeTheme_ ? activeTheme_() : fs::path();
    states_.clear();
    for (const auto &t : snap_.themes)
        states_[t.id] = themeState(t, snap_.status[t.id], active);

    // Default the selection to the theme in use, else the first one.
    bool known = std::any_of(snap_.themes.begin(), snap_.themes.end(),
                             [&](const GalleryTheme &t) { return t.id == selectedId_; });
    if (!known && !snap_.themes.empty()) {
        selectedId_ = snap_.themes.front().id;
        for (const auto &t : snap_.themes)
            if (states_[t.id].active)
                selectedId_ = t.id;
    }
}

bool ThemeGalleryView::passesFilter(const GalleryTheme &t) const {
    auto it = states_.find(t.id);
    bool installed = it != states_.end() && it->second.copy.installed();
    switch (filter_) {
    case Filter::All:
        return true;
    case Filter::Installed:
        return installed;
    case Filter::Available:
        return !installed;
    }
    return true;
}

CBitmap *ThemeGalleryView::previewFor(const std::string &id) {
    auto it = previews_.find(id);
    if (it != previews_.end())
        return it->second;
    auto st = snap_.status.find(id);
    if (st == snap_.status.end())
        return nullptr;
    auto bmp = loadImage(st->second.previewPath, 1.0);
    if (bmp)
        previews_[id] = bmp;
    return bmp;
}

const GalleryTheme *ThemeGalleryView::selectedTheme() const {
    for (const auto &t : snap_.themes)
        if (t.id == selectedId_)
            return &t;
    return nullptr;
}

void ThemeGalleryView::addHit(CRect r, Action a, const std::string &id) {
    r.offset(-getViewSize().left, -getViewSize().top);
    hits_.push_back({r, a, id});
}

// --- Drawing ---

void ThemeGalleryView::draw(CDrawContext *ctx) {
    hits_.clear();
    const CRect v = getViewSize();
    ctx->setDrawMode(kAntiAliasing | kNonIntegralMode);
    ctx->setFillColor(kBg);
    ctx->drawRect(v, kDrawFilled);

    drawHeader(ctx);

    CCoord top = v.top + kHeaderH;
    if (snap_.state == ThemeGallery::IndexState::Failed) {
        CRect banner(v.left + kMargin, top + 16, v.right - kMargin, top + 16 + kBannerH);
        drawBanner(ctx, banner);
        top = banner.bottom;
    }
    top += 20;
    CCoord bottom = v.bottom - kMargin;

    CRect chips(v.left + kMargin, top, v.left + kMargin + kGridW, top + kChipH);
    drawChips(ctx, chips);
    CRect grid(chips.left, chips.bottom + kGap, chips.right, bottom);
    gridArea_ = grid;
    gridArea_.offset(-v.left, -v.top);
    drawGrid(ctx, grid);

    CRect panel(chips.right + kMargin, top, v.right - kMargin, bottom);
    drawDetail(ctx, panel);
    setDirty(false);
}

void ThemeGalleryView::drawHeader(CDrawContext *ctx) {
    const CRect v = getViewSize();
    ctx->setFillColor(kLine);
    ctx->drawRect(CRect(v.left, v.top + kHeaderH - 1, v.right, v.top + kHeaderH), kDrawFilled);

    auto title = font(20, true);
    auto sub = font(12);
    CCoord tx = v.left + kMargin;
    text(ctx, i18n::str(i18n::StringId::GalleryTitle),
         CRect(tx, v.top + 16, tx + 400, v.top + 40), kText, title);
    text(ctx, "MonkSynth", CRect(tx, v.top + 40, tx + 400, v.top + 56), CColor(138, 130, 119),
         sub);

    // Right: refresh, open folder, Done.
    std::string done = i18n::str(i18n::StringId::GalleryDone);
    CCoord dw = buttonWidth(ctx, done, 88);
    CRect doneRect(v.right - kMargin - dw, v.top + 14, v.right - kMargin, v.top + 58);
    CRect folder(doneRect.left - 8 - 44, v.top + 14, doneRect.left - 8, v.top + 58);
    CRect refresh(folder.left - 4 - 44, v.top + 14, folder.left - 4, v.top + 58);

    button(ctx, doneRect, done, Style::Ghost, hoverAction_ == Action::Close);
    addHit(doneRect, Action::Close);
    for (auto [r, a] : {std::pair{folder, Action::OpenFolder}, std::pair{refresh, Action::Refresh}}) {
        if (hoverAction_ == a)
            fillRound(ctx, r, 10, kQuiet);
        addHit(r, a);
    }
    CPoint f = folder.getCenter();
    strokeIcon(ctx, [&](CGraphicsPath *p) {
        p->beginSubpath(CPoint(f.x - 9, f.y - 6));
        p->addLine(CPoint(f.x - 3, f.y - 6));
        p->addLine(CPoint(f.x - 1, f.y - 4));
        p->addLine(CPoint(f.x + 9, f.y - 4));
        p->addLine(CPoint(f.x + 9, f.y + 7));
        p->addLine(CPoint(f.x - 9, f.y + 7));
        p->closeSubpath();
    });
    // Status beside the refresh button.
    auto now = Clock::now();
    bool spin = spinning(now);
    std::string status;
    if (spin)
        status = i18n::str(i18n::StringId::GalleryChecking);
    else if (now < upToDateUntil_)
        status = i18n::str(i18n::StringId::GalleryUpToDate);
    if (!status.empty()) {
        auto f = font(12);
        text(ctx, status, CRect(refresh.left - 8 - 260, refresh.top, refresh.left - 8, refresh.bottom),
             kMuted, f, kRightText);
    }

    CPoint r = refresh.getCenter();
    // One turn per second while checking.
    double angle = spin ? std::fmod(std::chrono::duration<double>(now - spinStart_).count() * 360.0,
                                    360.0)
                        : 0.0;
    {
        CDrawContext::Transform spinTransform(*ctx, CGraphicsTransform().rotate(angle, r));
        strokeIcon(ctx, [&](CGraphicsPath *p) {
            // The usual "rotate clockwise" glyph on a 24-unit grid: a circle
            // from 3 o'clock round through 12 and on towards 1:30, with a corner
            // arrowhead at the top right. Angles in degrees, y down.
            const double kPi = 3.14159265358979;
            const double k = 0.8; // points per grid unit
            auto grid = [&](double x, double y) {
                return CPoint(r.x + (x - 12) * k, r.y + (y - 12) * k);
            };
            auto on = [&](double deg) {
                double a = deg * kPi / 180;
                return grid(12 + 9 * std::cos(a), 12 + 9 * std::sin(a));
            };
            p->beginSubpath(on(0));
            for (double deg = 10; deg <= 315; deg += 10)
                p->addLine(on(deg));
            p->addLine(grid(21, 8));
            p->beginSubpath(grid(21, 3));
            p->addLine(grid(21, 8));
            p->addLine(grid(16, 8));
        });
    }
}

void ThemeGalleryView::drawBanner(CDrawContext *ctx, const CRect &r) {
    fillRound(ctx, r, 10, kBannerBg);
    strokeRound(ctx, r, 10, kBannerBorder);
    auto f = font(13);
    std::string retry = i18n::str(i18n::StringId::GalleryRetry);
    CCoord bw = buttonWidth(ctx, retry, 96);
    CRect btn(r.right - 8 - bw, r.top + 6, r.right - 8, r.bottom - 6);
    button(ctx, btn, retry, Style::Ghost, hoverAction_ == Action::Refresh);
    addHit(btn, Action::Refresh);

    std::string msg = i18n::str(i18n::StringId::GalleryOffline);
    if (!snap_.indexError.empty())
        msg += " (" + snap_.indexError + ")";
    CRect textRect(r.left + 16, r.top, btn.left - 12, r.bottom);
    ctx->setFont(f);
    text(ctx, ellipsize(ctx, msg, textRect.getWidth()), textRect, CColor(233, 217, 190), f);
}

void ThemeGalleryView::drawChips(CDrawContext *ctx, const CRect &row) {
    int all = 0, installed = 0, available = 0;
    for (const auto &t : snap_.themes) {
        all++;
        auto it = states_.find(t.id);
        if (it != states_.end() && it->second.copy.installed())
            installed++;
        else
            available++;
    }
    struct Chip {
        Filter filter;
        Action action;
        const char *label;
        int count;
    };
    const Chip chips[] = {
        {Filter::All, Action::FilterAll, i18n::str(i18n::StringId::GalleryFilterAll), all},
        {Filter::Installed, Action::FilterInstalled,
         i18n::str(i18n::StringId::GalleryPillInstalled), installed},
        {Filter::Available, Action::FilterAvailable,
         i18n::str(i18n::StringId::GalleryFilterAvailable), available},
    };
    auto labelFont = font(13, true);
    auto countFont = font(11);
    CCoord x = row.left;
    for (const auto &chip : chips) {
        std::string count = std::to_string(chip.count);
        CCoord lw = textWidth(ctx, chip.label, labelFont);
        CCoord cw = textWidth(ctx, count, countFont);
        CRect r(x, row.top, x + 14 + lw + 8 + cw + 14, row.bottom);
        bool on = filter_ == chip.filter;
        if (on)
            fillRound(ctx, r, kChipH / 2, kText);
        else
            strokeRound(ctx, r, kChipH / 2,
                        hoverAction_ == chip.action ? kBorderHover : kLine);
        CColor fg = on ? kBg : kBody;
        text(ctx, chip.label, CRect(r.left + 14, r.top, r.left + 14 + lw, r.bottom), fg,
             labelFont);
        CColor countColor = on ? CColor(19, 17, 15, 170) : kMuted;
        text(ctx, count, CRect(r.right - 14 - cw, r.top, r.right - 14, r.bottom), countColor,
             countFont);
        addHit(r, chip.action);
        x = r.right + 8;
    }
}

void ThemeGalleryView::drawGrid(CDrawContext *ctx, const CRect &area) {
    ConcatClip clip(*ctx, area);
    CCoord y = area.top - scrollY_;
    int col = 0;

    auto visibleHit = [&](CRect r, Action a, const std::string &id) {
        r.bound(area);
        if (!r.isEmpty())
            addHit(r, a, id);
    };

    bool loading = snap_.state == ThemeGallery::IndexState::Loading ||
                   snap_.state == ThemeGallery::IndexState::Idle;
    if (snap_.themes.empty()) {
        CRect box(area.left, y, area.right, y + kCardH);
        auto f = font(14);
        text(ctx,
             i18n::str(loading ? i18n::StringId::GalleryLoading : i18n::StringId::GalleryOffline),
             box, kMuted, f, kCenterText);
        y += kCardH + kGap;
    }

    for (const auto &t : snap_.themes) {
        if (!passesFilter(t))
            continue;
        CRect card(area.left + col * (kCardW + kGap), y,
                   area.left + col * (kCardW + kGap) + kCardW, y + kCardH);
        if (card.rectOverlap(area)) {
            drawCard(ctx, t, card);
            visibleHit(card, Action::Select, t.id);
        }
        if (++col == 3) {
            col = 0;
            y += kCardH + kGap;
        }
    }

    // "Make your own theme" spans two columns; start a new row if it
    // doesn't fit in this one.
    if (col == 2) {
        col = 0;
        y += kCardH + kGap;
    }
    CRect own(area.left + col * (kCardW + kGap), y,
              area.left + col * (kCardW + kGap) + 2 * kCardW + kGap, y + kCardH);
    if (own.rectOverlap(area))
        drawMakeYourOwn(ctx, own, area);
    y = own.bottom;

    // Content height and a slim scroll indicator.
    CCoord content = y + scrollY_ - area.top;
    maxScroll_ = std::max<CCoord>(0, content - area.getHeight());
    if (scrollY_ > maxScroll_)
        scrollY_ = maxScroll_;
    if (maxScroll_ > 0) {
        CCoord track = area.getHeight();
        CCoord h = std::max<CCoord>(24, track * area.getHeight() / content);
        CCoord ty = area.top + (track - h) * (scrollY_ / maxScroll_);
        fillRound(ctx, CRect(area.right + 8, ty, area.right + 12, ty + h), 2,
                  CColor(255, 255, 255, 40));
    }
}

void ThemeGalleryView::drawCard(CDrawContext *ctx, const GalleryTheme &t, const CRect &card) {
    const ThemeState &state = states_[t.id];
    bool selected = t.id == selectedId_;
    bool hover = hoverAction_ == Action::Select && hoverId_ == t.id;

    fillRound(ctx, card, 12, kCard);
    CRect thumb(card.left + 1, card.top + 1, card.right - 1, card.top + kThumbH);
    fillRound(ctx, thumb, 11, kStage);
    if (auto *bmp = previewFor(t.id)) {
        // The top of the editor, where the monk's face is.
        CCoord w = kEditorW * kCardScale;
        CPoint origin(thumb.getCenter().x - w / 2, thumb.top - 16);
        drawScaled(ctx, bmp, origin, kCardScale, thumb);
    }
    if (selected) {
        strokeRound(ctx, card, 12, kAccent);
        CRect inner(card);
        inner.inset(1, 1);
        strokeRound(ctx, inner, 11, kAccent);
    } else {
        strokeRound(ctx, card, 12, hover ? kBorderHover : kBorder);
    }

    CCoord body = thumb.bottom;
    // The name keeps at least 96 pt; a long translated label gives way.
    CRect pill = drawPill(ctx, pillFor(state), card.right - 14, body + 36, kCardW - 28 - 96 - 8);
    CCoord tw = pill.left - 8 - (card.left + 14);
    auto nameFont = font(15, true);
    auto smallFont = font(12);
    ctx->setFont(nameFont);
    text(ctx, ellipsize(ctx, t.name, tw),
         CRect(card.left + 14, body + 16, card.left + 14 + tw, body + 36), kText, nameFont);
    std::string by = t.author.empty()
                         ? std::string()
                         : std::string(i18n::str(i18n::StringId::GalleryBy)) + t.author;
    ctx->setFont(smallFont);
    text(ctx, ellipsize(ctx, by, tw),
         CRect(card.left + 14, body + 38, card.left + 14 + tw, body + 56), kMuted, smallFont);
}

void ThemeGalleryView::drawMakeYourOwn(CDrawContext *ctx, const CRect &card, const CRect &clip) {
    strokeRound(ctx, card, 12, kBorder, true);
    CRect c(card.left + 22, card.top + 20, card.right - 22, card.bottom - 20);
    auto title = font(22, true);
    text(ctx, i18n::str(i18n::StringId::GalleryMakeOwnTitle),
         CRect(c.left, c.top, c.right, c.top + 28), kText, title);
    auto body = font(14);
    ctx->setFont(body);
    CCoord ly = c.top + 36;
    for (const auto &line :
         wrapText(ctx, i18n::str(i18n::StringId::GalleryMakeOwnBody), c.getWidth() - 40, 3)) {
        text(ctx, line, CRect(c.left, ly, c.right, ly + 20), kMuted, body);
        ly += 21;
    }
    std::string spec = i18n::str(i18n::StringId::GalleryThemeSpec);
    std::string submit = i18n::str(i18n::StringId::GallerySubmit);
    CCoord sw = buttonWidth(ctx, spec, 120);
    CCoord bw = buttonWidth(ctx, submit, 120);
    CRect specRect(c.left, c.bottom - 44, c.left + sw, c.bottom);
    CRect submitRect(specRect.right + 8, c.bottom - 44, specRect.right + 8 + bw, c.bottom);
    button(ctx, specRect, spec, Style::Ghost, hoverAction_ == Action::Spec);
    button(ctx, submitRect, submit, Style::Ghost, hoverAction_ == Action::Submit);
    for (auto [r, a] : {std::pair{specRect, Action::Spec}, std::pair{submitRect, Action::Submit}}) {
        r.bound(clip);
        if (!r.isEmpty())
            addHit(r, a);
    }
}

void ThemeGalleryView::drawDetail(CDrawContext *ctx, const CRect &panel) {
    fillRound(ctx, panel, 16, kPanel);
    strokeRound(ctx, panel, 16, kLine);
    CRect c(panel.left + kPanelPad, panel.top + kPanelPad, panel.right - kPanelPad,
            panel.bottom - kPanelPad);

    CRect stage(c.left, c.top, c.right, c.top + kStageH);
    fillRound(ctx, stage, 12, kStage);
    const GalleryTheme *t = selectedTheme();
    if (!t)
        return;

    if (auto *bmp = previewFor(t->id)) {
        CCoord w = kEditorW * kStageScale, h = kEditorH * kStageScale;
        CPoint origin(stage.getCenter().x - w / 2, stage.getCenter().y - h / 2);
        drawScaled(ctx, bmp, origin, kStageScale, stage);
    }

    const ThemeState &state = states_[t->id];
    const auto &status = snap_.status[t->id];

    // Name, with an "In use" pill beside it.
    auto nameFont = font(24, true);
    CCoord y = stage.bottom + 18;
    CCoord pillW = 0;
    if (state.active && !state.update) {
        auto pf = font(12, true);
        pillW = textWidth(ctx, i18n::str(i18n::StringId::GalleryPillInUse), pf) + 24 + 10;
    }
    ctx->setFont(nameFont);
    std::string name = ellipsize(ctx, t->name, c.getWidth() - pillW);
    CCoord nw = textWidth(ctx, name, nameFont);
    text(ctx, name, CRect(c.left, y, c.left + nw, y + 32), kText, nameFont);
    if (pillW > 0)
        drawPill(ctx, pillFor(state), c.left + nw + pillW, y + 16);
    y += 34;

    // "by author", linked when the theme has a URL, then version and size.
    auto meta = font(13);
    CCoord x = c.left;
    if (!t->author.empty()) {
        std::string prefix = i18n::str(i18n::StringId::GalleryBy);
        CCoord pw = textWidth(ctx, prefix, meta);
        text(ctx, prefix, CRect(x, y, x + pw, y + 20), kMuted, meta);
        x += pw;
        ctx->setFont(meta);
        std::string author = ellipsize(ctx, t->author, c.right - x - 120);
        CCoord aw = textWidth(ctx, author, meta);
        bool link = !t->url.empty();
        CRect ar(x, y, x + aw, y + 20);
        text(ctx, author, ar, link ? kAccent : kText, meta);
        if (link) {
            if (hoverAction_ == Action::AuthorLink) {
                ctx->setFillColor(kAccent);
                ctx->drawRect(CRect(ar.left, ar.bottom - 2, ar.right, ar.bottom - 1), kDrawFilled);
            }
            addHit(ar, Action::AuthorLink, t->id);
        }
        x += aw;
    }
    std::string facts;
    if (!t->version.empty())
        facts = "v" + t->version;
    facts += (facts.empty() ? "" : " \xC2\xB7 ") + formatSize(t->size);
    if (x > c.left)
        facts = " \xC2\xB7 " + facts;
    text(ctx, facts, CRect(x, y, c.right, y + 20), kMuted, meta);
    y += 34;

    // Description, or the last download error, in whatever room is left.
    CRect buttons(c.left, c.bottom - 44, c.right, c.bottom);
    bool isError = !state.error.empty();
    std::string desc = isError ? state.error
                               : (t->description.empty()
                                      ? i18n::str(i18n::StringId::GalleryNoDescription)
                                      : t->description);
    auto body = font(14);
    ctx->setFont(body);
    size_t maxLines = static_cast<size_t>(std::max<CCoord>(1, (buttons.top - 12 - y) / 21));
    for (const auto &line : wrapText(ctx, desc, c.getWidth(), maxLines)) {
        text(ctx, line, CRect(c.left, y, c.right, y + 20), isError ? kError : kBody, body);
        y += 21;
    }

    // Actions.
    auto hovered = [&](Action a) { return hoverAction_ == a; };
    if (status.installing) {
        progressBar(ctx, buttons, status.progress);
        return;
    }
    std::string sizeSuffix = " \xC2\xB7 " + formatSize(t->size);
    if (!state.copy.installed()) {
        button(ctx, buttons, i18n::str(i18n::StringId::GalleryDownload) + sizeSuffix,
               Style::Primary, hovered(Action::Download));
        addHit(buttons, Action::Download, t->id);
        return;
    }
    if (state.active) {
        if (state.update) {
            button(ctx, buttons, i18n::str(i18n::StringId::GalleryUpdate) + sizeSuffix,
                   Style::Primary, hovered(Action::Download));
            addHit(buttons, Action::Download, t->id);
        } else {
            button(ctx, buttons, i18n::str(i18n::StringId::GalleryCurrent), Style::Quiet, false);
        }
        return;
    }
    std::string second;
    Action secondAction = Action::None;
    if (state.update) {
        second = i18n::str(i18n::StringId::GalleryUpdate);
        secondAction = Action::Download;
    } else if (state.copy.user) {
        second = i18n::str(i18n::StringId::GalleryRemove);
        secondAction = Action::Remove;
    }
    CRect apply(buttons);
    if (secondAction != Action::None) {
        CCoord w = buttonWidth(ctx, second, 104);
        CRect r(buttons.right - w, buttons.top, buttons.right, buttons.bottom);
        apply.right = r.left - 8;
        button(ctx, r, second, Style::Ghost, hovered(secondAction));
        addHit(r, secondAction, t->id);
    }
    button(ctx, apply, i18n::str(i18n::StringId::GalleryApply), Style::Primary,
           hovered(Action::Apply));
    addHit(apply, Action::Apply, t->id);
}

// --- Input ---

const ThemeGalleryView::Hit *ThemeGalleryView::hitAt(const CPoint &local) const {
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it)
        if (it->rect.pointInside(local))
            return &*it;
    return nullptr;
}

void ThemeGalleryView::perform(const Hit &hit) {
    switch (hit.action) {
    case Action::None:
        break;
    case Action::Close:
        if (onClose_)
            onClose_();
        break;
    case Action::OpenFolder:
        openFolder(ThemeManager::getThemesDir());
        break;
    case Action::Refresh:
        gallery_->refresh();
        break;
    case Action::FilterAll:
    case Action::FilterInstalled:
    case Action::FilterAvailable:
        filter_ = hit.action == Action::FilterAll         ? Filter::All
                  : hit.action == Action::FilterInstalled ? Filter::Installed
                                                          : Filter::Available;
        scrollY_ = 0;
        invalid();
        break;
    case Action::Select:
        selectedId_ = hit.id;
        invalid();
        break;
    case Action::Download:
        gallery_->install(hit.id);
        break;
    case Action::Apply: {
        auto copy = ThemeGallery::installedCopy(hit.id);
        if (copy.installed() && onApply_)
            onApply_(copy.path, !copy.user);
        sync();
        invalid();
        break;
    }
    case Action::Remove: {
        // Only a folder directly inside the user themes dir, named by a
        // validated gallery id, and never the theme in use.
        auto copy = ThemeGallery::installedCopy(hit.id);
        fs::path active = activeTheme_ ? activeTheme_() : fs::path();
        if (copy.user && !ThemeGallery::isActive(copy, active)) {
            std::error_code ec;
            fs::remove_all(ThemeGallery::installDir(hit.id), ec);
            sync();
            invalid();
        }
        break;
    }
    case Action::Spec:
        openURL(std::string(ThemeGallery::kRepoUrl) + "#what-goes-in-a-theme-folder");
        break;
    case Action::Submit:
        openURL(std::string(ThemeGallery::kRepoUrl) + "#submitting-a-theme");
        break;
    case Action::AuthorLink:
        if (const GalleryTheme *t = selectedTheme())
            openURL(t->url); // openURL only accepts http(s)
        break;
    }
}

CMouseEventResult ThemeGalleryView::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;
    CPoint local = where;
    local.offset(-getViewSize().left, -getViewSize().top);
    if (const Hit *hit = hitAt(local)) {
        Hit h = *hit; // perform may redraw, which rebuilds hits_
        perform(h);
    }
    return kMouseEventHandled;
}

CMouseEventResult ThemeGalleryView::onMouseMoved(CPoint &where, const CButtonState & /*buttons*/) {
    CPoint local = where;
    local.offset(-getViewSize().left, -getViewSize().top);
    const Hit *hit = hitAt(local);
    Action action = hit ? hit->action : Action::None;
    std::string id = hit ? hit->id : std::string();
    if (action != hoverAction_ || id != hoverId_) {
        hoverAction_ = action;
        hoverId_ = id;
        invalid();
    }
    if (auto *frame = getFrame())
        frame->setCursor(hit ? kCursorHand : kCursorDefault);
    return kMouseEventHandled;
}

CMouseEventResult ThemeGalleryView::onMouseExited(CPoint & /*where*/,
                                                  const CButtonState & /*buttons*/) {
    if (hoverAction_ != Action::None) {
        hoverAction_ = Action::None;
        hoverId_.clear();
        invalid();
    }
    if (auto *frame = getFrame())
        frame->setCursor(kCursorDefault);
    return kMouseEventHandled;
}

void ThemeGalleryView::onMouseWheelEvent(MouseWheelEvent &event) {
    CPoint local = event.mousePosition;
    local.offset(-getViewSize().left, -getViewSize().top);
    if (!gridArea_.pointInside(local) || maxScroll_ <= 0 || event.deltaY == 0.)
        return;
    // Same direction handling as VSTGUI's CScrollbar.
    CCoord delta = event.deltaY;
    if (event.flags & MouseWheelEvent::DirectionInvertedFromDevice)
        delta = -delta;
    CCoord pixels = (event.flags & MouseWheelEvent::PreciseDeltas) ? delta * 10 : delta * 40;
    CCoord next = std::clamp<CCoord>(scrollY_ - pixels, 0, maxScroll_);
    if (next != scrollY_) {
        scrollY_ = next;
        invalid();
    }
    event.consumed = true;
}

} // namespace MonkSynth
